std::string(R"MT2gameplay([
  {
    "name": "actors.regions",
    "kind": "query",
    "parameters": [],
    "summary": "List saved root Region IDs and native level bounds.",
    "returns": "MT2ActorRegion[]",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_census.h"
    ]
  },
  {
    "name": "actors.list",
    "kind": "query",
    "parameters": [
      {
        "name": "region",
        "type": "integer",
        "description": "Optional root Region saved ID; omit for all root Regions.",
        "optional": true
      }
    ],
    "summary": "List current root-world NPCs, Monsters and Bosses with exact handles and native manager membership.",
    "returns": "MT2Actor[]",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_census.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/character_boss_retirement_read.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/monster_move_read.h"
    ]
  },
  {
    "name": "npcs.list",
    "kind": "query",
    "parameters": [
      {
        "name": "region",
        "type": "integer",
        "description": "Optional root Region saved ID; omit for all root Regions.",
        "optional": true
      }
    ],
    "summary": "List current root-world NPCs with exact handles and native manager membership.",
    "returns": "MT2Actor[]",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_census.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/character_boss_retirement_read.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/monster_move_read.h"
    ]
  },
  {
    "name": "monsters.list",
    "kind": "query",
    "parameters": [
      {
        "name": "region",
        "type": "integer",
        "description": "Optional root Region saved ID; omit for all root Regions.",
        "optional": true
      }
    ],
    "summary": "List current root-world Monsters with exact handles and native manager membership.",
    "returns": "MT2Actor[]",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_census.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/character_boss_retirement_read.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/monster_move_read.h"
    ]
  },
  {
    "name": "actors.get",
    "kind": "query",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2ActorHandle",
        "description": "Exact current actor UID and world epoch from actors.list."
      }
    ],
    "summary": "Read a current root-world actor's identity, transform, character and level; NPCs include online/tint/command state.",
    "returns": "MT2Actor",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_census.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_property_runtime.inc"
    ]
  },
  {
    "name": "quests.list",
    "kind": "query",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2ActorHandle",
        "description": "Exact current actor UID and world epoch from actors.list."
      }
    ],
    "summary": "Read a QuestGiver NPC's owned quest slots, authored fields, destination handles and daily counters with reciprocal weak ownership checks.",
    "returns": "MT2NpcQuest[]",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_quest_records.h"
    ]
  },
  {
    "name": "quests.get",
    "kind": "query",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2NpcQuestHandle",
        "description": "Current quest owner/epoch/slot/sequence handle returned by quests.list."
      }
    ],
    "summary": "Read a quest only while its exact current owner, slot and sequence still match.",
    "returns": "MT2NpcQuest",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_quest_records.h"
    ]
  },
  {
    "name": "quests.create",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2ActorHandle",
        "description": "Exact c)MT2gameplay")
+ R"MT2gameplay(urrent actor UID and world epoch from actors.list."
      }
    ],
    "summary": "Generate and append a native quest for a QuestGiver with fewer than five quests and an existing Building fallback destination. Preserve the original native Undo inverse, active visitor state and ledger with complete readback.",
    "returns": "MT2Action",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_quest_list_runtime.inc",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_quest_list_codes.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_quest_list_read.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_quest_visitor_reader.h"
    ],
    "native_result": "MT2QuestEditResult"
  },
  {
    "name": "quests.remove",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2NpcQuestHandle",
        "description": "Current quest owner/epoch/slot/sequence handle returned by quests.list."
      }
    ],
    "summary": "Remove a current quest through native NPC lifetime cleanup and verify its expired visitor references and reindexed survivors. Preserve the original native Undo inverse, active visitor state and ledger with complete readback.",
    "returns": "MT2Action",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_quest_list_runtime.inc",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_quest_list_codes.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_quest_list_read.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_quest_visitor_reader.h"
    ],
    "native_result": "MT2QuestEditResult"
  },
  {
    "name": "quests.swap",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2NpcQuestHandle",
        "description": "Current quest owner/epoch/slot/sequence handle returned by quests.list."
      },
      {
        "name": "slot",
        "type": "integer",
        "description": "Current zero-based destination slot in this NPC's quest list."
      }
    ],
    "summary": "Swap two quest slots through native ReorderQuest and confirm the complete owner/global records. Preserve the original native Undo inverse, active visitor state and ledger with complete readback.",
    "returns": "MT2Action",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_quest_list_runtime.inc",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_quest_list_codes.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_quest_list_read.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_quest_visitor_reader.h"
    ],
    "native_result": "MT2QuestEditResult"
  },
  {
    "name": "quests.set_name",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2NpcQuestHandle",
        "description": "Current quest owner/epoch/slot/sequence handle returned by quests.list."
      },
      {
        "name": "name",
        "type": "string",
        "description": "Nonempty authored title, at most 4096 UTF-8 bytes."
      }
    ],
    "summary": "Assign the quest title through the native String method. Preserve the original native Undo inverse, active visitor state and ledger with complete readback.",
    "returns": "MT2Action",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_quest_list_runtime.inc",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_quest_list_codes.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_quest_list_read.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_quest_visitor_reader.h"
    ],
    "native_result": "MT2QuestEditResult"
  },
  {
    "name": "quests.set_kills",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2NpcQuestHandle",
        "description": "Current quest owner/epoch/slot/sequence handle returned by quests.list."
      },
      {
        "name": "kills",
        "type": "integer",
        "description)MT2gameplay"
+ R"MT2gameplay(": "Native kill target from 0 through 1000000."
      }
    ],
    "summary": "Set the authored kill count with the original selector's field semantics. Preserve the original native Undo inverse, active visitor state and ledger with complete readback.",
    "returns": "MT2Action",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_quest_list_runtime.inc",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_quest_list_codes.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_quest_list_read.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_quest_visitor_reader.h"
    ],
    "native_result": "MT2QuestEditResult"
  },
  {
    "name": "quests.set_rewards",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2NpcQuestHandle",
        "description": "Current quest owner/epoch/slot/sequence handle returned by quests.list."
      },
      {
        "name": "defaults",
        "type": "boolean",
        "description": "Use native default awards; cannot be combined with xp or gold.",
        "optional": true
      },
      {
        "name": "xp",
        "type": "integer",
        "description": "Custom XP reward from 0 through int32 maximum; required with gold when defaults is false.",
        "optional": true
      },
      {
        "name": "gold",
        "type": "integer",
        "description": "Custom gold reward from 0 through int32 maximum; required with xp when defaults is false.",
        "optional": true
      }
    ],
    "summary": "Set custom or default awards through the original native reward setters. Preserve the original native Undo inverse, active visitor state and ledger with complete readback.",
    "returns": "MT2Action",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_quest_list_runtime.inc",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_quest_list_codes.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_quest_list_read.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_quest_visitor_reader.h"
    ],
    "native_result": "MT2QuestEditResult"
  },
  {
    "name": "quests.set_destination",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2NpcQuestHandle",
        "description": "Current quest owner/epoch/slot/sequence handle returned by quests.list."
      },
      {
        "name": "destination",
        "type": "MT2Handle",
        "description": "Current native Building, Monster Zone, Elite Monster or NPC destination handle."
      },
      {
        "name": "kills",
        "type": "integer",
        "description": "Optional Zone kill target from 1 through 1000000; other destination families use their native defaults.",
        "optional": true
      }
    ],
    "summary": "Run original SetDestination with the native default action, then verify all global quest records, reciprocal weak links, its single-operation native Undo inverse, recalculation task and unchanged ledger.",
    "returns": "MT2Action",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_quest_authoring_runtime.inc",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_quest_authoring_read.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/monster_quest_retirement.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/object_identity/remove_runtime/copy/undo_witness.cpp"
    ],
    "native_result": "MT2QuestDestinationResult"
  },
  {
    "name": "actors.boss_phases",
    "kind": "query",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2CharacterHandle",
        "description": "Current definition handle from characters.list; phases are owned native slots."
      }
    ],
    "summary": "Read native phase order, health, VFX/color and owner ability-slot membership.",
    "returns": "MT2BossPhase[]",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/boss_phase_reader.h"
    ]
  },
  {
)MT2gameplay"
+ R"MT2gameplay(    "name": "actors.place_boss",
    "kind": "action",
    "parameters": [
      {
        "name": "character",
        "type": "MT2CharacterHandle",
        "description": "Current kind 3 definition handle from characters.list."
      },
      {
        "name": "region",
        "type": "integer",
        "description": "Saved root Region ID containing the placement point."
      },
      {
        "name": "pose",
        "type": "MT2ActorPose",
        "description": "Complete finite native pose with a unit quaternion and positive scale."
      }
    ],
    "summary": "Purchase a Boss for the original 1000 cash and create it through native AddBoss; verify exact accounting, actor membership/pose/type/level, original tag progression and unchanged native Undo.",
    "returns": "MT2Action",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/boss_placement_calls.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/boss_placement_execution.inc",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/boss_placement_state.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/boss_placement_reader.h"
    ],
    "native_result": "MT2Actor"
  },
  {
    "name": "monsters.place",
    "kind": "action",
    "parameters": [
      {
        "name": "character",
        "type": "MT2CharacterHandle",
        "description": "Current kind 1 definition for rank 0/1 or kind 4 definition for rank 2."
      },
      {
        "name": "region",
        "type": "integer",
        "description": "Saved active root Region ID containing the placement point."
      },
      {
        "name": "rank",
        "type": "integer",
        "description": "Native Monster rank: 0 normal, 1 Elite, 2 DungeonBoss."
      },
      {
        "name": "level",
        "type": "integer",
        "description": "Level within the native Region's current minimum/maximum bounds."
      },
      {
        "name": "pose",
        "type": "MT2MonsterPose",
        "description": "Finite position and unit quaternion. Original spawning uses unit object scale."
      }
    ],
    "summary": "Spawn through original MonsterManager methods, sound and LevelChanged; confirm native destroy Undo, release edit, global/manager counters, full actor and quest/visitor survivors, and unchanged ledger.",
    "returns": "MT2Action",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/monster_action_runtime.inc",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/monster_action_codes.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/monster_zone_probe.inc",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/monster_zone_read.h"
    ],
    "native_result": "MT2Actor"
  },
  {
    "name": "npcs.place",
    "kind": "action",
    "parameters": [
      {
        "name": "character",
        "type": "MT2CharacterHandle",
        "description": "Current native definition handle for the NPC appearance."
      },
      {
        "name": "region",
        "type": "integer",
        "description": "Saved active root Region ID containing the placement point."
      },
      {
        "name": "type",
        "type": "integer",
        "description": "Native role: 0 QuestGiver, 1 Guard, 2 Service NPC."
      },
      {
        "name": "pose",
        "type": "MT2ActorPose",
        "description": "Complete finite pose with unit quaternion and positive scale."
      }
    ],
    "summary": "Create through original NPCManager AddNPC, including two generated quests for a QuestGiver. Verify native cash/destroy Undo, original TypeCost refund, release edit, tag progression, all quest/visitor and actor survivors, and unchanged ledger.",
    "returns": "MT2Action",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_action_runtime.inc",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_place_probe.inc",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_census.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/typed_action/npc_codec.inc)MT2gameplay"
