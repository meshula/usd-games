# Rendering a USD Scene from the Prototype DAG

This document describes the process a renderer must follow in traversing a USD composition on the way to bringing the description of a scene to a visual. The same principles apply to any system that must find its source data in a USD  composition, whether for visual rendering, audio, physical simulation and so on.

Hydra is OpenUSD's technology for rendering scenes from a USD composition; under the hood it follows the general pattern discussed here, however this document does not describe Hydra's architecture nor the many concerns of 
optimality and data management required to implement a high performance rendering engine.

This document refers to the output created by the tutorial program [`composition_tree.py`](../composition_tree.py). The tutorial program outputs a parseable *layer* tree and *prim / prototype* tree which comprise a map of the data required to find renderable geometry, and reason about prototypes and instances. The output can be read into a rendering program to be used as a rendering plan describing consolidated rendering batches and the order they must be drawn.

> Scope note: Hydra (and `UsdImaging`) already does all of this, robustly, with `HdInstancer`, draw-item batching, GPU-frustum culling, etc. Here we dissect the fundamental principles that lead to that structure. These foundations are intended to illuminate how to reason about and use acceleration structures in OpenUSD such as the `Usd` instancing API, `UsdGeomXformCache`, and `UsdGeomBBoxCache`.

The goal of this document is to demonstrate how to work out what should be uploaded to the GPU, and how many times, and how to group draw submissions so the GPU does the least redundant work.

Benchmark data, designed to stress the rendering path, is prepared at  [`../visual-check/`](../visual-check); in the discussion below we will refer to the composition tree tool output after it has processed the visual check data.

---

## 1. The structure of the render plan

| Tree (from `composition_tree.py`) | What it tells the renderer |
| --- | --- |
| **SECTION 1 — LAYERS** | Which files must be resolved and opened, and which are *instanceable* references (`[R,i]`). Instanceable arcs are the ones that will collapse many prims onto one shared GPU resource. |
| **SECTION 2 — PRIMS** | The composed scenegraph and the prototypes: the *masters* (`PN = ...`) are the unique things to upload once; the `->PN` use-sites are the transforms to batch. A `[* n]` master contains instances; it is a *node with children in the DAG*, so its transforms must be flattened before the full render plan can be known. |

---

## 2. From prims to GPU resources

There are three different roles for prims; and each role requires its own sort of optimization.

### 2a. Leaf prototypes: geometry uploaded once

A *leaf* prototype is a master whose subtree is only gprims — in the tool it is a `PN = ...` line. Note that these lines contain no `n` flag (as their are no further instances). Leaf prototypes real vertex data, and each is uploaded to the GPU exactly once as a vertex/index buffer, keyed by prototype path.

In the island scene the leaf geometry masters point to geometry payloads:

 -`Boat` (Hull, Mast), 
 - `Broadleaf` (Trunk, Canopy), 
 - `DecoratedPalm_*` (Trunk, Fronds), 
 - `Rock_*` (Geom), 
 - `MarketProp` (Stall, Proxy), plus the shared sub-parts 
     - `/hut/wall`, `/dock/plank`, `/foliage/leaf`, and
     -  the loose meshes `Roof`, `Mooring`, `Sea`, `Island` (terrain).

### 2b. Instances: transforms only, references to other geometry

An instance line in the output (e.g., `Boat_0 (Xform) [i ...] ->P0`)  carries no geometry. Each lines places an object - a transform plus a pointer (`->P0`) at a master. For the GPU it becomes one row in an *instance-transform buffer* attached to P0's draw. This is how we take advantage of a graphics' API's multidraw facilities; a single boat mesh is uploaded, along with one matrix to place each boat into the world.

### 2c. Nested prototypes: transform hierarchies to flatten

A master flagged `[* n]` contains instances of *other* masters. These are the DAG's internal nodes. All these nodes must be traversed in order to resolve the entire scene. The example scene contains this chain:

