# Helldivers 2 SageLibrary

[简体中文](README.md) · [Bingus Shared Loader](https://github.com/CowboyBingus/BingusSharedLoader)

A small **read-only** field finder for Helldivers 2 mod authors and AI agents. Build a local SQLite index once; then query resource names, 64-bit resource hashes, damage IDs, or projectile IDs through JSON CLI output or an optional MCP tool. Requires Python 3.10+; the CLI uses only the standard library. The repository ships **no game data**, cannot automatically decode newly wrapped game files, and never writes to the game process.

> **Lua MOD prerequisite:** Searching the index does **not** require Bingus. To develop/run the Lua mods described by this skill, install [BingusSharedLoader](https://github.com/CowboyBingus/BingusSharedLoader). It loads Lua addons; it is **not** a general-purpose game-field API. Match your loader and game versions.

## Quick start

Provide your own **decoded** files: an LDLD entity blob (raw or gzip), optionally decoded damage settings and a projectile table, plus optional newline-separated resource names and type names. [Filediver](https://github.com/xypwn/filediver) can help inspect game assets; extraction alone does not prove the data is decoded or establish field semantics. Supply only the inputs you actually have:

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
