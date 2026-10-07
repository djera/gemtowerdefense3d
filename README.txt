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

20 x 20 grid, 100 Unreal units per cell. Maps/BoardLayout.txt holds the
fixed ASCII map: S=Snow/grass, R=Road, I=Intersection, E=Entry, X=Exit.
Numbers 1-6 are checkpoints, visited in numeric order between E and X.
First text line is grid row 0; each line runs along positive world X.
Intersections use darker road material. Checkpoints are road tiles with
gold circular markers. The supplied layout has 332 S, 41 road,
19 intersection, 6 checkpoint, one entry and one exit tile.
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
Every placement must preserve a path from E through all checkpoints to X.
Roads may be blocked to create a maze; endpoints/checkpoint Ts are protected.

Right drag rotates the camera; middle drag pans. RESET CAMERA / Home
restores its original angle and position. HUD scales to smaller windows.
Gold starts at one billion; U upgrades chance using the supplied table.
2 / 4 merges matching gems one / two grades higher. T opens recipes.
V upgrades a selected special tower; X removes a selected stone in build.
Twenty editable prototype waves, lives, score, targeting and modifiers run
in the combat phase. Fixed waves are defined in Content/Data/Waves.json.
Gem stats/modifiers/models are in Content/Data/Gems.json; classic special
recipes are in Content/Data/Recipes.json. Great definitions are retained
for reference; random rolls and ordinary merges use the five agreed grades.
Tools/ExtractClassicDefinitions.py extracts factual values from the saved
Classic reference source, with explicit conversion to Unreal units.

Art uses pastel colors and three lighting bands, plus object outlines.
assets/ArtPalette.json defines the colors used by SetupProject.py.
assets/stone contains the Blender source, FBX and generator for maze stones.

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
