# Exercising the full generator capabilities

The base `generate_island.py` script comprises a full procedural-terrain pipeline. A heightfield provides the base geometery, and it is evolved and decorated with:

  - hydraulic erosion, 
  - a wind-driven biome/moisture model,
  - a flow-accumulation river network, and 
  - a set of towns joined by a procedural road network

## The three generators

| generator | what it builds | what it exercises |
| --- | --- | --- |
| `generate_island.py` | the full tropical island. terrain, vegetation, village, optional erosion, rivers, and towns | a heavy transform hierarchy |
| `generate_town.py` | just the road network and buildings, draped on the island field | department oriented layering |
| `generate_city.py` | nested PointInstancers (modules contain buildings contain districts) | point intstancer prototype propagation, depuplication |

`characterize_island.py` and `composition_tree.py` work with any stage, not just compositions following the games intent.

---

## Generating an island with all features

This command demonstrates the full features set; erosion, a biome driving vegetation, rivers, roads and buildings:

```sh
python generate_island.py --output /tmp/island_full --format usda \
    --target-prims 8000 \
    --erosion-iterations 30 \
    --with-rivers \
    --with-town \
    --road-geom both \
    --wind-dir 315 --wind-strength 0.6
```

The heightfield drives the generation process, and various derivative maps are created to drive the procedural layout.

Verify it composed:

```sh
python -c "
from pxr import Usd
s = Usd.Stage.Open('/tmp/island_full/island.usda')
print([p.GetName() for p in s.GetPrimAtPath('/Island').GetChildren()])"
# -> ['Terrain', 'Vegetation', 'Hydrology', 'Town', 'Village']
```

---

## Feature stack

### Base island

Raster-backed terrain (an analytic displacement rasterized once into a 16-bit heightfield), density-driven vegetation scatter, the port-bay village, and animated boats/tide. Exports `heightmap.png` + `populationmap.png`.

```sh
python generate_island.py \
    --output /tmp/base --format usda --target-prims 8000
```

### Hydraulic erosion — `--erosion-iterations N [--erosion-strength F]`

Runs deterministic grid erosion. A priority-flood fill is followed by D8/MFD flow accumulation modeling stream-power incision and thermal and talus smoothing. The height delta is added back to the field.

A strength of 0 (default) disables erosion. A strength of ~`30` gives ~3 m mean / ~15 m max incision. Subsequent "departments" (terrain mesh, vegetation placement, rivers, town draping) follow the new valleys.

```sh
python generate_island.py \
    --output /tmp/eroded --format usda  --target-prims 8000 \
    --erosion-iterations 30
```

### Wind, moisture & biome — `--wind-dir DEG --wind-strength F`

Wind and moisture are are always active, and shape the biome. The defaults are `315` (NW) and `0.6`.

`Moisture = elevation-dryness × orographic wind bias (windward wet / leeward rain shadow) × noise`

A Whittaker-style elevation and moisture lookup assigns a
discrete biome with these zones:

- beach
- wet_forest
- lowland
- dry_scrub
- montane
- dry_slope and alpine
- riparian near rivers.

Vegetation scatter weights species by biome affinity (palms in wet lowland/beach, rocks on dry/alpine slopes) and scales instance size by moisture. 

The terrain mesh carries a biome visualization by mapping the generatated biome map as a texture input to a `UsdPreviewSurface` material.

```sh
python generate_island.py \
    --output /tmp/eroded --format usda  --target-prims 8000 \
    --wind-dir 45 --wind-strength 0.9
```

### Rivers — `--with-rivers`

This option authors a directed `BasisCurves` channel network. Nodes locate sources, confluences, and coastal outlets. The width of the rivers is controlled by Strahler order.

In addition to the `BasisCurves`, a per-edge translucent-blue water-ribbon mesh is generated. Vegetation is pruned from channels.

Combine the rivers feature with erosion so the water has carved valleys to flow into. The wetness index subsequently feeds biome moisture; although wetness always exists, this flag introduces rivers.


```sh
python generate_island.py \
    --output /tmp/rivers --format usda --target-prims 8000 \
    --erosion-iterations 30 --with-rivers
```

The scene layout is organized like this:

```
/Island/Hydrology/Rivers
  /Nodes     (Points — sources, confluences, and outlets)
  /Channels  (BasisCurves — directed, width by Strahler order)
  /Water     (per-edge draped water-ribbon meshes)
```

### Town / road network — `--with-town [--road-geom {curves,ribbon,both}]`

This stage runs the citygen road network and composes it as an `/Island/Town` "department".

```
/Island/Town
  /Roads/Nodes     (Points — graph junctions, int ids)
  /Roads/Highways  (BasisCurves linear, per-curve startNode/endNode primvars)
  /Roads/Streets   (BasisCurves linear, per-curve startNode/endNode primvars)
  /Roads/Ribbon    (per-edge Highway/Street ribbon meshes; --road-geom ribbon|both)
  /Buildings       (PointInstancer of boxes, population-gated, pushed off roads)
```