+ R"MT2gameplay("
    ],
    "native_result": "MT2NpcPlacementResult"
  },
  {
    "name": "actors.move",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2ActorHandle",
        "description": "Exact current actor UID and world epoch from actors.list."
      },
      {
        "name": "pose",
        "type": "MT2ActorPose",
        "description": "Complete finite destination pose for a current on-map NPC, Monster or Boss. A Boss must stay within its native Region."
      }
    ],
    "summary": "Move through native BeginEdit, Undo Open, DoMove, Close and EndEdit. Restore prior selection and verify the complete native inverse, pose, visual lifetime and surviving state. Native Boss movement preserves its existing respawn anchor; NPC/Monster movement updates its spawn point.",
    "returns": "MT2Action",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/move_kiss/native_calls.cpp",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_move_runtime.inc",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/monster_move_runtime.inc",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/monster_move_destination.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/monster_move_read.h"
    ],
    "native_result": "MT2Actor"
  },
  {
    "name": "actors.remove",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2ActorHandle",
        "description": "Exact current actor UID and world epoch from actors.list."
      }
    ],
    "summary": "Remove an NPC or a Monster outside a generating Zone through original SelectionManager DestroyProp. Confirm UID/manager/spatial/selection absence, full quest and visitor cascade, native actor/quest inverse records, and unchanged zero-cost ledger.",
    "returns": "MT2Action",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_remove_runtime.inc",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_history_graph.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/monster_remove_runtime.inc",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/monster_remove_read.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_quest_cascade.h"
    ],
    "native_result": "MT2ActorRemovalResult"
  },
  {
    "name": "monsters.zones",
    "kind": "query",
    "parameters": [
      {
        "name": "region",
        "type": "integer",
        "description": "Optional saved root Region ID; omit for every root Region.",
        "optional": true
      }
    ],
    "summary": "Read current Monster Zones, native fixed-point polygons, authored properties and reciprocally owned generated Monster handles.",
    "returns": "MT2MonsterZone[]",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/monster_zone_read.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_quest_targets.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/native_polyshape.h"
    ]
  },
  {
    "name": "monsters.get_zone",
    "kind": "query",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2ZoneHandle",
        "description": "Exact current Monster Zone UID/epoch handle from monsters.zones."
      }
    ],
    "summary": "Read a current Monster Zone only while its full UID and root manager membership match.",
    "returns": "MT2MonsterZone",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/monster_zone_read.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_quest_targets.h"
    ]
  },
  {
    "name": "monsters.create_zone",
    "kind": "action",
    "parameters": [
      {
        "name": "character",
        "type": "MT2CharacterHandle",
        "description": "Current kind 1 definition for generated Monsters."
      },
      {
        "name": "region",
        "type": "integer",
        "description": "Saved active root Region ID."
      },
      {
        "name": "level",
        "type": "integer",
        "description": "Level within th)MT2gameplay"
+ R"MT2gameplay(e native Region's current bounds."
      },
      {
        "name": "stamps",
        "type": "MT2MonsterZoneStamp[]",
        "description": "1 through 64 original native paint stamps, at most 16384 vertices total, in clear space without an existing Zone intersection."
      }
    ],
    "summary": "Construct a private native Zone brush, paint and finish it with a private current-selection receiver. Native island/Region clipping applies. Verify final clipped geometry, generated Monsters and counters, complete native Undo, release points, unchanged survivors/quests/visitors/ledger, and owned receiver cleanup.",
    "returns": "MT2Action",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/monster_zone_action_runtime.inc",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/monster_zone_action_codes.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/monster_zone_effect.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/native_polyshape.h"
    ],
    "native_result": "MT2MonsterZone"
  },
  {
    "name": "monsters.set_zone",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2ZoneHandle",
        "description": "Current Monster Zone handle."
      },
      {
        "name": "settings",
        "type": "MT2MonsterZoneSettings",
        "description": "One or more native definition, density or level fields."
      }
    ],
    "summary": "Apply original Zone properties and rebuild generated Monsters through the native Undo transaction. Confirm old UID retirement, complete make/destroy/edit inverses, replacement membership and all surviving state.",
    "returns": "MT2Action",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/zone_edit_calls.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/zone_edit_stored.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/zone_edit_graph.inc",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/zone_edit_execute.inc"
    ],
    "native_result": "MT2MonsterZone"
  },
  {
    "name": "monsters.reset_zone",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2ZoneHandle",
        "description": "Current Monster Zone handle."
      }
    ],
    "summary": "Destroy and rebuild this Zone's generated Monsters through original lifetime and native Undo methods; verify full ownership, retired UIDs, replacement members and untouched authored properties.",
    "returns": "MT2Action",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/zone_edit_calls.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/zone_edit_execute.inc",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/zone_edit_graph.inc"
    ],
    "native_result": "MT2MonsterZone"
  },
  {
    "name": "monsters.respawn_zone",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2ZoneHandle",
        "description": "Current Monster Zone handle."
      }
    ],
    "summary": "Run the original Zone respawn command for dead members and confirm preserved membership, original spawn positions, death/timer reset and unchanged native Undo and ledger.",
    "returns": "MT2Action",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/zone_edit_calls.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/zone_edit_execute.inc"
    ],
    "native_result": "MT2MonsterZoneRespawnResult"
  },
  {
    "name": "actors.remove_boss",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2ActorHandle",
        "description": "Exact current actor UID and world epoch from actors.list."
      }
    ],
    "summary": "Destroy a Boss through the original SelectionManager command and confirm its native 1000 cash refund and refund Undo. The native game does not retain a Boss recreation inverse; the result states this explicitly.",
    "returns": "MT2Action",
    "native_evidence": [
      "MMORPGTycoon2-Mult)MT2gameplay"
+ R"MT2gameplay(iplayer@8a13602/src/game/boss_placement_calls.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/character_boss_retirement_read.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/object_identity/remove_runtime/copy/undo_witness.cpp"
    ],
    "native_result": "MT2BossRemovalResult"
  },
  {
    "name": "monsters.remove_zone",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2ZoneHandle",
        "description": "Current Monster Zone handle."
      }
    ],
    "summary": "Remove a Zone through original SelectionManager destruction, including generated Monsters and incoming quests. Confirm UID, manager, spatial and weak-reference retirement; complete native Zone/Monster/quest Undo records; exact counters and unchanged ledger.",
    "returns": "MT2Action",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/zone_edit_remove.inc",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/zone_edit_stored.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/zone_edit_graph.inc",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_quest_cascade.h"
    ],
    "native_result": "MT2MonsterZoneRemovalResult"
  },
  {
    "name": "monsters.paint_zone",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2ZoneHandle",
        "description": "Current Monster Zone handle to extend."
      },
      {
        "name": "stamps",
        "type": "MT2MonsterZoneStamp[]",
        "description": "1 through 64 native paint stamps within this owned root Region."
      }
    ],
    "summary": "Extend an existing Monster Zone through the original private brush transaction. Native island/Region clipping and culling of intersecting Zone families apply. Confirm every stamp's authored/walkable snapshots, emitted actor births/retirements, complete native inverse coverage, quest/visitor cascade, release points and selection cleanup.",
    "returns": "MT2Action",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/zone_edit_calls.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/zone_edit_execute.inc",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/zone_edit_graph.inc",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/zone_edit_stored.h"
    ],
    "native_result": "MT2ZonePaintResult"
  },
  {
    "name": "monsters.erase_zones",
    "kind": "action",
    "parameters": [
      {
        "name": "region",
        "type": "integer",
        "description": "Current owned root Region saved ID."
      },
      {
        "name": "stamps",
        "type": "MT2MonsterZoneStamp[]",
        "description": "1 through 64 native erase stamps within this Region."
      }
    ],
    "summary": "Erase native Zone coverage through the original private brush across intersecting Zone families. Confirm complete authored/walkable changes, generated Monster and Zone retirement, native Undo and quest/visitor cleanup. Return confirmed no-op only when the original brush acknowledges no emitted mutation and no Undo.",
    "returns": "MT2Action",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/zone_edit_calls.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/zone_edit_execute.inc",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/zone_edit_graph.inc",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/zone_edit_stored.h"
    ],
    "native_result": "MT2ZonePaintResult"
  },
  {
    "name": "npcs.rename",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2ActorHandle",
        "description": "Exact current actor UID and world epoch from actors.list."
      },
      {
        "name": "name",
        "type": "string",
        "description": "Nonempty NPC name, at most 256 UTF-8 bytes."
      }
    ],
    "summary": "Rename through the original native actor command and confirm the complete property result.",
    "returns": "MT2Action",
    "native_evidence": [
      "MMORP)MT2gameplay"
+ R"MT2gameplay(GTycoon2-Multiplayer@8a13602/src/game/npc_property_runtime.inc"
    ],
    "native_result": "MT2Actor"
  },
  {
    "name": "npcs.set_online",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2ActorHandle",
        "description": "Exact current actor UID and world epoch from actors.list."
      },
      {
        "name": "mode",
        "type": "integer",
        "description": "Native mode 0 through 2; 2 removes forced online/offline tint."
      }
    ],
    "summary": "Set the native online mode and verify its mode, forced-tint flag and color.",
    "returns": "MT2Action",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_property_runtime.inc"
    ],
    "native_result": "MT2Actor"
  },
  {
    "name": "npcs.change_level",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2ActorHandle",
        "description": "Exact current actor UID and world epoch from actors.list."
      },
      {
        "name": "delta",
        "type": "integer",
        "description": "-1 lowers or 1 raises the native command level within Region limits."
      }
    ],
    "summary": "Execute the original Raise Level or Lower Level command and verify the current level and preserved properties.",
    "returns": "MT2Action",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/npc_property_runtime.inc",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/typed_action/npc_property_action.h"
    ],
    "native_result": "MT2Actor"
  },
  {
    "name": "actors.create_boss_phase",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2CharacterHandle",
        "description": "Current definition handle from characters.list; phases are owned native slots."
      },
      {
        "name": "name",
        "type": "string",
        "description": "Optional authored phase name; omit for the native numbered default.",
        "optional": true
      }
    ],
    "summary": "Construct and attach a phase through original native allocation/ownership functions.",
    "returns": "MT2Action",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/boss_phase_property_runtime.inc"
    ],
    "native_result": "MT2BossPhase[]"
  },
  {
    "name": "actors.remove_boss_phase",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2CharacterHandle",
        "description": "Current definition handle from characters.list; phases are owned native slots."
      },
      {
        "name": "phase",
        "type": "integer",
        "description": "Current zero-based phase slot in actors.boss_phases."
      }
    ],
    "summary": "Remove an owned phase through native RemoveBossPhase and verify all remaining phase records.",
    "returns": "MT2Action",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/boss_phase_property_runtime.inc"
    ],
    "native_result": "MT2BossPhase[]"
  },
  {
    "name": "actors.reorder_boss_phase",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2CharacterHandle",
        "description": "Current definition handle from characters.list; phases are owned native slots."
      },
      {
        "name": "phase",
        "type": "integer",
        "description": "Current zero-based phase slot in actors.boss_phases."
      },
      {
        "name": "boundary",
        "type": "integer",
        "description": "Drop boundary from 0 through phase count; native semantics place the phase before this boundary."
      }
    ],
    "summary": "Reorder through native phase ownership and verify the complete final order.",
    "returns": "MT2Action",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/boss_phase_runtime.inc",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/boss_phase_reader.h"
    ],
    "native_result": "MT2BossPhase[]")MT2gameplay"
+ R"MT2gameplay(
  },
  {
    "name": "actors.set_boss_phase",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2CharacterHandle",
        "description": "Current definition handle from characters.list; phases are owned native slots."
      },
      {
        "name": "phase",
        "type": "integer",
        "description": "Current zero-based phase slot in actors.boss_phases."
      },
      {
        "name": "settings",
        "type": "MT2BossPhaseSettings",
        "description": "One or more authored name, health, VFX or color fields."
      }
    ],
    "summary": "Edit phase fields between native pre/post change events and verify every phase record.",
    "returns": "MT2Action",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/boss_phase_property.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/boss_phase_property_runtime.inc"
    ],
    "native_result": "MT2BossPhase[]"
  },
  {
    "name": "actors.add_phase_ability",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2CharacterHandle",
        "description": "Current definition handle from characters.list; phases are owned native slots."
      },
      {
        "name": "phase",
        "type": "integer",
        "description": "Current zero-based phase slot in actors.boss_phases."
      },
      {
        "name": "ability",
        "type": "integer",
        "description": "Current zero-based prototype slot in the owning character's ability array."
      }
    ],
    "summary": "Construct and attach an owned phase element for a current owner ability.",
    "returns": "MT2Action",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/boss_phase_property_runtime.inc"
    ],
    "native_result": "MT2BossPhase[]"
  },
  {
    "name": "actors.remove_phase_ability",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2CharacterHandle",
        "description": "Current definition handle from characters.list; phases are owned native slots."
      },
      {
        "name": "phase",
        "type": "integer",
        "description": "Current zero-based phase slot in actors.boss_phases."
      },
      {
        "name": "element",
        "type": "integer",
        "description": "Current zero-based element slot within this phase's abilities."
      }
    ],
    "summary": "Remove an element through native phase ownership between original change events.",
    "returns": "MT2Action",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/boss_phase_property_runtime.inc"
    ],
    "native_result": "MT2BossPhase[]"
  },
  {
    "name": "characters.list",
    "kind": "query",
    "parameters": [
      {
        "name": "include_deleted",
        "type": "boolean",
        "description": "Include native destroyed global slots.",
        "optional": true,
        "default": false
      }
    ],
    "summary": "List current native character type definitions and exact lifetime handles.",
    "returns": "MT2Character[]",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/character_definition_identity.h",
      "adapters/mt2/gameplay/characters/native-evidence.json"
    ]
  },
  {
    "name": "characters.get",
    "kind": "query",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2CharacterHandle",
        "description": "Current exact definition handle from characters.list/get; includes epoch, UID, kind and global index."
      }
    ],
    "summary": "Read a current character definition, names, flags and stats.",
    "returns": "MT2Character",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/character_definition_identity.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/character_rename_runtime.inc",
      "adapters/mt2/gameplay/characters/native-evidence.json"
    ]
  },
  {
    "name": "characters.stats",
    "kind": "que)MT2gameplay"
+ R"MT2gameplay(ry",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2CharacterHandle",
        "description": "Current exact definition handle from characters.list/get; includes epoch, UID, kind and global index."
      }
    ],
    "summary": "Read the two native attribute sets, speed and boss phase health.",
    "returns": "MT2CharacterStats",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/character_rename_runtime.inc",
      "adapters/mt2/gameplay/characters/native-evidence.json"
    ]
  },
  {
    "name": "characters.appearance",
    "kind": "query",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2CharacterHandle",
        "description": "Current exact definition handle from characters.list/get; includes epoch, UID, kind and global index."
      }
    ],
    "summary": "Read the complete owned costume record and weapon categories.",
    "returns": "MT2CharacterAppearance",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/costume_record_reader.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/costume_weapon_read.h",
      "adapters/mt2/gameplay/characters/native-evidence.json"
    ]
  },
  {
    "name": "characters.abilities",
    "kind": "query",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2CharacterHandle",
        "description": "Current exact definition handle from characters.list/get; includes epoch, UID, kind and global index."
      }
    ],
    "summary": "Read native ability slots, complete records and exact ability handles.",
    "returns": "MT2CharacterAbility[]",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/ability_edit_runtime.inc",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/ability_membership_runtime.inc",
      "adapters/mt2/gameplay/characters/native-evidence.json"
    ]
  },
  {
    "name": "characters.create",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2CharacterHandle",
        "description": "Current exact definition handle from characters.list/get; includes epoch, UID, kind and global index."
      },
      {
        "name": "kind",
        "type": "integer",
        "description": "Native character kind 0 through 4; defaults to the supplied base's kind.",
        "optional": true
      }
    ],
    "summary": "Create a native base character slot in the supplied base's owning collection. Omit kind to use the base kind.",
    "returns": "MT2Action",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/character_membership_runtime.inc",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/character_membership_codes.h",
      "adapters/mt2/gameplay/characters/native-evidence.json"
    ],
    "native_result": "MT2Character"
  },
  {
    "name": "characters.create_variant",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2CharacterHandle",
        "description": "Current exact definition handle from characters.list/get; includes epoch, UID, kind and global index."
      },
      {
        "name": "name",
        "type": "string",
        "description": "Unused nonempty variant name, up to 1024 UTF-8 bytes."
      }
    ],
    "summary": "Copy a base's complete native definition into a new variant using MakeVariant.",
    "returns": "MT2Action",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/character_membership_runtime.inc",
      "adapters/mt2/gameplay/characters/native-evidence.json"
    ],
    "native_result": "MT2Character"
  },
  {
    "name": "characters.remove",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2CharacterHandle",
        "description": "Current exact definition handle from characters.list/get; includes epoch, UID, kind and global index."
      }
    ],
    "summary": "Remove the selected character through its original owning collection and )MT2gameplay"
+ R"MT2gameplay(confirm definition UID retirement; native actor cleanup follows the game's lifecycle.",
    "returns": "MT2Action",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/character_membership_runtime.inc",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/character_membership_npc_completion.h",
      "adapters/mt2/gameplay/characters/native-evidence.json"
    ],
    "native_result": "table"
  },
  {
    "name": "characters.rename",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2CharacterHandle",
        "description": "Current exact definition handle from characters.list/get; includes epoch, UID, kind and global index."
      },
      {
        "name": "name",
        "type": "string",
        "description": "Unused nonempty name, up to 1024 UTF-8 bytes."
      }
    ],
    "summary": "Set both authored names through native SetName, including its native edited/day and user notifications.",
    "returns": "MT2Action",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/character_rename_runtime.inc",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/character_rename_codes.h",
      "adapters/mt2/gameplay/characters/native-evidence.json"
    ],
    "native_result": "MT2Character"
  },
  {
    "name": "characters.set_stat",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2CharacterHandle",
        "description": "Current exact definition handle from characters.list/get; includes epoch, UID, kind and global index."
      },
      {
        "name": "field",
        "type": "\"health\"|\"mana\"|\"rage\"|\"speed\"",
        "description": "Native character stat to set."
      },
      {
        "name": "value",
        "type": "number",
        "description": "Finite native stat value from 0 through 1000000."
      }
    ],
    "summary": "Set native level-one attributes or speed, then synchronize variants. Health sets the second attribute set to twice the first; boss health targets the first phase.",
    "returns": "MT2Action",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/character_rename_runtime.inc",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/typed_action/character_rename_action.h",
      "adapters/mt2/gameplay/characters/native-evidence.json"
    ],
    "native_result": "MT2CharacterStats"
  },
  {
    "name": "characters.set_appearance",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2CharacterHandle",
        "description": "Current exact definition handle from characters.list/get; includes epoch, UID, kind and global index."
      },
      {
        "name": "record",
        "type": "MT2CostumeRecord",
        "description": "Complete editable costume returned by characters.appearance."
      }
    ],
    "summary": "Deep-assign a complete costume with native transform construction and owner edit/costume notifications.",
    "returns": "MT2Action",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/costume_native_view.h",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/costume_edit_runtime.inc",
      "adapters/mt2/gameplay/characters/native-evidence.json"
    ],
    "native_result": "MT2CostumeRecord"
  },
  {
    "name": "characters.set_weapon_category",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2CharacterHandle",
        "description": "Current exact definition handle from characters.list/get; includes epoch, UID, kind and global index."
      },
      {
        "name": "category",
        "type": "string",
        "description": "Nonempty native weapon-category name."
      },
      {
        "name": "enabled",
        "type": "boolean",
        "description": "Add or remove this category."
      }
    ],
    "summary": "Update the native ordered weapon category set and verify its exact result.",
    "returns": "MT2Action",
  )MT2gameplay"
