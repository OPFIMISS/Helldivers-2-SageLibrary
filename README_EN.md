# Helldivers 2 SageLibrary

[简体中文](README.md) · [Bingus Shared Loader](https://github.com/CowboyBingus/BingusSharedLoader)

A small **read-only** field finder for Helldivers 2 mod authors and AI agents. Build a local SQLite index once; then query resource names, 64-bit resource hashes, damage IDs, or projectile IDs through JSON CLI output or an optional MCP tool. Requires Python 3.10+; the CLI uses only the standard library. The repository ships **no game data**, cannot automatically decode newly wrapped game files, and never writes to the game process.

> **Lua MOD prerequisite:** Searching the index does **not** require Bingus. To develop/run the Lua mods described by this skill, install [BingusSharedLoader](https://github.com/CowboyBingus/BingusSharedLoader). It loads Lua addons; it is **not** a general-purpose game-field API. Match your loader and game versions.

## Static fields and projectiles: DBS-2 example

Index your own decoded entities and run `python sagelib.py search 0x72170A55A1F37FF1`. In the verified build, `WeaponRoundsComponentData` maps this resource at offset 624 to **record index** 8, not capacity 8. Data begins at 800; stride is 136; the record begins at `800 + 8 * 136 = 1888`. Record +72 holds float32 shell capacity 2.0, +76 secondary capacity 0.0, +80/+84 spare/refill 40/40. A four-shell edit only changes `00000040→00008040` (float32). Check the indexed game EXE/DLL hashes; **the resulting addon has not yet been validated in gameplay**.

Use `python sagelib.py search damage:206` for damage, durable damage, penetration angles and demolition; use `python sagelib.py search projectile:96` for projectile speed, linked damage ID and impact explosion. Resource hashes, damage IDs and projectile IDs are distinct. Verify weapon binding and shared projectiles before changing a payload. C4 `DepositComponentData` integer reserve values and rifle `WeaponMagazineComponentData` integer magazine values do not apply to this float capacity.

## Quick start

Provide your own **decoded** files: an LDLD entity blob (raw or gzip), optionally decoded damage settings and a projectile table, plus optional newline-separated resource names and type names. [Filediver](https://github.com/xypwn/filediver) can help inspect game assets; extraction alone does not prove the data is decoded or establish field semantics. Supply only the inputs you actually have:

### Three steps: find the C4 backpack by ID

Clone the repository, enter it, then replace the example path with **your own decoded** entity data. A missing file or still-wrapped game data cannot yield field results.

```bash
git clone git@github.com:OPFIMISS/Helldivers-2-SageLibrary.git
cd Helldivers-2-SageLibrary
python sagelib.py build --entities /data/generated_entities.decoded.gz --source historical
python sagelib.py search 0x2A18F81C44A26771
```

The sample ID `0x2A18F81C44A26771` identifies the **C4 backpack resource**; it is not a damage ID. The search returns JSON. Relevant excerpt (the full result also includes input SHA-256):

```json
{
  "evidence": "historical_not_current",
  "results": [{
    "table_name": "DepositComponentData",
    "record_index": 10,
    "record_offset": 2448,
    "fields": {"starting": 6, "maximum": 6, "refill": 3}
  }]
}
```

`historical_not_current` means **this value comes from historical decoded input**, not a verified current-game reading. With a newline-separated `cracked.txt`, add `--names /data/cracked.txt` when building and search `python sagelib.py search c4_charge_backpack` instead. Build once, then search other IDs without rebuilding: with decoded damage data indexed, `python sagelib.py search damage:206` looks up a **damage ID**; use `projectile:96` for a projectile ID. For game-update checks, see “Updates and provenance” below.

### More inputs: damage and projectiles

```bash
python sagelib.py build --entities /data/generated_entities.decoded.gz --damage /data/generated_damage_settings.decoded.dl_bin --projectiles /data/ProjectileSettings.bin --names /data/cracked.txt --types /data/dl_type_names.txt --source historical
python sagelib.py search c4_charge_backpack
python sagelib.py search 0x2A18F81C44A26771
python sagelib.py search damage:206
python sagelib.py search projectile:96
```

Use `--db /path/index.sqlite3` to choose the database location. Name files contain one original resource path per line; hash searches work without them. Supported semantic candidates are Deposit starting/maximum/refill, WeaponReload Ability/Duration, and self-ID-checked damage/projectile records. With a type-name file, other components produce only `candidate_mapping_only_unknown_layout` resource references; unknown bytes are never interpreted as fields. Each JSON result includes provenance, source SHA-256 hashes, and offsets; stdout contains JSON only.

## Updates and provenance

| `evidence` | Meaning |
| --- | --- |
| `historical_not_current` | Historical reference. A coincidental executable hash match never promotes it to current. |
| `matching_build_export_not_live` | User-declared runtime export; current executable and game DLL hashes match the hashes recorded during indexing. **Not a live reading.** |
| `stale_game_build` | Installed game hashes changed. Do not reuse historical offsets. |
| `unverified_current_install` | Installed game unavailable/unset; current-version match cannot be checked. |

For a one-time **read-only export from one build**, rebuild with `--source runtime-export --game-root /path/to/game` and query with `--game-root /path/to/game`. `runtime-export` is **the indexer's assertion about the supplied files**, not independently verified proof of where those files came from. Do not mix old references and new exports under this label. On every game update, obtain and validate newly decoded data or take a new read-only export and rebuild. This tool does **not** automatically unwrap the game's `data/game/generated_*.dl_bin` format, attach to a game process, or guarantee a fresh field value when inputs are stale.

## Optional MCP server

Run `python -m pip install "mcp>=1.30,<2"`, configure your stdio MCP client to launch `python /absolute/path/Helldivers-2-SageLibrary/mcp_server.py`, and set `SAGELIB_DB` to the absolute path of the indexed SQLite file. Optionally set `SAGELIB_GAME_ROOT` to the game install root. The read-only tool `search_fields(query_text, limit=20)` does not accept arbitrary file paths, rebuild indexes, or modify the game. Client configuration formats vary. This optional entry point targets the official SDK 1.x; SDK 2.x has breaking API changes.

## Tests and skill

Run `python -m unittest discover -s tests -v`. Tests use synthetic bytes and no copyrighted assets. See the [HD2 Lua mod skill](skills/helldivers2-lua-mod/SKILL.md) and [C4 case study](skills/helldivers2-lua-mod/references/C4_CASE.md). A candidate number is not a confirmed gameplay effect; correlate peer records, lock the version, and verify behavior in game when needed.