```
VillageBlock  [* n]        ->  Hut_*  [* n]        ->  /hut/wall  (leaf)
   (24 in scene)               (4 per block)            (4 per hut)
```

To render these nested hierachies, the tranforms must be uniquely accumulated down to the leaves.

### 2d. PointInstancers: ready-made instance arrays

A `PointInstancer` (grass / pebble / shell in our example island) already stores per-instance `positions`, `orientations`, `scales`, and `protoIndices`. That is an instance-transform buffer authored on disk. Each point's transform is composed with the instancer's own world transform, and the resulting array is used to drive an instanced draw.

A point instancer's prototypes prototypes can themselves be native-instanced assets, so 2c's flattening applies *inside* a PointInstancer as well. Those prototypes themselves may become PointInstancers - see 2f.

### 2e. The metadata

**Purpose flags**

Purpose flags appear in the output document; these are used to to draw or skip categories of gprims. A render builds batches of these together. The flags are as follows:

- `r` Render purposes - final quality drawing
- `p` proxy - a stand in drawable meant to simplify or clarify the viewport versus the full complexity of the render purpose.
- `g` guide - UI only elements.

- `e` an authored extent on a leaf node; used for visibility culling.
- `h` an extents hint; authored on a node so that a whole component can be culled without recursing into it.
- `t` time sampled transform and extent: these objects need their transforms re-flattened and uploaded per frame as they vary over time.

### 2f. Nested PointInstancers add a *population* cost, not just a draw cost

Everything else in this document is about *steady-state drawing*: given the
resolved DAG, upload masters once and batch the world transforms. But when a
PointInstancer's prototypes are *themselves* PointInstancers (a PI-of-PI, e.g., modules point-instanced into buildings, buildings into districts), a second cost appears before the first render, at population time.

The imaging stack's `UsdImagingPiPrototypePropagatingSceneIndex` *propagates* one prototype per `prototypes`-relationship **target**, and feeds each into an `HdMergingSceneIndex`. The propagated-prototype count is driven by the relationship

```
N = D*B + B*M + D*B*M # districts × buildings × modules; see the README
```

Each propagated prototype can trigger an `HdMergingSceneIndex` input-table
rebuild during stage open and teardown, so on moderately deep scenes this
dominates open/quit wall time. The companion `generate_city.py` scene targets this path;  `composition_tree.py` shows the nested PIs (each `PI(protos,points)` line, plus the synthetic proto-DAG nodes in the render plan) and `characterize_island.py` reports `N` and the PI-nesting depth. 

For further study, please refer to the OpenUSD sources, in particular:
`UsdImagingNiPrototypePropagatingSceneIndex::_MergingSceneIndexOperations`, an algorithm that computes the full set of instances at population time.

---

## 3. Prep phase — upload masters once

```
uploaded = {}                          # prototype path -> GpuMesh
for proto in stage.GetPrototypes():
    if proto_is_leaf(proto):           # no 'n' flag: subtree is only gprims
        for gprim in gprims_of(proto): # usually 1-2 meshes
            uploaded[gprim.path] = gpu_upload(points, indices, ...)
```

The island contains only a few dozen prototypes, these must be individually uploaded. The payoff of working with the DAG is that to cover tens of thousands of instanced objects, we need only upload dozens of objects.

---

## 4. Flattening the DAG gives the numbers that matter