+ R"MT2gameplay(  "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/costume_edit_runtime.inc",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/costume_weapon_read.h",
      "adapters/mt2/gameplay/characters/native-evidence.json"
    ],
    "native_result": "string[]"
  },
  {
    "name": "characters.create_ability",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2CharacterHandle",
        "description": "Current exact definition handle from characters.list/get; includes epoch, UID, kind and global index."
      },
      {
        "name": "level",
        "type": "integer",
        "description": "Native authoring level, 1 through 100; native boss creation uses level 1.",
        "optional": true,
        "default": 1
      }
    ],
    "summary": "Create an ability using the game's original NewAbility command and a private native receiver.",
    "returns": "MT2Action",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/ability_membership_runtime.inc",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/ability_receiver_cleanup.h",
      "adapters/mt2/gameplay/characters/native-evidence.json"
    ],
    "native_result": "MT2CharacterAbility[]"
  },
  {
    "name": "characters.copy_ability",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2CharacterHandle",
        "description": "Current exact definition handle from characters.list/get; includes epoch, UID, kind and global index."
      },
      {
        "name": "ability",
        "type": "MT2AbilityHandle",
        "description": "Current ability owner, epoch, exact UID and owner-array slot from characters.abilities."
      },
      {
        "name": "level",
        "type": "integer",
        "description": "Target authoring level, 1 through 100.",
        "optional": true,
        "default": 1
      }
    ],
    "summary": "Copy a complete source ability, including across character owners, through the original native dock transaction. New copies use display slot -1; dock them separately when needed.",
    "returns": "MT2Action",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/ability_membership_runtime.inc",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/ability_receiver_cleanup.h",
      "adapters/mt2/gameplay/characters/native-evidence.json"
    ],
    "native_result": "MT2CharacterAbility[]"
  },
  {
    "name": "characters.remove_ability",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2CharacterHandle",
        "description": "Current exact definition handle from characters.list/get; includes epoch, UID, kind and global index."
      },
      {
        "name": "ability",
        "type": "MT2AbilityHandle",
        "description": "Current ability owner, epoch, exact UID and owner-array slot from characters.abilities."
      }
    ],
    "summary": "Remove an ability using its original native Icon transaction and ownership-aware private cleanup.",
    "returns": "MT2Action",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/ability_membership_runtime.inc",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/ability_receiver_cleanup.h",
      "adapters/mt2/gameplay/characters/native-evidence.json"
    ],
    "native_result": "MT2CharacterAbility[]"
  },
  {
    "name": "characters.dock_ability",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2CharacterHandle",
        "description": "Current exact definition handle from characters.list/get; includes epoch, UID, kind and global index."
      },
      {
        "name": "ability",
        "type": "MT2AbilityHandle",
        "description": "Current ability owner, epoch, exact UID and owner-array slot from characters.abilities."
      },
      {
        "name": "level",
        "type": "integer",
        "description": "Target authoring level, 1 through 100.")MT2gameplay"
+ R"MT2gameplay(
      },
      {
        "name": "display_slot",
        "type": "integer",
        "description": "Target display slot -1 through 11."
      }
    ],
    "summary": "Set native ability level and display slot through the original dock transaction.",
    "returns": "MT2Action",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/ability_membership_runtime.inc",
      "adapters/mt2/gameplay/characters/native-evidence.json"
    ],
    "native_result": "MT2CharacterAbility[]"
  },
  {
    "name": "characters.set_ability",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2CharacterHandle",
        "description": "Current exact definition handle from characters.list/get; includes epoch, UID, kind and global index."
      },
      {
        "name": "ability",
        "type": "MT2AbilityHandle",
        "description": "Current ability owner, epoch, exact UID and owner-array slot from characters.abilities."
      },
      {
        "name": "record",
        "type": "MT2AbilityRecord",
        "description": "Complete editable prototype record returned by characters.abilities; retain at least one effect."
      }
    ],
    "summary": "Edit native prototype strings, effects, costs, selectors, colors, timings, flags and level; simplify and notify its owner. Native normalization differences remain unknown.",
    "returns": "MT2Action",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/ability_edit_runtime.inc",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/typed_action/ability_edit_action.h",
      "adapters/mt2/gameplay/characters/native-evidence.json"
    ],
    "native_result": "MT2AbilityRecord"
  },
  {
    "name": "characters.import",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2CharacterHandle",
        "description": "Current exact definition handle from characters.list/get; includes epoch, UID, kind and global index."
      },
      {
        "name": "compressed_record",
        "type": "string",
        "description": "Lowercase hex of the PNG's embedded compressed RecordV1/V2 character definition, at most 30000 bytes before hex encoding."
      }
    ],
    "summary": "Validate an embedded prefab, round-trip its private original image, invoke native import once and confirm its original success notification through a private native pane. Preserve an existing design selection/notice; initialize a previously unbound hidden component. Reject unsaved native authoring input.",
    "returns": "MT2Action",
    "native_evidence": [
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/character_import_runtime.inc",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/character_import_native.cpp",
      "MMORPGTycoon2-Multiplayer@8a13602/src/game/character_import_payload.h",
      "adapters/mt2/gameplay/characters/native-evidence.json"
    ],
    "native_result": "table"
  },
  {
    "name": "dungeons.list",
    "kind": "query",
    "parameters": [],
    "summary": "List current root DungeonExterior handles and exact Dungeon UID strings.",
    "returns": "table",
    "native_evidence": [
      "Multiplayer/src/game/dungeon_scene_roster.h"
    ]
  },
  {
    "name": "dungeons.get",
    "kind": "query",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2Handle",
        "description": "Current root DungeonExterior handle with epoch and decimal UID string."
      }
    ],
    "summary": "Read native desired instance count, atmosphere, authored light and pool count.",
    "returns": "MT2DungeonState",
    "native_evidence": [
      "Multiplayer/src/game/dungeon_config_reader.h",
      "Multiplayer/src/game/dungeon_light_runtime.inc",
      "Multiplayer/src/game/dungeon_sound_runtime.inc"
    ]
  },
  {
    "name": "dungeons.layout",
    "kind": "query",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2Handle",
        "description": "Current root Dungeon)MT2gameplay"
+ R"MT2gameplay(Exterior handle with epoch and decimal UID string."
      }
    ],
    "summary": "Read authored layout cells, open flags, Theme keys, packed cell colors and palette.",
    "returns": "MT2DungeonLayout",
    "native_evidence": [
      "Multiplayer/src/game/dungeon_layout_reader.h"
    ]
  },
  {
    "name": "dungeons.erase_paint",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2Handle",
        "description": "Current root DungeonExterior handle with epoch and decimal UID string."
      },
      {
        "name": "paths",
        "type": "table",
        "description": "Array of polygon paths; each path has at least three x/z integer pairs in native clipping coordinates. At most 100000 points total."
      }
    ],
    "summary": "Erase existing Dungeon Canvas paint inside supplied native clipping paths through the original Canvas erase and retained DungeonPaint Undo graph.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/native_polyshape.h",
      "Multiplayer/src/game/dungeon_input_calls.h",
      "Multiplayer/src/game/dungeon_input_port.inc",
      "Multiplayer/src/game/dungeon_input_native.h"
    ],
    "native_result": "table"
  },
  {
    "name": "dungeons.paint",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2Handle",
        "description": "Current root DungeonExterior handle with epoch and decimal UID string."
      },
      {
        "name": "paths",
        "type": "table",
        "description": "Array of polygon paths with at least three x/z integer pairs each, in native clipping coordinates. At most 100000 points."
      },
      {
        "name": "fill_records",
        "type": "table",
        "description": "Canonical breadth-first mmoFill rows: {parentIndex,childIndex,{type,bits,text},tokens}; root parent=-1, token types 0..4, float tokens carry uint32 bits. At most 2048 rows, 16384 tokens and 65536 text bytes."
      }
    ],
    "summary": "Load a detached native Fill from portable Records, confirm its exact native save readback, then paint supplied clipping paths through original Stroke/Canvas lifecycle and DungeonPaint Undo.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/dungeon_fill_record.h",
      "Multiplayer/src/game/dungeon_fill_native.cpp",
      "Multiplayer/src/game/subscriber_lifecycle/record_decode.cpp",
      "Multiplayer/src/game/subscriber_lifecycle/record_graph.cpp",
      "Multiplayer/src/game/dungeon_input_calls.h",
      "Multiplayer/src/game/dungeon_input_port.inc",
      "Multiplayer/src/game/dungeon_input_native.h"
    ],
    "native_result": "table"
  },
  {
    "name": "dungeons.instances",
    "kind": "query",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2Handle",
        "description": "Current root DungeonExterior handle with epoch and decimal UID string."
      }
    ],
    "summary": "List current saved Instance indices and started flags with reciprocal scene validation.",
    "returns": "MT2DungeonInstance[]",
    "native_evidence": [
      "Multiplayer/src/game/dungeon_config_reader.h",
      "Multiplayer/src/game/dungeon_scene_roster.h"
    ]
  },
  {
    "name": "dungeons.gizmos",
    "kind": "query",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2Handle",
        "description": "Current root DungeonExterior handle with epoch and decimal UID string."
      },
      {
        "name": "instance",
        "type": "integer",
        "description": "Saved Instance.index; omitted selects the authored template scene.",
        "optional": true
      }
    ],
    "summary": "List Gizmo handles, identity, transforms, trigger/state, contents and door edges in one scene.",
    "returns": "MT2DungeonGizmo[]",
    "native_evidence": [
      "Multiplayer/src/game/dungeon_gizmo_reader.h"
    ]
  },
  {
    "name": "dungeons.monsters",
    "kind": "query",
    "parameters)MT2gameplay"
+ R"MT2gameplay(": [
      {
        "name": "handle",
        "type": "MT2Handle",
        "description": "Current root DungeonExterior handle with epoch and decimal UID string."
      },
      {
        "name": "instance",
        "type": "integer",
        "description": "Saved Instance.index; omitted selects the authored template scene.",
        "optional": true
      }
    ],
    "summary": "List scene Monster handles, rank, CharacterTypeID, level, tag, status and authored pose.",
    "returns": "table",
    "native_evidence": [
      "Multiplayer/src/game/monster_zone_read.h",
      "Multiplayer/src/game/dungeon_authored_sample.inc"
    ]
  },
  {
    "name": "dungeons.spawn_monster",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2Handle",
        "description": "Current root DungeonExterior handle with epoch and decimal UID string."
      },
      {
        "name": "character",
        "type": "MT2CharacterHandle",
        "description": "Live Character definition with kind=1 from mt2.characters."
      },
      {
        "name": "rank",
        "type": "integer",
        "description": "Native Monster rank, 0..2."
      },
      {
        "name": "level",
        "type": "integer",
        "description": "Positive native Monster level."
      },
      {
        "name": "position",
        "type": "number[]",
        "description": "Three finite coordinates within the selected scene Region."
      },
      {
        "name": "rotation",
        "type": "number[]",
        "description": "Normalized quaternion x/y/z/w.",
        "optional": true,
        "default": [
          0,
          0,
          0,
          1
        ]
      },
      {
        "name": "instance",
        "type": "integer",
        "description": "Saved Instance.index; omitted selects the authored template scene.",
        "optional": true
      }
    ],
    "summary": "Spawn one Dungeon Monster from a live Monster Character definition through native creation, Destroy inverse and Undo; apply the selected level, achievement and placement sound.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/dungeon_input_calls.h",
      "Multiplayer/src/game/dungeon_input_port.inc",
      "Multiplayer/src/game/dungeon_input_action_calls.h",
      "Multiplayer/src/game/monster_zone_read.h",
      "Multiplayer/src/game/character_definition_identity.h"
    ],
    "native_result": "table"
  },
  {
    "name": "dungeons.set_gizmo_runtime",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2Handle",
        "description": "Current root DungeonExterior handle with epoch and decimal UID string."
      },
      {
        "name": "gizmo",
        "type": "MT2Handle",
        "description": "Current Gizmo handle in the selected Dungeon scene."
      },
      {
        "name": "state",
        "type": "MT2DungeonGizmoRuntime",
        "description": "One or more live runtime fields to adopt."
      },
      {
        "name": "instance",
        "type": "integer",
        "description": "Saved Instance.index; omitted selects the authored template scene.",
        "optional": true
      }
    ],
    "summary": "Set exact live triggered/state/immediate fields by compare-and-exchange and on_map through native SetOnMap; scalar adoption does not dispatch trigger callbacks.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/dungeon_gizmo_live_native.inc",
      "Multiplayer/src/game/dungeon_gizmo_live_read.h",
      "Multiplayer/src/game/dungeon_gizmo_live_state.h"
    ],
    "native_result": "table"
  },
  {
    "name": "dungeons.set_gizmo_contents",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2Handle",
        "description": "Current root DungeonExterior handle with epoch and decimal UID string."
      },
      {
        "name": "gizmo",
        "type": "MT2Handle",
        "description":)MT2gameplay"
+ R"MT2gameplay( "Current container Gizmo in the selected Dungeon scene."
      },
      {
        "name": "contents",
        "type": "MT2DungeonGizmoContents",
        "description": "One or more content counts, each 0..5."
      },
      {
        "name": "instance",
        "type": "integer",
        "description": "Saved Instance.index; omitted selects the authored template scene.",
        "optional": true
      }
    ],
    "summary": "Adopt one or more native container content counts by guarded compare-and-exchange and exact authored readback; existing native editor limits 0..5 apply.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/dungeon_gizmo_reader.h",
      "Multiplayer/src/game/dungeon_gizmo_record.h",
      "Multiplayer/src/game/typed_action/dungeon_gizmo_action.h",
      "Multiplayer/src/game/dungeon_gizmo_live_native.inc"
    ],
    "native_result": "table"
  },
  {
    "name": "dungeons.rename_gizmo",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2Handle",
        "description": "Current root DungeonExterior handle with epoch and decimal UID string."
      },
      {
        "name": "gizmo",
        "type": "MT2Handle",
        "description": "Current Gizmo handle in the selected Dungeon scene."
      },
      {
        "name": "name",
        "type": "string",
        "description": "Nonempty native name, at most 256 UTF-8 bytes without NUL."
      },
      {
        "name": "instance",
        "type": "integer",
        "description": "Saved Instance.index; omitted selects the authored template scene.",
        "optional": true
      }
    ],
    "summary": "Rename a scene-owned Gizmo through its original native Rename method and confirm the exact resulting name.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/dungeon_gizmo_rename.inc"
    ],
    "native_result": "table"
  },
  {
    "name": "dungeons.move_gizmo",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2Handle",
        "description": "Current root DungeonExterior handle with epoch and decimal UID string."
      },
      {
        "name": "gizmo",
        "type": "MT2Handle",
        "description": "Current Gizmo in this offline template."
      },
      {
        "name": "position",
        "type": "number[]",
        "description": "Three finite destination coordinates within the current Region."
      },
      {
        "name": "rotation",
        "type": "number[]",
        "description": "Normalized quaternion x/y/z/w; omitted preserves rotation.",
        "optional": true
      },
      {
        "name": "scale",
        "type": "number[]",
        "description": "Three positive finite scale coordinates; omitted preserves scale.",
        "optional": true
      }
    ],
    "summary": "Move one offline template Gizmo through the original Move/Undo transaction and preserve local selection.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/dungeon_gizmo_control_port.inc",
      "Multiplayer/src/game/move_kiss/native_calls.cpp",
      "Multiplayer/src/game/dungeon_gizmo_live_codes.h"
    ],
    "native_result": "table"
  },
  {
    "name": "dungeons.destroy_gizmo",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2Handle",
        "description": "Current root DungeonExterior handle with epoch and decimal UID string."
      },
      {
        "name": "gizmo",
        "type": "MT2Handle",
        "description": "Current Gizmo in this offline template."
      }
    ],
    "summary": "Destroy one offline template Gizmo through native DestroyProps, confirm original Undo and preserve surviving local selection.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/dungeon_gizmo_control_calls.h",
      "Multiplayer/src/game/dungeon_gizmo_control_port.inc",
      "Multiplayer/src/game/dungeon_gizmo_li)MT2gameplay"
