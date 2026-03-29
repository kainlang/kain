# Build Log - GASShowcase 
 
**Build Date**: Tue 02 24 2026 05:37 
**Status**: FAILED 
**Error Type**: KAIN COMPILATION 
 
## KAIN Compilation Errors 
 
```
 KAIN Compiler v0.1.0
🚀 Building UE5 Plugin: GASShowcase
📍 Plugin directory: .

📚 Loaded stdlib from: m:\Code\Kain\stdlib\ue5
📁 Source files: 18 (stdlib: 12, user: 6)
   📚 Stdlib files:
      1. actor.kn
      2. common.kn
      3. components.kn
      4. gameplay.kn
      5. materials.kn
      6. math.kn
      7. particles.kn
      8. patterns.kn
      9. shaders.kn
      10. skeletal_mesh.kn
      11. utilities.kn
      12. world.kn
   📝 User files:
      1. gas.kn
      2. test_cues.kn
      3. test_effects.kn
      4. test_phase4.kn
      5. test_targets.kn
      6. test_tasks.kn

🔍 Validating source files...
   ✓ actor.kn validated
   ✓ common.kn validated
   ✓ components.kn validated
   ✓ gameplay.kn validated
   ✓ materials.kn validated
   ✓ math.kn validated
   ✓ particles.kn validated
   ✓ patterns.kn validated
   ✓ shaders.kn validated
   ✓ skeletal_mesh.kn validated
   ✓ utilities.kn validated
   ✓ world.kn validated
Runtime error: 299 parse error(s) found:

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:733:43
   |
733 |             apply_damage_to_target(target, beam_damage_per_second * delta_time, "Damage.Magical.Fire")
   |                                           ^
   |
   Expected attribute (@instancing, @ability_tags, etc.) or method (fn) in ability body

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:752:74
   |
752 |     fn activate_ability(handle, actor_info, activation_info, trigger_event_data):
   |                                                                          ^
   |
   Dedent

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:848:4
   |
848 | # ───────────────────────────────────────────────────────────────────────────
   |    ^
   |
   Expected attribute (@instancing, @ability_tags, etc.) or method (fn) in ability body

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:888:12
   |
888 |     @net_execution(policy: "ServerInitiated")
   |            ^
   |
   Dedent

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:903:40
   |
903 |     fn activate_ability(handle, actor_info, activation_info, trigger_event_data):
   |                                        ^
   |
   Expected attribute (@instancing, @ability_tags, etc.) or method (fn) in ability body

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:915:70
   |
915 | # ───────────────────────────────────────────────────────────────────────────
   |                                                                      ^
   |
   Dedent

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:931:49
   |
931 |     fn activate_ability(handle, actor_info, activation_info, trigger_event_data):
   |                                                 ^
   |
   Expected attribute (@instancing, @ability_tags, etc.) or method (fn) in ability body

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:959:71
   |
959 |             end_ability(handle, actor_info, activation_info, true, true)
   |                                                                       ^
   |
   Dedent

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:995:3
   |
995 |     @net_execution(policy: "ServerInitiated")
   |   ^
   |
   Expected attribute (@instancing, @ability_tags, etc.) or method (fn) in ability body

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1016:7
   |
1016 | # Effects modify attributes with instant, duration, or infinite policies.
   |       ^
   |
   Dedent

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1089:37
   |
1089 |     tags: ["Effect.Buff.Movement", "Effect.Buff.Combat"]
   |                                     ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1089:41
   |
1089 |     tags: ["Effect.Buff.Movement", "Effect.Buff.Combat"]
   |                                         ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1092:14
   |
1092 |     @application_tag_requirements
   |              ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1095:10
   |
1095 |     cues: ["GameplayCue.Effect.Buff.Applied"]
   |          ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1097:17
   |
1097 | struct ArmorBuffEffect:
   |                 ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1100:52
   |
1100 |     @modifier(attribute: "Defense", operation: "Add")
   |                                                    ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1103:12
   |
1103 |     armor_multiplier: 1.5
   |            ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1103:16
   |
1103 |     armor_multiplier: 1.5
   |                ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1107:20
   |
1107 |     tags: ["Status.Buff.Armor", "Status.Buff.Fortified"]
   |                    ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1109:8
   |
1109 |     require: ["Status.Alive"]
   |        ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1111:27
   |
1111 |     cues: ["GameplayCue.Effect.Buff.Applied"]
   |                           ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1115:2
   |
1115 |     duration: 3.0
   |  ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1117:20
   |
1117 |     movement_speed: 0.0
   |                    ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1117:24
   |
1117 |     movement_speed: 0.0
   |                        ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1122:8
   |
1122 |     @block_abilities_with_tag
   |        ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1125:24
   |
1125 |     cancel: ["Ability.Channeled"]
   |                        ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1128:21
   |
1128 |     ignore: ["Status.Immune.CC", "Status.Immune.CC.Stun"]
   |                     ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1130:22
   |
1130 |     require: ["Cleanse.CC"]
   |                      ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1148:10
   |
1148 |     @stacking
   |          ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1148:14
   |
1148 |     @stacking
   |              ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1152:5
   |
1152 |     @gameplay_cues
   |     ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1153:29
   |
1153 |     cues: ["GameplayCue.Effect.Debuff.Applied"]
   |                             ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1154:31
   |
1154 | # ───────────────────────────────────────────────────────────────────────────
   |                               ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1155:47
   |
1155 | # PERIODIC EFFECTS - DOT (Damage Over Time) and HOT (Heal Over Time)
   |                                               ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1156:27
   |
1156 | # ───────────────────────────────────────────────────────────────────────────
   |                           ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1159:10
   |
1159 |     @duration(type: "HasDuration")
   |          ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1159:14
   |
1159 |     @duration(type: "HasDuration")
   |              ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1164:3
   |
1164 |     @modifier(attribute: "Health", operation: "Add")
   |   ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1166:12
   |
1166 |     @stacking
   |            ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1169:2
   |
1169 |     duration_policy: "RefreshOnSuccessfulApplication"
   |  ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1170:23
   |
1170 |     period_policy: "ResetOnSuccessfulApplication"
   |                       ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1171:60
   |
1171 |     expiration_policy: "RemoveSingleStackAndRefreshDuration"
   |                                                            ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1173:32
   |
1173 |     tags: ["Effect.Damage.DOT", "Effect.Type.Periodic"]
   |                                ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1176:3
   |
1176 |     @application_tag_requirements
   |   ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1176:7
   |
1176 |     @application_tag_requirements
   |       ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1179:2
   |
1179 |     @ongoing_tag_requirements
   |  ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1181:18
   |
1181 |     @removal_tag_requirements
   |                  ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1182:30
   |
1182 |     require: ["Cleanse.Fire"]
   |                              ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1184:35
   |
1184 |     cues: ["GameplayCue.Effect.Burn.Start", "GameplayCue.Effect.Burn.Loop", "GameplayCue.Effect.Burn.End"]
   |                                   ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1186:2
   |
1186 | struct PoisonEffect:
   |  ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1187:30
   |
1187 |     @duration(type: "HasDuration")
   |                              ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1192:5
   |
1192 |     @modifier(attribute: "Health", operation: "Add")
   |     ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1192:9
   |
1192 |     @modifier(attribute: "Health", operation: "Add")
   |         ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1194:35
   |
1194 |     @modifier(attribute: "MovementSpeed", operation: "Multiply")
   |                                   ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1195:10
   |
1195 |     speed_reduction: 0.9
   |          ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1197:18
   |
1197 |     type: "AggregateBySource"
   |                  ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1199:35
   |
1199 |     duration_policy: "NeverRefresh"
   |                                   ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1203:6
   |
1203 |     tags: ["Status.Debuff.Poisoned", "Status.Debuff"]
   |      ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1204:21
   |
1204 |     @application_tag_requirements
   |                     ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1208:18
   |
1208 |     cues: ["GameplayCue.Effect.Poison.Start", "GameplayCue.Effect.Poison.Loop", "GameplayCue.Effect.Poison.End"]
   |                  ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1208:66
   |
1208 |     cues: ["GameplayCue.Effect.Poison.Start", "GameplayCue.Effect.Poison.Loop", "GameplayCue.Effect.Poison.End"]
   |                                                                  ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1212:1
   |
1212 |     duration: 8.0
   | ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1212:5
   |
1212 |     duration: 8.0
   |     ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1216:30
   |
1216 |     @modifier(attribute: "Health", operation: "Add", magnitude_type: "AttributeBased")
   |                              ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1218:10
   |
1218 |         coefficient: -0.05
   |          ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1219:32
   |
1219 |         backing_attribute: "MaxHealth"
   |                                ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1220:40
   |
1220 |         calculation_type: "AttributeMagnitude"
   |                                        ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1225:5
   |
1225 |     tags: ["Effect.Damage.DOT"]
   |     ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1227:37
   |
1227 |     tags: ["Status.Debuff.Bleeding"]
   |                                     ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1231:10
   |
1231 |     @gameplay_cues
   |          ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1249:19
   |
1249 |     @gameplay_cues
   |                   ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1250:3
   |
1250 |     cues: ["GameplayCue.Effect.Heal.Start", "GameplayCue.Effect.Heal.End"]
   |   ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1250:56
   |
1250 |     cues: ["GameplayCue.Effect.Heal.Start", "GameplayCue.Effect.Heal.End"]
   |                                                        ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1253:35
   |
1253 |     @duration(type: "HasDuration")
   |                                   ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1256:13
   |
1256 |     period: 1.0
   |             ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1264:6
   |
1264 |     @application_tag_requirements
   |      ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1266:16
   |
1266 | # ───────────────────────────────────────────────────────────────────────────
   |                ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1267:25
   |
1267 | # INFINITE EFFECTS - Last forever until removed
   |                         ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1268:73
   |
1268 | # ───────────────────────────────────────────────────────────────────────────
   |                                                                         ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1271:9
   |
1271 |     @duration(type: "Infinite")
   |         ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1273:14
   |
1273 |     period: 1.0
   |              ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1278:23
   |
1278 |     tags: ["Effect.Heal.HOT", "Effect.Type.Periodic"]
   |                       ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1278:27
   |
1278 |     tags: ["Effect.Heal.HOT", "Effect.Type.Periodic"]
   |                           ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1280:8
   |
1280 |     tags: ["Status.Buff.Regeneration"]
   |        ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1283:11
   |
1283 | @gameplay_effect
   |           ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1287:8
   |
1287 |     period: 1.0
   |        ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1289:1
   |
1289 |     @modifier(attribute: "Mana", operation: "Add", magnitude_type: "AttributeBased")
   | ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1290:10
   |
1290 |     mana_regen:
   |          ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1293:2
   |
1293 |         calculation_type: "AttributeMagnitude"
   |  ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1295:27
   |
1295 |     tags: ["Effect.Heal.HOT"]
   |                           ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1299:9
   |
1299 | struct SwordMasteryEffect:
   |         ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1303:25
   |
1303 |     @modifier(attribute: "CriticalChance", operation: "Add")
   |                         ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1303:29
   |
1303 |     @modifier(attribute: "CriticalChance", operation: "Add")
   |                             ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1304:20
   |
1304 |     crit_bonus: 0.05
   |                    ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1310:14
   |
1310 | struct FireImmunityEffect:
   |              ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1316:6
   |
1316 |     @immunity
   |      ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1317:48
   |
1317 |     immune_to: ["Effect.Damage.Fire", "Effect.CC.Burn"]
   |                                                ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1319:12
   |
1319 |     remove: ["Effect.Damage.Fire", "Status.Debuff.Burning"]
   |            ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1321:6
   |
1321 | struct PhysicalImmunityEffect:
   |      ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1324:15
   |
1324 |     tags: ["Effect.Buff.Immunity"]
   |               ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1329:14
   |
1329 | @gameplay_effect
   |              ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1329:18
   |
1329 | @gameplay_effect
   |                  ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1331:28
   |
1331 |     @duration(type: "Infinite")
   |                            ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1335:30
   |
1335 |     tags: ["Status.Immune.CC"]
   |                              ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1337:27
   |
1337 |     immune_to: ["Effect.CC"]
   |                           ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1340:14
   |
1340 | # ───────────────────────────────────────────────────────────────────────────
   |              ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1341:9
   |
1341 | # COST EFFECTS - Resource consumption for abilities
   |         ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1342:51
   |
1342 | # ───────────────────────────────────────────────────────────────────────────
   |                                                   ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1346:35
   |
1346 |     @modifier(attribute: "Mana", operation: "Add")
   |                                   ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1346:39
   |
1346 |     @modifier(attribute: "Mana", operation: "Add")
   |                                       ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1349:3
   |
1349 |     tags: ["Effect.Cost.Mana"]
   |   ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1352:30
   |
1352 |     @duration(type: "Instant")
   |                              ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1353:40
   |
1353 |     @modifier(attribute: "Stamina", operation: "Add")
   |                                        ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1355:10
   |
1355 |     @owned_tags
   |          ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1358:15
   |
1358 | struct HealthCostEffect:
   |               ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1378:17
   |
1378 |     require: ["Status.Channeling"]
   |                 ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1378:21
   |
1378 |     require: ["Status.Channeling"]
   |                     ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1379:17
   |
1379 | # ───────────────────────────────────────────────────────────────────────────
   |                 ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1381:3
   |
1381 | # ───────────────────────────────────────────────────────────────────────────
   |   ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1381:46
   |
1381 | # ───────────────────────────────────────────────────────────────────────────
   |                                              ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1383:23
   |
1383 | struct JumpCooldownEffect:
   |                       ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1386:11
   |
1386 |     @owned_tags
   |           ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1389:31
   |
1389 |     tags: ["Cooldown.Ability.Jump"]
   |                               ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1389:35
   |
1389 |     tags: ["Cooldown.Ability.Jump"]
   |                                   ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1391:12
   |
1391 | struct MeleeAttackCooldownEffect:
   |            ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1395:43
   |
1395 |     tags: ["Cooldown.Ability.Attack.Melee"]
   |                                           ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1400:33
   |
1400 |     @duration(type: "HasDuration")
   |                                 ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1403:11
   |
1403 |     tags: ["Cooldown.Ability.Skill1"]
   |           ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1406:11
   |
1406 | @gameplay_effect
   |           ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1406:15
   |
1406 | @gameplay_effect
   |               ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1409:15
   |
1409 |     duration: 5.0
   |               ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1413:7
   |
1413 |     tags: ["Cooldown.Ability.Skill2"]
   |       ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1414:7
   |
1414 | @gameplay_effect
   |       ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1416:13
   |
1416 |     @duration(type: "HasDuration")
   |             ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1419:34
   |
1419 |     tags: ["Cooldown.Ability.Skill3"]
   |                                  ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1419:38
   |
1419 |     tags: ["Cooldown.Ability.Skill3"]
   |                                      ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1421:16
   |
1421 |     tags: ["Cooldown.Ability.Skill3"]
   |                ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1423:14
   |
1423 | struct FireBeamCooldownEffect:
   |              ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1424:30
   |
1424 |     @duration(type: "HasDuration")
   |                              ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1428:8
   |
1428 |     @granted_tags
   |        ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1432:23
   |
1432 |     @duration(type: "HasDuration")
   |                       ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1432:27
   |
1432 |     @duration(type: "HasDuration")
   |                           ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1434:7
   |
1434 |     @owned_tags
   |       ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1436:4
   |
1436 |     @granted_tags
   |    ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1437:37
   |
1437 |     tags: ["Cooldown.Ability.Ultimate"]
   |                                     ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1442:11
   |
1442 |     @owned_tags
   |           ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1442:15
   |
1442 |     @owned_tags
   |               ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1443:33
   |
1443 |     tags: ["Cooldown.Ability.Buff"]
   |                                 ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1445:32
   |
1445 |     tags: ["Cooldown.Ability.Buff"]
   |                                ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1447:24
   |
1447 | struct InvulnerabilityCooldownEffect:
   |                        ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1449:9
   |
1449 |     duration: 30.0
   |         ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1471:6
   |
1471 | struct GlobalCooldownEffect:
   |      ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1471:10
   |
1471 | struct GlobalCooldownEffect:
   |          ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1473:13
   |
1473 |     duration: 1.5
   |             ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1475:16
   |
1475 |     tags: ["Cooldown.Global.GCD"]
   |                ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1478:15
   |
1478 | # ───────────────────────────────────────────────────────────────────────────
   |               ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1478:19
   |
1478 | # ───────────────────────────────────────────────────────────────────────────
   |                   ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1479:12
   |
1479 | # COMPLEX EFFECTS - Conditional, overflow, immunity
   |            ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1479:53
   |
1479 | # COMPLEX EFFECTS - Conditional, overflow, immunity
   |                                                     ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1481:10
   |
1481 | @gameplay_effect
   |          ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1481:14
   |
1481 | @gameplay_effect
   |              ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1484:11
   |
1484 |     @modifier(attribute: "Health", operation: "Add", magnitude_type: "AttributeBased")
   |           ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1484:51
   |
1484 |     @modifier(attribute: "Health", operation: "Add", magnitude_type: "AttributeBased")
   |                                                   ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1486:17
   |
1486 |         coefficient: 0.2
   |                 ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1488:43
   |
1488 |         calculation_type: "AttributeMagnitude"
   |                                           ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1488:47
   |
1488 |         calculation_type: "AttributeMagnitude"
   |                                               ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1490:38
   |
1490 |     tags: ["Effect.Heal.Instant", "Effect.Lifesteal"]
   |                                      ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1494:3
   |
1494 | struct VampirismEffect:
   |   ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1495:19
   |
1495 |     @duration(type: "Infinite")
   |                   ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1497:16
   |
1497 |     lifesteal_bonus: 0.15
   |                ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1516:71
   |
1516 |     @modifier(attribute: "Health", operation: "Add", magnitude_type: "AttributeBased")
   |                                                                       ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1516:75
   |
1516 |     @modifier(attribute: "Health", operation: "Add", magnitude_type: "AttributeBased")
   |                                                                           ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1518:21
   |
1518 |         coefficient: -0.3
   |                     ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1520:12
   |
1520 |         calculation_type: "AttributeMagnitude"
   |            ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1522:47
   |
1522 |     tags: ["Effect.Damage.Instant", "Effect.Reflected"]
   |                                               ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1522:51
   |
1522 |     tags: ["Effect.Damage.Instant", "Effect.Reflected"]
   |                                                   ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1525:4
   |
1525 |     @duration(type: "HasDuration")
   |    ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1527:12
   |
1527 |     @modifier(attribute: "MaxHealth", operation: "Add")
   |            ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1530:18
   |
1530 |     tags: ["Effect.Buff.Shield"]
   |                  ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1530:22
   |
1530 |     tags: ["Effect.Buff.Shield"]
   |                      ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1532:27
   |
1532 |     tags: ["Status.Buff.Shield"]
   |                           ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1534:28
   |
1534 |     overflow: ["OverhealShieldEffect"]
   |                            ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1537:34
   |
1537 |     @duration(type: "HasDuration")
   |                                  ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1538:2
   |
1538 |     duration: 5.0
   |  ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1540:24
   |
1540 |     tags: ["Effect.Buff.Immunity"]
   |                        ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1542:27
   |
1542 |     tags: ["Status.Immune.AllDamage", "Status.Immune.CC", "Status.Buff.Invulnerability"]
   |                           ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1544:17
   |
1544 |     immune_to: ["Effect.Damage", "Effect.CC"]
   |                 ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1544:21
   |
1544 |     immune_to: ["Effect.Damage", "Effect.CC"]
   |                     ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1546:1
   |
1546 |     remove: ["Effect.Damage", "Effect.CC", "Status.Debuff"]
   | ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1546:58
   |
1546 |     remove: ["Effect.Damage", "Effect.CC", "Status.Debuff"]
   |                                                          ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1548:76
   |
1548 |     cues: ["GameplayCue.Effect.Invulnerability.Start", "GameplayCue.Effect.Invulnerability.End"]
   |                                                                            ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1548:80
   |
1548 |     cues: ["GameplayCue.Effect.Invulnerability.Start", "GameplayCue.Effect.Invulnerability.End"]
   |                                                                                ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1550:22
   |
1550 | struct BlockDefenseBuffEffect:
   |                      ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1552:15
   |
1552 |     @modifier(attribute: "Defense", operation: "Multiply")
   |               ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1554:23
   |
1554 |     @modifier(attribute: "Armor", operation: "Multiply")
   |                       ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1554:27
   |
1554 |     @modifier(attribute: "Armor", operation: "Multiply")
   |                           ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1556:1
   |
1556 |     @owned_tags
   | ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1557:43
   |
1557 |     tags: ["Effect.Buff.Defensive", "Effect.Buff.Block"]
   |                                           ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1561:13
   |
1561 | struct ParryWindowEffect:
   |             ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1561:17
   |
1561 | struct ParryWindowEffect:
   |                 ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1563:12
   |
1563 |     duration: 0.3
   |            ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1565:32
   |
1565 |     tags: ["Effect.Buff.Parry"]
   |                                ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1569:27
   |
1569 |     on_damage_received: ["ParryCounterEffect"]
   |                           ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1569:31
   |
1569 |     on_damage_received: ["ParryCounterEffect"]
   |                               ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1571:24
   |
1571 | struct ParryCounterEffect:
   |                        ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1573:24
   |
1573 |     @modifier(attribute: "Health", operation: "Add", magnitude_type: "AttributeBased")
   |                        ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1575:17
   |
1575 |         coefficient: -1.5
   |                 ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1575:21
   |
1575 |         coefficient: -1.5
   |                     ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1577:15
   |
1577 |         calculation_type: "AttributeMagnitude"
   |               ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1579:12
   |
1579 |     tags: ["Effect.Damage.Instant", "Effect.Counter"]
   |            ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1582:4
   |
1582 |     @duration(type: "HasDuration")
   |    ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1582:8
   |
1582 |     @duration(type: "HasDuration")
   |        ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1584:11
   |
1584 |     @owned_tags
   |           ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1586:15
   |
1586 |     @granted_tags
   |               ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1590:16
   |
1590 | # ============================================================================
   |                ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1590:20
   |
1590 | # ============================================================================
   |                    ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1590:77
   |
1590 | # ============================================================================
   |                                                                             ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1592:15
   |
1592 | # ============================================================================
   |               ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1603:77
   |
1603 |     # Complex tag query: (Buffed OR Empowered) AND Alive AND NOT (Stunned OR Silenced)
   |                                                                             ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1603:81
   |
1603 |     # Complex tag query: (Buffed OR Empowered) AND Alive AND NOT (Stunned OR Silenced)
   |                                                                                 ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1606:7
   |
1606 |                   and all(["Status.Alive", "Status.Condition.Conscious"])
   |       ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1607:73
   |
1607 |                   and not(any(["Status.CC.Stunned", "Status.CC.Silenced"]))
   |                                                                         ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1609:2
   |
1609 |         let asc = get_ability_system_component()
   |  ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1610:35
   |
1610 |         let owner_tags = asc.get_owned_gameplay_tags()
   |                                   ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1610:39
   |
1610 |         let owner_tags = asc.get_owned_gameplay_tags()
   |                                       ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1612:2
   |
1612 |     fn activate_ability(handle, actor_info, activation_info, trigger_event_data):
   |  ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1612:45
   |
1612 |     fn activate_ability(handle, actor_info, activation_info, trigger_event_data):
   |                                             ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1613:14
   |
1613 |         if !commit_ability(handle, actor_info, activation_info):
   |              ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1614:10
   |
1614 |             end_ability(handle, actor_info, activation_info, true, true)
   |          ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1616:14
   |
1616 |         apply_damage_to_target(100.0, "Damage.Physical")
   |              ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1616:18
   |
1616 |         apply_damage_to_target(100.0, "Damage.Physical")
   |                  ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1616:53
   |
1616 |         apply_damage_to_target(100.0, "Damage.Physical")
   |                                                     ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1617:50
   |
1617 |         end_ability(handle, actor_info, activation_info, true, false)
   |                                                  ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1619:29
   |
1619 | struct VulnerabilityExploitAbility:
   |                             ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1622:5
   |
1622 |     @net_execution(policy: "ServerInitiated")
   |     ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1622:9
   |
1622 |     @net_execution(policy: "ServerInitiated")
   |         ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1625:12
   |
1625 |     # Target must have vulnerability AND NOT have immunity
   |            ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1627:80
   |
1627 |     target_valid: any(["Weakness.Fire", "Weakness.Ice", "Weakness.Lightning"])
   |                                                                                ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1629:11
   |
1629 |     fn can_activate_ability(handle, actor_info, source_tags, target_tags) -> Bool:
   |           ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1629:15
   |
1629 |     fn can_activate_ability(handle, actor_info, source_tags, target_tags) -> Bool:
   |               ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1630:29
   |
1630 |         let target = get_target_actor()
   |                             ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1632:8
   |
1632 |             return false
   |        ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1633:35
   |
1633 |         let target_asc = get_ability_system_component_from_actor(target)
   |                                   ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1634:17
   |
1634 |         let target_tags = target_asc.get_owned_gameplay_tags()
   |                 ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1635:51
   |
1635 |         return evaluate_query(target_valid, target_tags)
   |                                                   ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1635:55
   |
1635 |         return evaluate_query(target_valid, target_tags)
   |                                                       ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1636:55
   |
1636 |     fn activate_ability(handle, actor_info, activation_info, trigger_event_data):
   |                                                       ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1637:26
   |
1637 |         if !commit_ability(handle, actor_info, activation_info):
   |                          ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1638:64
   |
1638 |             end_ability(handle, actor_info, activation_info, true, true)
   |                                                                ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1641:7
   |
1641 |         let target_asc = get_ability_system_component_from_actor(target)
   |       ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1642:14
   |
1642 |         var damage_multiplier: Float = 1.0
   |              ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1645:23
   |
1645 |         elif target_asc.has_matching_gameplay_tag("Weakness.Ice"):
   |                       ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1645:27
   |
1645 |         elif target_asc.has_matching_gameplay_tag("Weakness.Ice"):
   |                           ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1646:37
   |
1646 |             damage_multiplier = 1.8
   |                                     ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1648:12
   |
1648 |             damage_multiplier = 1.5
   |            ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1649:19
   |
1649 |         apply_damage_effect(target, 50.0 * damage_multiplier, "Damage.TrueDamage.PureDamage")
   |                   ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1649:95
   |
1649 |         apply_damage_effect(target, 50.0 * damage_multiplier, "Damage.TrueDamage.PureDamage")
   |                                                                                               ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1651:17
   |
1651 | # ───────────────────────────────────────────────────────────────────────────
   |                 ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1651:21
   |
1651 | # ───────────────────────────────────────────────────────────────────────────
   |                     ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1651:78
   |
1651 | # ───────────────────────────────────────────────────────────────────────────
   |                                                                              ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1652:50
   |
1652 | # TAG EVENT HANDLERS - Reactive state management
   |                                                  ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1653:57
   |
1653 | # ───────────────────────────────────────────────────────────────────────────
   |                                                         ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1657:12
   |
1657 |     @replicated
   |            ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1657:16
   |
1657 |     @replicated
   |                ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1662:5
   |
1662 |     state is_in_combat: Bool = false
   |     ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1667:5
   |
1667 |         cancel_all_abilities()
   |     ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1670:30
   |
1670 |         apply_gameplay_cue("GameplayCue.Effect.Stun.Start")
   |                              ^
   |
   Expected 'type' parameter in @duration

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1670:34
   |
1670 |         apply_gameplay_cue("GameplayCue.Effect.Stun.Start")
   |                                  ^
   |
   Expected identifier, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1672:6
   |
1672 |     @on_tag_removed("Status.CC.Stunned")
   |      ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1673:18
   |
1673 |     fn on_unstunned():
   |                  ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1675:12
   |
1675 |         play_animation("Idle")
   |            ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1695:51
   |
1695 |         apply_gameplay_cue("GameplayCue.Effect.Burn.End")
   |                                                   ^
   |
   @tag_query

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1722:2
   |
1722 |         set_life_span(5.0)
   |  ^
   |
   Dedent

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1729:42
   |
1729 |         let query = all(["Status.Alive"]) and not(any(["Status.CC"]))
   |                                          ^
   |
   @target_tag_query

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1776:20
   |
1776 |     fn initialize_ability_system():
   |                    ^
   |
   Dedent

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1825:36
   |
1825 |     fn apply_damage_to_self(damage: Float, damage_type: String):
   |                                    ^
   |
   Newline("\n               ")

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1829:13
   |
1829 |         ability_system_component.apply_gameplay_effect_spec_to_self(effect_spec)
   |             ^
   |
   Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1845:26
   |
1845 |         let gameplay_tag = request_gameplay_tag(tag)
   |                          ^
   |
   Expected Eq, got Newline("\n    ")

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1974:4
   |
1974 |     fn remove_fire_effects():
   |    ^
   |
   Dedent

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:1987:26
   |
1987 |         let asc = get_ability_system_component()
   |                          ^
   |
   Expected Eq, got Newline("\n    ")

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:2011:37
   |
2011 |         println("Health changed: {old_value} -> {new_value}")
   |                                     ^
   |
   Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:2043:53
   |
2043 | # ============================================================================
   |                                                     ^
   |
   LBrace

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:2118:30
   |
2118 |                 Server_ActivateAbility(ability_tag)
   |                              ^
   |
   Expected identifier, got Indent

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:2124:28
   |
2124 |         let asc = get_ability_system_component()
   |                            ^
   |
   Expected identifier, got Indent

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:2130:29
   |
2130 |         let asc = get_ability_system_component()
   |                             ^
   |
   Expected identifier, got Indent

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:2135:33
   |
2135 |     fn add_minimal_tag(tag: String):
   |                                 ^
   |
   Expected identifier, got Indent

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:2140:25
   |
2140 | # ============================================================================
   |                         ^
   |
   Expected identifier, got Indent

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:2144:9
   |
2144 | # ============================================================================
   |         ^
   |
   Expected identifier, got Indent

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:2163:1
   |
2163 |     @on_tag_removed("Status.Debuff.Burning")
   | ^
   |
   Expected Comma, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:2185:7
   |
2185 |     level: Float
   |       ^
   |
   Expected Eq, got Newline("\n    ")

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:2210:52
   |
2210 | # ✅ Attribute Delegates (on_health_changed, on_out_of_health)
   |                                                    ^
   |
   Expected Comma, got Colon

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:2264:1
   |
2264 | 
   | ^
   |
   Dedent

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:2264:1
   |
2264 | 
   | ^
   |
   Expected Eq, got Newline("\n    ")

❌ Parse error in m:\Code\Factory\Example_GAS\gas.kn:
   gas.kn:2264:1
   |
2264 | 
   | ^
   |
   Dedent

❌ Source file not found: m:\Code\Factory\Example_GAS\test_cues.kn

❌ Source file not found: m:\Code\Factory\Example_GAS\test_effects.kn

❌ Source file not found: m:\Code\Factory\Example_GAS\test_phase4.kn

❌ Source file not found: m:\Code\Factory\Example_GAS\test_targets.kn

❌ Source file not found: m:\Code\Factory\Example_GAS\test_tasks.kn
```
