std::string(R"MT2gameplay(
mt2.actors = mt2.actors or {}
mt2.characters = mt2.characters or {}
mt2.development = mt2.development or {}
mt2.dungeons = mt2.dungeons or {}
mt2.experimental = mt2.experimental or {}
mt2.management = mt2.management or {}
mt2.modular = mt2.modular or {}
mt2.monsters = mt2.monsters or {}
mt2.network = mt2.network or {}
mt2.npcs = mt2.npcs or {}
mt2.paths = mt2.paths or {}
mt2.projects = mt2.projects or {}
mt2.quests = mt2.quests or {}
mt2.regions = mt2.regions or {}
mt2.scenery = mt2.scenery or {}
mt2.subscribers = mt2.subscribers or {}
mt2.terrain = mt2.terrain or {}
mt2.walls = mt2.walls or {}
mt2.world = mt2.world or {}
mt2.actors.regions = function()
    return call('gameplay.query', {operation='actors.regions', arguments={}})
end
mt2.actors.list = function(region)
    return call('gameplay.query', {operation='actors.list', arguments={["region"]=region}})
end
mt2.npcs.list = function(region)
    return call('gameplay.query', {operation='npcs.list', arguments={["region"]=region}})
end
mt2.monsters.list = function(region)
    return call('gameplay.query', {operation='monsters.list', arguments={["region"]=region}})
end
mt2.actors.get = function(handle)
    return call('gameplay.query', {operation='actors.get', arguments={["handle"]=handle}})
end
mt2.quests.list = function(handle)
    return call('gameplay.query', {operation='quests.list', arguments={["handle"]=handle}})
end
mt2.quests.get = function(handle)
    return call('gameplay.query', {operation='quests.get', arguments={["handle"]=handle}})
end
mt2.quests.create = function(handle)
    return call('gameplay.action', {operation='quests.create', arguments={["handle"]=handle}})
end
mt2.quests.remove = function(handle)
    return call('gameplay.action', {operation='quests.remove', arguments={["handle"]=handle}})
end
mt2.quests.swap = function(handle, slot)
    return call('gameplay.action', {operation='quests.swap', arguments={["handle"]=handle, ["slot"]=slot}})
end
mt2.quests.set_name = function(handle, name)
    return call('gameplay.action', {operation='quests.set_name', arguments={["handle"]=handle, ["name"]=name}})
end
mt2.quests.set_kills = function(handle, kills)
    return call('gameplay.action', {operation='quests.set_kills', arguments={["handle"]=handle, ["kills"]=kills}})
end
mt2.quests.set_rewards = function(handle, defaults, xp, gold)
    return call('gameplay.action', {operation='quests.set_rewards', arguments={["handle"]=handle, ["defaults"]=defaults, ["xp"]=xp, ["gold"]=gold}})
end
mt2.quests.set_destination = function(handle, destination, kills)
    return call('gameplay.action', {operation='quests.set_destination', arguments={["handle"]=handle, ["destination"]=destination, ["kills"]=kills}})
end
mt2.actors.boss_phases = function(handle)
    return call('gameplay.query', {operation='actors.boss_phases', arguments={["handle"]=handle}})
end
mt2.actors.place_boss = function(character, region, pose)
    return call('gameplay.action', {operation='actors.place_boss', arguments={["character"]=character, ["region"]=region, ["pose"]=pose}})
end
mt2.monsters.place = function(character, region, rank, level, pose)
    return call('gameplay.action', {operation='monsters.place', arguments={["character"]=character, ["region"]=region, ["rank"]=rank, ["level"]=level, ["pose"]=pose}})
end
mt2.npcs.place = function(character, region, type, pose)
    return call('gameplay.action', {operation='npcs.place', arguments={["character"]=character, ["region"]=region, ["type"]=type, ["pose"]=pose}})
end
mt2.actors.move = function(handle, pose)
    return call('gameplay.action', {operation='actors.move', arguments={["handle"]=handle, ["pose"]=pose}})
end
mt2.actors.remove = function(handle)
    return call('gameplay.action', {operation='actors.remove', arguments={["handle"]=handle}})
end
mt2.monsters.zones = function(region)
    return call('gameplay.query', {operation='monsters.zones', arguments={["region"]=region}})
end
mt2.monsters.get_zone = function(handle)
    return call('gameplay)MT2gameplay")
+ R"MT2gameplay(.query', {operation='monsters.get_zone', arguments={["handle"]=handle}})
end
mt2.monsters.create_zone = function(character, region, level, stamps)
    return call('gameplay.action', {operation='monsters.create_zone', arguments={["character"]=character, ["region"]=region, ["level"]=level, ["stamps"]=stamps}})
end
mt2.monsters.set_zone = function(handle, settings)
    return call('gameplay.action', {operation='monsters.set_zone', arguments={["handle"]=handle, ["settings"]=settings}})
end
mt2.monsters.reset_zone = function(handle)
    return call('gameplay.action', {operation='monsters.reset_zone', arguments={["handle"]=handle}})
end
mt2.monsters.respawn_zone = function(handle)
    return call('gameplay.action', {operation='monsters.respawn_zone', arguments={["handle"]=handle}})
end
mt2.actors.remove_boss = function(handle)
    return call('gameplay.action', {operation='actors.remove_boss', arguments={["handle"]=handle}})
end
mt2.monsters.remove_zone = function(handle)
    return call('gameplay.action', {operation='monsters.remove_zone', arguments={["handle"]=handle}})
end
mt2.monsters.paint_zone = function(handle, stamps)
    return call('gameplay.action', {operation='monsters.paint_zone', arguments={["handle"]=handle, ["stamps"]=stamps}})
end
mt2.monsters.erase_zones = function(region, stamps)
    return call('gameplay.action', {operation='monsters.erase_zones', arguments={["region"]=region, ["stamps"]=stamps}})
end
mt2.npcs.rename = function(handle, name)
    return call('gameplay.action', {operation='npcs.rename', arguments={["handle"]=handle, ["name"]=name}})
end
mt2.npcs.set_online = function(handle, mode)
    return call('gameplay.action', {operation='npcs.set_online', arguments={["handle"]=handle, ["mode"]=mode}})
end
mt2.npcs.change_level = function(handle, delta)
    return call('gameplay.action', {operation='npcs.change_level', arguments={["handle"]=handle, ["delta"]=delta}})
end
mt2.actors.create_boss_phase = function(handle, name)
    return call('gameplay.action', {operation='actors.create_boss_phase', arguments={["handle"]=handle, ["name"]=name}})
end
mt2.actors.remove_boss_phase = function(handle, phase)
    return call('gameplay.action', {operation='actors.remove_boss_phase', arguments={["handle"]=handle, ["phase"]=phase}})
end
mt2.actors.reorder_boss_phase = function(handle, phase, boundary)
    return call('gameplay.action', {operation='actors.reorder_boss_phase', arguments={["handle"]=handle, ["phase"]=phase, ["boundary"]=boundary}})
end
mt2.actors.set_boss_phase = function(handle, phase, settings)
    return call('gameplay.action', {operation='actors.set_boss_phase', arguments={["handle"]=handle, ["phase"]=phase, ["settings"]=settings}})
end
mt2.actors.add_phase_ability = function(handle, phase, ability)
    return call('gameplay.action', {operation='actors.add_phase_ability', arguments={["handle"]=handle, ["phase"]=phase, ["ability"]=ability}})
end
mt2.actors.remove_phase_ability = function(handle, phase, element)
    return call('gameplay.action', {operation='actors.remove_phase_ability', arguments={["handle"]=handle, ["phase"]=phase, ["element"]=element}})
end
mt2.characters.list = function(include_deleted)
    if include_deleted == nil then include_deleted = false end
    return call('gameplay.query', {operation='characters.list', arguments={["include_deleted"]=include_deleted}})
end
mt2.characters.get = function(handle)
    return call('gameplay.query', {operation='characters.get', arguments={["handle"]=handle}})
end
mt2.characters.stats = function(handle)
    return call('gameplay.query', {operation='characters.stats', arguments={["handle"]=handle}})
end
mt2.characters.appearance = function(handle)
    return call('gameplay.query', {operation='characters.appearance', arguments={["handle"]=handle}})
end
mt2.characters.abilities = function(handle)
    return call('gameplay.query', {operation='characters.abilities', arguments={["handle"]=handle}})
end
mt2.characters.create = function(handle, kind)
    retu)MT2gameplay"
+ R"MT2gameplay(rn call('gameplay.action', {operation='characters.create', arguments={["handle"]=handle, ["kind"]=kind}})
end
mt2.characters.create_variant = function(handle, name)
    return call('gameplay.action', {operation='characters.create_variant', arguments={["handle"]=handle, ["name"]=name}})
end
mt2.characters.remove = function(handle)
    return call('gameplay.action', {operation='characters.remove', arguments={["handle"]=handle}})
end
mt2.characters.rename = function(handle, name)
    return call('gameplay.action', {operation='characters.rename', arguments={["handle"]=handle, ["name"]=name}})
end
mt2.characters.set_stat = function(handle, field, value)
    return call('gameplay.action', {operation='characters.set_stat', arguments={["handle"]=handle, ["field"]=field, ["value"]=value}})
end
mt2.characters.set_appearance = function(handle, record)
    return call('gameplay.action', {operation='characters.set_appearance', arguments={["handle"]=handle, ["record"]=record}})
end
mt2.characters.set_weapon_category = function(handle, category, enabled)
    return call('gameplay.action', {operation='characters.set_weapon_category', arguments={["handle"]=handle, ["category"]=category, ["enabled"]=enabled}})
end
mt2.characters.create_ability = function(handle, level)
    if level == nil then level = 1 end
    return call('gameplay.action', {operation='characters.create_ability', arguments={["handle"]=handle, ["level"]=level}})
end
mt2.characters.copy_ability = function(handle, ability, level)
    if level == nil then level = 1 end
    return call('gameplay.action', {operation='characters.copy_ability', arguments={["handle"]=handle, ["ability"]=ability, ["level"]=level}})
end
mt2.characters.remove_ability = function(handle, ability)
    return call('gameplay.action', {operation='characters.remove_ability', arguments={["handle"]=handle, ["ability"]=ability}})
end
mt2.characters.dock_ability = function(handle, ability, level, display_slot)
    return call('gameplay.action', {operation='characters.dock_ability', arguments={["handle"]=handle, ["ability"]=ability, ["level"]=level, ["display_slot"]=display_slot}})
end
mt2.characters.set_ability = function(handle, ability, record)
    return call('gameplay.action', {operation='characters.set_ability', arguments={["handle"]=handle, ["ability"]=ability, ["record"]=record}})
end
mt2.characters.import = function(handle, compressed_record)
    return call('gameplay.action', {operation='characters.import', arguments={["handle"]=handle, ["compressed_record"]=compressed_record}})
end
mt2.dungeons.list = function()
    return call('gameplay.query', {operation='dungeons.list', arguments={}})
end
mt2.dungeons.get = function(handle)
    return call('gameplay.query', {operation='dungeons.get', arguments={["handle"]=handle}})
end
mt2.dungeons.layout = function(handle)
    return call('gameplay.query', {operation='dungeons.layout', arguments={["handle"]=handle}})
end
mt2.dungeons.erase_paint = function(handle, paths)
    return call('gameplay.action', {operation='dungeons.erase_paint', arguments={["handle"]=handle, ["paths"]=paths}})
end
mt2.dungeons.paint = function(handle, paths, fill_records)
    return call('gameplay.action', {operation='dungeons.paint', arguments={["handle"]=handle, ["paths"]=paths, ["fill_records"]=fill_records}})
end
mt2.dungeons.instances = function(handle)
    return call('gameplay.query', {operation='dungeons.instances', arguments={["handle"]=handle}})
end
mt2.dungeons.gizmos = function(handle, instance)
    return call('gameplay.query', {operation='dungeons.gizmos', arguments={["handle"]=handle, ["instance"]=instance}})
end
mt2.dungeons.monsters = function(handle, instance)
    return call('gameplay.query', {operation='dungeons.monsters', arguments={["handle"]=handle, ["instance"]=instance}})
end
mt2.dungeons.spawn_monster = function(handle, character, rank, level, position, rotation, instance)
    if rotation == nil then rotation = {0, 0, 0, 1} end
    return call('gameplay.action', {op)MT2gameplay"
+ R"MT2gameplay(eration='dungeons.spawn_monster', arguments={["handle"]=handle, ["character"]=character, ["rank"]=rank, ["level"]=level, ["position"]=position, ["rotation"]=rotation, ["instance"]=instance}})
end
mt2.dungeons.set_gizmo_runtime = function(handle, gizmo, state, instance)
    return call('gameplay.action', {operation='dungeons.set_gizmo_runtime', arguments={["handle"]=handle, ["gizmo"]=gizmo, ["state"]=state, ["instance"]=instance}})
end
mt2.dungeons.set_gizmo_contents = function(handle, gizmo, contents, instance)
    return call('gameplay.action', {operation='dungeons.set_gizmo_contents', arguments={["handle"]=handle, ["gizmo"]=gizmo, ["contents"]=contents, ["instance"]=instance}})
end
mt2.dungeons.rename_gizmo = function(handle, gizmo, name, instance)
    return call('gameplay.action', {operation='dungeons.rename_gizmo', arguments={["handle"]=handle, ["gizmo"]=gizmo, ["name"]=name, ["instance"]=instance}})
end
mt2.dungeons.move_gizmo = function(handle, gizmo, position, rotation, scale)
    return call('gameplay.action', {operation='dungeons.move_gizmo', arguments={["handle"]=handle, ["gizmo"]=gizmo, ["position"]=position, ["rotation"]=rotation, ["scale"]=scale}})
end
mt2.dungeons.destroy_gizmo = function(handle, gizmo)
    return call('gameplay.action', {operation='dungeons.destroy_gizmo', arguments={["handle"]=handle, ["gizmo"]=gizmo}})
end
mt2.dungeons.duplicate_gizmo = function(handle, gizmo, position, rotation, scale)
    return call('gameplay.action', {operation='dungeons.duplicate_gizmo', arguments={["handle"]=handle, ["gizmo"]=gizmo, ["position"]=position, ["rotation"]=rotation, ["scale"]=scale}})
end
mt2.dungeons.place_gizmo = function(handle, tool, type, position, variant, rotation, scale)
    if variant == nil then variant = "" end
    if rotation == nil then rotation = {0, 0, 0, 1} end
    if scale == nil then scale = {1, 1, 1} end
    return call('gameplay.action', {operation='dungeons.place_gizmo', arguments={["handle"]=handle, ["tool"]=tool, ["type"]=type, ["position"]=position, ["variant"]=variant, ["rotation"]=rotation, ["scale"]=scale}})
end
mt2.dungeons.buildings = function(handle, instance)
    return call('gameplay.query', {operation='dungeons.buildings', arguments={["handle"]=handle, ["instance"]=instance}})
end
mt2.dungeons.place_building = function(handle, type, variant, position, rotation, scale)
    if rotation == nil then rotation = {0, 0, 0, 1} end
    if scale == nil then scale = {1, 1, 1} end
    return call('gameplay.action', {operation='dungeons.place_building', arguments={["handle"]=handle, ["type"]=type, ["variant"]=variant, ["position"]=position, ["rotation"]=rotation, ["scale"]=scale}})
end
mt2.dungeons.configure_instances = function(handle, count)
    return call('gameplay.action', {operation='dungeons.configure_instances', arguments={["handle"]=handle, ["count"]=count}})
end
mt2.dungeons.set_atmosphere = function(handle, atmosphere)
    return call('gameplay.action', {operation='dungeons.set_atmosphere', arguments={["handle"]=handle, ["atmosphere"]=atmosphere}})
end
mt2.dungeons.set_lighting = function(handle, spinners, sliders, colors)
    return call('gameplay.action', {operation='dungeons.set_lighting', arguments={["handle"]=handle, ["spinners"]=spinners, ["sliders"]=sliders, ["colors"]=colors}})
end
mt2.dungeons.set_palette = function(handle, colors, defaults)
    if defaults == nil then defaults = false end
    return call('gameplay.action', {operation='dungeons.set_palette', arguments={["handle"]=handle, ["colors"]=colors, ["defaults"]=defaults}})
end
mt2.dungeons.open_cell = function(handle, cell, theme)
    return call('gameplay.action', {operation='dungeons.open_cell', arguments={["handle"]=handle, ["cell"]=cell, ["theme"]=theme}})
end
mt2.dungeons.close_cell = function(handle, cell)
    return call('gameplay.action', {operation='dungeons.close_cell', arguments={["handle"]=handle, ["cell"]=cell}})
end
mt2.dungeons.set_cell_theme = function(handle, cell, theme)
    return call('gamep)MT2gameplay"
+ R"MT2gameplay(lay.action', {operation='dungeons.set_cell_theme', arguments={["handle"]=handle, ["cell"]=cell, ["theme"]=theme}})
end
mt2.dungeons.apply_cell_palette = function(handle, cell)
    return call('gameplay.action', {operation='dungeons.apply_cell_palette', arguments={["handle"]=handle, ["cell"]=cell}})
end
mt2.dungeons.make_instance = function(handle)
    return call('gameplay.action', {operation='dungeons.make_instance', arguments={["handle"]=handle}})
end
mt2.dungeons.destroy_instance = function(handle, instance)
    return call('gameplay.action', {operation='dungeons.destroy_instance', arguments={["handle"]=handle, ["instance"]=instance}})
end
mt2.dungeons.reset_instance = function(handle, instance)
    return call('gameplay.action', {operation='dungeons.reset_instance', arguments={["handle"]=handle, ["instance"]=instance}})
end
mt2.dungeons.force_reset_instance = function(handle, instance)
    return call('gameplay.action', {operation='dungeons.force_reset_instance', arguments={["handle"]=handle, ["instance"]=instance}})
end
mt2.dungeons.set_gizmo_state = function(handle, gizmo, state, instance)
    return call('gameplay.action', {operation='dungeons.set_gizmo_state', arguments={["handle"]=handle, ["gizmo"]=gizmo, ["state"]=state, ["instance"]=instance}})
end
mt2.dungeons.move_building = function(handle, building, position, rotation, scale)
    return call('gameplay.action', {operation='dungeons.move_building', arguments={["handle"]=handle, ["building"]=building, ["position"]=position, ["rotation"]=rotation, ["scale"]=scale}})
end
mt2.dungeons.destroy_building = function(handle, building)
    return call('gameplay.action', {operation='dungeons.destroy_building', arguments={["handle"]=handle, ["building"]=building}})
end
mt2.dungeons.duplicate_building = function(handle, building, position, rotation, scale)
    return call('gameplay.action', {operation='dungeons.duplicate_building', arguments={["handle"]=handle, ["building"]=building, ["position"]=position, ["rotation"]=rotation, ["scale"]=scale}})
end
mt2.management.fees = function()
    return call('gameplay.query', {operation='management.fees', arguments={}})
end
mt2.management.loans = function()
    return call('gameplay.query', {operation='management.loans', arguments={}})
end
mt2.management.staff = function()
    return call('gameplay.query', {operation='management.staff', arguments={}})
end
mt2.management.statistics = function()
    return call('gameplay.query', {operation='management.statistics', arguments={}})
end
mt2.management.release = function()
    return call('gameplay.query', {operation='management.release', arguments={}})
end
mt2.management.policies = function()
    return call('gameplay.query', {operation='management.policies', arguments={}})
end
mt2.management.rules = function()
    return call('gameplay.query', {operation='management.rules', arguments={}})
end
mt2.management.campaigns = function()
    return call('gameplay.query', {operation='management.campaigns', arguments={}})
end
mt2.management.services = function(building)
    return call('gameplay.query', {operation='management.services', arguments={["building"]=building}})
end
mt2.management.release_cycle = function()
    return call('gameplay.query', {operation='management.release_cycle', arguments={}})
end
mt2.management.set_purchase_fee = function(value)
    return call('gameplay.action', {operation='management.set_purchase_fee', arguments={["value"]=value}})
end
mt2.management.set_subscription_fee = function(value)
    return call('gameplay.action', {operation='management.set_subscription_fee', arguments={["value"]=value}})
end
mt2.management.allocate_release_funds = function(value)
    return call('gameplay.action', {operation='management.allocate_release_funds', arguments={["value"]=value}})
end
mt2.management.set_respawn_seconds = function(value)
    return call('gameplay.action', {operation='management.set_respawn_seconds', arguments={["value"]=value}})
end
mt2.management.hire_gm = function()
)MT2gameplay"
+ R"MT2gameplay(    return call('gameplay.action', {operation='management.hire_gm', arguments={}})
end
mt2.management.fire_gm = function()
    return call('gameplay.action', {operation='management.fire_gm', arguments={}})
end
mt2.management.take_loan = function(key)
    return call('gameplay.action', {operation='management.take_loan', arguments={["key"]=key}})
end
mt2.management.repay_loan = function(key)
    return call('gameplay.action', {operation='management.repay_loan', arguments={["key"]=key}})
end
mt2.management.purchase_statistic = function(index)
    return call('gameplay.action', {operation='management.purchase_statistic', arguments={["index"]=index}})
end
mt2.management.spend_statistics = function(values)
    return call('gameplay.action', {operation='management.spend_statistics', arguments={["values"]=values}})
end
mt2.management.set_policy = function(policy, field, value)
    return call('gameplay.action', {operation='management.set_policy', arguments={["policy"]=policy, ["field"]=field, ["value"]=value}})
end
mt2.management.set_respawn_delay = function(value)
    return call('gameplay.action', {operation='management.set_respawn_delay', arguments={["value"]=value}})
end
mt2.management.set_xp = function(values)
    return call('gameplay.action', {operation='management.set_xp', arguments={["values"]=values}})
end
mt2.management.set_feature = function(index, enabled)
    return call('gameplay.action', {operation='management.set_feature', arguments={["index"]=index, ["enabled"]=enabled}})
end
mt2.management.set_release_type = function(type)
    return call('gameplay.action', {operation='management.set_release_type', arguments={["type"]=type}})
end
mt2.management.set_release_title = function(text)
    return call('gameplay.action', {operation='management.set_release_title', arguments={["text"]=text}})
end
mt2.management.set_release_announcement = function(text)
    return call('gameplay.action', {operation='management.set_release_announcement', arguments={["text"]=text}})
end
mt2.management.set_release_changelog = function(text)
    return call('gameplay.action', {operation='management.set_release_changelog', arguments={["text"]=text}})
end
mt2.management.reset_release_text = function()
    return call('gameplay.action', {operation='management.reset_release_text', arguments={}})
end
mt2.management.enable_release_custom = function()
    return call('gameplay.action', {operation='management.enable_release_custom', arguments={}})
end
mt2.management.show_release = function()
    return call('gameplay.action', {operation='management.show_release', arguments={}})
end
mt2.management.begin_release = function()
    return call('gameplay.action', {operation='management.begin_release', arguments={}})
end
mt2.management.abort_release = function()
    return call('gameplay.action', {operation='management.abort_release', arguments={}})
end
mt2.management.skip_release = function()
    return call('gameplay.action', {operation='management.skip_release', arguments={}})
end
mt2.management.publish_release = function()
    return call('gameplay.action', {operation='management.publish_release', arguments={}})
end
mt2.management.set_service_price = function(building, ordinal, currency, value)
    return call('gameplay.action', {operation='management.set_service_price', arguments={["building"]=building, ["ordinal"]=ordinal, ["currency"]=currency, ["value"]=value}})
end
mt2.management.set_service_sync = function(building, ordinal, value)
    return call('gameplay.action', {operation='management.set_service_sync', arguments={["building"]=building, ["ordinal"]=ordinal, ["value"]=value}})
end
mt2.management.start_campaign = function(type)
    return call('gameplay.action', {operation='management.start_campaign', arguments={["type"]=type}})
end
mt2.management.abort_campaign = function(type)
    return call('gameplay.action', {operation='management.abort_campaign', arguments={["type"]=type}})
end
mt2.management.set_buzz = function(positive, negative)
    return c)MT2gameplay"
+ R"MT2gameplay(all('gameplay.action', {operation='management.set_buzz', arguments={["positive"]=positive, ["negative"]=negative}})
end
mt2.management.purge_expired_awards = function()
    return call('gameplay.action', {operation='management.purge_expired_awards', arguments={}})
end
mt2.management.add_award = function(name, source, impressions, conversions, good)
    return call('gameplay.action', {operation='management.add_award', arguments={["name"]=name, ["source"]=source, ["impressions"]=impressions, ["conversions"]=conversions, ["good"]=good}})
end
mt2.modular.get_layout = function(building)
    return call('gameplay.query', {operation='modular.get_layout', arguments={["building"]=building}})
end
mt2.modular.history = function()
    return call('gameplay.query', {operation='modular.history', arguments={}})
end
mt2.modular.add_block = function(building, position)
    return call('gameplay.action', {operation='modular.add_block', arguments={["building"]=building, ["position"]=position}})
end
mt2.modular.remove_block = function(building, position)
    return call('gameplay.action', {operation='modular.remove_block', arguments={["building"]=building, ["position"]=position}})
end
mt2.modular.add_entrance = function(building, position)
    return call('gameplay.action', {operation='modular.add_entrance', arguments={["building"]=building, ["position"]=position}})
end
mt2.modular.remove_entrance = function(building, position)
    return call('gameplay.action', {operation='modular.remove_entrance', arguments={["building"]=building, ["position"]=position}})
end
mt2.modular.undo = function(history)
    return call('gameplay.action', {operation='modular.undo', arguments={["history"]=history}})
end
mt2.modular.redo = function(history)
    return call('gameplay.action', {operation='modular.redo', arguments={["history"]=history}})
end
mt2.modular.place_prefab = function(layout, plan, theme, pose, clearance)
    return call('gameplay.action', {operation='modular.place_prefab', arguments={["layout"]=layout, ["plan"]=plan, ["theme"]=theme, ["pose"]=pose, ["clearance"]=clearance}})
end
mt2.walls.place_curve = function(curve)
    return call('gameplay.action', {operation='walls.place_curve', arguments={["curve"]=curve}})
end
mt2.paths.place_route = function(region, start, end_, start_node, end_node, start_split, end_split)
    return call('gameplay.action', {operation='paths.place_route', arguments={["region"]=region, ["start"]=start, ["end"]=end_, ["start_node"]=start_node, ["end_node"]=end_node, ["start_split"]=start_split, ["end_split"]=end_split}})
end
mt2.paths.graph = function()
    return call('gameplay.query', {operation='paths.graph', arguments={}})
end
mt2.paths.place = function(region, start, end_, start_node, end_node)
    return call('gameplay.action', {operation='paths.place', arguments={["region"]=region, ["start"]=start, ["end"]=end_, ["start_node"]=start_node, ["end_node"]=end_node}})
end
mt2.paths.link = function(start_region, end_region, name, start, end_, start_node, end_node)
    return call('gameplay.action', {operation='paths.link', arguments={["start_region"]=start_region, ["end_region"]=end_region, ["name"]=name, ["start"]=start, ["end"]=end_, ["start_node"]=start_node, ["end_node"]=end_node}})
end
mt2.walls.place = function(plan)
    return call('gameplay.action', {operation='walls.place', arguments={["plan"]=plan}})
end
mt2.paths.remove = function(handle)
    return call('gameplay.action', {operation='paths.remove', arguments={["handle"]=handle}})
end
mt2.paths.undo = function(receipt)
    return call('gameplay.action', {operation='paths.undo', arguments={["receipt"]=receipt}})
end
mt2.paths.redo = function(receipt)
    return call('gameplay.action', {operation='paths.redo', arguments={["receipt"]=receipt}})
end
mt2.regions.set_type = function(handle, type, position)
    return call('gameplay.action', {operation='regions.set_type', arguments={["handle"]=handle, ["type"]=type, ["position"]=position}})
end
mt2.regions.list = functi)MT2gameplay"
+ R"MT2gameplay(on(offset, limit)
    if offset == nil then offset = 0 end
    if limit == nil then limit = 100 end
    return call('gameplay.query', {operation='regions.list', arguments={["offset"]=offset, ["limit"]=limit}})