+ R"MT2gameplay(ve_codes.h"
    ],
    "native_result": "table"
  },
  {
    "name": "dungeons.duplicate_gizmo",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2Handle",
        "description": "Current root DungeonExterior handle with epoch and decimal UID string."
      },
      {
        "name": "gizmo",
        "type": "MT2Handle",
        "description": "Current Gizmo in this offline template."
      },
      {
        "name": "position",
        "type": "number[]",
        "description": "Three finite destination coordinates within the current Region."
      },
      {
        "name": "rotation",
        "type": "number[]",
        "description": "Normalized quaternion x/y/z/w; omitted preserves rotation.",
        "optional": true
      },
      {
        "name": "scale",
        "type": "number[]",
        "description": "Three positive finite scale coordinates; omitted preserves scale.",
        "optional": true
      }
    ],
    "summary": "Duplicate one offline template Gizmo through native payment, Duplicate, inverse graph and Undo, preserving local selection.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/dungeon_gizmo_control_port.inc",
      "Multiplayer/src/game/duplicate_calls.h",
      "Multiplayer/src/game/dungeon_gizmo_live_codes.h"
    ],
    "native_result": "table"
  },
  {
    "name": "dungeons.place_gizmo",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2Handle",
        "description": "Current root DungeonExterior handle with epoch and decimal UID string."
      },
      {
        "name": "tool",
        "type": "string",
        "description": "Native placement family: door, key or treasure."
      },
      {
        "name": "type",
        "type": "string",
        "description": "Native Gizmo type resource key."
      },
      {
        "name": "position",
        "type": "number[]",
        "description": "Three finite world position coordinates."
      },
      {
        "name": "variant",
        "type": "string",
        "description": "Native variant resource key.",
        "optional": true,
        "default": ""
      },
      {
        "name": "rotation",
        "type": "number[]",
        "description": "Normalized quaternion x/y/z/w.",
        "optional": true,
        "default": [
          0,
          0,
          0,
          1
        ]
      },
      {
        "name": "scale",
        "type": "number[]",
        "description": "Three positive scale coordinates.",
        "optional": true,
        "default": [
          1,
          1,
          1
        ]
      }
    ],
    "summary": "Place a Door, Key or Treasure in the offline template through original purchase, creation, inverse graph and Undo; native cost and sound apply.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/dungeon_input_calls.h",
      "Multiplayer/src/game/dungeon_input_native.h",
      "Multiplayer/src/game/dungeon_input_port.inc",
      "Multiplayer/src/game/dungeon_input_action_calls.h",
      "Multiplayer/src/game/duplicate_calls.h"
    ],
    "native_result": "table"
  },
  {
    "name": "dungeons.buildings",
    "kind": "query",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2Handle",
        "description": "Current root DungeonExterior handle with epoch and decimal UID string."
      },
      {
        "name": "instance",
        "type": "integer",
        "description": "Saved Instance.index; omitted selects the authored template scene.",
        "optional": true
      }
    ],
    "summary": "List Building handles, types, variants and positions in one Dungeon scene.",
    "returns": "MT2DungeonBuilding[]",
    "native_evidence": [
      "Multiplayer/src/game/dungeon_scene_roster.h",
      "Multiplayer/src/game/dungeon_building_children.h"
    ]
  },
  {
    "name": "dungeons.place_building",
    "kind": "action",
    "parameters":)MT2gameplay"
+ R"MT2gameplay( [
      {
        "name": "handle",
        "type": "MT2Handle",
        "description": "Current root DungeonExterior handle with epoch and decimal UID string."
      },
      {
        "name": "type",
        "type": "string",
        "description": "Existing native BuildingDefinition type; DungeonPortal is supported, DungeonExterior and Scenery are excluded."
      },
      {
        "name": "variant",
        "type": "string",
        "description": "Existing native filtered Building variant key."
      },
      {
        "name": "position",
        "type": "number[]",
        "description": "Three finite coordinates within the active template Region."
      },
      {
        "name": "rotation",
        "type": "number[]",
        "description": "Normalized quaternion x/y/z/w.",
        "optional": true,
        "default": [
          0,
          0,
          0,
          1
        ]
      },
      {
        "name": "scale",
        "type": "number[]",
        "description": "Three positive finite scale coordinates.",
        "optional": true,
        "default": [
          1,
          1,
          1
        ]
      }
    ],
    "summary": "Place a native Building or DungeonPortal through an isolated original Build receiver and maker/Undo. Requires the requested offline template to already be the native active scene; normal native tool and selection changes apply.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/placement_kiss/native_build.cpp",
      "Multiplayer/src/game/placement_kiss/native_family.cpp",
      "Multiplayer/src/game/placement_kiss/effect_cleanup.h",
      "Multiplayer/src/game/dungeon_build_tasks.inc",
      "Multiplayer/src/game/dungeon_building_identity.inc"
    ],
    "native_result": "table"
  },
  {
    "name": "dungeons.configure_instances",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2Handle",
        "description": "Current root DungeonExterior handle with epoch and decimal UID string."
      },
      {
        "name": "count",
        "type": "integer",
        "description": "Desired number of instances, 1..10."
      }
    ],
    "summary": "Run the original native config transaction and confirm desired count; normal native costs apply.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/dungeon_config_runtime.inc",
      "Multiplayer/src/game/dungeon_config_codes.h"
    ],
    "native_result": "table"
  },
  {
    "name": "dungeons.set_atmosphere",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2Handle",
        "description": "Current root DungeonExterior handle with epoch and decimal UID string."
      },
      {
        "name": "atmosphere",
        "type": "integer",
        "description": "Native atmosphere row, 0..5."
      }
    ],
    "summary": "Run the native Dungeon sound command and confirm the selected atmosphere.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/dungeon_sound_runtime.inc",
      "Multiplayer/src/game/dungeon_sound_codes.h"
    ],
    "native_result": "table"
  },
  {
    "name": "dungeons.set_lighting",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2Handle",
        "description": "Current root DungeonExterior handle with epoch and decimal UID string."
      },
      {
        "name": "spinners",
        "type": "integer[]",
        "description": "Four native raw integer Spinner inputs in native widget order."
      },
      {
        "name": "sliders",
        "type": "integer[]",
        "description": "Three native raw integer Slider inputs in native widget order."
      },
      {
        "name": "colors",
        "type": "table",
        "description": "Four RGBA color arrays, each with four finite channels in 0..1."
      }
    ],
    "summary": "Apply native lighting inputs and confirm the exact 23 authored li)MT2gameplay"
+ R"MT2gameplay(ght values from native conversion.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/dungeon_light_input.h",
      "Multiplayer/src/game/dungeon_light_runtime.inc",
      "Multiplayer/src/game/dungeon_light_codes.h"
    ],
    "native_result": "table"
  },
  {
    "name": "dungeons.set_palette",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2Handle",
        "description": "Current root DungeonExterior handle with epoch and decimal UID string."
      },
      {
        "name": "colors",
        "type": "table",
        "description": "Four RGBA color arrays, each with four finite channels in 0..1."
      },
      {
        "name": "defaults",
        "type": "boolean",
        "description": "Use Theme defaults for subsequent native tile edits.",
        "optional": true,
        "default": false
      }
    ],
    "summary": "Set the four packed palette colors and defaults flag through native explicit-owner leaves; existing cells retain their colors.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/dungeon_palette_record.h",
      "Multiplayer/src/game/dungeon_palette_runtime.inc",
      "Multiplayer/src/game/dungeon_palette_codes.h"
    ],
    "native_result": "table"
  },
  {
    "name": "dungeons.open_cell",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2Handle",
        "description": "Current root DungeonExterior handle with epoch and decimal UID string."
      },
      {
        "name": "cell",
        "type": "integer",
        "description": "Zero-based authored layout cell index."
      },
      {
        "name": "theme",
        "type": "string",
        "description": "Existing native ThemeManager resource key."
      }
    ],
    "summary": "Open one eligible cell through an isolated native edit and Undo transaction; existing open cells return changed=false.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/dungeon_cells_calls.h",
      "Multiplayer/src/game/dungeon_cells_native.h",
      "Multiplayer/src/game/dungeon_cells_port.inc"
    ],
    "native_result": "table"
  },
  {
    "name": "dungeons.close_cell",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2Handle",
        "description": "Current root DungeonExterior handle with epoch and decimal UID string."
      },
      {
        "name": "cell",
        "type": "integer",
        "description": "Zero-based authored layout cell index."
      }
    ],
    "summary": "Close one unobstructed open cell through the native edit and Undo transaction; closed cells return changed=false.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/dungeon_cells_calls.h",
      "Multiplayer/src/game/dungeon_cells_native.h",
      "Multiplayer/src/game/dungeon_cells_port.inc"
    ],
    "native_result": "table"
  },
  {
    "name": "dungeons.set_cell_theme",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2Handle",
        "description": "Current root DungeonExterior handle with epoch and decimal UID string."
      },
      {
        "name": "cell",
        "type": "integer",
        "description": "Zero-based authored layout cell index."
      },
      {
        "name": "theme",
        "type": "string",
        "description": "Existing native ThemeManager resource key."
      }
    ],
    "summary": "Apply a native Theme to one open cell through the native edit and Undo transaction.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/dungeon_cells_calls.h",
      "Multiplayer/src/game/dungeon_cells_port.inc"
    ],
    "native_result": "table"
  },
  {
    "name": "dungeons.apply_cell_palette",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2Handle",
        "description": "Current)MT2gameplay"
+ R"MT2gameplay( root DungeonExterior handle with epoch and decimal UID string."
      },
      {
        "name": "cell",
        "type": "integer",
        "description": "Zero-based authored layout cell index."
      }
    ],
    "summary": "Apply the current Dungeon palette to one open cell through native SetCellData and Undo.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/dungeon_cells_calls.h",
      "Multiplayer/src/game/dungeon_cells_port.inc"
    ],
    "native_result": "table"
  },
  {
    "name": "dungeons.make_instance",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2Handle",
        "description": "Current root DungeonExterior handle with epoch and decimal UID string."
      }
    ],
    "summary": "Run native MakeInstance and confirm one appended pool member; native camera effects apply.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/dungeon_instance_native.cpp",
      "Multiplayer/src/game/dungeon_instance_native_codes.h"
    ],
    "native_result": "table"
  },
  {
    "name": "dungeons.destroy_instance",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2Handle",
        "description": "Current root DungeonExterior handle with epoch and decimal UID string."
      },
      {
        "name": "instance",
        "type": "integer",
        "description": "Saved Instance.index within this Dungeon pool, not array position."
      }
    ],
    "summary": "Run native DestroyInstance and confirm removal from its Dungeon pool.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/dungeon_instance_native.cpp",
      "Multiplayer/src/game/dungeon_instance_native_guard.h"
    ],
    "native_result": "table"
  },
  {
    "name": "dungeons.reset_instance",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2Handle",
        "description": "Current root DungeonExterior handle with epoch and decimal UID string."
      },
      {
        "name": "instance",
        "type": "integer",
        "description": "Saved Instance.index within this Dungeon pool, not array position."
      }
    ],
    "summary": "Run native Instance Reset and confirm the same pool member is retained unstarted.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/dungeon_instance_native.cpp",
      "Multiplayer/src/game/dungeon_instance_native_codes.h"
    ],
    "native_result": "table"
  },
  {
    "name": "dungeons.force_reset_instance",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2Handle",
        "description": "Current root DungeonExterior handle with epoch and decimal UID string."
      },
      {
        "name": "instance",
        "type": "integer",
        "description": "Saved Instance.index within this Dungeon pool, not array position."
      }
    ],
    "summary": "Run native Instance ForceReset and confirm the same pool member is retained unstarted.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/dungeon_instance_native.cpp",
      "Multiplayer/src/game/dungeon_instance_native_codes.h"
    ],
    "native_result": "table"
  },
  {
    "name": "dungeons.set_gizmo_state",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2Handle",
        "description": "Current root DungeonExterior handle with epoch and decimal UID string."
      },
      {
        "name": "gizmo",
        "type": "MT2Handle",
        "description": "Current Gizmo in the selected active Dungeon scene."
      },
      {
        "name": "state",
        "type": "integer",
        "description": "Original native State choice 1..3; non-Door Gizmos accept trigger=1 or reset=3."
      },
      {
        "name": "instance",
        "type": "integer",
        "description": "Saved Instance.index; omitted se)MT2gameplay"
+ R"MT2gameplay(lects the authored template scene.",
        "optional": true
      }
    ],
    "summary": "Run original native Gizmo SetState including trigger, Door unlock, animation and gameplay callbacks, then confirm state/trigger/scene unlocked bits. Requires the requested native active scene; the original info_gizmo Window is rebound to this target without human input.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/dungeon_gizmo_runtime.inc",
      "Multiplayer/src/game/dungeon_gizmo_record.h",
      "Multiplayer/src/game/dungeon_gizmo_live_codes.h",
      "Multiplayer/src/game/typed_action/dungeon_gizmo_action.h"
    ],
    "native_result": "table"
  },
  {
    "name": "dungeons.move_building",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2Handle",
        "description": "Current root DungeonExterior handle with epoch and decimal UID string."
      },
      {
        "name": "building",
        "type": "MT2Handle",
        "description": "Current native Building in this offline template; DungeonExterior and Scenery are excluded."
      },
      {
        "name": "position",
        "type": "number[]",
        "description": "Three finite destination coordinates within the active template Region."
      },
      {
        "name": "rotation",
        "type": "number[]",
        "description": "Normalized quaternion x/y/z/w; omitted preserves rotation.",
        "optional": true
      },
      {
        "name": "scale",
        "type": "number[]",
        "description": "Three positive finite scale coordinates; omitted preserves scale.",
        "optional": true
      }
    ],
    "summary": "Move one native Building or DungeonPortal through the original Move/Undo transaction. Requires the requested offline template to already be the native active scene; normal native tool and selection changes apply.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/dungeon_building_engine.inc",
      "Multiplayer/src/game/dungeon_building_identity.inc",
      "Multiplayer/src/game/dungeon_building_graph.h",
      "Multiplayer/src/game/move_kiss/native_calls.cpp",
      "Multiplayer/src/game/ui_remove/diagnostic/ui_diagnostic.cpp",
      "Multiplayer/src/game/duplicate_port.inc",
      "Multiplayer/src/game/duplicate_calls.h",
      "Multiplayer/src/game/duplicate_codeguards.h"
    ],
    "native_result": "table"
  },
  {
    "name": "dungeons.destroy_building",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2Handle",
        "description": "Current root DungeonExterior handle with epoch and decimal UID string."
      },
      {
        "name": "building",
        "type": "MT2Handle",
        "description": "Current native Building in this offline template; DungeonExterior and Scenery are excluded."
      }
    ],
    "summary": "Remove one native Building or DungeonPortal through original DestroyProps and confirm retained native Undo. Requires the requested offline template to already be the native active scene; normal native tool and selection changes apply.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/dungeon_building_engine.inc",
      "Multiplayer/src/game/dungeon_building_identity.inc",
      "Multiplayer/src/game/dungeon_building_graph.h",
      "Multiplayer/src/game/move_kiss/native_calls.cpp",
      "Multiplayer/src/game/ui_remove/diagnostic/ui_diagnostic.cpp",
      "Multiplayer/src/game/duplicate_port.inc",
      "Multiplayer/src/game/duplicate_calls.h",
      "Multiplayer/src/game/duplicate_codeguards.h"
    ],
    "native_result": "table"
  },
  {
    "name": "dungeons.duplicate_building",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2Handle",
        "description": "Current root DungeonExterior handle with epoch and decimal UID string."
      },
      {
        "name": "building",
       )MT2gameplay"
