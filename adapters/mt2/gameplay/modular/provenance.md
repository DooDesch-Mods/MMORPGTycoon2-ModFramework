# Modular Building Lua adapter

Original Framework implementation under its MIT license. ABI addresses, class address points, field offsets, native transaction order and executable digest facts were derived from the released Multiplayer REA source at commit `8a13602`. No Multiplayer networking, transport, hook controller or replication implementation is included.

Native evidence:

- `src/game/modular_calls.h`: callable signatures, owned graph allocation and construction, block/entrance admission and mutation, original release-point notification and native history label cleanup.
- `src/game/modular_transaction.h`: native Begin/End scope, Layout inverse before mutation, block release-before-edit and entrance release-after-edit order.
- `src/game/modular_identity.h` and `duplicate_building_history.h`: exact modular Scenery/base class, UID, root/Region ownership, art flag, Layout/Plan owners and transform.
- `src/game/modular_layout_graph.h`: single complete Layout/Plan/transform inverse payload and original virtual method.
- `src/game/subscriber_lifecycle/capture.h` and `record_graph.cpp`: native serializer ABI, record ownership and scalar/tree layouts. The adapter independently reads all original token bits and ordered children within explicit resource budgets. It rejects external SaveContext references.
- `src/game/modular_code.h`: 53 full native body size/SHA-256 facts in `native_pins.inc`. Other directly invoked helpers use the Framework's exact executable pristine-entry verification.
- `src/game/building_history_slice/native_bridge.cpp` and `object_identity/remove_runtime/copy/undo_witness.cpp`: native Undo/Redo ABI, exact top ownership, 20-item stack retention, locked/drained inverse graph checks.

The adapter implements existing Building block and entrance authoring, standalone authored prefab placement and receipt-based native Undo/Redo. All public Building handles use epoch plus full UID. Native addresses remain private. An uncertain history attempt quarantines its receipt until its graph leaves native history. A new action never retries a native mutation. Cleanup failures retain the original failure and the cleanup cause.

The `layout` and `plan` result fields retain native records as ordered rows `[parentIndex, childIndex, [type, bits, text], tokens]`. A root parent is `-1`. Token types are native values 0 through 4; strings retain text and scalar values retain all 32 bits. These are complete inspection values, not native pointer handles.

Prefab loading validates complete native record trees, converts only the detached integer grid representation required by the original factory loader, constructs each owned native Record, resolves the original load context, and captures the complete detached native values before placement. Native Load rebuilds bounds and resets layout origin Y; readback compares all other authored values and checks the resulting geometry fields. Placement calls the original purchase, terrain adjustment, AddBuilding, scenery-clear, release-point, Cash/Destroy inverse and native commit sequence. It does not need an open editor, preview or selected UI object.

The complete root Region Building/Scenery census uses original native serializers and canonical UID manifests. Every cleared Scenery inverse must preserve the original UID, type, root, complete transform, stored height and automatic flag, with exact dependency sets. Cash readback covers the complete financial record and all twenty categories; purchase uses category six expense, while native history records signed income. Release edit readback preserves all eight counters and checks the one original building edit increment.

Prefab history handles Services and Quest destinations added after placement. It captures every Building's ordered Service membership and complete DailyCount values, derives the native first reciprocal removal, validates all MakeService inverses and verifies restoration and unaffected rows. It captures global Quest hash/array membership, owner lists, all record fields, DailyCount values, ratings, task phase and weak ownership. Original SetNPCQuests inverses must cover every affected owner and authored definition. Readback verifies the native removal/renumbering and restoration/swap-last/append behavior; original runtime Quest values are recomputed by the native setter during restoration. Other owner lists remain exact.

Additional native evidence: `src/game/modular_port.inc`, `modular_graph.inc`, `modular_dependencies.inc`, `subscriber_lifecycle/record_decode.cpp`, `duplicate_service_history.h`, `duplicate_quest_history.h`, `monster_quest_retirement.h`, `duplicate_history_value_calls.h` and `duplicate_localized_history.h`.

Validation is an isolated MSVC build and synthetic tests for bounded record reads, native string termination, streaming and extent failures, authored schema/grid conversion, point domains, reciprocal Service closure, Quest removal/renumbering, full financial projection and overflow, stale-world/UID rejection and native Undo/Redo stack transitions. No game or UI mutation was performed. Live native acceptance remains untested because runtime ownership belongs to another chat.