end
mt2.regions.get = function(handle)
    return call('gameplay.query', {operation='regions.get', arguments={["handle"]=handle}})
end
mt2.regions.presets = function()
    return call('gameplay.query', {operation='regions.presets', arguments={}})
end
mt2.regions.rename = function(handle, name)
    return call('gameplay.action', {operation='regions.rename', arguments={["handle"]=handle, ["name"]=name}})
end
mt2.regions.set_tint = function(handle, values)
    return call('gameplay.action', {operation='regions.set_tint', arguments={["handle"]=handle, ["values"]=values}})
end
mt2.regions.set_light = function(handle, values)
    return call('gameplay.action', {operation='regions.set_light', arguments={["handle"]=handle, ["values"]=values}})
end
mt2.regions.set_ambient = function(handle, values)
    return call('gameplay.action', {operation='regions.set_ambient', arguments={["handle"]=handle, ["values"]=values}})
end
mt2.regions.set_sky = function(handle, values)
    return call('gameplay.action', {operation='regions.set_sky', arguments={["handle"]=handle, ["values"]=values}})
end
mt2.regions.set_particles = function(handle, type)
    return call('gameplay.action', {operation='regions.set_particles', arguments={["handle"]=handle, ["type"]=type}})
end
mt2.regions.set_particle_color = function(handle, values)
    return call('gameplay.action', {operation='regions.set_particle_color', arguments={["handle"]=handle, ["values"]=values}})
