"""Read-only, version-aware search of user-supplied Helldivers 2 data."""

import argparse
from contextlib import closing
import gzip
import hashlib
import json
import os
import re
import sqlite3
import struct
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent
DEFAULT_DB = Path(os.environ.get("SAGELIB_DB", str(ROOT / ".sagelib" / "index.sqlite3")))
DEFAULT_GAME = os.environ.get("SAGELIB_GAME_ROOT")
MAX_INPUT = 256 * 1024 * 1024
SCHEMAS = {
    "DepositComponentData": (5488, 928, 152, 30),
    "WeaponReloadComponentData": (27968, 7968, 80, 250),
}


def resource_hash(name):
    data = name.encode("utf-8")
    mask, mix = (1 << 64) - 1, 0xC6A4A7935BD1E995
    value = len(data) * mix & mask
    end = len(data) // 8 * 8
    for (word,) in struct.iter_unpack("<Q", data[:end]):
        word = word * mix & mask
        word ^= word >> 47
        value = (value ^ (word * mix & mask)) * mix & mask
    if data[end:]:
        value = (value ^ int.from_bytes(data[end:], "little")) * mix & mask
    value ^= value >> 47
    value = value * mix & mask
    return value ^ (value >> 47)


def type_hash(name):
    value = 5381
    for character in name:
        value = (value * 33 + ord(character)) & 0xFFFFFFFF
    return (value - 5381) & 0xFFFFFFFF


def digest(path):
    result = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(chunk)
    return result.hexdigest().upper()


def read_data(path):
    path = Path(path)
    if path.stat().st_size > MAX_INPUT:
        raise ValueError("Input file too large")
    opener = gzip.open if path.suffix == ".gz" else open
    with opener(path, "rb") as stream:
        raw = stream.read(MAX_INPUT + 1)
    if len(raw) > MAX_INPUT:
        raise ValueError("Decoded input too large")
    return raw


def fingerprint_game(path):
    game = Path(path)
    files = (game / "bin" / "helldivers2.exe", game / "data" / "game" / "game.dll")
    if not all(item.is_file() for item in files):
        raise ValueError("Game executable or game.dll not found")
    return {"exe": digest(files[0]), "game_dll": digest(files[1])}


def decode_type_names(path):
    if path is None:
        return {type_hash(name): name for name in SCHEMAS}
    with Path(path).open(encoding="utf-8", errors="replace") as stream:
        return {type_hash(name): name for raw in stream
                if (name := raw.strip()) and not name.startswith("#") and name.endswith("ComponentData")}


def entity_tables(raw, names):
    prefix = b"LDLD" + struct.pack("<I", 1)
    offset = 0
    while (offset := raw.find(prefix, offset)) != -1:
        if offset + 24 > len(raw):
            break
        kind, size = struct.unpack_from("<II", raw, offset + 8)
        name = names.get(kind)
        if name and 0 < size <= len(raw) - offset - 24:
            yield name, raw[offset + 24:offset + 24 + size]
        offset += 8


def create_schema(database):
    database.executescript("""
        CREATE TABLE meta (key TEXT PRIMARY KEY, value TEXT NOT NULL);
        CREATE TABLE names (hash TEXT NOT NULL, name TEXT NOT NULL, PRIMARY KEY (hash, name));
        CREATE TABLE records (
            category TEXT NOT NULL, identifier TEXT NOT NULL, resource_hash TEXT,
            table_name TEXT, map_offset INTEGER, record_index INTEGER,
            record_offset INTEGER, fields TEXT NOT NULL, confidence TEXT NOT NULL,
            PRIMARY KEY (category, identifier, table_name)
        );
        CREATE INDEX records_resource ON records(resource_hash);
        CREATE INDEX records_identifier ON records(identifier);
        CREATE INDEX names_lookup ON names(name COLLATE NOCASE);
    """)


def add_entities(database, raw, names):
    count = 0
    for name, data in entity_tables(raw, names):
        schema = SCHEMAS.get(name)
        if not schema or len(data) != schema[0]:
            for offset in range(0, min(len(data), 65536) - 15, 16):
                resource, index, reserved = struct.unpack_from("<QII", data, offset)
                if reserved or index >= 10000 or resource >> 32 == 0 or resource & 0xFFFFFFFF == 0:
                    continue
                identifier = f"0x{resource:016X}"
                database.execute("INSERT OR IGNORE INTO records VALUES (?,?,?,?,?,?,?,?,?)",
                                 ("entity", identifier, identifier, name, offset, index, None,
                                  "{}", "candidate_mapping_only_unknown_layout"))
                count += 1
            continue
        _, data_start, stride, rows = schema
        if data_start + rows * stride != len(data):
            continue
        for offset in range(0, data_start, 16):
            if offset + 16 > data_start:
                break
            resource, index, reserved = struct.unpack_from("<QII", data, offset)
            if not resource or reserved or index >= rows:
                continue
            record = data_start + index * stride
            if name == "DepositComponentData":
                values = struct.unpack_from("<III", data, record)
                fields = {"starting": values[0], "maximum": values[1], "refill": values[2]}
                confidence = "layout_correlated_not_semantically_proven"
            else:
                fields = {"ability_id": struct.unpack_from("<I", data, record + 4)[0],
                          "duration": struct.unpack_from("<f", data, record + 56)[0],
                          "zero_duration_uses_default_ability": True}
                confidence = "layout_documented_verify_behavior_in_game"
            identifier = f"0x{resource:016X}"
            database.execute("INSERT OR REPLACE INTO records VALUES (?,?,?,?,?,?,?,?,?)",
                             ("entity", identifier, identifier, name, offset, index, record,
                              json.dumps(fields), confidence))
            count += 1
    return count