`--road-geom` selects the road representation:

- `curves` (default) — boid-ready `BasisCurves` carrying the intersection graph.
- `ribbon` — per-edge draped ribbon-quad meshes, bridged over water.
- `both` — curves (the graph substrate) *and* ribbons (the visual surface).

```sh
python generate_island.py \
    --output /tmp/island_full --format usda --target-prims 8000 \
    --with-town --road-geom both
```

---

## Standalone town (`generate_town.py`)

Grows just the road network and buildings, draped on the same island field. Useful for iterating on road parameters without regenerating the whole island. Roads are pruned to keep them on land.

```sh
python generate_town.py --output /tmp/town_demo --format usda \
    --population island --segment-limit 400 --erosion-iterations 20 --road-geom both
```

Road-network parameters:

| flag | default | meaning |
| --- | --- | --- |
| `--segment-limit N` | `2000` | max road segments grown |
| `--population {noise,island}` | `island` | placement heat map (island = raster-backed, water-gated) |
| `--highway-length / --street-length F` | `160 / 90` | target segment length (m) |
| `--snap-distance F` | `30` | snap / intersection radius (m) |
| `--branch-angle-dev / --straight-angle-dev F` | `3 / 15` | branch / continuation angle deviation (deg) |
| `--min-intersection-dev F` | `30` | min direction difference to keep an intersection (deg) |
| `--normal-pop-threshold / --highway-pop-threshold F` | `0.15 / 0.15` | population above which streets grow / highways branch |
| `--road-grade-weight F` | `2.0` | terrain-following bias (arterials hug isoclines; `0` = pop-only citygen) |
| `--road-grade-cone F` | `25` | half-angle the grade bias may steer within (deg) |
| `--road-drape-step F` | auto | edge subdivision spacing for terrain draping (m) |
| `--building-period N` | `5` | place buildings around every Nth segment |
| `--buildings-per-segment N` | `10` | placement attempts per selected segment (kept if clear) |
| `--road-geom {curves,ribbon,both}` | `curves` | road geometry representation |
| `--erosion-iterations N` / `--erosion-strength F` | `0 / 1.0` | carve the terrain the roads drape on |

---

## City (`generate_city.py`) — nested PointInstancers

This script exercises nested point instancers to demonstrate how instances are propagated and merged. The number of propagated prototypes, `N`, (propagated prototypes) is driven by the `prototypes` relationship:

`N = D*B + B*M + D*B*M`.

```sh
python3 generate_city.py --output /tmp/city_demo --format usda -M 10 -B 12 -D 3
# -> authored pairs 156, propagated prototypes N = 516
```

See the [README](../README.md#city--nested-point-instancing) for the full flag table and cost model.

---

## Verifying a stage

### Characterize

Run without options, characterize will compute various statistics and give a scene a grade as to its usefulness as a benchmark. **PASS* indicates a good workload, other grades areas of potential improvement.

```sh
python3 characterize_island.py --stage /tmp/island_full/island.usda
```

### Composition tree (annotated LAYERS + PRIMS dump)

```sh
python3 composition_tree.py --stage /tmp/island_full/island.usda \
    --output /tmp/tree.txt
    
python composition_tree.py --stage /tmp/island_full/island.usda \
    --max-lines 200
```

Adding `--render-plan out.lglcap` also writes the machine-parseable render plan including primvar arrays ready for GPU binding.

---

## Raster artifacts

Every run exports 16-bit/8-bit PNGs useful for artistic touch up or repainting.

| file |  content |
| --- | --- |
| `heightmap.png` |  the (optionally eroded) terrain heightfield |
| `populationmap.png` | town-placement heat map (empty over water, dense at the bay) |
| `moisturemap.png` | wind-driven moisture |
| `biomemap.png` | discrete biome classification (also copied to `mat/terrain_biome.png` for the terrain material) |
| `wetnessmap.png` | channel-proximity and drainage wetness |

---

## Scene layer stack (full island)

```
island.usda   (subLayers = [village, town, hydrology, vegetation, terrain]; defaultPrim /Island)
├── village.usda      /Island/Village    block instances, docks, animated boats, market props, nested-PI flower beds
├── town.usda         /Island/Town       (subLayers = [buildings, roads]) road-curve graph + ribbons + building PI
├── hydrology.usda    /Island/Hydrology  directed river curve network + water ribbons
├── vegetation.usda   /Island/Vegetation biome-weighted instanceable scatter + PointInstancers
├── terrain.usda      /Island/Terrain    km-scale eroded meshes + biome material + animated sea plane
└── assetLibrary.usda  → geo/*.usda      component models referencing raw geometry
```