end
mt2.regions.styles = function()
    return call('gameplay.query', {operation='regions.styles', arguments={}})
end
mt2.regions.set_styles = function(handle, styles)
    return call('gameplay.action', {operation='regions.set_styles', arguments={["handle"]=handle, ["styles"]=styles}})
end
mt2.terrain.heights = function(handle, offset, limit)
    if offset == nil then offset = 0 end
    if limit == nil then limit = 256 end
    return call('gameplay.query', {operation='terrain.heights', arguments={["handle"]=handle, ["offset"]=offset, ["limit"]=limit}})
end
mt2.terrain.change_height = function(handle, position, up, radius)
    if radius == nil then radius = 1 end
    return call('gameplay.action', {operation='terrain.change_height', arguments={["handle"]=handle, ["position"]=position, ["up"]=up, ["radius"]=radius}})
end
mt2.regions.set_levels = function(handle, minimum, maximum, position)
    return call('gameplay.action', {operation='regions.set_levels', arguments={["handle"]=handle, ["minimum"]=minimum, ["maximum"]=maximum, ["position"]=position}})
end
mt2.regions.set_preset = function(handle, name)
    return call('gameplay.action', {operation='regions.set_preset', arguments={["handle"]=handle, ["name"]=name}})
end
mt2.regions.purchase = function(handle, position)
    return call('gameplay.action', {operation='regions.purchase', arguments={["handle"]=handle, ["position"]=position}})
