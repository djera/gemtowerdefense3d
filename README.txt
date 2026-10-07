GEM TOWER DEFENSE — UNREAL ENGINE 5.8

RunGame.bat launches the playable prototype in a window.
OpenEditor.bat opens the project. Press Play to run it in the editor.

MAIN WORKING FOLDER
C:\Users\nejcr\Documents\projects\gemtowerdefense
Continue all development in this folder. The project targets Unreal 5.8;
OpenEditor.bat, RunGame.bat and BuildProject.bat use this project's files
and the installed UE_5.8 engine. No conversion copy is needed.
Sibling folders named "gemtowerdefense 5.8", "gemtowerdefense 5.8 - 2",
and similar are older conversion copies, not the active project.

20 x 20 grid, 100 Unreal units per cell. Content/Data/BoardLayout.txt holds the
fixed ASCII map: S=Snow/grass, R=Road, I=Intersection, E=Entry, X=Exit.
Numbers 1-6 are checkpoints, visited in numeric order between E and X.
First text line is grid row 0; each line runs along positive world X.
Intersections use darker road material. Checkpoints are road tiles with
gold circular markers. The current editable layout has 332 S, 42 road,
18 intersection, 6 checkpoint, one entry and one exit tile.
Entry and exit use road materials. S uses snow or grass in patches, with
soft blending only between S tiles; all road and waypoint edges stay hard.
The saved level includes the board so terrain is visible in the editor.
The sidebar's Map tab displays the live 20x20 ASCII array; Selected shows
gem/enemy damage, range and modifiers. Scroll longer modifier lists.
REGENERATE (or R) makes six random checkpoints, each defined by one number
with three neighboring I tiles in a rotated T shape. Roads connect all
six checkpoints. Entry and exit are distinct boundary cells connected to
the roads. Everything else is S. Regeneration starts a fresh run.
D restores the supplied default layout. Startup always uses that layout.
Malformed or missing map files fall back to the embedded supplied layout.

Placement centers snap every 50 units (half a cell). Every gem reserves
a 100 x 100 axis-aligned footprint regardless of the mesh shape. Touching
edges are allowed. Overlap and placements extending beyond the grid are
rejected. Imported models are uniformly scaled to 90 units wide, leaving
a little visual space inside their reserved footprints.

Left click empty ground to offer a random gem. After five offers, clicks
only select objects. Select an offered gem and press PLACE (P): it becomes
a tower, the other four become stone blocks, and the wave starts.
CLEAR GEMS (C) clears the board during the build phase so gems can be
re-placed. Left click gems, stones or enemies to inspect their live stats.
Green preview = valid; red = overlap, protected tile, or blocked route.
Road tiles R/checkpoints/E/X are enumerated once, from 1 to N, along the
E -> checkpoints 1..6 -> X journey, including required roads on branches.
I intersections remain traversable connectors, but do not need to be visited
and receive no debug index. The current board has 50 numbered required tiles.
Ground enemies take a shortest half-cell-grid path to each required tile in order;
snow/grass can be used for detours. Flying enemies fly directly between those same
required road targets. Each 2x2 road tile is excluded if a gem or rock overlaps any
of its 1x1 half-cell quadrants. Edge-only contact does not exclude a tile.
Indices stay stable while placing/removing objects. Checkpoints, E and X cannot
be excluded. A placement is invalid if a checkpoint OR any remaining mandatory
road tile becomes unreachable. I tiles may now be blocked like other road tiles.
Road debug numbers are visible by default: #N = required, red xN = excluded.
F6 toggles the overlay. Selecting an enemy shows its next required road index.
VerifyRoadOrder.py independently checks shortest legs, complete coverage, partial
obstruction, stone conversion, stable indices and rejection of enclosed targets.

Right drag rotates the camera; middle drag pans. RESET CAMERA / Home
restores its original angle and position. HUD scales to smaller windows.
Gold starts at one billion; U upgrades chance using the supplied table.
2 / 4 merges matching gems one / two grades higher. T opens recipes.
V upgrades a selected special tower; X removes a selected stone in build.
Twenty editable prototype waves, lives, score, targeting and modifiers run
in the combat phase. Fixed waves are defined in Content/Data/Waves.json.
Gem stats/modifiers/models are in Content/Data/Gems.json; classic special
recipes are in Content/Data/Recipes.json. Great is available through combining; random placement still uses the five-tier
chance table. Great currently uses the Perfect model; each definition has an
editable model path. F5 reloads data on an empty build board.
Tools/ImportTowerDefinitions.py imports the pinned Tower.m reference. It
overwrites the definitions, so do not run it after making balance edits.
Content/Data/ImportNotes.txt documents source repairs, units and editing.
The previous JavaScript extractor is historical and should not be rerun.

Art uses pastel base colors with three high-contrast lighting bands,
near-black object outlines and ink borders around road/intersection cells.
Snow/grass still blends softly only across S-S borders. Maze stones are
dark angular boulders with a broad flat base, embedded slightly into the
terrain, and sparse ink fracture marks.
assets/ArtPalette.json defines colors; assets/CelStyle.json controls band
strengths, lighting thresholds, outline width (cm) and road border width
(fraction of a tile). Run SetupProject.py to apply either file's changes.
assets/stone contains the Blender source, FBX and generator for maze stones.

Cel colors, shade bands and outlines are procedural, so their sharpness is
controlled by render resolution rather than a painted texture's resolution.
The game and editor render at 150% per axis (2.25x native pixel count) and
downsample, with FXAA and automatic/DPI resolution reductions disabled.
For lower GPU cost, set r.ScreenPercentage in Config/DefaultEngine.ini and
ManualScreenPercentage in Config/DefaultEditor.ini to 100. Restart the editor
after changing these defaults. S-S terrain blending remains intentionally soft.

Architecture: GemPrototype.h/.cpp contains the typed board, isometric
controller, placement logic, HUD and game mode. GemGameplay.cpp contains
rounds, pathfinding and combat; GemCameraHandler handles camera state.
Terrain uses seven instanced mesh batches. The camera fits around the HUD.

First-time rebuilding on another computer:
1. Install Unreal 5.8 and the Visual Studio C++ game development tools.
2. Build GemTowerDefenseEditor, Win64, Development.
3. Content assets are already generated. SetupProject.py can regenerate
   them through the Unreal Python commandlet if needed.

VerifyProject.py checks placement, numbered endpoints, 100 generated maps,
terrain materials and camera state through Unreal Python. Launch with
-GemSmokeTest to exercise route blocking, five-offer selection, stone
conversion, clear, merges, and enemies, and capture gameplay previews.