def add_damage(database, raw):
    stride = 76
    matches = []
    position = 0
    while (position := raw.find(b"LDLD" + struct.pack("<I", 1), position)) != -1:
        if position + 40 <= len(raw):
            size = struct.unpack_from("<I", raw, position + 12)[0]
            count, remainder = divmod(size - 16, stride)
            start = position + 40
            if (remainder == 0 and 0 < count <= 10000 and start + count * stride == len(raw)):
                ids = [struct.unpack_from("<I", raw, start + index * stride)[0] for index in range(count)]
                if len(set(ids)) == count and all(ids):
                    matches.append((start, ids))
        position += 8
    if len(matches) != 1:
        raise ValueError("Damage records do not match the decoded, self-ID layout")
    start, identifiers = matches[0]
    for index, damage_id in enumerate(identifiers):
        offset = start + index * stride
        identifier = str(damage_id)
        fields = {"damage": struct.unpack_from("<i", raw, offset + 4)[0],
                  "durable_damage": struct.unpack_from("<i", raw, offset + 8)[0],
                  "armor_penetration": list(struct.unpack_from("<4I", raw, offset + 12)),
                  "demolition": struct.unpack_from("<I", raw, offset + 28)[0]}
        database.execute("INSERT INTO records VALUES (?,?,?,?,?,?,?,?,?)",
                         ("damage", identifier, None, "DamageSettings", None, index, offset,
                          json.dumps(fields), "documented_binary_layout_not_weapon_binding"))
    return len(identifiers)


