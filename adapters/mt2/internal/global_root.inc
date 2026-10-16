#include "global_root.h"
#include <limits>
namespace mt2mp::game::native {
namespace {
DWORD global_root_copy(const void* from,void* to,size_t bytes){
    __try {std::memcpy(to,from,bytes);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
struct LoadedRootSnapshot {
    void* map=nullptr;
    void* shard=nullptr;
    void* array=nullptr;
    int32_t count=0,capacity=0;
    std::vector<void*> regions;
    std::vector<int32_t> region_ids;
};
bool read_loaded_root(LoadedRootSnapshot& root,std::string& error) {
    if (!check_thread(error)) return false;
    if (!in_game()) return fail(error,"global_root_no_active_world");
    int32_t state=-1;
    if (!read_at(singleton(mode_slot),0x2c4,state)||state!=4)
        return fail(error,"global_root_mode_state actual="+std::to_string(state)+" required=4");
    struct Guard {uintptr_t rva; const char* bytes; size_t size;};
    const Guard guards[]={
        {0x334acd,"\x48\x8d\xbb\xc0\x01\x00\x00",7},
        {0x334adb,"\x48\x89\xf9\xe8\xbd\x35\x1d\x00",8},
        {0x5080af,"\x48\x8d\x05\x6a\x2c\x24\x01",7},
        {0x5080d5,"\xc7\x43\x48\x00\x00\x00\x00",7}};
    unsigned char bytes[8]{};
    for(const auto& guard:guards) {
        const auto* address=reinterpret_cast<void*>(image.load()+guard.rva);
        if(!readable(address,guard.size)||global_root_copy(address,bytes,guard.size)||std::memcmp(bytes,guard.bytes,guard.size))
            return fail(error,"global_root_build_contract rva="+std::to_string(guard.rva));
    }
    root.map=singleton(0x13df8c0);
    if(!has_vtable(root.map,0x1742850))return fail(error,"global_root_map_vtable");
    const auto map_address=reinterpret_cast<uintptr_t>(root.map);
    if(map_address>(std::numeric_limits<uintptr_t>::max)()-0x1c0)return fail(error,"global_root_map_address_overflow");
    root.shard=reinterpret_cast<void*>(map_address+0x1c0);
    if(!has_vtable(root.shard,0x174ad10))return fail(error,"global_root_embedded_shard_vtable");
    int32_t type=-1;
    if(!read_at(root.shard,0x48,type)||type!=0)return fail(error,"global_root_embedded_shard_type actual="+std::to_string(type)+" required=0");
    if(!read_at(root.map,0x358,root.array)||!read_at(root.map,0x360,root.count)||!read_at(root.map,0x364,root.capacity))
        return fail(error,"global_root_array_unreadable");
    if(!root.array||root.count<=0||root.count>16384||root.capacity<root.count||!readable(root.array,size_t(root.count)*sizeof(void*)))
        return fail(error,"global_root_array_bounds count="+std::to_string(root.count)+" capacity="+std::to_string(root.capacity)+" required=1..16384 capacity>=count");
    root.regions.clear();root.region_ids.clear();root.regions.reserve(size_t(root.count));root.region_ids.reserve(size_t(root.count));
    for(int32_t i=0;i<root.count;++i){
        void* region=nullptr;void* shard=nullptr;int32_t id=-1;
        if(!read_at(root.array,size_t(i)*sizeof(void*),region)||!has_vtable(region,0x1787920))
            return fail(error,"global_root_region_vtable index="+std::to_string(i));
        if(!read_at(region,0x70,shard)||shard!=root.shard)
            return fail(error,"global_root_region_shard index="+std::to_string(i));
        if(!read_at(region,0x834,id)||id<0)
            return fail(error,"global_root_region_id index="+std::to_string(i));
        if(std::find(root.regions.begin(),root.regions.end(),region)!=root.regions.end())
            return fail(error,"global_root_duplicate_region index="+std::to_string(i));
        if(std::find(root.region_ids.begin(),root.region_ids.end(),id)!=root.region_ids.end())
            return fail(error,"global_root_duplicate_region_id id="+std::to_string(id));
        root.regions.push_back(region);root.region_ids.push_back(id);
    }
    return true;
}
bool same_loaded_root(const LoadedRootSnapshot& a,const LoadedRootSnapshot& b){
    return a.map==b.map&&a.shard==b.shard&&a.array==b.array&&a.count==b.count&&a.capacity==b.capacity&&a.regions==b.regions&&a.region_ids==b.region_ids;
}
}

bool read_building_global(uint64_t uid, BuildingState& state, std::string& error) {
    if (!check_thread(error)) return false;
    if (!in_game()) return fail(error, "building read rejected: no active world");
    state = {};
    LoadedRootSnapshot root;
    if(!read_loaded_root(root,error))return false;
    const std::string target = " uid=" + std::to_string(uid);
    auto reject = [&](const char* cause) { return fail(error, std::string("building read rejected: ") + cause + target); };
    void* building = nullptr;
    bool object_found = false;
    const DWORD exception = find_typed_object(uid, 0x1655090, building, &object_found);
    if (exception) return fail(error, "building lookup failed: exception=" + std::to_string(exception) + target);
    if (!building) return reject(object_found ? "typed_uid_wrong_type" : "typed_uid_missing");
    void* active_shard = root.shard;
    void* shard = nullptr;
    void* region = nullptr;
    if (!read_at(building, 0x170, shard)) return reject("building_shard_unreadable");
    if (!read_at(building, 0x178, region)) return reject("region_unreadable");
    if (!region) return reject("region_missing");
    if(shard==active_shard&&std::find(root.regions.begin(),root.regions.end(),region)==root.regions.end())
        return reject("global_root_region_not_in_map");
    if (shard != active_shard) {
        if (!has_vtable(shard, 0x174ad10) || !has_vtable(active_shard, 0x174ad10))
            return reject("outer_shard_mismatch: shard_vtable_invalid");
        int32_t shard_type = -1;
        if (!read_at(shard, 0x48, shard_type)) return reject("dungeon_shard_type_unreadable");
        if (shard_type != 1 && shard_type != 2) return reject("dungeon_shard_type_not1_or2");
        int32_t root_type = -1;
        if (!read_at(active_shard, 0x48, root_type) || root_type != 0)
            return reject("dungeon_root_shard_type_not0");
        void* dungeon = nullptr;
        void* instance = nullptr;
        if (shard_type == 2) {
            if (!read_at(shard, 0x20, instance) || !has_vtable(instance, 0x1719440))
                return reject("dungeon_instance_receiver_invalid");
            void* typed_instance = nullptr;
            const DWORD instance_exception = cast_dungeon_instance(instance, typed_instance);
            if (instance_exception) return fail(error, "dungeon instance RTTI lookup failed: exception=" + std::to_string(instance_exception) + target);
            if (typed_instance != instance) return reject("dungeon_instance_rtti_mismatch");
            void* reverse_shard = nullptr;
            void* reverse_region = nullptr;
            if (!read_at(instance, 0x38, reverse_shard) || reverse_shard != shard)
                return reject("dungeon_instance_shard_reciprocity_mismatch");
            if (!read_at(instance, 0x40, reverse_region) || reverse_region != region)
                return reject("dungeon_instance_region_reciprocity_mismatch");
            if (!read_at(instance, 0x30, dungeon)) return reject("dungeon_instance_owner_unreadable");
        } else if (!read_at(shard, 0x18, dungeon)) {
            return reject("dungeon_receiver_invalid");
        }
        if (!has_vtable(dungeon, 0x16c0550))
            return reject("dungeon_receiver_invalid");
        void* typed_dungeon = nullptr;
        const DWORD dungeon_exception = cast_dungeon(dungeon, typed_dungeon);
        if (dungeon_exception) return fail(error, "dungeon RTTI lookup failed: exception=" + std::to_string(dungeon_exception) + target);
        if (typed_dungeon != dungeon) return reject("dungeon_rtti_mismatch");
        void* reverse_shard = nullptr;
        void* reverse_region = nullptr;
        if (!read_at(dungeon, 0x48, reverse_shard) || !reverse_shard || (shard_type == 1 && reverse_shard != shard))
            return reject("dungeon_shard_reciprocity_mismatch");
        if (!read_at(dungeon, 0x50, reverse_region) || !reverse_region || (shard_type == 1 && reverse_region != region))
            return reject("dungeon_region_reciprocity_mismatch");
        if (shard_type == 2) {
            int32_t template_type = -1;
            void* template_owner = nullptr;
            if (reverse_shard == shard || !has_vtable(reverse_shard, 0x174ad10) ||
                !read_at(reverse_shard, 0x48, template_type) || template_type != 1 ||
                !read_at(reverse_shard, 0x18, template_owner) || template_owner != dungeon)
                return reject("dungeon_instance_template_shard_reciprocity_mismatch");
            void* instances = nullptr;
            int32_t count = 0, capacity = 0;
            if (!read_at(dungeon, 0x4c0, instances) || !read_at(dungeon, 0x4c8, count) || !read_at(dungeon, 0x4cc, capacity))
                return reject("dungeon_instance_owner_list_unreadable");
            if (!instances || count <= 0 || count > 1000000 || capacity < count)
                return fail(error, "building read rejected: dungeon_instance_owner_list_bounds count=" + std::to_string(count) +
                    " capacity=" + std::to_string(capacity) + " required_count=1..1000000 capacity>=count" + target);
            int occurrences = 0;
            for (int32_t i = 0; i < count; ++i) {
                void* candidate = nullptr;
                if (!read_at(instances, size_t(i) * sizeof(void*), candidate)) return reject("dungeon_instance_owner_list_entry_unreadable");
                if (candidate == instance && ++occurrences > 1) return reject("dungeon_instance_owner_list_duplicate");
            }
            if (occurrences != 1) return reject("dungeon_instance_not_in_owner_list");
            void* final_instances = nullptr;
            int32_t final_count = 0, final_capacity = 0;
            if (!read_at(dungeon, 0x4c0, final_instances) || !read_at(dungeon, 0x4c8, final_count) ||
                !read_at(dungeon, 0x4cc, final_capacity) || final_instances != instances || final_count != count || final_capacity != capacity)
                return reject("dungeon_instance_owner_list_changed_during_read");
        }
        void* exterior = nullptr;
        if (!read_at(dungeon, 0x6f8, exterior) || !exterior) return reject("dungeon_exterior_missing");
        if (exterior == building) return reject("dungeon_exterior_cycle");
        uint64_t exterior_uid = 0;
        if (!read_at(exterior, 0x18, exterior_uid)) return reject("dungeon_exterior_uid_unreadable");
        void* typed_exterior = nullptr;
        const DWORD exterior_exception = find_typed_object(exterior_uid, 0x1655090, typed_exterior);
        if (exterior_exception) return fail(error, "dungeon exterior lookup failed: exception=" + std::to_string(exterior_exception) + target);
        if (typed_exterior != exterior) return reject("dungeon_exterior_registry_mismatch");
        void* reverse_dungeon = nullptr;
        void* exterior_shard = nullptr;
        if (!read_at(exterior, 0xa38, reverse_dungeon) || reverse_dungeon != dungeon)
            return reject("dungeon_exterior_reciprocity_mismatch");
        if (!read_at(exterior, 0x170, exterior_shard) || exterior_shard != active_shard)
            return reject("dungeon_exterior_not_direct_root");
        // The direct-root check bounds this nested read to a single exterior building.
        BuildingState exterior_state;
        if (!read_building_global(exterior_uid, exterior_state, error)) {
            const std::string cause = error;
            return fail(error, "building dungeon exterior read failed: " + cause + target);
        }
        if (exterior_state.type != "DungeonExterior") return reject("dungeon_exterior_type_mismatch");
        state.dungeon_membership = true;
        state.dungeon_instance_membership = shard_type == 2;
        state.exterior_uid = exterior_uid;
    }
    if (!read_at(building, 0x18, state.prop.uid)) return reject("uid_unreadable");
    if (state.prop.uid != uid) return reject("uid_mismatch");
    if (!read_at(building, 0x110, state.prop.x) || !read_at(building, 0x114, state.prop.y) ||
        !read_at(building, 0x118, state.prop.z)) return reject("transform_unreadable");
    if (!std::isfinite(state.prop.x) || !std::isfinite(state.prop.y) || !std::isfinite(state.prop.z))
        return reject("transform_nonfinite");
    if (!read_game_string(building, 0x768, state.type)) return reject("type_string_unreadable");
    if (!read_game_string(building, 0x788, state.variant)) return reject("variant_string_unreadable");
    LoadedRootSnapshot final_root;
    if(!read_loaded_root(final_root,error))return false;
    if(!same_loaded_root(root,final_root))return reject("global_root_changed_during_read");
    void* final_building=nullptr;
    const DWORD final_exception=find_typed_object(uid,0x1655090,final_building);
    if(final_exception)return fail(error,"global building final lookup exception="+std::to_string(final_exception)+target);
    if(final_building!=building)return reject("global_root_final_registry_mismatch");
    void* final_shard=nullptr;void* final_region=nullptr;uint64_t final_uid=0;
    if(!read_at(building,0x170,final_shard)||final_shard!=shard||!read_at(building,0x178,final_region)||final_region!=region||!read_at(building,0x18,final_uid)||final_uid!=uid)
        return reject("global_root_building_changed_during_read");
    return true;
}

}