+ R"MT2gameplay( "type": "MT2Handle",
        "description": "Current native Building in this offline template; DungeonExterior and Scenery are excluded."
      },
      {
        "name": "position",
        "type": "number[]",
        "description": "Three finite destination coordinates within the active template Region."
      },
      {
        "name": "rotation",
        "type": "number[]",
        "description": "Normalized quaternion x/y/z/w; omitted preserves rotation.",
        "optional": true
      },
      {
        "name": "scale",
        "type": "number[]",
        "description": "Three positive finite scale coordinates; omitted preserves scale.",
        "optional": true
      }
    ],
    "summary": "Duplicate one native Building or DungeonPortal through original payment, Duplicate, native release edit, selection and Undo transaction. Requires the requested offline template to already be the native active scene; normal native tool and selection changes apply.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/dungeon_building_engine.inc",
      "Multiplayer/src/game/dungeon_building_identity.inc",
      "Multiplayer/src/game/dungeon_building_graph.h",
      "Multiplayer/src/game/move_kiss/native_calls.cpp",
      "Multiplayer/src/game/ui_remove/diagnostic/ui_diagnostic.cpp",
      "Multiplayer/src/game/duplicate_port.inc",
      "Multiplayer/src/game/duplicate_calls.h",
      "Multiplayer/src/game/duplicate_codeguards.h"
    ],
    "native_result": "table"
  },
  {
    "name": "management.fees",
    "kind": "query",
    "parameters": [],
    "summary": "Read game purchase and subscription fees",
    "returns": "MT2ManagementFees",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/management_runtime.cpp"
    ]
  },
  {
    "name": "management.loans",
    "kind": "query",
    "parameters": [],
    "summary": "Read current loans and eligible loan offers",
    "returns": "MT2ManagementLoans",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/management_runtime.cpp"
    ]
  },
  {
    "name": "management.staff",
    "kind": "query",
    "parameters": [],
    "summary": "Read Game Master employment and exact UID handles",
    "returns": "MT2ManagementStaff",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/management_runtime.cpp"
    ]
  },
  {
    "name": "management.statistics",
    "kind": "query",
    "parameters": [],
    "summary": "Read software statistics and development point balances",
    "returns": "MT2ManagementStatistics",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/management_runtime.cpp"
    ]
  },
  {
    "name": "management.release",
    "kind": "query",
    "parameters": [],
    "summary": "Read software version and the current release",
    "returns": "MT2ManagementRelease",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/management_runtime.cpp"
    ]
  },
  {
    "name": "management.policies",
    "kind": "query",
    "parameters": [],
    "summary": "Read chat and cheat Game Master policies",
    "returns": "MT2ManagementPolicy[]",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/management_runtime.cpp"
    ]
  },
  {
    "name": "management.rules",
    "kind": "query",
    "parameters": [],
    "summary": "Read experience, respawn, and feature rules",
    "returns": "MT2ManagementRules",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/management_runtime.cpp"
    ]
  },
  {
    "name": "management.campaigns",
    "kind": "query",
    "parameters": [],
    "summary": "Read active advertising campaigns and native daily costs",
    "returns": "MT2ManagementCampaigns",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/management_runtime.cpp"
    ]
  },
  {
    "name": "management.services",
    "kind": "query",
    "parameters": [
      {
        "name": "building",
        "type": "MT2Handle",
        "description": "Curr)MT2gameplay"
+ R"MT2gameplay(ent building handle returned by the world object API"
      }
    ],
    "summary": "Read services owned by the exact current building",
    "returns": "MT2ManagementService[]",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/management_services.inc"
    ]
  },
  {
    "name": "management.release_cycle",
    "kind": "query",
    "parameters": [],
    "summary": "Read current native release phase, history count, and free points",
    "returns": "MT2ManagementReleaseCycle",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/release_cycle_runtime.inc"
    ]
  },
  {
    "name": "management.set_purchase_fee",
    "kind": "action",
    "parameters": [
      {
        "name": "value",
        "type": "integer",
        "description": "Nonnegative int32 native value"
      }
    ],
    "summary": "Set the game purchase fee",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/ui_price_runtime.cpp"
    ],
    "native_result": "table"
  },
  {
    "name": "management.set_subscription_fee",
    "kind": "action",
    "parameters": [
      {
        "name": "value",
        "type": "integer",
        "description": "Nonnegative int32 native value"
      }
    ],
    "summary": "Set the monthly subscription fee",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/ui_price_runtime.cpp"
    ],
    "native_result": "table"
  },
  {
    "name": "management.allocate_release_funds",
    "kind": "action",
    "parameters": [
      {
        "name": "value",
        "type": "integer",
        "description": "Nonnegative int32 native value"
      }
    ],
    "summary": "Set allocated release funds using the original charge or refund",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/management_runtime.cpp"
    ],
    "native_result": "table"
  },
  {
    "name": "management.set_respawn_seconds",
    "kind": "action",
    "parameters": [
      {
        "name": "value",
        "type": "integer",
        "description": "Nonnegative int32 native value"
      }
    ],
    "summary": "Set subscriber respawn wait in seconds",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/game_rules_native.cpp"
    ],
    "native_result": "table"
  },
  {
    "name": "management.hire_gm",
    "kind": "action",
    "parameters": [],
    "summary": "Hire one Game Master",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/management_runtime.cpp"
    ],
    "native_result": "table"
  },
  {
    "name": "management.fire_gm",
    "kind": "action",
    "parameters": [],
    "summary": "Fire one Game Master",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/management_runtime.cpp"
    ],
    "native_result": "table"
  },
  {
    "name": "management.take_loan",
    "kind": "action",
    "parameters": [
      {
        "name": "key",
        "type": "string",
        "description": "Exact key from management.loans"
      }
    ],
    "summary": "Take an eligible loan offer by exact key",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/management_runtime.cpp"
    ],
    "native_result": "table"
  },
  {
    "name": "management.repay_loan",
    "kind": "action",
    "parameters": [
      {
        "name": "key",
        "type": "string",
        "description": "Exact key from management.loans"
      }
    ],
    "summary": "Repay the current loan by exact key",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/management_runtime.cpp"
    ],
    "native_result": "table"
  },
  {
    "name": "management.purchase_statistic",
    "kind": "action",
    "parameters": [
      {
        "name": "index",
        "type": "integer",
        "description": "Native zero-based statis)MT2gameplay"
+ R"MT2gameplay(tic index, 0 through 9"
      }
    ],
    "summary": "Purchase the next native statistic project",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/management_runtime.cpp"
    ],
    "native_result": "table"
  },
  {
    "name": "management.spend_statistics",
    "kind": "action",
    "parameters": [
      {
        "name": "values",
        "type": "integer[]",
        "description": "Ten nonnegative point allocations in native statistic order"
      }
    ],
    "summary": "Spend development points through each original statistic function",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/management_runtime.cpp"
    ],
    "native_result": "table"
  },
  {
    "name": "management.set_policy",
    "kind": "action",
    "parameters": [
      {
        "name": "policy",
        "type": "integer",
        "description": "0 for chat or 1 for cheating"
      },
      {
        "name": "field",
        "type": "string",
        "description": "enabled, watchSeconds, checkLog, isBannable, banFavorites, banInfluencers, or warnsBeforeBan"
      },
      {
        "name": "value",
        "type": "boolean|integer",
        "description": "Boolean field or watchSeconds 0 through 3600 or warnsBeforeBan 0 through 30"
      }
    ],
    "summary": "Set one native GM policy property and notify its owner",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/gm_policy_native.cpp"
    ],
    "native_result": "table"
  },
  {
    "name": "management.set_respawn_delay",
    "kind": "action",
    "parameters": [
      {
        "name": "value",
        "type": "boolean",
        "description": "Requested native Boolean value"
      }
    ],
    "summary": "Set whether respawn waits for subscribers",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/game_rules_native.cpp"
    ],
    "native_result": "table"
  },
  {
    "name": "management.set_xp",
    "kind": "action",
    "parameters": [
      {
        "name": "values",
        "type": "integer[]",
        "description": "Exactly thirteen editable native XP values; the hidden first word is preserved"
      }
    ],
    "summary": "Apply the complete editable experience rule block",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/game_rules_native.cpp"
    ],
    "native_result": "table"
  },
  {
    "name": "management.set_feature",
    "kind": "action",
    "parameters": [
      {
        "name": "index",
        "type": "integer",
        "description": "0 for PvP duels or 1 for parties"
      },
      {
        "name": "enabled",
        "type": "boolean",
        "description": "Requested feature state; technology must be available"
      }
    ],
    "summary": "Set an unlocked gameplay feature through the original native setter",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/game_rules_native.cpp"
    ],
    "native_result": "table"
  },
  {
    "name": "management.set_release_type",
    "kind": "action",
    "parameters": [
      {
        "name": "type",
        "type": "integer",
        "description": "1 for hotfix, 2 for minor, or 3 for major"
      }
    ],
    "summary": "Set release kind and calculate the next native version",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/release_edit_native.cpp"
    ],
    "native_result": "table"
  },
  {
    "name": "management.set_release_title",
    "kind": "action",
    "parameters": [
      {
        "name": "text",
        "type": "string",
        "description": "Native text, up to 50000 bytes without NUL"
      }
    ],
    "summary": "Set the release planner title",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/release_edit_native.cpp"
    ],
    "nativ)MT2gameplay"
+ R"MT2gameplay(e_result": "table"
  },
  {
    "name": "management.set_release_announcement",
    "kind": "action",
    "parameters": [
      {
        "name": "text",
        "type": "string",
        "description": "Native text, up to 50000 bytes without NUL"
      }
    ],
    "summary": "Set the release announcement; empty text restores its native default",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/release_edit_native.cpp"
    ],
    "native_result": "table"
  },
  {
    "name": "management.set_release_changelog",
    "kind": "action",
    "parameters": [
      {
        "name": "text",
        "type": "string",
        "description": "Native text, up to 50000 bytes without NUL"
      }
    ],
    "summary": "Set and enable a custom changelog; empty text restores its native default",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/release_edit_native.cpp"
    ],
    "native_result": "table"
  },
  {
    "name": "management.reset_release_text",
    "kind": "action",
    "parameters": [],
    "summary": "Restore native default announcement and changelog and disable custom text",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/release_edit_native.cpp"
    ],
    "native_result": "table"
  },
  {
    "name": "management.enable_release_custom",
    "kind": "action",
    "parameters": [],
    "summary": "Enable the current native custom release text",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/release_edit_native.cpp"
    ],
    "native_result": "table"
  },
  {
    "name": "management.show_release",
    "kind": "action",
    "parameters": [],
    "summary": "Check native release eligibility and show its announcement; launch releases publish immediately",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/release_cycle_runtime.inc"
    ],
    "native_result": "table"
  },
  {
    "name": "management.begin_release",
    "kind": "action",
    "parameters": [],
    "summary": "Begin the current release celebration; hotfixes publish immediately",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/release_cycle_runtime.inc"
    ],
    "native_result": "table"
  },
  {
    "name": "management.abort_release",
    "kind": "action",
    "parameters": [],
    "summary": "Abort the current release celebration",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/release_cycle_runtime.inc"
    ],
    "native_result": "table"
  },
  {
    "name": "management.skip_release",
    "kind": "action",
    "parameters": [],
    "summary": "Skip the current release celebration and publish through the native path",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/release_cycle_runtime.inc"
    ],
    "native_result": "table"
  },
  {
    "name": "management.publish_release",
    "kind": "action",
    "parameters": [],
    "summary": "Publish the current announced release by skipping its active celebration",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/release_cycle_runtime.inc"
    ],
    "native_result": "table"
  },
  {
    "name": "management.set_service_price",
    "kind": "action",
    "parameters": [
      {
        "name": "building",
        "type": "MT2Handle",
        "description": "Current building handle returned by the world object API"
      },
      {
        "name": "ordinal",
        "type": "integer",
        "description": "Zero-based service ordinal from management.services"
      },
      {
        "name": "currency",
        "type": "integer",
        "description": "0 or 1 in the native Price currency enum"
      },
      {
        "name": "value",
        "type": "integer",
        "description": "Nonnegat)MT2gameplay"
+ R"MT2gameplay(ive int32 native value"
      }
    ],
    "summary": "Set the exact owned service price and update its shared configuration if synchronized",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/management_services.inc"
    ],
    "native_result": "table"
  },
  {
    "name": "management.set_service_sync",
    "kind": "action",
    "parameters": [
      {
        "name": "building",
        "type": "MT2Handle",
        "description": "Current building handle returned by the world object API"
      },
      {
        "name": "ordinal",
        "type": "integer",
        "description": "Zero-based service ordinal from management.services"
      },
      {
        "name": "value",
        "type": "boolean",
        "description": "Requested native Boolean value"
      }
    ],
    "summary": "Set service price synchronization when allowed by its prototype",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/management_services.inc"
    ],
    "native_result": "table"
  },
  {
    "name": "management.start_campaign",
    "kind": "action",
    "parameters": [
      {
        "name": "type",
        "type": "integer",
        "description": "Native campaign type, 0 through 8"
      }
    ],
    "summary": "Pay for and start a seven-day native advertising campaign",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/marketing_authority/native_port.cpp"
    ],
    "native_result": "table"
  },
  {
    "name": "management.abort_campaign",
    "kind": "action",
    "parameters": [
      {
        "name": "type",
        "type": "integer",
        "description": "Native campaign type, 0 through 8"
      }
    ],
    "summary": "Abort the current native advertising campaign",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/marketing_authority/native_port.cpp"
    ],
    "native_result": "table"
  },
  {
    "name": "management.set_buzz",
    "kind": "action",
    "parameters": [
      {
        "name": "positive",
        "type": "integer",
        "description": "Signed int32 positive-buzz accumulator"
      },
      {
        "name": "negative",
        "type": "integer",
        "description": "Signed int32 negative-buzz accumulator"
      }
    ],
    "summary": "Set both native marketing buzz accumulators with guarded stores",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/marketing_authority/native_port.cpp"
    ],
    "native_result": "table"
  },
  {
    "name": "management.purge_expired_awards",
    "kind": "action",
    "parameters": [],
    "summary": "Purge native awards older than the current six-day retention interval",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/marketing_authority/native_port.cpp"
    ],
    "native_result": "table"
  },
  {
    "name": "management.add_award",
    "kind": "action",
    "parameters": [
      {
        "name": "name",
        "type": "string",
        "description": "Native LocString base text, up to 256 bytes, without arguments"
      },
      {
        "name": "source",
        "type": "string",
        "description": "Native LocString source base text, up to 256 bytes, without arguments"
      },
      {
        "name": "impressions",
        "type": "integer",
        "description": "Nonnegative int32 impression count"
      },
      {
        "name": "conversions",
        "type": "integer",
        "description": "Nonnegative int32 conversion count"
      },
      {
        "name": "good",
        "type": "boolean",
        "description": "Whether the native award is positive"
      }
    ],
    "summary": "Construct and add a native marketing award for the current game day",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/marketing_authority/native_port)MT2gameplay"
