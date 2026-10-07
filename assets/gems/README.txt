GEM ASSET PACK

40 FBX files: 8 gemstone types, each with Chipped, Flawed, Normal,
Flawless and Perfect quality. Five distinct shape designs, flat-shaded
facets, UV coordinates, bottom-tip origin, up to 20 cm wide and 23 cm tall.
Chipped: narrow jagged shard with a deeply broken outline.
Flawed: elongated rough hexagonal crystal with uneven faces.
Normal: wide oval faceted stone.
Flawless: square step cut with clipped corners.
Perfect: broad brilliant crown, thin girdle and long arrow-like pointed pavilion.
Perfect is shown tilted in the preview to reveal its point; exported meshes
remain upright along Z with their tip at the origin.
Perfect uses a stylized round brilliant cut, not a certified optical cut.
Materials are intentionally opaque for readable tower-defense gameplay.
Flawed quality uses uneven/chipped facets rather than internal crack textures.

UNREAL IMPORT
1. Open your Unreal project. Enable Python Editor Script Plugin and restart.
2. Tools > Execute Python Script: choose Unreal/import_gems.py.
   It imports meshes, builds materials and assigns baked opal textures.
3. In a C++ project, copy GemActor.h and GemActor.cpp into your game's
   Source/<Module>/ folder and compile. Create a Blueprint child of GemActor.
4. Place the actor and choose Type and Quality in Details. Runtime changes
   should use SetGem so the displayed mesh updates immediately.
   Content must stay under /Game/Gems for the supplied actor paths.

Blueprint-only alternative: create enums matching the manifest, an Actor
with a Static Mesh component and a map of Type+Quality to imported mesh.
In Construction Script select the mesh using Type and Quality.

SOURCE
GemLibrary.blend contains all 40 objects, preview lights and camera.
generate_gems.py recreates the pack with Blender 4.3.
Textures contains the five UV-baked opal color maps.
manifest.json contains palette, dimensions and polygon counts.

VALIDATION
Generator checks closed manifold edges, UV presence and 40 FBX exports.
Unreal integration source is provided but has not been compiled or run in
an Unreal project. Blender procedural shaders do not transfer through FBX;
use the included Unreal importer for the intended material appearance.
