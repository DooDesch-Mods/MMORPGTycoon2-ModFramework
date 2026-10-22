"""Check that each public Scenery operation has an actual adapter dispatch."""
import json
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[3]
DOMAIN = ROOT / "adapters/mt2/gameplay/scenery"


class SceneryContractTests(unittest.TestCase):
    def test_declared_operations_match_implemented_dispatch(self):
        rows = json.loads((DOMAIN / "operations.json").read_text(encoding="utf-8"))
        names = [row["name"] for row in rows]
        source = (DOMAIN / "scenery.cpp").read_text(encoding="utf-8")
        dispatch = set(re.findall(r'operation\s*[!=]=\s*"(scenery\.[a-z_]+)"', source))
        self.assertEqual(len(names), len(set(names)))
        self.assertEqual(set(names), dispatch)
        for row in rows:
            self.assertTrue(row["native_evidence"])
            self.assertTrue(row["summary"])
            self.assertRegex(row["name"], r"^scenery\.[a-z_]+$")

    def test_group_handle_contract_remains_address_free(self):
        types = json.loads((DOMAIN / "types.json").read_text(encoding="utf-8"))
        self.assertEqual(types["MT2SceneryGroup"]["handle"], "MT2Handle")
        self.assertEqual(types["MT2SceneryGroupMember"]["handle"], "MT2Handle")
        self.assertFalse(any("address" in field for value in types.values() for field in value))


if __name__ == "__main__":
    unittest.main()