+ R"MT2gameplay(.cpp"
    ],
    "native_result": "table"
  },
  {
    "name": "modular.get_layout",
    "kind": "query",
    "parameters": [
      {
        "name": "building",
        "type": "MT2Handle",
        "description": "Exact current-epoch modular Building handle."
      }
    ],
    "summary": "Capture the complete native Layout and Plan records for an existing modular Scenery/base Building.",
    "returns": "MT2ModularLayout",
    "native_evidence": [
      "src/game/modular_calls.h",
      "src/game/modular_transaction.h",
      "src/game/modular_layout_graph.h",
      "src/game/modular_identity.h",
      "src/game/modular_code.h",
      "src/game/subscriber_lifecycle/record_graph.cpp",
      "src/game/building_history_slice/native_bridge.cpp"
    ]
  },
  {
    "name": "modular.history",
    "kind": "query",
    "parameters": [],
    "summary": "List retained modular edit receipts and exact Undo/Redo top readiness.",
    "returns": "MT2ModularHistory[]",
    "native_evidence": [
      "src/game/modular_calls.h",
      "src/game/modular_transaction.h",
      "src/game/modular_layout_graph.h",
      "src/game/modular_identity.h",
      "src/game/modular_code.h",
      "src/game/subscriber_lifecycle/record_graph.cpp",
      "src/game/building_history_slice/native_bridge.cpp"
    ]
  },
  {
    "name": "modular.add_block",
    "kind": "action",
    "parameters": [
      {
        "name": "building",
        "type": "MT2Handle",
        "description": "Existing modular Building."
      },
      {
        "name": "position",
        "type": "number[]",
        "description": "Three finite world coordinates; add_block applies the original native position adjustment."
      }
    ],
    "summary": "Add one native modular block at the adjusted world position. Commit a complete original Layout/Plan/pose inverse to native Undo.",
    "returns": "MT2Action",
    "native_evidence": [
      "src/game/modular_calls.h",
      "src/game/modular_transaction.h",
      "src/game/modular_layout_graph.h",
      "src/game/modular_identity.h",
      "src/game/modular_code.h",
      "src/game/subscriber_lifecycle/record_graph.cpp",
      "src/game/building_history_slice/native_bridge.cpp"
    ],
    "native_result": "MT2ModularEditResult"
  },
  {
    "name": "modular.remove_block",
    "kind": "action",
    "parameters": [
      {
        "name": "building",
        "type": "MT2Handle",
        "description": "Existing modular Building."
      },
      {
        "name": "position",
        "type": "number[]",
        "description": "Three finite world coordinates; add_block applies the original native position adjustment."
      }
    ],
    "summary": "Remove a native modular block at the world position. Commit a complete original Layout/Plan/pose inverse to native Undo.",
    "returns": "MT2Action",
    "native_evidence": [
      "src/game/modular_calls.h",
      "src/game/modular_transaction.h",
      "src/game/modular_layout_graph.h",
      "src/game/modular_identity.h",
      "src/game/modular_code.h",
      "src/game/subscriber_lifecycle/record_graph.cpp",
      "src/game/building_history_slice/native_bridge.cpp"
    ],
    "native_result": "MT2ModularEditResult"
  },
  {
    "name": "modular.add_entrance",
    "kind": "action",
    "parameters": [
      {
        "name": "building",
        "type": "MT2Handle",
        "description": "Existing modular Building."
      },
      {
        "name": "position",
        "type": "number[]",
        "description": "Three finite world coordinates; add_block applies the original native position adjustment."
      }
    ],
    "summary": "Add a native modular entrance at the world position. Commit a complete original Layout/Plan/pose inverse to native Undo.",
    "returns": "MT2Action",
    "native_evidence": [
      "src/game/modular_calls.h",
      "src/game/modular_transaction.h",
      "src/game/modular_layout_graph.h",
      "src/game/modular_identity.h",
      "src/game/modular_code.h",
      "src/g)MT2gameplay"
+ R"MT2gameplay(ame/subscriber_lifecycle/record_graph.cpp",
      "src/game/building_history_slice/native_bridge.cpp"
    ],
    "native_result": "MT2ModularEditResult"
  },
  {
    "name": "modular.remove_entrance",
    "kind": "action",
    "parameters": [
      {
        "name": "building",
        "type": "MT2Handle",
        "description": "Existing modular Building."
      },
      {
        "name": "position",
        "type": "number[]",
        "description": "Three finite world coordinates; add_block applies the original native position adjustment."
      }
    ],
    "summary": "Remove a native modular entrance at the world position. Commit a complete original Layout/Plan/pose inverse to native Undo.",
    "returns": "MT2Action",
    "native_evidence": [
      "src/game/modular_calls.h",
      "src/game/modular_transaction.h",
      "src/game/modular_layout_graph.h",
      "src/game/modular_identity.h",
      "src/game/modular_code.h",
      "src/game/subscriber_lifecycle/record_graph.cpp",
      "src/game/building_history_slice/native_bridge.cpp"
    ],
    "native_result": "MT2ModularEditResult"
  },
  {
    "name": "modular.undo",
    "kind": "action",
    "parameters": [
      {
        "name": "history",
        "type": "string",
        "description": "Opaque receipt from a confirmed modular edit or prefab placement."
      }
    ],
    "summary": "Undo one confirmed modular edit or prefab placement when its exact graph is the current native stack top. Prefab history includes cleared Scenery and reciprocal Service/Quest closure.",
    "returns": "MT2Action",
    "native_evidence": [
      "src/game/modular_calls.h",
      "src/game/modular_transaction.h",
      "src/game/modular_layout_graph.h",
      "src/game/modular_identity.h",
      "src/game/modular_code.h",
      "src/game/subscriber_lifecycle/record_graph.cpp",
      "src/game/building_history_slice/native_bridge.cpp"
    ],
    "native_result": "MT2ModularHistoryResult"
  },
  {
    "name": "modular.redo",
    "kind": "action",
    "parameters": [
      {
        "name": "history",
        "type": "string",
        "description": "Opaque receipt from a confirmed modular edit or prefab placement."
      }
    ],
    "summary": "Redo one confirmed modular edit or prefab placement when its exact graph is the current native stack top. Prefab history includes cleared Scenery and reciprocal Service/Quest closure.",
    "returns": "MT2Action",
    "native_evidence": [
      "src/game/modular_calls.h",
      "src/game/modular_transaction.h",
      "src/game/modular_layout_graph.h",
      "src/game/modular_identity.h",
      "src/game/modular_code.h",
      "src/game/subscriber_lifecycle/record_graph.cpp",
      "src/game/building_history_slice/native_bridge.cpp"
    ],
    "native_result": "MT2ModularHistoryResult"
  },
  {
    "name": "modular.place_prefab",
    "kind": "action",
    "parameters": [
      {
        "name": "layout",
        "type": "table",
        "description": "Complete mmoBuildingLayout native record rows, such as get_layout().layout. Grid integer cells are authored directly; Load rebuilds bounds and resets origin Y."
      },
      {
        "name": "plan",
        "type": "table",
        "description": "Complete mmoBuildingPlan native record rows, such as get_layout().plan."
      },
      {
        "name": "theme",
        "type": "string",
        "description": "Canonical native Theme name."
      },
      {
        "name": "pose",
        "type": "number[]",
        "description": "Quaternion XYZW, world position XYZ and positive scale XYZ (10 finite numbers). Native terrain height adjusts world Y."
      },
      {
        "name": "clearance",
        "type": "number[]",
        "description": "Local clearance minimum XYZ and maximum XYZ (6 finite numbers). The native transaction clears intersecting Scenery and retains every inverse."
      }
    ],
    "summary": "Load authored Layout/Plan records, purchase and place a modular Scenery/base Build)MT2gameplay"
+ R"MT2gameplay(ing through the complete native transaction, with receipt-based Undo/Redo.",
    "returns": "MT2Action",
    "native_evidence": [
      "src/game/modular_calls.h",
      "src/game/modular_transaction.h",
      "src/game/modular_layout_graph.h",
      "src/game/modular_identity.h",
      "src/game/modular_code.h",
      "src/game/subscriber_lifecycle/record_graph.cpp",
      "src/game/building_history_slice/native_bridge.cpp",
      "src/game/modular_port.inc",
      "src/game/modular_graph.inc",
      "src/game/modular_dependencies.inc",
      "src/game/duplicate_service_history.h",
      "src/game/duplicate_quest_history.h"
    ],
    "native_result": "MT2ModularPrefabResult"
  },
  {
    "name": "walls.place_curve",
    "kind": "action",
    "parameters": [
      {
        "name": "curve",
        "type": "MT2WallCurve",
        "description": "Native path/node type, resource name and complete ordered control-point positions including height. No caller-authored spline or sampled preview is required."
      }
    ],
    "summary": "Compute and place a native Wall curve from 2..11 ordered control points. Original AddPointAt calculates segment velocities and PlotSpline samples and validates terrain/collisions, supplying the native preview and spline-length price quote. The same isolated native receiver owns Purchase/Finish and model/ghost cleanup. Returns native spline records and sampled preview, created Path/node handles, changed and removed split Paths, cost and an owned history receipt. Requires one owned active-root Region; supplied endpoint handles pin native intersections/split Paths. Original validation causes are retained and uncertain actions must not be replayed.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/wall_pass_cursor.h",
      "Multiplayer/src/game/world_path_wall_native.inc",
      "Multiplayer/src/game/world_path_capture.inc",
      "Multiplayer/src/game/wall_private_lifecycle.h"
    ],
    "native_result": "MT2WallCurveResult"
  },
  {
    "name": "paths.place_route",
    "kind": "action",
    "parameters": [
      {
        "name": "region",
        "type": "integer",
        "description": "Owned native Region ID."
      },
      {
        "name": "start",
        "type": "number[]",
        "description": "Three finite start coordinates; native ground projection and auto snapping apply."
      },
      {
        "name": "end",
        "type": "number[]",
        "description": "Three finite end coordinates; native ground projection and auto snapping apply."
      },
      {
        "name": "start_node",
        "type": "MT2Handle",
        "description": "Optional exact current Intersection that native start hover must choose.",
        "optional": true
      },
      {
        "name": "end_node",
        "type": "MT2Handle",
        "description": "Optional exact current Intersection that native end hover must choose.",
        "optional": true
      },
      {
        "name": "start_split",
        "type": "MT2Handle",
        "description": "Optional exact current Path that native start hover must split.",
        "optional": true
      },
      {
        "name": "end_split",
        "type": "MT2Handle",
        "description": "Optional exact current Path that native end hover must split.",
        "optional": true
      }
    ],
    "summary": "Execute the whole original native Road hover/pin/Press transaction, accepting native auto snapping, split targets and up to ten native preview splines. Returns all created Paths/nodes, changed Path before/after values, removed Paths, cost, native spline count and owned history receipt. Native geometry and mixed topology readback own the result; one final Press follows only an acknowledged no-effect pin phase.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/ui_path_runtime.inc",
      "Multiplayer/src/game/path_reader.h",
      "Multiplayer/src/game/path_execution.inc",
      "Multiplayer/)MT2gameplay"
+ R"MT2gameplay(src/game/mixed_path_reader.h",
      "Multiplayer/src/game/world_path_history.inc"
    ],
    "native_result": "MT2PathRouteResult"
  },
  {
    "name": "paths.graph",
    "kind": "query",
    "parameters": [],
    "summary": "Read the complete saved root-world mixed Path graph, including all eleven Path types, six Intersection types, reciprocal node edges and cross-Region membership. Fields contain native handles, Region IDs, names, waypoints and native distances.",
    "returns": "MT2PathGraph",
    "native_evidence": [
      "Multiplayer/src/game/mixed_path_reader.h",
      "Multiplayer/src/game/mixed_path_graph.h"
    ]
  },
  {
    "name": "paths.place",
    "kind": "action",
    "parameters": [
      {
        "name": "region",
        "type": "integer",
        "description": "Owned native Region ID."
      },
      {
        "name": "start",
        "type": "number[]",
        "description": "Three finite coordinates; native surface projection supplies y."
      },
      {
        "name": "end",
        "type": "number[]",
        "description": "Three finite coordinates; native surface projection supplies y."
      },
      {
        "name": "start_node",
        "type": "MT2Handle",
        "description": "Current RoadNode in the selected Region; position comes from this node.",
        "optional": true
      },
      {
        "name": "end_node",
        "type": "MT2Handle",
        "description": "Distinct current RoadNode in the selected Region; position comes from this node.",
        "optional": true
      }
    ],
    "summary": "Create one native Road segment through an isolated original Road cursor hover/press, native purchase and Undo. Native ground projection sets endpoint height; optional native node handles pin connections. Splitting existing Paths and multi-spline Road previews are rejected. Returns the created Path, cost and owned history receipt; normal native costs and tool effects apply.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/ui_path_runtime.inc",
      "Multiplayer/src/game/path_execution.inc",
      "Multiplayer/src/game/path_reader.h"
    ],
    "native_result": "MT2Path"
  },
  {
    "name": "paths.link",
    "kind": "action",
    "parameters": [
      {
        "name": "start_region",
        "type": "integer",
        "description": "Owned native start Region ID."
      },
      {
        "name": "end_region",
        "type": "integer",
        "description": "Distinct saved native end Region ID."
      },
      {
        "name": "name",
        "type": "string",
        "description": "Existing native Pass resource name, 1..256 bytes without NUL."
      },
      {
        "name": "start",
        "type": "number[]",
        "description": "Three finite coordinates within the start Region."
      },
      {
        "name": "end",
        "type": "number[]",
        "description": "Three finite coordinates within the end Region."
      },
      {
        "name": "start_node",
        "type": "MT2Handle",
        "description": "Current Intersection in the start Region.",
        "optional": true
      },
      {
        "name": "end_node",
        "type": "MT2Handle",
        "description": "Current Intersection in the end Region.",
        "optional": true
      }
    ],
    "summary": "Create a native cross-Region Pass through an isolated original Pass receiver and Finish transaction. Confirms both Regions, supplied two-point shape, endpoint identities, remote membership and native Undo. Returns the Pass and owned history receipt; normal native tool effects apply.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/world_path_replay.inc",
      "Multiplayer/src/game/wall_pass_cursor.h",
      "Multiplayer/src/game/mixed_path_reader.h"
    ],
    "native_result": "MT2Path"
  },
  {
    "name": "walls.place",
    "kind": "action",
    "parameters": [
      {
        "name": "plan",
        "type": "MT2WallPlan",
        ")MT2gameplay"
+ R"MT2gameplay(description": "Native spline and preview plan in one owned active root Region. This low-level interface accepts the proven native 12-float spline records directly."
      }
    ],
    "summary": "Execute a bounded explicit native Wall plan through isolated original Wall/Path/model construction, native Purchase and Finish, then owned cleanup. Native ghost Length supplies the price quote. Supports explicit current node and split-Path handles, including preview split targets. Confirms reciprocal mixed topology, created Path family/scene, retained original Undo and unchanged unrelated Paths. Returns created Paths, native length, cost and owned history receipt.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/world_path_wall_native.inc",
      "Multiplayer/src/game/wall_private_lifecycle.h",
      "Multiplayer/src/game/wall_private_codes.h",
      "Multiplayer/src/game/wall_pass_cursor.h"
    ],
    "native_result": "MT2WallResult"
  },
  {
    "name": "paths.remove",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2Handle",
        "description": "Current native Path handle from paths.graph."
      }
    ],
    "summary": "Remove any current mixed Path, including Wall or Pass, through original native Destroy command and edit transaction. Confirms Path removal, native orphan policy for endpoint types0/1, retained other Intersections, remote/selection retirement and exact Make/Cash inverse UID set. Returns refund, removed node count and owned history receipt.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/world_path_remove.inc",
      "Multiplayer/src/game/mixed_path_remove.h",
      "Multiplayer/src/game/path_delete_execution.inc"
    ],
    "native_result": "MT2PathRemoval"
  },
  {
    "name": "paths.undo",
    "kind": "action",
    "parameters": [
      {
        "name": "receipt",
        "type": "string",
        "description": "Opaque receipt returned by a confirmed Path or Wall placement/removal action."
      }
    ],
    "summary": "Run original native undo for one owned Path/Wall receipt at the exact native stack top. Confirms the complete saved mixed topology and exact source/inverse stack transition. Up to twenty receipts are retained in the current World lifetime; an unresolved native history attempt quarantines its receipt.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/world_path_history.inc",
      "Multiplayer/src/game/path_history_runtime.inc",
      "Multiplayer/src/game/building_history_slice/native_bridge.cpp",
      "Multiplayer/src/game/building_history_slice/stack_transition.h"
    ],
    "native_result": "MT2PathHistoryResult"
  },
  {
    "name": "paths.redo",
    "kind": "action",
    "parameters": [
      {
        "name": "receipt",
        "type": "string",
        "description": "Opaque receipt returned by a confirmed Path or Wall placement/removal action."
      }
    ],
    "summary": "Run original native redo for one owned Path/Wall receipt at the exact native stack top. Confirms the complete saved mixed topology and exact source/inverse stack transition. Up to twenty receipts are retained in the current World lifetime; an unresolved native history attempt quarantines its receipt.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/world_path_history.inc",
      "Multiplayer/src/game/path_history_runtime.inc",
      "Multiplayer/src/game/building_history_slice/native_bridge.cpp",
      "Multiplayer/src/game/building_history_slice/stack_transition.h"
    ],
    "native_result": "MT2PathHistoryResult"
  },
  {
    "name": "regions.set_type",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2RegionHandle",
        "description": "Exact Region handle from this world epoch."
      },
      {
        "name": "type",
        "type": "integer",
        "description": "Original nat)MT2gameplay"
+ R"MT2gameplay(ive type: 0 lowers to Ocean; 1 raises to land."
      },
      {
        "name": "position",
        "type": "number[]",
        "description": "Three world coordinates whose original native hit resolves to this Region."
      }
    ],
    "summary": "Apply the original private Lift/Land transaction to an unowned Region, clear its native contents and retain the original Undo graph.",
    "returns": "MT2Action",
    "native_evidence": [
      "multiplayer-source/src/game/region_properties_runtime.inc",
      "multiplayer-source/src/game/mixed_path_reader.h",
      "multiplayer-source/src/game/connected_network_reader.h",
      "adapters/mt2/gameplay/region_type/native_evidence.json",
      "adapters/mt2/gameplay/region_type/provenance.md"
    ],
    "native_result": "MT2RegionTypeResult"
  },
  {
    "name": "regions.list",
    "kind": "query",
    "parameters": [
      {
        "name": "offset",
        "type": "integer",
        "description": "Zero-based page offset.",
        "optional": true,
        "default": 0
      },
      {
        "name": "limit",
        "type": "integer",
        "description": "Maximum returned handles, 1..128.",
        "optional": true,
        "default": 100
      }
    ],
    "summary": "List saved root-world Region handles using bounded pages.",
    "returns": "MT2RegionPage",
    "native_evidence": [
      "Multiplayer/src/game/region_saved_identity.h",
      "Multiplayer/src/game/region_properties_runtime.inc",
      "Multiplayer/src/game/region_appearance_runtime.inc"
    ]
  },
  {
    "name": "regions.get",
    "kind": "query",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2RegionHandle",
        "description": "Current world epoch and saved Region ID."
      }
    ],
    "summary": "Read a Region name, type, levels, styles and complete environment settings.",
    "returns": "MT2RegionDetails",
    "native_evidence": [
      "Multiplayer/src/game/region_saved_identity.h",
      "Multiplayer/src/game/region_properties_runtime.inc",
      "Multiplayer/src/game/region_appearance_runtime.inc"
    ]
  },
  {
    "name": "regions.presets",
    "kind": "query",
    "parameters": [],
    "summary": "Read the native Region preset names.",
    "returns": "string[]",
    "native_evidence": [
      "Multiplayer/src/game/region_saved_identity.h",
      "Multiplayer/src/game/region_properties_runtime.inc",
      "Multiplayer/src/game/region_appearance_runtime.inc"
    ]
  },
  {
    "name": "regions.rename",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2RegionHandle",
        "description": "Current Region identity."
      },
      {
        "name": "name",
        "type": "string",
        "description": "Region name, 1..256 UTF-8 bytes."
      }
    ],
    "summary": "Rename a Region through its native widget notification.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/region_saved_identity.h",
      "Multiplayer/src/game/region_properties_runtime.inc",
      "Multiplayer/src/game/region_appearance_runtime.inc"
    ],
    "native_result": "MT2RegionDetails"
  },
  {
    "name": "regions.set_tint",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2RegionHandle",
        "description": "Current Region identity."
      },
      {
        "name": "values",
        "type": "number[]",
        "description": "Four finite native color channels."
      }
    ],
    "summary": "Set the native Region tint.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/region_saved_identity.h",
      "Multiplayer/src/game/region_properties_runtime.inc",
      "Multiplayer/src/game/region_appearance_runtime.inc"
    ],
    "native_result": "MT2RegionDetails"
  },
  {
    "name": "regions.set_light",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2RegionHandle",
        "description": "C)MT2gameplay"
+ R"MT2gameplay(urrent Region identity."
      },
      {
        "name": "values",
        "type": "number[]",
        "description": "Four finite native color channels."
      }
    ],
    "summary": "Set native Region light.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/region_saved_identity.h",
      "Multiplayer/src/game/region_properties_runtime.inc",
      "Multiplayer/src/game/region_appearance_runtime.inc"
    ],
    "native_result": "MT2RegionDetails"
  },
  {
    "name": "regions.set_ambient",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2RegionHandle",
        "description": "Current Region identity."
      },
      {
        "name": "values",
        "type": "number[]",
        "description": "Four finite native color channels."
      }
    ],
    "summary": "Set native Region ambient light.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/region_saved_identity.h",
      "Multiplayer/src/game/region_properties_runtime.inc",
      "Multiplayer/src/game/region_appearance_runtime.inc"
    ],
    "native_result": "MT2RegionDetails"
  },
  {
    "name": "regions.set_sky",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2RegionHandle",
        "description": "Current Region identity."
      },
      {
        "name": "values",
        "type": "number[]",
        "description": "Sixteen finite native spline coefficients, as returned by get()."
      }
    ],
    "summary": "Set native Region sky spline coefficients.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/region_saved_identity.h",
      "Multiplayer/src/game/region_properties_runtime.inc",
      "Multiplayer/src/game/region_appearance_runtime.inc"
    ],
    "native_result": "MT2RegionDetails"
  },
  {
    "name": "regions.set_particles",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2RegionHandle",
        "description": "Current Region identity."
      },
      {
        "name": "type",
        "type": "integer",
        "description": "Native particle kind 0..6; zero disables particles."
      }
    ],
    "summary": "Set the original Region particle type and enabled state.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/region_saved_identity.h",
      "Multiplayer/src/game/region_properties_runtime.inc",
      "Multiplayer/src/game/region_appearance_runtime.inc"
    ],
    "native_result": "MT2RegionDetails"
  },
  {
    "name": "regions.set_particle_color",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2RegionHandle",
        "description": "Current Region identity."
      },
      {
        "name": "values",
        "type": "number[]",
        "description": "Four finite native color channels."
      }
    ],
    "summary": "Set the original Region particle color.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/region_saved_identity.h",
      "Multiplayer/src/game/region_properties_runtime.inc",
      "Multiplayer/src/game/region_appearance_runtime.inc"
    ],
    "native_result": "MT2RegionDetails"
  },
  {
    "name": "regions.styles",
    "kind": "query",
    "parameters": [],
    "summary": "Read the nine canonical terrain style names.",
    "returns": "string[]",
    "native_evidence": [
      "Multiplayer/src/game/region_saved_identity.h",
      "Multiplayer/src/game/region_properties_runtime.inc",
      "Multiplayer/src/game/region_appearance_runtime.inc"
    ]
  },
  {
    "name": "regions.set_styles",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2RegionHandle",
        "description": "Current Region identity."
      },
      {
        "name": "styles",
        "type": "string[]",
        "description": "Four exact names returned by styles()."
      }
)MT2gameplay"
+ R"MT2gameplay(    ],
    "summary": "Apply all four terrain styles through the original native EditManager transaction.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/region_saved_identity.h",
      "Multiplayer/src/game/region_properties_runtime.inc",
      "Multiplayer/src/game/region_appearance_runtime.inc"
    ],
    "native_result": "MT2RegionDetails"
  },
  {
    "name": "terrain.heights",
    "kind": "query",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2RegionHandle",
        "description": "Current owned Region identity."
      },
      {
        "name": "offset",
        "type": "integer",
        "description": "First cell index.",
        "optional": true,
        "default": 0
      },
      {
        "name": "limit",
        "type": "integer",
        "description": "Number of returned heights, 1..4096.",
        "optional": true,
        "default": 256
      }
    ],
    "summary": "Read a bounded page of the original Region height grid.",
    "returns": "MT2TerrainHeightPage",
    "native_evidence": [
      "Multiplayer/src/game/terrain_height_grid.h"
    ]
  },
  {
    "name": "terrain.change_height",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2RegionHandle",
        "description": "Current owned Region identity."
      },
      {
        "name": "position",
        "type": "number[]",
        "description": "Finite world position inside the supplied Region."
      },
      {
        "name": "up",
        "type": "boolean",
        "description": "True raises the selected cell by 20; false lowers it by 20."
      },
      {
        "name": "radius",
        "type": "number",
        "description": "Native brush radius, greater than zero and at most 10000.",
        "optional": true,
        "default": 1
      }
    ],
    "summary": "Use the original height brush once and confirm the complete grid, 100 cash payment, restored cursor and unchanged Undo history.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/ui_terrain_height.inc",
      "Multiplayer/src/game/terrain_height_grid.h"
    ],
    "native_result": "MT2TerrainHeightResult"
  },
  {
    "name": "regions.set_levels",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2RegionHandle",
        "description": "Current owned Region identity."
      },
      {
        "name": "minimum",
        "type": "integer",
        "description": "Minimum level within the native widget domain."
      },
      {
        "name": "maximum",
        "type": "integer",
        "description": "Maximum level within the native widget domain."
      },
      {
        "name": "position",
        "type": "number[]",
        "description": "Finite XYZ point inside the Region; defaults to its center.",
        "optional": true
      }
    ],
    "summary": "Set the Region level range with the original tool transaction, complete actor level clamp, restored input and native Undo inverse.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/region_level_runtime.inc",
      "Multiplayer/src/game/region_level_reader.h"
    ],
    "native_result": "MT2RegionDetails"
  },
  {
    "name": "regions.set_preset",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2RegionHandle",
        "description": "Current Region identity."
      },
      {
        "name": "name",
        "type": "string",
        "description": "Exact native preset name returned by presets()."
      }
    ],
    "summary": "Apply an original Region preset, confirm its styles and refresh the visible native inspector.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/ui_region_runtime.cpp"
    ],
    "native_result": "MT2RegionDetails"
  },
  {
    "name": "regions.purchase",
    "kind": "action",
    "parameters": [
      {
        "name": )MT2gameplay"
+ R"MT2gameplay("handle",
        "type": "MT2RegionHandle",
        "description": "Current Region identity."
      },
      {
        "name": "position",
        "type": "number[]",
        "description": "Finite XYZ point inside the Region; defaults to its center.",
        "optional": true
      }
    ],
    "summary": "Buy unowned land through the original native tool, with complete actor level clamp, payment and inverse readback.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/region_properties_runtime.inc",
      "Multiplayer/src/game/region_ownership_native.inc",
      "Multiplayer/src/game/region_sale_runtime.inc"
    ],
    "native_result": "MT2RegionDetails"
  },
  {
    "name": "regions.sell",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2RegionHandle",
        "description": "Current Region identity."
      },
      {
        "name": "position",
        "type": "number[]",
        "description": "Finite XYZ point inside the Region; defaults to its center.",
        "optional": true
      }
    ],
    "summary": "Sell owned land through the original native tool, with complete actor level clamp, refund and inverse readback.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/region_properties_runtime.inc",
      "Multiplayer/src/game/region_ownership_native.inc",
      "Multiplayer/src/game/region_sale_runtime.inc"
    ],
    "native_result": "MT2RegionDetails"
  },
  {
    "name": "terrain.contours",
    "kind": "query",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2RegionHandle",
        "description": "Current Region identity."
      },
      {
        "name": "style",
        "type": "integer",
        "description": "Native terrain style index, 0..3."
      },
      {
        "name": "path",
        "type": "integer",
        "description": "Zero-based polygon path index.",
        "optional": true,
        "default": 0
      },
      {
        "name": "offset",
        "type": "integer",
        "description": "First vertex in that path.",
        "optional": true,
        "default": 0
      },
      {
        "name": "limit",
        "type": "integer",
        "description": "Maximum returned vertices, 1..4096.",
        "optional": true,
        "default": 256
      }
    ],
    "summary": "Read a bounded page of exact native integer terrain polygon coordinates.",
    "returns": "MT2TerrainContourPage",
    "native_evidence": [
      "Multiplayer/src/game/native_polyshape.h",
      "Multiplayer/src/game/ui_terrain_runtime.inc"
    ]
  },
  {
    "name": "terrain.paint",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2RegionHandle",
        "description": "Current Region identity."
      },
      {
        "name": "style",
        "type": "integer",
        "description": "Native terrain style index, 0..3."
      },
      {
        "name": "stamps",
        "type": "MT2TerrainStamp[]",
        "description": "1..64 native paint stamps; shapes use the exact integer coordinate system returned by contours()."
      },
      {
        "name": "label",
        "type": "string",
        "description": "Native edit description, 1..256 UTF-8 bytes.",
        "optional": true,
        "default": "Paint terrain"
      }
    ],
    "summary": "Complete an original terrain paint stroke and confirm its contour inverse, native Undo, Region and ledger readback. The result states whether contours changed.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/ui_terrain_runtime.inc",
      "Multiplayer/src/game/terrain_history_runtime.inc"
    ],
    "native_result": "MT2TerrainPaintResult"
  },
  {
    "name": "scenery.stroke",
    "kind": "action",
    "parameters": [
      {
        "name": "kind",
        "type": "string",
        "description": "paint, category_erase or erase. category_erase uses native brush mode 1; er)MT2gameplay"
+ R"MT2gameplay(ase uses the general eraser."
      },
      {
        "name": "region",
        "type": "integer",
        "description": "Owned Region ID in the actual native active root world."
      },
      {
        "name": "finish_position",
        "type": "number[]",
        "description": "Three finite coordinates for the original whole-stroke Finish and paid cash inverse position."
      },
      {
        "name": "stamps",
        "type": "MT2SceneryStrokeStamp[]",
        "description": "One to 4096 ordered complete canonical PolyShapes in native integer clipping units. Painting supplies each stamp's complete generated dot poses. Total points and total dots are each bounded to one million."
      },
      {
        "name": "type",
        "type": "string",
        "description": "Native catalog directory. Required for paint and category_erase; omitted for erase.",
        "optional": true
      },
      {
        "name": "variant",
        "type": "string",
        "description": "Exact native catalog filename, required unless random_variant is true; omitted for erase.",
        "optional": true
      },
      {
        "name": "random_variant",
        "type": "boolean",
        "description": "Use the native random variant for the chosen directory. Requires an empty variant.",
        "optional": true,
        "default": false
      },
      {
        "name": "replace",
        "type": "boolean",
        "description": "Native brush replacement setting. General erase requires false.",
        "optional": true,
        "default": false
      },
      {
        "name": "bounds",
        "type": "number[]",
        "description": "Six finite native brush bounds values. Omitted uses six zeros. General erase requires six zeros.",
        "optional": true
      }
    ],
    "summary": "Run complete ordered native SceneryBrush or SceneryEraser AddPaint stamps and one original Finish through isolated private receivers. Returns created/removed Scenery rows, cumulative birth count, paid/refund amounts and before/after Building/Monster bug-anchor records. Confirms complete born/removed full-UID Make/Destroy/cash inverse coverage, saved Make records, unchanged surviving Scenery, Group member removals and native Release edits. Preserves normal native cost, replacement, variant RNG and brush bug effects; confirms general erase leaves RNG/bugs unchanged. Temporarily journals the original Cursor finish position and restores it plus surviving selection. Requires the root world to be the actual native active scene. Partial native owners are retained on uncertain failures; never replay such a result.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/scenery_stroke_calls.h",
      "Multiplayer/src/game/scenery_stroke_port.inc",
      "Multiplayer/src/game/scenery_stroke_native.h",
      "Multiplayer/src/game/scenery_stroke_graph.h",
      "Multiplayer/src/game/scenery_make_history.h",
      "Multiplayer/src/game/scenery_stroke_bugs.h",
      "Multiplayer/src/game/scenery_stroke_transition.h"
    ],
    "native_result": "MT2SceneryStrokeResult"
  },
  {
    "name": "scenery.transform",
    "kind": "action",
    "parameters": [
      {
        "name": "targets",
        "type": "MT2SceneryTransform[]",
        "description": "One to 4096 unique free Scenery handles with finite destination position and optional quaternion/positive scale. Group members must be transformed through their whole Group."
      }
    ],
    "summary": "Move, rotate or scale multiple free Scenery in one original native Begin/Open/DoMove-per-target/Close/End transaction. Returns target results in input order. Confirms exact inverse UID/prior-pose order, destination Region, unchanged other Scenery and unchanged Groups. Requires the root world to be the actual active scene and owned destination Regions; normal native tool effects apply.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/selection_move_native.h",
 )MT2gameplay"
