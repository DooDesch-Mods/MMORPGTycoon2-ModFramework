# Management adapter provenance

This is original MT2 Framework adapter code under the Framework's MIT license.
It does not include the Multiplayer transport, action controller, session code,
or Multiplayer runtime implementations.

Native calling conventions, singleton locations, class address points, member
offsets, original function addresses, and digest facts were checked against
Multiplayer source commit `8a13602`, which retains the reverse-engineering
evidence for executable SHA-256
`bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6`:

- `src/game/management_runtime.cpp` and `management_services.inc`: staff,
  statistics, loans, funds, campaign admission, and service price operations.
- `src/game/ui_price_runtime.cpp`: original purchase/subscription fee setters.
- `src/game/live_host_state/native_port.cpp` and `native_guards.h`: financial
  record ownership, native loan bodies, and twenty accounting categories.
- `src/game/gm_policy_state.h` and `gm_policy_native.cpp`: named policy fields,
  native domains, compare-and-exchange stores, and owner notification.
- `src/game/game_rules_state.h` and `game_rules_native.cpp`: XP, respawn, and
  unlocked gameplay features.
- `src/game/marketing_authority/readback.cpp`, `native_port.cpp`, and
  `native_pins.h`: campaign identity, start, abort, and cost accounting.
- `src/game/release_edit_reader.h`, `release_edit_native.cpp`, and
  `release_edit_pins.inc`: software versions and release text ownership.
- `src/game/release_cycle_runtime.inc`, `release_cycle_native.cpp`, and
  `release_cycle_pins.inc`: native release eligibility, announcement,
  celebration, skip, abort, and publication paths.

Digest include files contain addresses, lengths, and SHA-256 values only.
`tests/gameplay/management/generate_native_pins.py` regenerates these facts from
the sibling reference checkout without copying executable bytes into this
repository. Native helpers with no retained body digest use
`Context::verify_original`, which checks the exact supported executable's
pristine bytes and any acknowledged Framework route composition.

The paired fee action uses the two original Business setters also observed in
the Overview `UpdateSaleValues` path: Purchase at `0x54ce0` and Subscription at
`0x54cf0`. Both four-byte native bodies are checked before either call. The full
twenty-category financial ledger must remain unchanged. Both inputs are checked
before execution; a partial native failure is unknown, with no rollback or retry.
This wrapper does not import the Multiplayer UI hook, controller or transport.

All operations require the current loaded world and game thread. Public object
references use epoch-scoped UID strings. Native mutations set the attempted
receipt before the call, preserve faults, compare intended state with native
readback, and compare the complete current financial record, including all
twenty category counters and amounts. No mutation is retried.

`publish_release` uses the original Skip operation on an active announcement or
celebration. It does not bypass release eligibility or directly send LevelUp on
an idle release. Launch publication retains the original immediate native path.

The focused tests compile the actual adapter and exercise synthetic query
memory, argument domains, overflow, and rejection before mutation. They do not
establish effects in the running game. No game process or user save was used for
this implementation slice.
