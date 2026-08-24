# Twisted by Dwarf Animation Studio

source: [ACM DL Siggraph Link -> https://dl.acm.org/doi/10.1145/3819990.3820025](https://dl.acm.org/doi/10.1145/3819990.3820025)

## Introduction

> Dwarf Animation Studio is producing Twisted, an 80-minute animated feature directed by Lino DiSalvo, on a 16-month timeline including pre-production. To meet this schedule without compromising creative ambition, we built a pipeline in which Unreal Engine 5.6 is the final rendering and compositing environment.

> This paper presents the technical systems we built to make this work: a multi-shot lighting workflow on nested Unreal Level Sequences with automatic shot group generation; a dual-format animation pipeline combining FBX and Alembic with per-asset tagging and a data-driven Blueprint architecture; a modified USD exporter that outputs environment data once per sequence with lightweight per-shot override files; a painterly look development pipeline built on custom shaders; and a performance strategy within a hard 16 GB VRAM limit on an RTX 5080.

> Our pipeline is built on Perforce for version control, ShotGrid for production tracking and automation, Unreal Engine 5.6 as the primary creative and rendering environment, and the ICTools framework (heavily customized) for asset publishing and build automation. Maya, Blender, Substance Painter, and Houdini handle asset creation. ... Environments live in Unreal Levels and SubLevels. Shots are assembled in nested Level Sequences. Overrides, animation, lighting, and post-process all live in Unreal's data structures. ... post-process materials, custom shaders, stylized look development, and what would traditionally be compositing work all live in Unreal. Artists see approximately 95% of the final image in the viewport in real-time. The remaining delta (final motion blur and full anti-aliasing) is confirmed at render time...

For a painterly looks silhouettes were broken up by duplicating geometry with vertex position and opacity broken up. A post processing pass introduced screen space distortion via brush shapes.

Depth of field was implemented via a Kuwahara post-pass filter. UVs were written to an AOV for use by painterly post passes where world space effects didn't hold up - since UVs are attached to surfaces as they move, they provide a stable anchor for affects in screen space, such as sampling brushes. Normals were quantized to break smooth surface lighting. Decals were used as projective texture lights.

Fur used a proprietary groom tool adapted to match UE's behavior. Only animated guides were persisted, relying on run time interpolation to populate the groom, and proceduralism to perform the groom.

Monolithic level files and local storage introduced artist friction, anxiety about Perforce pushes, losing work, and so on.

## USD Usage

Sequences were USD exported as a "Sequence Persistent Level", with individual shots exported as USD layers containing only the overrides and asset loading states for that shot.

### Sequence

A set of layers shared commonly amongst a set of shots

- Built from assets containing a bounding box and a usd payload.

### Shot

A temporally coherent bit of business taking place within a single sequence.

- Overrides on the sequence layers
- Asset oading states for the shot

### Asset

A single thing in a sequence

- has a low fidelity texture variant to defer needing to load all textures
- this helps keep a shot within physical GPU limitations, and also reduces load times
- texture and lighting data overwhelmed GPU memory long before geometry did. Keeping things in memory meant avoiding engine provided lighting, including bakes such as shadow maps and Lumen, as much as possible.

### Geometry Cache

> We benchmarked Alembic against Unreal geometry caches across asset types, deformation complexity, and shot lengths (measuring file size, VRAM cost, import time, and playback performance) using approximately 3000 Alembic files from our earlier production “My Dad the Bounty Hunter” as a test dataset. The results were not what we expected. When only a transform is animated with no mesh deformation, Unreal's geometry cache performs poorly: it duplicates the full mesh at every frame, producing a file larger than the equivalent Alembic, which in that case stores only the transform. Invert the situation (mesh deformation with no transform) and the geometry cache outperforms Alembic significantly, compressing deformation data without visual loss.

In Shot Grid, assets were tagged with the export strategy needed per asset.

### Department Layers

Dwarf implemented an emulation of USD's layered departmental pattern by repurposing UE's Sequencer as a layer composition system:

> ... Every shot has a master Level Sequence \[Hierarchy\]. Within it, each department owns a dedicated SubSequence (LSS): previs, layout, animation, FX, and lighting. Departments work exclusively within their own LSS. Each LSS carries a hierarchy bias (a priority value that determines which department's opinion wins on conflict). From lowest to highest: Previs → Layout → Animation → FX → Lighting. The composition is deterministic, and the mental model maps directly onto USD opinion layering for artists familiar with it. Layered override systems of this kind are broadly adopted across the industry. What we extended is the multi-shot lighting layer above it (the shot group workflow), running entirely within Unreal's native data structures, with no USD roundtrip cost or risk of losing native engine features on asset types. ... The system works, but we are already questioning whether the depth of nesting approaches will be reliably resolved by Unreal. Silent failures, where an override appears to apply but are beaten by a layer the artist cannot see, are the most expensive class of bug in a collaborative pipeline.

In practice, different departments put their work in their own layers due to workflow, and access and version control patterns; layout artists might put lights in the layout layer, confusing lighting artists who had no easy way to discover which layer was contributing lights.

Arguably this friction reflects the consequence of emulating a layered workflow in Sequencer, and would be more naturally addressed by introducing true compositional layering in UE without the use of Sequencer to accomoplish it; this would naturally enable reflection to USD backed layers and overrides.
