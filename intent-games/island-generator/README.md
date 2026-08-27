# Island Generator Scripts

This set of scripts are procedural generators, and analysis tools, built
to deterministically and reproducibly create instancing-heavy assets that
demonstrate the Games Intent.

These scripts are structured to realistically demonstrate how large scale
environments may be constructed and traversed, and should provide a
realistic stress test for USD-game engine integrations.

## Acknowledgment

The implementation of the road network was partially created by porting [`https://github.com/t-mw/citygen-godot`](`https://github.com/t-mw/citygen-godot`), by Tobias Mansfield-Williams. An explanation of the algorithm and an interactive demonstration may be found here: ['https://www.tmwhere.com/city_generation.html']('https://www.tmwhere.com/city_generation.html'). 

## Navigating the code

- ['island_lib/']('island_lib/') contains all the procedural bits, asset constructors, island geology generation, road layout and so on.
  - geo.py - raw geometry files (geo/<component>.usda) + intra-geo instancing
  - assets.py - component models: /Assets/* referencing geo + VillageBlock assembly
  - scatter.py - seeded density-driven placement + island and bay heightfield
  - layers.py - sublayer/master assembly, purposes, extentsHint, animation
  - city.py - nested point-instancing city (modules -> buildings -> districts)

- ['generate_island.py']('generate_island.py') creates a tropical island with a 1km radius and a parametric complexity level can be used to target a few prims to millions. It has:
  - village in a port bay
  - heavy native instancing
  - point instancers
  - `extentsHint`
  - purpose
  - animation (water moves, trees move)
  - boats
-['generate_city.py']('generate_city.py') creates a city on the island using nested point instancing.
  - modules contain districts, which contain buildings
- ['generate_town.py`]('generate_town.py') creates a much more complex population of the island
  - road network included bridges over rivers and streams
- ['characterize_island.py']('characterize_island.py') analyzes a generated island and outputs numerous statistics
  - prim counts
  - imageable prim counts
  - instances of all varieties
  - and so on
- ['composition_tree.py']('composition_tree.py') analyzes a generated island and outputs a rendering plan
  - the prototype layers
  - the instances and where to draw them, in what order

## Some useful detail

Some features the island is meant to exercise:

### Nested native instancing and a prototype-dependency DAG

Geo files instance repeated elements, e.g.:
  * `hut` -> walls
  * `dock`-> planks, 
  * `foliage`-> leaves, 

Kinds are exercised in a hierarchy of prototypes:
  * `VillageBlock` (assembly) references `Hut`
  * `Hut` components are `instanceable` and them selves instance `wall` components.

A typical generated island will have tens of thousands of `PointInstancers` to instantiate grass, pebbles, and shells

The shops have nested point instancers, the shop instances have flower beds which have a non-instanceable bed-instancer that contains flower point instancers.

Component model roots have an authored `extentsHint`

Purpose inheritance through instancing is exercised: a fraction of instances author `purpose` (`proxy`); geo meshes carry `render`, `proxy`, or `guide`.

Time-varying extents and xforms are demonstrated.
  * Animated boats (bob + drift), 
  * tide (sea surface translates), and 
  * a fraction of the foliage sways.

The transform hierarchies are very deep.

## The generated Scene

The scene follows the games intent structure.
  - models carry their geometry in a payload
  - layer structure is a hybrid, department-sublayer composition:
    - `geo/<component>.usda` — raw geometry files (one per *logical* unit: palm, broadleaf, rock, hut, boat, dock, market, foliage). Simple box meshes, but repeated elements are `instanceable` internal references to an in-file `class` master (hut walls, dock planks, foliage leaves).
    - `assetLibrary.usda` — thin *model interfaces*: `/Assets/<Component>` prims tagged with `kind`/`extentsHint` that **reference** their geo file.
      - `VillageBlock` is an `assembly` that references `instanceable` `Hut` components
    - Per-department sublayers reference the `instanceable` library assets, composed into the master `island.usd*`

```
island.usda   (subLayers = [village, vegetation, terrain]; defaultPrim /Island)
├── village.usda      /Island/Village    assembly instances (blocks), docks, animated boats, market props, nested-PI flower beds
├── vegetation.usda   /Island/Vegetation bulk instanceable scatter + PointInstancers
├── terrain.usda      /Island/Terrain    km-scale unique meshes + animated sea plane
└── assetLibrary.usda  -> geo/*.usda      component models referencing raw geometry
```

## Usage (island)

The city generator's CLI is documented in its own section below; the shared `characterize_island.py` / `composition_tree.py` invocations shown here apply to both.

Generate (all arguments have deterministic defaults):

```sh
# Default: 50k-prim target, usdc, 2 km, 96 frames, 10% animated
python3 generate_island.py --output ./output

# Smaller/text for inspection or determinism checks
python3 generate_island.py --output ./output --target-prims 10000 --format usda

# Scale up
python3 generate_island.py --output ./output --target-prims 100000
```

| flag | default | meaning |
| --- | --- | --- |
| `--output DIR` | `./output` | output directory |
| `--format {usda,usdc}` | `usdc` | department/master layer format (library is always `usda`) |
| `--target-prims N` | `50000` | approximate authored prim count; primarily scales native-instance scatter |
| `--size-km F` | `2.0` | island edge length in km |
| `--seed N` | `1751` | RNG seed (same seed ⇒ byte-identical `.usda`) |
| `--animated-fraction F` | `0.1` | fraction of scattered prims that get time-sampled sway |
| `--frames N` | `96` | animation frame count |

The island is grown as a full procedural-terrain pipeline on top of the base scene and shows off
  — hydraulic erosion, 
  - a wind-driven biome/moisture model, 
  - a river network (`--with-rivers`), and 
  - a procedural road network + town

  All of these are draped on the heightfield, and controlled by flags
    - `--erosion-iterations`, `--wind-dir/--wind-strength`, `--with-rivers`, `--with-town`, `--road-geom`), the standalone `generate_town`, 

Refer to [`docs/generator-capabilities.md`](docs/generator-capabilities.md) for more details.

Characterize:

```sh
python3 characterize_island.py --stage ./output/island.usda
python3 characterize_island.py --stage ./output/island.usda --json report.json

# Calibrate the targets against shipped reference assets
python3 characterize_island.py --stage ./output/island.usda \
    --compare /path/to/alab.usd /path/to/moorelane.usd /path/to/kitchen.usd
```

The characterizer exits non-zero if the primary stage **FAIL**s the targets or the smoke bbox check fails, so it can gate CI.

Inspect the composition as an annotated text tree (a worked-example tutorial of the Sdf layer API and the Usd instancing/prototype API):

```sh
python3 composition_tree.py --stage ./output/island.usda --output tree.txt
python3 composition_tree.py --stage ./output/island.usda --max-lines 200
```

It writes two sections: **LAYERS** (the file-level composition graph — subLayers and reference/payload arcs, scoped to what each referenced prim actually pulls in, with instance counts) and **PRIMS** (the composed scenegraph plus each prototype drawn as its own subtree, so the prototype-dependency DAG is visible).

Each line carries single-character flags (legends are emitted in the file); `--max-lines N` caps lines per section for large scenes.

For a worked example of how that prototype DAG maps onto GPU render-prep — what to upload once, how to flatten nested instancing into world-space draw counts, and how to multi-batch submissions — see [`docs/rendering-from-the-dag.md`](docs/rendering-from-the-dag.md).

## City / nested point-instancing

`generate_city.py` targets a **different subsystem** from the island: the population-time propagation and merge the imaging stack performs when a scene nests PointInstancers inside PointInstancers. It reproduces a simple city, built by point-instancing three tiers:

- **M modules** — box `Mesh` prims in an invisible library.
- **B buildings** — `PointInstancer`s (also in an invisible library), each
  listing *all* M modules as its `prototypes`.
- **D districts** — one prototypical district (a `PointInstancer` over all B
  buildings) plus D-1 internal *references* to it, each with a translate.

The library `def`s live in a sublayer (`cityLibrary.usda`); the master (`city.usda`) carries library-root `over`s with `visibility = "invisible"` plus the districts.

When dealing with nested point instancers, Hydra (in `UsdImagingPiPrototypePropagatingSceneIndex`) creates one propagated prototype per
`prototypes` **relationship target**, feeding them into `HdMergingSceneIndex`.

The count of propagated prototypes `N` (the merging-scene-index inputs, and the consequent population/teardown cost) is:

```
N = D*B          (each district instancer over the shared building library)
  + B*M          (each building instancer over the shared module library)
  + D*B*M        (each propagated building copy re-propagating its modules)
```

Example set ups:

```sh
# Defaults: M=25, B=50, D=5  ->  N = 7750
python3 generate_city.py --output ./output_city

# Scale the relationship (N) independently of the drawn point counts
python3 generate_city.py --output ./output_city -M 100 -B 50 -D 5     # N = 30,250
python3 generate_city.py --output ./output_city -M 3 -B 4 -D 2        # N = 44

# Characterize (grades N and PI-nesting depth) and dump the nested-PI DAG
python3 characterize_island.py --stage ./output_city/city.usda
python3 composition_tree.py --stage ./output_city/city.usda --output city_tree.txt
```

| flag | default | meaning |
| --- | --- | --- |
| `--output DIR` | `./output_city` | output directory |
| `--format {usda,usdc}` | `usdc` | master layer format (library is always `usda`) |
| `--modules / -M N` | `25` | distinct module meshes in the library (drives `N`) |
| `--buildings / -B N` | `50` | distinct building instancers in the library (drives `N`) |
| `--districts / -D N` | `5` | districts: 1 def + D-1 references to it (drives `N`) |
| `--modules-per-building N` | `1000` | module *instances* per building (point count; not `N`) |
| `--buildings-per-district N` | `100` | building *instances* per district (point count; not `N`) |
| `--instanceable` | off | mark district refs instanceable -> native instancing instead (changes what is tested; `N` formula no longer applies) |
| `--draw-mode {bounds,cards,origin,default}` | `bounds` | `model:drawMode` on modules + building instancers |
| `--seed N` | `0` | RNG seed for `protoIndices` (same seed ⇒ byte-identical `.usda`) |



## Determinism

The generators use only explicitly-seeded `random.Random` and index-derived values. The same arguments therefore always produce  byte-identical* `.usda` output:

```sh
python3 generate_island.py --output ./a --target-prims 10000 --format usda
python3 generate_island.py --output ./b --target-prims 10000 --format usda
diff -r ./a ./b   # no differences

python3 generate_city.py --output ./c --format usda -M 5 -B 6 -D 3
python3 generate_city.py --output ./d --format usda -M 5 -B 6 -D 3
diff -r ./c ./d   # no differences
```

Note that `composition_tree.py`'s *render-plan* proto **ids** for the island are not stable across runs, because USD's `/__Prototype_N` prototype names aren't; the plan's geometry and root set are. The city has no native prototypes, so its render plan is fully stable.

## Generator Notes

The island terrain is raster-backed: the analytic displacement is rasterized once into a fixed-range 16-bit heightfield (`IslandField`), and `height(x, y)` samples that grid with bilinear (default) or bicubic interpolation. The generator also exports the grid as an inspectable `heightmap.png` artifact.

Pillow (PIL) is used to export it.

A population heat map is derived from that same heightfield (`IslandPopulation` in `island_lib/population.py`). It uses a decorrelated fractal noise field gated by a smooth-stepped filter of the terrain height. It's biased to be empty under the ocean, denser around the harbour bay, and fades out toward the peaks. It is raster-backed and exported as `populationmap.png`.

A future follow up would be to allow these maps to be artist painted and passed in to the generator.
