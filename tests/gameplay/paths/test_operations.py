"""Check implemented Path/Wall dispatch and the address-free public contract."""
import importlib.util
import json
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[3]
DOMAIN = ROOT / "adapters/mt2/gameplay/paths"


class PathContractTests(unittest.TestCase):
    def test_saved_descriptors_match_authoring_source(self):
        spec = importlib.util.spec_from_file_location("path_descriptors", Path(__file__).with_name("write_operations.py"))
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        self.assertEqual(module.ROWS, json.loads((DOMAIN / "operations.json").read_text(encoding="utf-8")))

    def test_declarations_match_real_native_dispatch(self):
        rows = json.loads((DOMAIN / "operations.json").read_text(encoding="utf-8"))
        names = [row["name"] for row in rows]
        source = (DOMAIN / "paths.cpp").read_text(encoding="utf-8")
        dispatch = set(re.findall(r'operation\s*[!=]=\s*"((?:paths|walls)\.[a-z_]+)"', source))
        self.assertEqual(len(names), len(set(names)))
        self.assertEqual(set(names), dispatch)
        for row in rows:
            self.assertTrue(row["native_evidence"])
            self.assertIn(row["kind"], ["query", "action"])

    def test_public_types_use_original_uid_handles_and_opaque_history(self):
        types = json.loads((DOMAIN / "types.json").read_text(encoding="utf-8"))
        self.assertEqual(types["MT2Path"]["handle"], "MT2Handle")
        self.assertEqual(types["MT2PathNode"]["handle"], "MT2Handle")
        self.assertEqual(types["MT2PathHistoryResult"]["receipt"], "string")
        self.assertFalse(any("address" in field or "pointer" in field for fields in types.values() for field in fields))


if __name__ == "__main__":
    unittest.main()
