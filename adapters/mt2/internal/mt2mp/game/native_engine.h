#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include <string>
#include <vector>

namespace mt2mp::game::native {

enum class EntryPoint { GameUpdate, ModeUpdate, ClockUpdate, ModeCommand, BeginEdit, EndEdit, SoftwareTick, SoftwareTickerTick,
    SoftwareCalculateRating, SoftwareCalculateFun, BusinessNextDayFinances, SubscriberNewDay,
    NeedSetSet, NeedSetIncrement, NeedSetAddAssign, NeedSetSubtractAssign, SubscriberBrainTick, SubscriberUpdateNeeds };

using SubscriberNeeds = std::array<float, 7>;
enum class NeedsWriteLeaf : uint32_t { Set, Increment, AddAssign, SubtractAssign, Count };
enum class NeedsTickLeaf : uint32_t { BrainTick, UpdateNeeds, Count };
struct NeedsWriteCounters {
    uint64_t entered = 0, forwarded = 0, selected = 0, suppressed = 0, permitted = 0;
    uint64_t guard_rejected = 0, wrong_thread = 0;
};
struct NeedsTickCounters { uint64_t entered = 0, forwarded = 0, guard_rejected = 0; };
struct SubscriberNeedsReceipt {
    bool written = false, already_applied = false, before_readable = false, after_readable = false;
    uint64_t epoch = 0, revision = 0;
    int32_t serialized_id = -1;
    uint32_t attempted_setters = 0, completed_setters = 0, native_exception = 0;
    SubscriberNeeds before{}, after{};
};
struct SubscriberNeedsReplicaState {
    bool armed = false, bound = false, logged_in = false;
    uint64_t epoch = 0, applied_revision = 0;
    int32_t serialized_id = -1;
    uint32_t installed_hook_mask = 0;
    SubscriberNeeds initial{}, applied{};
    std::array<NeedsWriteCounters, 4> writes{};
    std::array<NeedsTickCounters, 2> ticks{};
    std::string error;
};

enum class DayTickLeaf : uint32_t { BusinessFinances, Subscriber, Count };
enum class DayTickGuard : uint32_t {
    Accepted, ResolverUnavailable, WrongThread, NoActiveWorld, SingletonUnavailable,
    SingletonVtableMismatch, ReceiverMismatch, ReceiverVtableMismatch, SubscriberIdUnreadable,
    SubscriberIdInvalid, DayArgumentInvalid, InvalidLeaf, Count
};
struct DayTickCounters {
    uint64_t entered = 0, forwarded = 0, accepted = 0;
    uint64_t receiver_guard_rejected = 0, wrong_thread = 0, deferred_logs = 0;
    DayTickGuard first_rejection = DayTickGuard::Accepted, last_guard = DayTickGuard::Accepted;
    int32_t last_valid_day = -1, last_valid_subscriber_id = -1;
    std::array<uint64_t, static_cast<std::size_t>(DayTickGuard::Count)> guards{};
};
struct DayTickState {
    bool resolver_ready = false, thread_bound = false;
    uint64_t invalid_observe_calls = 0, invalid_forward_calls = 0;
    std::array<DayTickCounters, static_cast<std::size_t>(DayTickLeaf::Count)> ticks{};
};

enum class SoftwareTickGuard : uint32_t {
    Accepted, ResolverUnavailable, WrongThread, NoActiveWorld, SingletonUnavailable,
    ReceiverMismatch, PrimaryVtableMismatch, SecondaryVtableMismatch, TickSlotMismatch, Count
};

struct SoftwareTickCounters {
    uint64_t entered = 0, forwarded = 0;
    std::array<uint64_t, static_cast<std::size_t>(SoftwareTickGuard::Count)> guards{};
};

enum class SoftwareCalcLeaf : uint32_t { Rating, Fun, Count };
struct SoftwareCache { float rating = 0, fun = 0; };
struct SoftwareCalculationCounters {
    uint64_t entered = 0, forwarded = 0, suppressed = 0, guard_rejected = 0;
};
struct SoftwareCacheReceipt {
    bool written = false, already_applied = false;
    uint64_t epoch = 0, revision = 0;
    SoftwareCache before{}, after{};
};
struct SoftwareState {
    float rating = 0, fun = 0;
    std::array<SoftwareTickCounters, 2> ticks{};
    bool replica_armed = false, replica_bound = false;
    uint64_t bound_epoch = 0, applied_revision = 0;
    float initial_rating = 0, initial_fun = 0;
    std::array<SoftwareCalculationCounters, 2> calculations{};
    std::string replica_error;
};

struct ClockState {
    float time_scale = 0;
    float pause_scale = 0;
    int32_t flat_day = 0;
    int32_t seconds = 0;
    bool paused = true;
};

struct Capabilities {
    bool clock = false;
    bool snapshot_request = false;
    bool load_request = false;
    bool action_replay = false;
    bool live_state_apply = false;
};

struct WorldStatus {
    int32_t active_mode_index = -1;
    int32_t requested_mode_index = -1;
    int32_t in_game_state = -1;
    int32_t edit_operations = -1;
    int32_t saves_in_progress = -1;
    bool world_present = false;
    bool quitting = false;
    bool title_transition_pending = false;
    bool creation_model_available = false;
    bool crash_report_active = false;
    bool crash_report_dismiss_pending = false;
    bool map_task_pending = false;
    bool map_dirty = false;
    int32_t map_regions = -1;
};

struct PropState {
    uint64_t uid = 0;
    float x = 0;
    float y = 0;
    float z = 0;
};

struct BuildingState {
    PropState prop;
    std::string type;
    std::string variant;
    bool dungeon_membership = false;
    bool dungeon_instance_membership = false;
    uint64_t exterior_uid = 0;
    std::array<float,4> rotation{0,0,0,1};
    std::array<float,3> scale{1,1,1};
};

struct WorldPropState {
    PropState prop;
    bool region_active = false;
    bool building = false;
    std::string type;
    std::string variant;
};

struct SimulationState {
    ClockState clock;
    int32_t cash = 0;
    bool record_present = false;
    int32_t record_day = -1;
    int32_t record_cash = 0;
    int32_t record_income = 0;
    int32_t record_expense = 0;
    int32_t logged_in_subscribers = 0;
    std::array<int32_t, 20> line_counts{};
    std::array<int32_t, 20> line_income{};
};

struct SubscriberNeedsState {
    int32_t serialized_id = -1;
    int32_t subscriber_count = 0;
    int32_t satisfaction = 0, addiction = 0;
    float logged_in_seconds = 0;
    bool logged_in = false;
    // Health, Interact, Advance, Danger, GetLoot, Scenery, InventorySpace.
    std::array<float, 7> needs{};
};

struct SubscriberCandidates {
    int32_t existing_count = 0;
    int32_t scanned_count = 0;
    int32_t eligible_count = 0;
    bool scan_complete = false;
    std::vector<int32_t> serialized_ids;
};

struct RegionState {
    int32_t type = 0;
    bool owned = false;
    bool building_manager_present = false;
    bool path_manager_present = false;
    int32_t purchase_price = 0;
    int32_t cash = 0;
};

struct CameraState {
    float x = 0, y = 0, z = 0;
    float focus_x = 0, focus_y = 0, focus_z = 0;
    float distance = 0;
    float minimum_distance = 0, maximum_distance = 0;
    int32_t zoom_index = -1, target_zoom_index = -1;
    float desired_distance = 0, zoom_table_value = 0, zoom_lower_neighbor = 0;
    float representable_target = 0;
    float edit_weight = 0;
    bool focus_pending = false;
    float focus_blend = 0, focus_end_blend = 0;
    bool focus_target_available = false;
    float target_x = 0, target_y = 0, target_z = 0, target_distance = 0;
};

// Initialize before installing hooks so prologue guards see the original image.
bool initialize(std::string& error);
bool ready();
bool read_resolver_receipt(uintptr_t& image, uint32_t& game_thread, std::string& executable_sha256, std::string& error);
Capabilities capabilities();
void* entrypoint(EntryPoint point);
// Business uses void(void*,int32_t); Subscriber uses void(void*). Observation never gates the original call.
DayTickGuard observe_day_tick(void* receiver, DayTickLeaf leaf, int32_t day_argument = -1);
// Call only after the original returns normally, including when observation rejects its receiver.
void record_day_tick_forwarded(DayTickLeaf leaf);
const char* day_tick_guard_name(DayTickGuard guard);
// Process-lifetime counters remain readable across title/load transitions and from status threads.
void read_day_tick_state(DayTickState& state);
// Both native tick entries use void(void*); the ticker receiver is the software object plus 0x18.
SoftwareTickGuard observe_software_tick(void* receiver, bool ticker_receiver);
void record_software_tick_forwarded(bool ticker_receiver);
const char* software_tick_guard_name(SoftwareTickGuard guard);
bool read_software_state(SoftwareState& state, std::string& error);
// Each calculation entry uses void(void*) with the canonical software receiver.
bool software_calculation_should_forward(void* receiver, SoftwareCalcLeaf leaf);
void record_software_calculation_forwarded(SoftwareCalcLeaf leaf);
// Arm in title mode before loading a diagnostic world. Tick itself must still run.
bool arm_software_replica(uint64_t epoch, std::string& error, const std::string& world_folder = "");
bool disarm_software_replica(uint64_t epoch, std::string& error);
bool retire_software_replica(std::string& error);
bool apply_software_cache(uint64_t epoch, uint64_t revision, const SoftwareCache& expected_before,
    const SoftwareCache& host_values, SoftwareCacheReceipt& receipt, std::string& error);
// Bind from a native game Update detour, before invoking any bridge operation.
bool bind_game_thread();
bool in_game();
bool read_world_status(WorldStatus& state, std::string& error);
bool read_prop(uint64_t uid, PropState& state, std::string& error);
bool list_props(size_t limit, std::vector<PropState>& states, std::string& error);
bool list_world_props(size_t limit, std::vector<WorldPropState>& states, std::string& error);
bool read_simulation_state(SimulationState& state, std::string& error);
// IDs come from the native serialized subscriber id, not the general object UID.
bool read_subscriber_needs(int32_t serialized_id, SubscriberNeedsState& state, std::string& error);
bool read_subscriber_candidates(SubscriberCandidates& state, std::string& error);
// Bits 0..3 are the four write hooks; bits 4..5 are the optional original tick observers.
void set_subscriber_needs_hook_mask(uint32_t mask);
bool subscriber_needs_hooks_ready();
// Set/Increment use void(void*,int32_t,float). +=/-= use void*(void*,const void*), returning this when suppressed.
bool subscriber_needs_write_should_forward(void* receiver, NeedsWriteLeaf leaf, int32_t index = -1, float value = 0);
void record_subscriber_needs_write_forwarded(NeedsWriteLeaf leaf);
// Tick observers return an accounting token only. Both original functions must always run.
bool observe_subscriber_needs_tick(void* brain, NeedsTickLeaf leaf);
void record_subscriber_needs_tick_forwarded(NeedsTickLeaf leaf, bool selected_observation);
bool arm_subscriber_needs_replica(uint64_t epoch, int32_t serialized_id, const SubscriberNeeds& expected_initial, std::string& error);
bool disarm_subscriber_needs_replica(uint64_t epoch, std::string& error);
bool apply_subscriber_needs(uint64_t epoch, int32_t serialized_id, uint64_t revision,
    const SubscriberNeeds& expected_before, const SubscriberNeeds& host_needs, SubscriberNeedsReceipt& receipt, std::string& error);
bool read_subscriber_needs_replica_state(SubscriberNeedsReplicaState& state, std::string& error);
// Applies one missing building expense through native accounting services.
bool diagnostic_reconcile_building_ledger(const SimulationState& expected_before,
    const SimulationState& host_after, SimulationState& local, std::string& error);
bool read_camera_state(CameraState& state, std::string& error);
// True confirms the native focus target. Observe later frames for camera motion.
bool diagnostic_focus_camera(float x, float y, float z, float distance, CameraState& state, std::string& error);
bool read_region_state(float x, float z, RegionState& state, std::string& error);
bool diagnostic_purchase_region(float x, float z, RegionState& state, std::string& error);
bool read_building(uint64_t uid, BuildingState& state, std::string& error);
// Uses the native undo move primitive; limited to diagnostic worlds.
bool diagnostic_move_prop(const PropState& expected, const PropState& target, std::string& error);
// Grounded, unrotated placement through native catalog/collision/purchase services.
bool diagnostic_place_building(const std::string& type, const std::string& variant,
    float x, float z, BuildingState& created, std::string& error);
bool probe_building_placement(const std::string& type, const std::string& variant,
    float x, float z, PropState& grounded, std::string& error);
// Host operation IDs must map to returned local UIDs in the replication layer.
bool diagnostic_replicate_building(const BuildingState& host, BuildingState& local, std::string& error);
bool read_clock(ClockState& state, std::string& error);
bool pause_load_clock(void* receiver, bool& changed, ClockState& actual, std::string& error);
bool set_clock(bool paused, float speed, std::string& error);
bool read_saves_in_progress(int32_t& count, std::string& error);
// Takes user/save/<game-folder>/<filename>.vrz in the engine virtual filesystem.
// True means the native task was dispatched. Confirm persistence via the file hook.
bool read_active_world_folder(std::string& folder, std::string& error);
bool capture_snapshot(const std::string& virtual_filename, std::string& error);
// Accepts save/<game-folder>/<filename>.vrz while the title mode is active.
// True means a load was requested, not that world activation completed.
bool request_load(const std::string& filename, std::string& error);
// Return-to-title queues the engine's autosave and waits in the native update path.
bool request_return_to_title(std::string& error);
// Requires title mode and no transition/save. Call after the mailbox acknowledgement.
bool request_exit_game(std::string& error);
// Dismisses the active native crash dialog without submitting a report.
bool request_dismiss_crash_report(std::string& error);
bool dispatch_command(const std::string& command, std::string& error);
// Diagnostic test worlds use the native creation window's model and commands.
bool prepare_test_world(const std::string& name, std::string& error);
bool test_world_ready(bool& complete, std::string& error);
bool create_test_world(const std::string& name, std::string& error);

} // namespace mt2mp::game::native
