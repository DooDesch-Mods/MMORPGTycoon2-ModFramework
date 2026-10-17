---
title: Retained originals and typed imports
---

# Retained originals and typed imports

Framework 0.1.5 provides experimental retained-hook and typed-import ownership through `MT2FW_MT2_QueryOwnership`. Its declarations are in `adapters/mt2/sdk/native_ownership.h` and the developer package's `sdk/native/mt2` directory. ServicesV1, RoutesV1 and stable plugin ABI 1 keep their existing declarations. Query from an active Framework callback with the exact borrowed host pointer.

## Disable without freeing originals

`hook_disable` restores the authenticated original entry while retaining the actual trampoline, hook owner and native receiver state. It accepts a prepared or already disabled owned target without another mutation. `hook_resume` enables the same hook and trampoline again. The private state is `DISABLED_RETAINED` (5). Shared Framework targets and other owners reject.

This is required by the current Multiplayer native-free retirement, Move failure and published building-history failure paths. Mapping those calls to remove would free storage still used by late entries. Retention stays until process exit, including after resume. `hook_remove` returns busy for retained routes. Fresh unpublished ordinary prepared hooks remain removable; the sized native-free target is sticky from preparation, matching its original reclamation prohibition.

The host emits the normal mod `stop` event before retiring physical routes. A mod must use this event to suspend sessions, transport and workers while preserving published native receiver storage. For retained native state, PluginShutdown and host/context destruction wait until process exit. Expected retention is reported as info. A changed foreign route, unconfirmed restoration or in-flight callback remains a failed retirement and retains its original cause. It is not reported as completed cleanup.

Retained managers and host contexts live in process-owned storage without a static destructor. DLL teardown cannot destroy a manager before the late native entries that still own its originals. A repeated shutdown does not repeat an unknown reversal, and retained state cannot be replaced by another initialization in the same process.

## Exact free import ownership

The service only controls the verified executable's `msvcrt.dll!free` slot at RVA `0x1c52a00`, with `malloc` at `0x1c52a70` as a sibling guard. There is no caller-selected DLL, symbol, address or slot. The backend checks the named PE import and the exact game hash. It pins the imported CRT and actual resolved free/malloc provider modules, holds their files against replacement, and reports paths, SHA256, file sizes, fixed version presence and export forwarders. Missing fixed version metadata is explicit, not a fabricated version. Provider file reads occur during preparation, never per free call.

1. Call `free_prepare` with the existing native free observer entry. Require the exact original/provider/sibling-slot receipt.
2. Publish the current Multiplayer observer disabled with the sized trampoline and `receipt.original`. Keep its existing body, world, thread and ownership guards.
3. Enable the sized hook, then call `free_enable` and require exchanged pointer, restored initial protection and live route acknowledgement before enabling the observer.
4. Suspend and quiesce the existing observer. Call `free_disable` and retaining `hook_disable` for the sized route. Validate actual original pointers, protection and retained originals.
5. `free_remove` can retire import metadata only while prepared or confirmed disabled. It cannot remove an enabled or unknown import or free pinned provider modules.

The IAT points directly to the existing Multiplayer observer. No extra per-free dispatcher changes the original caller address. The service does not reimplement the accepted delete/reclamation policy or execute a free during control operations.

Every pointer exchange is one aligned compare-and-exchange against an expected measured pointer. A competing foreign pointer remains untouched. Receipts separate attempted, exchanged, protection restored, live confirmed, Win32/SEH errors and original cause. `protection_restored` is meaningful for an attempted exchange; an untouched prepared import does not claim a restoration occurred.

An exchange that never started remains distinguishable from an unknown mutation. Unknown enable cannot be replayed. Readback does not change state or erase the original cause. Explicit cleanup first authenticates the current provider, sibling slot and actual pointer. It can restore an acknowledged owned replacement or repair initial protection after a measured prior restoration failure. It refuses a foreign pointer. The Framework owns protection changes; the plugin validates the receipt instead of repeating raw VirtualProtect writes.

## Passive Mode.Command observation

`command_subscribe` observes the actual gameplay Mode.Command entry before Framework dispatch. Each borrowed record has invocation, receiver, thread, exact bounded text and a readable/error status. It can count the original `LevelUp` callback without installing another title/game command hook. There is no argument replacement, continuation or suppression result. Callback failure is retained while the original dispatch still proceeds normally. Use the existing `ingame.command` route/code receipt for physical owner admission.

## Accepted SDL events

`input_subscribe` observes only events accepted by the single Framework SDL_PollEvent provider, after its reserved-key and UI-consumption checks. It includes unbound key-down, key-up and other forwarded events. It cannot filter input or install another event pump.

The record supplies a borrowed SDL_Event copy, byte size, compiled SDL version, thread and invocation. Validate size/version before decoding. The event structure is copied independently for each observer; pointer payloads remain borrowed and must be copied if retained. Observer failure cannot suppress the event returned to the game or change another mod's event structure. Null polls and consumed events are not emitted. This does not claim that an injected event produced a native game effect; that still requires acknowledgement or visible readback.

All observers belong to the mod. A busy unsubscribe retains callback data until its invocation returns and stops future calls. Automatic retirement uses the same lifetime rule. These services remain private native contracts and do not isolate memory faults.

## Acceptance

Synthetic tests cover retaining disable/resume, original-pointer lifetime, exact-provider admission, owned/foreign CAS, protection failure and explicit recovery, never-executed versus unknown operations, metadata retirement, passive command/input subscriptions and callback lifetime. Native input byte-limit checks cover the unchanged default, Chat's 240-byte boundary and invalid/multibyte UTF-8. These fixtures do not prove physical input or arbitrary native plugin behavior. Retained game tests cover the specific migrated action cycle, initial-save metadata, Profiler measurements and window lifecycle; full Multiplayer compatibility is not established.