`GetInstances()` (and the tool's prototype subtrees) report **authored** multiplicity — how many times a master is used *within its immediate parent
prototype*. The GPU needs the **flattened world** count — how many times it is drawn across the whole scene. They differ by the product of the counts along every path from the root.

A typical island scene might look like this:

| Leaf master | authored (`GetInstances`) | **world draws (flattened)** | why they differ |
| --- | ---: | ---: | --- |
| `/Assets/Boat` | 40 | **40** | used directly in the scene; no nesting |
| `/Assets/Rock_0` | 500 | **500** | direct scatter |
| `/dock/plank` | 5 | **30** | 5 planks × 6 docks |
| `/hut/wall` | 12 | **384** | 4 walls × (48+24+24 hut placements) |
| `/foliage/leaf` | 6 | **18,012** | 6 leaves × 3,002 FoliageClumps |
| `/Assets/Hut_0` | 2 | **48** | 2 per block × 24 VillageBlocks |

The flattening is a memoized propagation up the DAG:

```
world_count(P) = scene_uses(P)                         # instances of P in the main tree
               + Σ_Q  world_count(Q) * uses_of_P_within(Q)   # for every parent master Q
```

The same recursion that produces these counts also produces the transforms. Walking each path from root to leaf accumulating local matrices gives each levels local to world transform. When rendering the plan this must be done only once for static branches; it needs redoing per frame only for branches flagged with `t`.

> `UsdGeomBBoxCache`'s  `_PrototypeBBoxResolver` walks this same DAG to compute a bound once per prototype and then transforms it per instance. Render-prep flattening and bbox computation are the same traversal. The cache's parallel scheduling is on the render hot path.

---

## 5. Batching strategy

The batching rule follows from the principle in section 4:

> **One instanced draw call per (leaf master × material/purpose), with the flattened world transforms as its instance buffer.**

For a typical island's native geometry that is ~20 instanced draws covering ~30k placements. Refinements:

- **Split by purpose.** Keep `render` / `proxy` / `guide` in separate instance buffers so a mode switch is a batch-level include/exclude - don't test per-instance. The generator provides a number of these to exercise, for example, `MarketProp` contributes a `render` Stall *and* a `proxy` box; `Dock` contributes a `guide` Mooring.
- **Split static vs dynamic.** Static instance buffers are uploaded once;
  dynamic ones (the `t`-flagged boats, tide, swaying foliage) are re-uploaded per frame. Most of the scene is static, so per-frame upload is tiny.
- **PointInstancers are their own batches.** Grass (30,000), pebbles (15,000), shells (8,000) each become an instanced draw of their prototype mesh. Note the  compounding: the grass instancer's prototype is a `FoliageClump` (6 leaves), so grass alone is 30,000 × 6 = **180,000** leaf draws — handled as one  instanced draw of the leaf mesh with a 180k-row transform buffer built by composing point transforms with the intra-clump leaf transforms.
- **Cull before you submit.** Use section 2e's world AABBs to drop off-screen instances from each buffer. With `extentsHint` you can reject an entire component (and its whole sub-DAG) with one box, or bounding sphere test.

Sketch of the submission loop:

```
for (mesh, purpose), instances in batches.items():
    if purpose is disabled_this_pass: continue
    visible = [x for x in instances if aabb_in_frustum(x.world_aabb)]   # or GPU-side
    if not visible: continue
    bind(uploaded[mesh]); bind(material_for(mesh, purpose))
    upload_instance_buffer(visible)              # once if static, per-frame if dynamic
    draw_instanced(index_count(mesh), len(visible))
```

## Road and River Graphs

The Road and River graphs use `BasisCurves` to create a structural graph that can be traversed for wayfinding.

- **`curve` records — the road/river line graph.** Each `BasisCurves` curve becomes one `curve` record with its world-space control points (line strip),  a per-class colour (`highway` / `street` / `river`), a width, and the graph adjacency `startNode`/`endNode` (from `primvars:{roadnet,hydro}:{start,end}Node`, default `-1`). The adjacency indexes into the `point` set below, so the road and river networks are encoded as a drawable graph, not just loose polylines — a boid or routing consumer can read the topology straight from the plan.

- **`point` records — the graph nodes.** Each `Points` point (road junctions, river sources, confluences, outlets) becomes one `point` record carrying its `id` and world-space position, drawn as `GL_POINTS` or a billboard.

```
proto ... endproto            # unchanged; now also single-use hero-mesh protos
root <pid> <m16>              # unchanged; a hero mesh gets an identity root
curve <class> <r> <g> <b> <width> <startNode> <endNode> <nverts> <x0 y0 z0 ...>   # v2
point <class> <r> <g> <b> <id> <x> <y> <z>                                        # v2
```

Curve/point coordinates are **world-space** — the prim's local-to-world is already baked in. A consumer should draw them directly with no matrix stack: line strips for `curve`, points/billboards for `point`. 

The header advertises the totals:

`# protos=P roots=R curves=C points=Pt`.

> **Note — proto ids are not stable across stage opens (by design).** USD does  not guarantee a stable binding between an authored asset and its `/__Prototype_N` path. 
> Numbering is reassigned per stage open. Because the  plan keys proto ids off those paths, a given asset may land on a different `proto` id — and thus a different debug colour — from one run to the next, even for byte-identical input. This is intentional USD behaviour, not a bug in the plan... 
> What *is* deterministic is everything that matters to a consumer: the geometry, transforms, `curve`s, and `point`s are byte-stable across runs; only the integer *labeling* permutes. So a consumer (and any determinism check) must key on the records themselves — comparing, e.g., the sorted vertex positions/normals and the `curve`/`point` sections, never on the absolute proto ids.

---

## 6. Making the submissions efficient

- **Multi-Draw-Indirect (MDI).** Pack every batch's index counts, instance counts, and base offsets into indirect buffers and issue one (or a few) MDI calls. The per-batch structure from section 5 is intended for this.
- **GPU-driven culling.** Upload all world AABBs once; a compute pass writes the per-batch visible instance lists and the MDI counts. The CPU shouldn't touch per-instance visibility. (The AABBs are, again, the bbox-cache output.)
- **State sorting.** Order batches by pipeline/material to minimize bind churn; instancing already removes per-placement binds.
- **Parallel prep mirrors the prototype resolver.** Flattening independent
  sub-DAGs (rocks vs. huts vs. foliage) is embarrassingly parallel, exactly like resolving independent prototypes for bounds. A "finish-soon" scheduler that keeps each worker on one sub-DAG until done tends to beat fair-sharing here; this benchmark is partially intended to help prove or disprove this.

---

## 7. What Hydra does for you

The concepts above are not reinvented in a vacuum, and exist in Hydra's architecture.

| Concept here | Hydra / UsdImaging equivalent |
| --- | --- |
| Leaf master upload | `HdRprim` (e.g. `HdMesh`) + resource registry buffer aggregation |
| Instance transforms / flattening | `HdInstancer` (nested instancers compose transforms) |
| PointInstancer arrays | `UsdImagingPointInstancerAdapter` → `HdInstancer` |
| Per-batch instanced draw / MDI | draw items + draw batches (`HdSt_...DrawBatch`) |
| Purpose gating | render tags (`render`, `proxy`, `guide`) |
| World AABBs for culling | extent/`extentsHint` → `HdRenderPass` culling |

Reading `composition_tree.py`'s output is, in effect, reading the plan Hydra
builds internally.

---

## 8. Optimization possibilities

- Materials/shading are treated as an opaque "material per mesh"; real binding (UsdShade, texture residency, parameter buffers) is its own subject.
- Subdivision/refinement, skinning, and blendshapes are ignored — the benchmark geometry is deliberately trivial (boxes) because only *scene structure* matters for bbox/instancing.
- We assume static topology; only transforms/extents animate.
- Level-of-detail and payload streaming (deferring `[R,i]` loads) are out of
  scope but slot in naturally, per section 3.

---

## 9. Try it

```sh
# Draw the trees this document reads from:
python composition_tree.py --stage visual-check/island.usda \
   --output tree.txt

# In tree.txt:
#   * find a master by its '<- /Assets/...' label in the 'prototypes' list
#   * a [* n] master is a DAG node: its '->PN' children multiply through
#   * the counts in §4 are what you'd flatten those into for the GPU
```

The exercise: pick any `PN` master, read section 4's formula, and predict its world draw count from the tool's counts before checking it against the table.