end
mt2.regions.sell = function(handle, position)
    return call('gameplay.action', {operation='regions.sell', arguments={["handle"]=handle, ["position"]=position}})
end
mt2.terrain.contours = function(handle, style, path, offset, limit)
    if path == nil then path = 0 end
    if offset == nil then offset = 0 end
    if limit == nil then limit = 256 end
    return call('gameplay.query', {operation='terrain.contours', arguments={["handle"]=handle, ["style"]=style, ["path"]=path, ["offset"]=offset, ["limit"]=limit}})
end
mt2.terrain.paint = function(handle, style, stamps, label)
    if label == nil then label = "Paint terrain" end
    return call('gameplay.action', {operation='terrain.paint', arguments={["handle"]=handle, ["style"]=style, ["stamps"]=stamps, ["label"]=label}})
end
mt2.scenery.stroke = function(kind, region, finish_position, stamps, type, variant, random_variant, replace, bounds)
    if random_variant ==)MT2gameplay"
+ R"MT2gameplay( nil then random_variant = false end
    if replace == nil then replace = false end
    return call('gameplay.action', {operation='scenery.stroke', arguments={["kind"]=kind, ["region"]=region, ["finish_position"]=finish_position, ["stamps"]=stamps, ["type"]=type, ["variant"]=variant, ["random_variant"]=random_variant, ["replace"]=replace, ["bounds"]=bounds}})
