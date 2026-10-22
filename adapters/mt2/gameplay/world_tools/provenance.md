# World tools native adapter

This is original MT2 Framework code under its MIT license. The implementation
uses native ABI, layout, address, and digest facts retained in Multiplayer commit
`8a13602`. It does not copy Multiplayer transport, session, authority, action
controller, or snapshot adoption implementations.

Evidence sources in that reference checkout:

- `src/game/developer_staff/read.cpp`, `action_native.cpp`, `action_calls.cpp`,
  `scalar.cpp`, `scalar_calls.cpp`, and `entry_fingerprints.h`: the fifty-person
  pool, native Hire/Fire command transaction, hiring charge, employment update,
  manager membership, and guarded jobless actor properties.
- `src/game/general_project_reader.h`, `general_project_state.h`,
  `general_project_runtime.cpp`, and `general_project_pins.inc`: source-keyed
  research, art, statistic, and other native project catalogue entries; original
  payment, start, cancel, commit, and worker allocation calls.
- `src/game/tutorial_replication/start_action_native.inc`, `start_ledger.h`,
  `staff_native.inc`, `startingpoint_native.inc`, and
  `startingpoint_commit_intent.inc`: research price accounting, source ownership,
  worker bounds, native completion, and deployment prerequisites.
- `src/game/connected_network_reader.h`, `topology_fields.h`,
  `network_pipe_preview.h`, `ui_network_runtime.inc`, and `ui_network_pipe.inc`:
  committed topology, node placement, Cable preview, Junction construction,
  original purchase/finish transactions, and inverse history.
- `src/game/network_remove_runtime.inc` and
  `object_identity/remove_runtime/copy/undo_witness.cpp`: original network
  deletion, UID retirement, and native undo ownership.

`native_pins.inc` contains digest facts only. Its generator reads the sibling
reference source and never writes executable bytes. The longest retained body
is used for a shared address. Calls without a retained digest use the common
exact-executable and acknowledged-route verifier. Public values contain typed
fields, epoch-scoped UID handles, and source values; no native pointer is public.

Developer Hire/Fire invokes the original synchronous native command on the
already-owned DevelopmentWindow. It does not open the window, change its
visibility, send input, or wait for a user callback. The separate original
employment setter runs only after the immediate native pool and financial
readback match the expected transaction.

Network placement uses original native payment, creation, and inverse-history
functions. Cable placement constructs a private native behaviour and validates
its original preview before Finish. Successful retirement destroys that private
behaviour after the committed graph and inverse operations are checked. An
exception or uncertain partial construction is retained as unknown and is never
automatically replayed or cleaned up as though ownership were known.

The source does not expose network move. The retained network runtime proves
placement and deletion transactions; no corresponding server-node/pipe move
transaction was found in that scope. Construction developer job attachment also
requires additional native SaveContext/link resolution and job-queue closure;
the available scalar action proof admits only actors without a job or carried
object. Research assignment uses the proved current-project worker allocator.

The focused build compiles the actual adapter. Synthetic tests exercise native
project/network queries, financial and numeric boundaries, and rejection of a
stale epoch or foreign shard. They do not establish effects in a running game.
No game process, input, user save, or native UI was changed during authoring.
