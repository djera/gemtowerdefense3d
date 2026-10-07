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
fixed ASCII map: S=Snow, G=Grass, R=Road, I=Intersection, C=Checkpoint.
First text line is grid row 0; each line runs along positive world X.
Intersections use darker road material. Checkpoints are road tiles with
gold circular markers. The supplied layout has 332 S, 43 road,
19 intersection and 6 checkpoint tiles. S uses snow or grass in patches.
The saved level includes the board so terrain is visible in the editor.
The sidebar displays the live 20x20 ASCII array.
REGENERATE (or R) makes five random checkpoints, each defined by one C
with three neighboring I tiles in a rotated T shape. Roads connect all
five checkpoints. Everything else is S. Regeneration clears placed gems.
D restores the supplied default layout. Startup always uses that layout.
Malformed or missing map files fall back to the embedded supplied layout.

Placement centers snap every 50 units (half a cell). Every gem reserves
a 100 x 100 axis-aligned footprint regardless of the mesh shape. Touching
edges are allowed. Overlap and placements extending beyond the grid are
rejected. Imported models are uniformly scaled to 90 units wide, leaving
a little visual space inside their reserved footprints.

1–8 selects gem type; left click places, right click removes.
Green preview = valid; red preview = overlapping another footprint.
This template uses Perfect gems to demonstrate placement.

The right sidebar is reserved for future controls. The header contains
placeholder score, wave and lives. No enemies, waves or combat yet.

Architecture: GemPrototype.h/.cpp contains the typed board, isometric
controller, placement logic, HUD and game mode. Terrain uses four instanced
mesh batches. The camera automatically fits the board around the HUD.

First-time rebuilding on another computer:
1. Install Unreal 5.8 and the Visual Studio C++ game development tools.
2. Build GemTowerDefenseEditor, Win64, Development.
3. Content assets are already generated. SetupProject.py can regenerate
   them through the Unreal Python commandlet if needed.

VerifyProject.py exercises grid boundaries, half-cell snapping, overlap,
edge-touching, removal and tile type generation through Unreal Python.
