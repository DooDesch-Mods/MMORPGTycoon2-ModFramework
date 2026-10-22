"""Public Dungeon declarations must match the implemented native dispatch."""
import importlib.util
import json
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[3]
DOMAIN = ROOT / "adapters/mt2/gameplay/dungeons"


class DungeonContractTests(unittest.TestCase):
    def test_saved_descriptors_match_authoring_source(self):
        spec = importlib.util.spec_from_file_location("dungeon_descriptors", Path(__file__).with_name("write_operations.py"))
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        self.assertEqual(module.ROWS, json.loads((DOMAIN / "operations.json").read_text(encoding="utf-8")))

    def test_no_discovery_only_or_undeclared_dispatch(self):
        rows = json.loads((DOMAIN / "operations.json").read_text(encoding="utf-8"))
        names = [row["name"] for row in rows]
        implemented = set(re.findall(r'operation\s*[!=]=\s*"(dungeons\.[a-z_]+)"', (DOMAIN / "dungeons.cpp").read_text(encoding="utf-8")))
        self.assertEqual(len(names), len(set(names)))
        self.assertEqual(set(names), implemented)
        for row in rows:
            self.assertRegex(row["name"], r"^dungeons\.[a-z_]+$")
            self.assertTrue(row["native_evidence"])
            self.assertTrue(row["summary"])
            self.assertIn(row["kind"], ["query", "action"])

    def test_handle_parameters_keep_epoch_and_uid_type(self):
        rows = json.loads((DOMAIN / "operations.json").read_text(encoding="utf-8"))
        for row in rows:
            for parameter in row["parameters"]:
                if parameter["name"] in ["handle", "gizmo"]:
                    self.assertEqual(parameter["type"], "MT2Handle")
        types = json.loads((DOMAIN / "types.json").read_text(encoding="utf-8"))
        self.assertEqual(types["MT2DungeonState"]["dungeon_uid"], "string")


if __name__ == "__main__":
    unittest.main()