+ R"MT2gameplay(     "Multiplayer/src/game/selection_move_runtime.inc",
      "Multiplayer/src/game/selection_move_reader.h",
      "Multiplayer/src/game/scenery_kiss/lifecycle_native.cpp"
    ],
    "native_result": "MT2SceneryProp[]"
  },
  {
    "name": "scenery.duplicate",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2Handle",
        "description": "Current free Scenery or whole Group handle in the root world."
      },
      {
        "name": "position",
        "type": "number[]",
        "description": "Three finite destination coordinates within the current root world."
      },
      {
        "name": "rotation",
        "type": "number[]",
        "description": "Normalized quaternion x/y/z/w; omitted preserves source rotation.",
        "optional": true
      },
      {
        "name": "scale",
        "type": "number[]",
        "description": "Three positive finite scale coordinates; omitted preserves source scale.",
        "optional": true
      }
    ],
    "summary": "Duplicate free Scenery or a whole Group through original native capability/price/payment, virtual Duplicate, cash/destroy inverses and localized Undo. Confirms created pose, whole Group member ordering/relative poses and untouched sources, plus native release edit and selection readback. Returns a Scenery prop or Group table. Requires the root world to be the native active scene; normal native costs and selection changes apply.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/duplicate_calls.h",
      "Multiplayer/src/game/duplicate_port.inc",
      "Multiplayer/src/game/duplicate_native.h",
      "Multiplayer/src/game/duplicate_codeguards.h",
      "Multiplayer/src/game/duplicate_transition.h"
    ],
    "native_result": "table"
  },
  {
    "name": "scenery.list",
    "kind": "query",
    "parameters": [],
    "summary": "Read current root-world Scenery handles, native poses and Region IDs from every actual Region/category roster. A pose contains quaternion x/y/z/w, position x/y/z and scale x/y/z, in that order.",
    "returns": "MT2SceneryProp[]",
    "native_evidence": [
      "Multiplayer/src/game/duplicate_reader.h",
      "Multiplayer/src/game/group_reader.h"
    ]
  },
  {
    "name": "scenery.move_group",
    "kind": "action",
    "parameters": [
      {
        "name": "group",
        "type": "MT2Handle",
        "description": "Current root-world native Group handle."
      },
      {
        "name": "position",
        "type": "number[]",
        "description": "Three finite destination coordinates in the current root world."
      },
      {
        "name": "rotation",
        "type": "number[]",
        "description": "Normalized quaternion x/y/z/w; omitted preserves rotation.",
        "optional": true
      },
      {
        "name": "scale",
        "type": "number[]",
        "description": "Three positive finite scale coordinates; omitted preserves scale.",
        "optional": true
      }
    ],
    "summary": "Move, rotate or scale a whole native Group through original DoMove and Undo, preserving its ordered member identities and relative poses. Confirms native destination pose and unchanged other Groups/Scenery. Requires the root world to be the actual native active scene; normal native selection and tool effects apply.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/group_object_calls.h",
      "Multiplayer/src/game/group_object_runtime.inc",
      "Multiplayer/src/game/group_object_move_reader.h",
      "Multiplayer/src/game/group_object_graph.h",
      "Multiplayer/src/game/move_kiss/native_calls.cpp"
    ],
    "native_result": "MT2SceneryGroup"
  },
  {
    "name": "scenery.destroy_group",
    "kind": "action",
    "parameters": [
      {
        "name": "group",
        "type": "MT2Handle",
        "description": "Current root-world native Group handle."
      }
    ],
    "summary": "Destroy a whole native Group th)MT2gameplay"
+ R"MT2gameplay(rough original SelectionManager DestroyProp, retaining normal native refund and Undo behavior. Confirms removal of the Group and every owned Scenery member, plus unchanged unrelated members. Returns destroyed=true and member count. Requires the root world to be the actual native active scene; normal native selection and tool effects apply.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/group_object_calls.h",
      "Multiplayer/src/game/group_object_runtime.inc",
      "Multiplayer/src/game/group_object_transition.h"
    ],
    "native_result": "table"
  },
  {
    "name": "scenery.groups",
    "kind": "query",
    "parameters": [],
    "summary": "Read root-world Groups with exact Group and member handles, native ten-float poses, Region IDs and relative member poses.",
    "returns": "MT2SceneryGroup[]",
    "native_evidence": [
      "Multiplayer/src/game/group_reader.h",
      "Multiplayer/src/game/duplicate_reader.h"
    ]
  },
  {
    "name": "scenery.group",
    "kind": "action",
    "parameters": [
      {
        "name": "handles",
        "type": "MT2Handle[]",
        "description": "Two or more ordered free Scenery or existing Group handles. Existing Group members must be selected through their parent Group. The first existing Group becomes the target."
      }
    ],
    "summary": "Group or merge root-world Scenery through explicitly rooted native Group creation and the whole original GroupSelection transaction. Confirms ordered membership, unchanged member poses, reciprocal parents and exact native Undo. Requires the root world to be the native active scene and the current native tool to permit selection; normal native selection changes apply.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/group_runtime.inc",
      "Multiplayer/src/game/group_native.h",
      "Multiplayer/src/game/group_inverse.h",
      "Multiplayer/src/game/group_transition.h"
    ],
    "native_result": "MT2SceneryGroup"
  },
  {
    "name": "scenery.ungroup",
    "kind": "action",
    "parameters": [
      {
        "name": "handles",
        "type": "MT2Handle[]",
        "description": "One or more ordered native Group handles in the current root world."
      }
    ],
    "summary": "Run the whole original UngroupSelection transaction and confirm removal, free surviving member handles and unchanged member poses, plus original per-Group saved inverse records. Returns ungrouped count and members. Requires the root world to be the native active scene and the current native tool to permit selection; normal native selection changes apply.",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer/src/game/group_runtime.inc",
      "Multiplayer/src/game/group_native.h",
      "Multiplayer/src/game/group_inverse.h",
      "Multiplayer/src/game/group_transition.h"
    ],
    "native_result": "table"
  },
  {
    "name": "world.entities",
    "kind": "query",
    "parameters": [
      {
        "name": "limit",
        "type": "integer",
        "description": "Maximum returned native objects, 1..128.",
        "optional": true,
        "default": 100
      }
    ],
    "summary": "Read a bounded native world-object inventory with exact handles and building classification.",
    "returns": "MT2WorldEntity[]",
    "native_evidence": [
      "adapters/mt2/native_engine.cpp: list_world_props"
    ]
  },
  {
    "name": "subscribers.candidates",
    "kind": "query",
    "parameters": [],
    "summary": "Read the existing bounded native subscriber-candidate scan, preserving whether it completed.",
    "returns": "MT2SubscriberCandidates",
    "native_evidence": [
      "adapters/mt2/native_engine.cpp: read_subscriber_candidates"
    ]
  },
  {
    "name": "subscribers.get",
    "kind": "query",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2SubscriberHandle",
        "description": "Subscriber identity returned by candidates() in the current )MT2gameplay"
+ R"MT2gameplay(world epoch."
      }
    ],
    "summary": "Read a subscriber's satisfaction, addiction, login state and seven native needs.",
    "returns": "MT2SubscriberState",
    "native_evidence": [
      "adapters/mt2/native_engine.cpp: read_subscriber_needs"
    ]
  },
  {
    "name": "development.staff",
    "kind": "query",
    "parameters": [],
    "summary": "Read all fifty native developer slots, employment and job/membership state",
    "returns": "MT2DeveloperPool",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/developer_staff/read.cpp"
    ]
  },
  {
    "name": "development.research",
    "kind": "query",
    "parameters": [],
    "summary": "Read current development progress and all shared project sources",
    "returns": "MT2ProjectSnapshot",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/general_project_reader.h"
    ]
  },
  {
    "name": "projects.catalogue",
    "kind": "query",
    "parameters": [],
    "summary": "Read native project eligibility and technology purchase/deployment flags",
    "returns": "MT2ProjectCatalogueRow[]",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/general_project_runtime.cpp"
    ]
  },
  {
    "name": "projects.current",
    "kind": "query",
    "parameters": [],
    "summary": "Read current source, work, code lines, bugs and desired workers",
    "returns": "MT2ProjectProgress",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/general_project_reader.h"
    ]
  },
  {
    "name": "network.snapshot",
    "kind": "query",
    "parameters": [],
    "summary": "Read committed server nodes, pipes, waypoints and reciprocal connections",
    "returns": "MT2ServerNetwork",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/connected_network_reader.h"
    ]
  },
  {
    "name": "network.catalogue",
    "kind": "query",
    "parameters": [],
    "summary": "Read node catalogue keys and current native definition costs",
    "returns": "MT2ServerNodeDefinition[]",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/ui_network_runtime.inc"
    ]
  },
  {
    "name": "development.hire",
    "kind": "action",
    "parameters": [],
    "summary": "Pay the original 500 hiring charge and employ one native developer",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/developer_staff/action_native.cpp"
    ],
    "native_result": "table"
  },
  {
    "name": "development.fire",
    "kind": "action",
    "parameters": [],
    "summary": "Fire the last employed developer when it has no job or carried object",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/developer_staff/action_native.cpp"
    ],
    "native_result": "table"
  },
  {
    "name": "development.set_state",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2Handle",
        "description": "Current developer handle from development.staff"
      },
      {
        "name": "value",
        "type": "integer",
        "description": "Native state 0 Idle, 1 GoingToJob, 2 DoingJob, 3 TransportJob, 4 GoingHome, 5 TechGoingHome, 6 TechDevelop or 7 TechDebug; actor must have no job or carried object"
      }
    ],
    "summary": "Set a jobless developer state through the verified native property loader",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/developer_staff/scalar_calls.cpp"
    ],
    "native_result": "table"
  },
  {
    "name": "development.set_active",
    "kind": "action",
    "parameters": [
      {
        "name": "handle",
        "type": "MT2Handle",
        "description": "Current developer handle from development.staff"
      },
      {
        "name": "value",
        "type": "boolean",
        "description": "Requested active flag; actor must have no job or carried object"
      }
    ],
    "summary": "Set a jobless develope)MT2gameplay"
+ R"MT2gameplay(r active property through the native property loader",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/developer_staff/scalar_calls.cpp"
    ],
    "native_result": "table"
  },
  {
    "name": "projects.start",
    "kind": "action",
    "parameters": [
      {
        "name": "source",
        "type": "MT2ProjectSource",
        "description": "Exact current source table from projects.catalogue; includes its world epoch"
      }
    ],
    "summary": "Pay for and start the selected native research, art, statistic or shared project source",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/general_project_runtime.cpp"
    ],
    "native_result": "table"
  },
  {
    "name": "projects.cancel",
    "kind": "action",
    "parameters": [],
    "summary": "Cancel and retire the current native project",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/general_project_runtime.cpp"
    ],
    "native_result": "table"
  },
  {
    "name": "projects.commit",
    "kind": "action",
    "parameters": [],
    "summary": "Deploy and retire the natively completed current project",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/general_project_runtime.cpp"
    ],
    "native_result": "table"
  },
  {
    "name": "projects.set_workers",
    "kind": "action",
    "parameters": [
      {
        "name": "count",
        "type": "integer",
        "description": "Desired worker count 0 through 50, bounded by employed staff and technology maximum"
      }
    ],
    "summary": "Allocate workers to the current native project",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/tutorial_replication/staff_native.inc"
    ],
    "native_result": "table"
  },
  {
    "name": "development.assign_research_workers",
    "kind": "action",
    "parameters": [
      {
        "name": "count",
        "type": "integer",
        "description": "Desired worker count 0 through 50, bounded by employed staff and technology maximum"
      }
    ],
    "summary": "Allocate workers to the current native project",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/tutorial_replication/staff_native.inc"
    ],
    "native_result": "table"
  },
  {
    "name": "network.place_node",
    "kind": "action",
    "parameters": [
      {
        "name": "key",
        "type": "string",
        "description": "Exact key from network.catalogue"
      },
      {
        "name": "position",
        "type": "number[]",
        "description": "Finite XYZ point; original native surface and legality checks apply"
      }
    ],
    "summary": "Buy and place a server node with its original native inverse history",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/ui_network_runtime.inc"
    ],
    "native_result": "table"
  },
  {
    "name": "network.place_pipe",
    "kind": "action",
    "parameters": [
      {
        "name": "start",
        "type": "MT2Handle",
        "description": "Current starting network node"
      },
      {
        "name": "end",
        "type": "MT2Handle|nil",
        "description": "Existing endpoint node in the same region; omit to create a Junction",
        "optional": true
      },
      {
        "name": "position",
        "type": "number[]|nil",
        "description": "Finite XYZ endpoint for a new Junction; required when end is omitted",
        "optional": true
      }
    ],
    "summary": "Create the proved native Cable connection between nodes or to a new Junction, with payment and inverse history",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/ui_network_pipe.inc"
    ],
    "native_result": "table"
  },
  {
    "name": "network.remove",
    "kind": "action",
    ")MT2gameplay"
+ R"MT2gameplay(parameters": [
      {
        "name": "handle",
        "type": "MT2Handle",
        "description": "Current server node or pipe handle"
      }
    ],
    "summary": "Remove the network object through native destruction and verify UID retirement and inverse history",
    "returns": "MT2Action",
    "native_evidence": [
      "Multiplayer commit 8a13602: src/game/network_remove_runtime.inc"
    ],
    "native_result": "table"
  }
]
)MT2gameplay"
