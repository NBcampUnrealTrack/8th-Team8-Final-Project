# Stage_Final

Playable Unreal Engine 5.5 forest level based on the approved four-lane layout.
Uses the project's existing low-poly trees, boulders, ruined stone walls,
grass and mud textures, matching the supplied gameplay screenshots.

- Level: `/Game/Game/Stage/Stage_Final`
- File: `Content/Game/Stage/Stage_Final.umap`
- Connected stage-table row: `Stage07` in `DT_SFStage`.
- Nominal map bounds: 360 m east-west by 288 m north-south.
- Four physical attack lanes, each with a 22 m nominal width.
- Two nexus spawn anchors with racial editor previews: Beast northwest (team 1), Etherian southeast (team 0).
- Eight LanePath splines describe the same four corridors, one direction per team.
- No other buildings, units, spawners or resource nodes are authored.
- Six hidden impassable terrain meshes retain collision and NavArea_Null modifiers; forest islands and natural stone borders provide their visible scenery.
- 1,687 trees and 3,730 grass, bush and gravel instances use native foliage batching. Rocks and ruined walls remain individually editable.

## Empty resource reserves

Each nexus has 54 m east-west by 36 m north-south of reserved flat ground behind it. The editor-only TargetPoint actors in `Stage Final/Resource Reserves (empty)` identify their centers; they are excluded from cooked gameplay.

| Base | World center (cm) | Reserved world X | Reserved world Y |
| --- | --- | --- | --- |
| Beast | X=11400, Y=-12600, Z=0 | 9600 to 13200 | -15300 to -9900 |
| Etherian | X=-11400, Y=12600, Z=0 | -13200 to -9600 | 9900 to 15300 |

These areas sit beyond the lane splines so mineral and ether placement is not blocked by the game's no-building-on-lanes rule. Scenery is excluded from the full reserved rectangles, including a margin for canopies and rock bounds.

## Editing

Open `Stage_Final` from the Unreal Content Browser. Terrain, nexuses, navigation, lane splines and resource reserve locators have separate Outliner folders. Materials and terrain meshes live under `/Game/Game/Stage/StageFinalArt`.

The authoring geometry and proportions are in `Scripts/Editor/final_map_layout.py`; matching OBJ sources are retained in `ContentSource/Stage_Final`. `create_stage_final.py` refuses to overwrite an existing authored level. `dress_stage_final.py` refreshes only its tagged environment actors and its dedicated foliage types, preserving gameplay placements. `build_verify_capture_stage_final.py` rebuilds navigation, checks all four routes and both resource reserves, and exports real perspective renders of the level.

Launch gameplay through the final stage selection in the lobby to initialize the Human player and Beast enemy catalogs correctly. Resource nodes and other buildings can be placed later in the reserved space.

## Validation and real level previews

Navigation was rebuilt after forest dressing. Every attack-lane segment, all
four complete routes, both resource reserve clearings and all six blocked
regions passed checks. Native foliage instance counts were checked against
the saved level, and both full reserve rectangles remain free of scenery.

A disposable editor simulation selected Stage07 and used the normal stage
travel path. It reached `Playing`, spawned exactly the Human and Beast nexuses,
and passed a runtime pathfinding check. Test progress remained in memory only.

Real perspective renders: `ForestOverview.png`, `EtherianBase.png`,
`BeastBase.png` and `LaneView.png` in this folder.
