---
title: Recover an interrupted original process cohort
---

# Recover an interrupted original process cohort

The optional native **Step recovery draft 1** is separate from Step draft 9.
Framework 0.5.0 includes this optional extension. Query
`MT2FW_MT2_QueryStepRecoveryDraft` with exact draft version **1** and sizes.
An absent export or an unavailable implementation grants no recovery authority.
The interface alone does not establish game or Multiplayer compatibility or
provide automatic reconnect. The stable native ABI and Lua API remain unchanged.

Read the [machine contract](../public/api/experimental/step-recovery.json) and
`adapters/mt2/sdk/native_step_recovery_draft.h`. This extension is restricted to
the exact active native Multiplayer owner and its retained registered provider.
Its callbacks and user storage must remain alive until successful Step unbind.

## Freeze the original processes before activation

Bind a normal Step lease first. Bind the Recovery observer while that lease is
ARMED, before activation. Copy physical Steam identities and original handshake
process nonces for all populated slots 1 through 4. Each identity and nonce must
be nonzero and unique; unused slots contain zeroes. A restarted process under
the same Steam account is a different process and cannot acknowledge its
predecessor's hold or retirement.

`observe` is a nonblocking transport callback. It runs at a fresh outer boundary
before normal live membership proof, after Framework validates the local
native hold. It cannot run a command, Save, ordinary controller tick or Next.
`local_proof` reads the original retained NativeOpen and selected-file receipt,
bypassing only live membership liveness. Framework still checks the registered
provider, current world epoch, generation, snapshot identity, game thread,
native pointers, original routes and actual held scheduler. Do not call the
normal proof callback with fabricated healthy membership.

A healthy observation preserves the original Step flow. An interruption creates
a new local interruption nonce, last completed tick and untouched next tick.
The previous Step receipt may still contain its last completed original's
attempt/return flags; those flags are not an interruption certificate.
A failed Step, uncommitted original effect or unknown native owner cannot enter
recovery. Recovery never clears FAILED and never lends Next for that lease.

## Capture one fresh Host snapshot

Explicit operations run only in the separate Recovery service, on the retained
game thread. Every request names the exact lease, interruption nonce, new
canonical recovery UUID and fixed original survivor cohort.

| Operation | Required result |
| --- | --- |
| `BEGIN` | Freeze every currently observed surviving original process. Do not shrink the set during the attempt. |
| `PREPARE` | Host consumes its one native preparation attempt before changing the Clock or releasing workers. |
| `SEAL` | Read a new full covered Host witness after actual preparation. |
| `CAPTURE` | Compare the witness again, consume once, and invoke one original Save callback. |
| `PUMP` | Continue only the consumed original writer. Compare covered state after actual completion. |
| `PUBLISH` | Re-read and confirm durable full file and companion bytes and hashes. |
| `PREFLIGHT_RETIRE` | Admit original Quit for this Host publication or an original Client's actual interruption hold. |

Preparation preserves the selected Clock scale as `selected_clock_bits` for the
new session's resume intent. It pauses through the original setter, retires
empty normal retrieval ownership, releases the scheduler once, calls original
Shard flush and FinishAllTasks, then restores a held scheduler once. It verifies
GL ownership, queues and unchanged completed tick and Go/Timer counters. It
does not execute Go or Timer, edit a task queue, or adopt RNG state.

Flush and Resume suppression during Save require the independently consumed
Recovery certificate, matching lease, session, world, generation, interruption,
UUID and covered witness. A broad recovery flag or an old snapshot certificate
cannot authorize them. Ordinary Step snapshot masks and ACK rules are unchanged.
Preparation and capture failures retain their consumed attempts and original
causes. Do not retry them. Accepted requests are not confirmed file publication.

## Retire and start a new session

The extension returns local admission, not peer retirement acknowledgements.
Execute original Quit through the existing guarded native command path and
wait for actual Title transition, destroyed old-world ownership and Step
callback retirement. Each surviving original process acknowledges only its own
retirement, naming its original nonce. Departed processes remain absent.

The Multiplayer protocol must retain all survivor acknowledgements for the
fixed attempt. Only then may Host publish a new session and process cohort,
transfer the freshly captured full file and companion, and use the normal load,
bootstrap witness agreement and resume path. Delayed old-epoch packets cannot
authorize new work. Other survivors may have different completed ticks; they
reload the Host snapshot rather than pretending their old worlds match it.

Validate recovery with the matching Multiplayer implementation in an owned test
profile. Source checks and controlled-body tests do not prove the effects of a
live disconnect and rejoin.