end
mt2.scenery.transform = function(targets)
    return call('gameplay.action', {operation='scenery.transform', arguments={["targets"]=targets}})
end
mt2.scenery.duplicate = function(handle, position, rotation, scale)
    return call('gameplay.action', {operation='scenery.duplicate', arguments={["handle"]=handle, ["position"]=position, ["rotation"]=rotation, ["scale"]=scale}})
end
mt2.scenery.list = function()
    return call('gameplay.query', {operation='scenery.list', arguments={}})
end
mt2.scenery.move_group = function(group, position, rotation, scale)
    return call('gameplay.action', {operation='scenery.move_group', arguments={["group"]=group, ["position"]=position, ["rotation"]=rotation, ["scale"]=scale}})
end
mt2.scenery.destroy_group = function(group)
    return call('gameplay.action', {operation='scenery.destroy_group', arguments={["group"]=group}})
end
mt2.scenery.groups = function()
    return call('gameplay.query', {operation='scenery.groups', arguments={}})
end
mt2.scenery.group = function(handles)
    return call('gameplay.action', {operation='scenery.group', arguments={["handles"]=handles}})
end
mt2.scenery.ungroup = function(handles)
    return call('gameplay.action', {operation='scenery.ungroup', arguments={["handles"]=handles}})
end
mt2.world.entities = function(limit)
    if limit == nil then limit = 100 end
    return call('gameplay.query', {operation='world.entities', arguments={["limit"]=limit}})
