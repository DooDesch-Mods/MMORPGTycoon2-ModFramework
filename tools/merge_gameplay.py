"""Merge authored gameplay descriptors into the canonical public API source."""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
source = ROOT / "api/contract.json"
contract = json.loads(source.read_text(encoding="utf-8"))
contract["functions"] = [f for f in contract["functions"] if "gameplay_kind" not in f]
common = {
    "capabilities": ["world.read"], "since": "1.2.0", "minimum_framework_version": "0.3.0",
    "experimental_contract": "gameplay", "experimental_contract_version": 1,
    "stability": "experimental-gameplay-1", "thread": "Framework game-thread callback.",
    "lifecycle": "After world_ready and before world_leave; do not perform game calls in render callbacks.",
    "side_effects": "Reads a bounded native snapshot without changing the world.",
    "reload": "Old handles expire on world change. Accepted actions from replaced mod instances are rejected before native execution.",
    "persistence": "No implicit persistence. Do not persist live handles across world loads.",
    "example": "examples/gameplay/main.lua",
}
rows = []
cmake_sources = (ROOT / "adapters/mt2/CMakeLists.txt").read_text(encoding="utf-8")
dispatch_source = (ROOT / "adapters/mt2/gameplay/dispatch.cpp").read_text(encoding="utf-8")
for directory in sorted((ROOT / "adapters/mt2/gameplay").iterdir()):
    if not directory.is_dir() or not (directory / "operations.json").is_file():
        continue
    module_source = f"gameplay/{directory.name}/{directory.name}.cpp"
    module_header = f'"{directory.name}/{directory.name}.h"'
    if module_source not in cmake_sources or module_header not in dispatch_source:
        raise RuntimeError(f"Unwired gameplay descriptor: {directory.name}")
    for row in json.loads((directory / "operations.json").read_text()):
        function = {**common, **row}
        function["name"] = "mt2." + row["name"]
        function["group"] = row["name"].split(".")[0]
        function["gameplay_kind"] = function.pop("kind")
        function["host_operations"] = ["gameplay." + function["gameplay_kind"]]
        if function["gameplay_kind"] == "action":
            function["capabilities"] = ["objects.write"]
            function["native_result"] = row["returns"]
            function["returns"] = "MT2Action"
            function["side_effects"] = "Queues one game-thread mutation. Accepted is not success; inspect actions.status or action_completed. Unknown outcomes are never automatically replayed."
        rows.append(function)
    types = directory / "types.json"
    if types.is_file():
        contract["types"].update(json.loads(types.read_text()))
contract["functions"] += rows
contract["functions"].append({**common,
    "name": "mt2.experimental.operations", "group": "experimental", "parameters": [],
    "returns": "table[]", "summary": "Discover the experimental Lua gameplay operations in this runtime.",
    "errors": ["The game adapter is unavailable or the mod lacks world.read."],
    "gameplay_kind": "discovery", "host_operations": ["gameplay.operations"],
    "lifecycle": "An active game-thread callback; a loaded world is not required."})
contract["framework_version"] = "0.3.0"
contract["api_version"] = "1.2.0"
contract["experimental_contracts"]["gameplay"] = {"version": 1, "path": "api/experimental/gameplay.json"}
source.write_text(json.dumps(contract, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
print(json.dumps({"gameplay_operations": len(rows), "public_functions": len(contract["functions"])}))