def add_projectiles(database, raw):
    stride = 272
    if len(raw) < 16 or (len(raw) - 16) % stride:
        raise ValueError("Projectile table has an unsupported layout")
    seen = set()
    for index in range((len(raw) - 16) // stride):
        offset = 16 + index * stride
        projectile_id = struct.unpack_from("<I", raw, offset)[0]
        if not projectile_id or projectile_id in seen:
            raise ValueError("Missing or repeated projectile self-ID")
        seen.add(projectile_id)
        fields = {"speed": struct.unpack_from("<f", raw, offset + 32)[0],
                  "damage_id": struct.unpack_from("<I", raw, offset + 60)[0],
                  "impact_explosion": struct.unpack_from("<I", raw, offset + 144)[0]}
        database.execute("INSERT INTO records VALUES (?,?,?,?,?,?,?,?,?)",
                         ("projectile", str(projectile_id), None, "ProjectileSettings", None,
                          index, offset, json.dumps(fields), "layout_documented_not_weapon_binding"))
    return len(seen)


def add_names(database, filename):
    if filename is None:
        return 0
    count = 0
    with Path(filename).open(encoding="utf-8", errors="replace") as stream:
        for raw in stream:
            name = raw.strip()
            if not name or name.startswith("#"):
                continue
            database.execute("INSERT OR IGNORE INTO names VALUES (?,?)",
                             (f"0x{resource_hash(name):016X}", name))
            count += 1
    return count


def build(database_file, *, entities=None, damage=None, projectiles=None, names=None,
          types=None, game_root=None, source="historical"):
    if not (entities or damage or projectiles):
        raise ValueError("At least one decoded input is required")
    if source == "runtime-export" and not game_root:
        raise ValueError("runtime-export requires --game-root to pin executable and DLL hashes")
    database_file = Path(database_file)
    database_file.parent.mkdir(parents=True, exist_ok=True)
    temporary = database_file.with_name(database_file.name + ".new")
    if temporary.exists():
        temporary.unlink()
    try:
        with closing(sqlite3.connect(temporary)) as database:
            with database:
                create_schema(database)
                inputs = {key: value for key, value in (("entities", entities), ("damage", damage),
                                                        ("projectiles", projectiles)) if value}
                provenance = {key: digest(path) for key, path in inputs.items()}
                version = fingerprint_game(game_root) if game_root else None
                metadata = {"format": 1, "source": source, "game": version, "input_sha256": provenance}
                database.execute("INSERT INTO meta VALUES (?,?)", ("provenance", json.dumps(metadata)))
                statistics = {"names": add_names(database, names)}
                if entities:
                    statistics["entity_records"] = add_entities(database, read_data(entities), decode_type_names(types))
                if damage:
                    statistics["damage_records"] = add_damage(database, read_data(damage))
                if projectiles:
                    statistics["projectiles"] = add_projectiles(database, read_data(projectiles))
        os.replace(temporary, database_file)
        return {"database": str(database_file), "statistics": statistics, "provenance": metadata}
    except Exception:
        if temporary.exists():
            temporary.unlink()
        raise


def confidence(meta, game_root):
    if meta["source"] == "historical":
        return "historical_not_current"
    if not game_root or not meta["game"]:
        return "unverified_current_install"
    try:
        return ("matching_build_export_not_live" if fingerprint_game(game_root) == meta["game"]
                else "stale_game_build")
    except (OSError, ValueError):
        return "unverified_current_install"


def query(database_file, term, *, game_root=None, limit=20):
    if not 1 <= limit <= 100:
        raise ValueError("limit must be between 1 and 100")
    if not Path(database_file).is_file():
        raise ValueError("Index missing: first run build")
    with closing(sqlite3.connect(Path(database_file).resolve().as_uri() + "?mode=ro", uri=True)) as database:
        database.row_factory = sqlite3.Row
        meta = json.loads(database.execute("SELECT value FROM meta WHERE key='provenance'").fetchone()[0])
        if meta.get("format") != 1:
            raise ValueError("Unsupported index version: rebuild the index")
        matching = re.fullmatch(r"(damage|projectile):(\d+)", term, re.I)
        if matching:
            rows = database.execute("SELECT * FROM records WHERE category=? AND identifier=? LIMIT ?",
                                    (matching[1].lower(), str(int(matching[2])), limit)).fetchall()
        elif re.fullmatch(r"(?:0x)?[0-9a-fA-F]{1,16}", term):
            identifier = f"0x{int(term, 16):016X}"
            rows = database.execute("SELECT * FROM records WHERE resource_hash=? LIMIT ?", (identifier, limit)).fetchall()
        else:
            # Bound the names scan; patterns are never interpreted as SQL wildcards.
            escaped = term.lower().replace("\\", "\\\\").replace("%", "\\%").replace("_", "\\_")
            rows = database.execute("""SELECT DISTINCT records.* FROM records
                JOIN names ON names.hash = records.resource_hash
                WHERE LOWER(names.name) LIKE ? ESCAPE '\\' ORDER BY records.table_name, records.identifier LIMIT ?""",
                ("%" + escaped + "%", limit)).fetchall()
        result = []
        for row in rows:
            item = dict(row)
            item["fields"] = json.loads(item["fields"])
            if item["resource_hash"]:
                item["names"] = [name for (name,) in database.execute(
                    "SELECT name FROM names WHERE hash=? LIMIT 5", (item["resource_hash"],))]
            result.append(item)
    level = confidence(meta, game_root)
    return {"query": term, "evidence": level, "game": meta["game"],
            "input_sha256": meta["input_sha256"], "results": result,
            "warning": ("Field semantics and weapon bindings require independent checks; "
                        "historical/unverified/stale data must not be used as current game values.")}


def main(argv=None):
    parser = argparse.ArgumentParser(description="Local HD2 SageLibrary field index (read-only)")
    commands = parser.add_subparsers(dest="command", required=True)
    index = commands.add_parser("build", help="Index user-supplied decoded data")
    index.add_argument("--entities", type=Path, help="Decoded generated_entities data (raw or .gz)")
    index.add_argument("--damage", type=Path, help="Decoded damage settings .dl_bin")
    index.add_argument("--projectiles", type=Path, help="Exported projectile table .bin")
    index.add_argument("--names", type=Path, help="Optional resource-name lines, e.g. cracked.txt")
    index.add_argument("--types", type=Path, help="Optional type-name lines, e.g. dl_type_names.txt")
    index.add_argument("--source", choices=("historical", "runtime-export"), default="historical")
    index.add_argument("--game-root", type=Path)
    index.add_argument("--db", type=Path, default=DEFAULT_DB)
    search = commands.add_parser("search", help="Search names, resource hashes or damage:/projectile: IDs")
    search.add_argument("term")
    search.add_argument("--db", type=Path, default=DEFAULT_DB)
    search.add_argument("--game-root", type=Path, default=DEFAULT_GAME)
    search.add_argument("--limit", type=int, default=20)
    args = parser.parse_args(argv)
    try:
        if args.command == "build":
            result = build(args.db, entities=args.entities, damage=args.damage,
                           projectiles=args.projectiles, names=args.names, types=args.types,
                           game_root=args.game_root, source=args.source)
        else:
            result = query(args.db, args.term, game_root=args.game_root, limit=args.limit)
        print(json.dumps(result, ensure_ascii=False, indent=2))
    except (OSError, ValueError, sqlite3.Error) as error:
        print(json.dumps({"error": str(error)}, ensure_ascii=False), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