end
mt2.subscribers.candidates = function()
    return call('gameplay.query', {operation='subscribers.candidates', arguments={}})
end
mt2.subscribers.get = function(handle)
    return call('gameplay.query', {operation='subscribers.get', arguments={["handle"]=handle}})
end
mt2.development.staff = function()
    return call('gameplay.query', {operation='development.staff', arguments={}})
end
mt2.development.research = function()
    return call('gameplay.query', {operation='development.research', arguments={}})
end
mt2.projects.catalogue = function()
    return call('gameplay.query', {operation='projects.catalogue', arguments={}})
end
mt2.projects.current = function()
    return call('gameplay.query', {operation='projects.current', arguments={}})
end
mt2.network.snapshot = function()
    return call('gameplay.query', {operation='network.snapshot', arguments={}})
end
mt2.network.catalogue = function()
    return call('gameplay.query', {operation='network.catalogue', arguments={}})
end
mt2.development.hire = function()
    return call('gameplay.action', {operation='development.hire', arguments={}})
end
mt2.development.fire = function()
    return call('gameplay.action', {operation='development.fire', arguments={}})
end
mt2.development.set_state = function(handle, value)
    return call('gameplay.action', {operation='development.set_state', arguments={["handle"]=handle, ["value"]=value}})
end
mt2.development.set_active = function(handle, value)
    return call('gameplay.action', {operation='development.set_active', arguments={["handle"]=handle, ["value"]=value}})
end
mt2.projects.start = function(source)
    return call('gameplay.action', {operation='projects.start', arguments={["source"]=source}})
end
mt2.projects.cancel = function()
    return call('gameplay.action', {operation='projects.cancel', arguments={}})
end
mt2.projects.commit = function()
    return call('gameplay.action', {operation='projects.commit', arguments={}})
end
mt2.projects.set_workers = function(count)
    return call('gameplay.action', {operation='projects.set_workers', arguments={["count"]=count}})
end
mt2.development.assign_research_workers = function(count)
    return call('gameplay.ac)MT2gameplay"
+ R"MT2gameplay(tion', {operation='development.assign_research_workers', arguments={["count"]=count}})
end
mt2.network.place_node = function(key, position)
    return call('gameplay.action', {operation='network.place_node', arguments={["key"]=key, ["position"]=position}})
end
mt2.network.place_pipe = function(start, end_, position)
    return call('gameplay.action', {operation='network.place_pipe', arguments={["start"]=start, ["end"]=end_, ["position"]=position}})
end
mt2.network.remove = function(handle)
    return call('gameplay.action', {operation='network.remove', arguments={["handle"]=handle}})
end
mt2.experimental.operations = function()
    return call('gameplay.operations')
end
)MT2gameplay"
