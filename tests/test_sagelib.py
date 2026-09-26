import json
import sqlite3
import struct
import tempfile
import unittest
from pathlib import Path

from sagelib import build, query, resource_hash, type_hash


class SageLibraryTests(unittest.TestCase):
    def test_offline_index_and_build_invalidation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            game = root / "game"
            (game / "bin").mkdir(parents=True)
            (game / "data" / "game").mkdir(parents=True)
            (game / "bin" / "helldivers2.exe").write_bytes(b"first-exe")
            (game / "data" / "game" / "game.dll").write_bytes(b"first-dll")
            c4 = "content/fac_helldivers/equipment/backpacks/c4_charge_backpack/c4_charge_backpack"
            names = root / "names.txt"
            unknown_name = "content/test/unknown_entity"
            names.write_text(c4 + "\n" + unknown_name + "\n", encoding="utf-8")
            types = root / "types.txt"
            types.write_text("DepositComponentData\nUnknownComponentData\nWeaponRoundsComponentData\n", encoding="utf-8")
            entities = root / "entities.bin"
            deposit = bytearray(5488)
            struct.pack_into("<QII", deposit, 688, resource_hash(c4), 10, 0)
            struct.pack_into("<III", deposit, 2448, 6, 6, 3)
            header = b"LDLD" + struct.pack("<III", 1, 0xC435BA85, len(deposit)) + b"\0" * 8
            unknown = bytearray(48)
            struct.pack_into("<QII", unknown, 0, resource_hash(unknown_name), 0, 0)
            extra = b"LDLD" + struct.pack("<III", 1, type_hash("UnknownComponentData"), len(unknown)) + b"\0" * 8
            entities.write_bytes(header + deposit + extra + unknown)
            rounds = bytearray(4336)
            struct.pack_into("<QII", rounds, 624, 0x72170A55A1F37FF1, 8, 0)
            struct.pack_into("<ffII", rounds, 800 + 8 * 136 + 72, 2.0, 0.0, 40, 40)
            rounds_header = b"LDLD" + struct.pack("<III", 1, type_hash("WeaponRoundsComponentData"), len(rounds)) + b"\0" * 8
            entities.write_bytes(entities.read_bytes() + rounds_header + rounds)
            damage = root / "damage.decoded"
            record = bytearray(76)
            struct.pack_into("<Iii4II", record, 0, 206, 450, 225, 4, 4, 4, 0, 20)
            damage.write_bytes(b"LDLD" + struct.pack("<III", 1, 3769052400, 92)
                               + b"\0" * 24 + record)
            projectiles = root / "projectiles.bin"
            first, second = bytearray(272), bytearray(272)
            struct.pack_into("<I", first, 0, 132)
            struct.pack_into("<I", second, 0, 96)
            struct.pack_into("<IfI", second, 0, 96, 1.0, 0)
            struct.pack_into("<I", second, 60, 206)
            struct.pack_into("<I", second, 144, 300)
            projectiles.write_bytes(b"\0" * 16 + first + second)
            database = root / "index.sqlite3"
            build(database, entities=entities, damage=damage, projectiles=projectiles,
                  names=names, types=types, game_root=game, source="runtime-export")
            c4_result = query(database, "c4_charge_backpack", game_root=game)
            self.assertEqual(c4_result["evidence"], "matching_build_export_not_live")
            self.assertEqual(c4_result["results"][0]["fields"],
                             {"starting": 6, "maximum": 6, "refill": 3})
            dbs2 = query(database, "0x72170A55A1F37FF1", game_root=game)["results"][0]
            self.assertEqual(dbs2["record_index"], 8)
            self.assertEqual(dbs2["record_offset"], 1888)
            self.assertEqual(dbs2["fields"], {"magazine_capacity": 2.0,
                                                "magazine_capacity_secondary": 0.0,
                                                "ammo_capacity": 40, "ammo_refill": 40})
            self.assertEqual(query(database, "damage:206", game_root=game)["results"][0]["fields"]["damage"], 450)
            self.assertEqual(query(database, "projectile:96", game_root=game)["results"][0]["fields"]["impact_explosion"], 300)
            candidate = query(database, unknown_name, game_root=game)["results"]
            self.assertEqual(candidate[0]["confidence"], "candidate_mapping_only_unknown_layout")
            self.assertEqual(candidate[0]["fields"], {})
            self.assertEqual(len(query(database, "c4_charge_backpack%", game_root=game)["results"]), 0)
            (game / "data" / "game" / "game.dll").write_bytes(b"changed-dll")
            self.assertEqual(query(database, c4, game_root=game)["evidence"], "stale_game_build")
            projectiles.write_bytes(b"invalid")
            with self.assertRaises(ValueError):
                build(database, projectiles=projectiles, source="historical")
            self.assertEqual(query(database, "damage:206")["results"][0]["fields"]["damage"], 450)

    def test_historical_never_promoted(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            projectiles = root / "projectiles.bin"
            projectile = bytearray(272)
            struct.pack_into("<I", projectile, 0, 10)
            projectiles.write_bytes(b"\0" * 16 + projectile)
            database = root / "index.sqlite3"
            build(database, projectiles=projectiles)
            self.assertEqual(query(database, "projectile:10")["evidence"], "historical_not_current")
            with self.assertRaises(ValueError):
                build(database, projectiles=projectiles, source="runtime-export")


if __name__ == "__main__":
    unittest.main()
