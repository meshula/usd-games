
"""Generate the synthetic tropical-island bbox benchmark scene.

Produces a five-layer USD scene (asset library + terrain/vegetation/village
sublayers composed into a master ``island.usd*``) designed to exercise the
parallel code paths in ``UsdGeomBBoxCache``: nested native instancing,
PointInstancers, ``extentsHint`` model short-circuits, purpose inheritance
through instances, time-varying extents/xforms, and deep xform hierarchies.

The generator is fully deterministic: a given ``--seed`` (and the other
arguments) yields byte-identical ``.usda`` output across runs.

Examples:

    python3 generate_island.py --output ./output --target-prims 10000
    python3 generate_island.py --target-prims 100000 --format usdc
    python3 generate_island.py --seed 42 --animated-fraction 0.2 --frames 48
    python3 generate_island.py --with-town     # + /Island/Town road network

With ``--with-town`` the procedural road network + buildings are authored as a
fourth ``/Island/Town`` department and sublayered into the master, giving a full
island demonstrator (terrain + vegetation + village + town).
"""

import argparse
import os
import sys
import time

# Ensure the sibling island_lib package is importable regardless of CWD.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from island_lib import layers


def _parse_args(argv=None):
    p = argparse.ArgumentParser(
        description="Generate the tropical-island bbox benchmark scene.")
    p.add_argument("--output", default="./output",
                   help="Output directory for the generated layers "
                        "(default: ./output).")
    p.add_argument("--format", choices=("usda", "usdc"), default="usdc",
                   help="Crate (usdc) or text (usda) for the generated "
                        "department/master layers (default: usdc). The asset "
                        "library is always written as usda.")
    p.add_argument("--target-prims", type=int, default=50000,
                   help="Approximate authored prim count; primarily scales the "
                        "native-instance scatter counts (default: 50000).")
    p.add_argument("--size-km", type=float, default=2.0,
                   help="Island edge length in kilometres (default: 2.0).")
    p.add_argument("--seed", type=int, default=1751,
                   help="RNG seed for deterministic placement (default: 1751).")
    p.add_argument("--animated-fraction", type=float, default=0.1,
                   help="Fraction of scattered prims that receive time-sampled "
                        "sway (default: 0.1).")
    p.add_argument("--frames", type=int, default=96,
                   help="Number of animation frames authored (default: 96).")
    p.add_argument("--with-town", action="store_true",
                   help="Also author the /Island/Town department (procedural "
                        "road network + buildings, --population island) and "
                        "sublayer it into the master as [village, town, "
                        "vegetation, terrain].")
    p.add_argument("--wind-dir", type=float, default=315.0,
                   help="Prevailing wind direction in degrees (the windward, "
                        "wetter side faces toward it; the leeward side is a rain "
                        "shadow).  Shapes the biome/moisture map (default: 315).")
    p.add_argument("--wind-strength", type=float, default=0.6,
                   help="Strength in [0, 1] of the windward/leeward moisture "
                        "asymmetry (default: 0.6).")
    p.add_argument("--with-rivers", action="store_true",
                   help="Author the /Island/Hydrology river curve network "
                        "(flow-accumulation rivers, width by Strahler order) and "
                        "prune vegetation off the channels.  (Wetness always "
                        "feeds the biome moisture regardless of this flag.)")
    p.add_argument("--erosion-iterations", type=int, default=0,
                   help="Hydraulic-erosion iterations to carve valleys into the "
                        "heightfield before anything else samples it (0 = off; "
                        "~30 gives visible valleys). All departments follow.")
    p.add_argument("--erosion-strength", type=float, default=1.0,
                   help="Scales the per-iteration incision rate (default: 1.0).")
    p.add_argument("--road-geom", choices=("curves", "ribbon", "both"),
                   default="curves",
                   help="Town road geometry: boid-ready BasisCurves (default), "
                        "per-edge draped ribbon meshes (bridged over water), or "
                        "both.  Only meaningful with --with-town.")
    return p.parse_args(argv)


def main(argv=None):
    args = _parse_args(argv)
    if args.target_prims <= 0:
        print("error: --target-prims must be positive", file=sys.stderr)
        return 2
    if not (0.0 <= args.animated_fraction <= 1.0):
        print("error: --animated-fraction must be in [0, 1]", file=sys.stderr)
        return 2

    out_dir = os.path.abspath(args.output)
    print("Generating island scene:")
    print("  output           : {}".format(out_dir))
    print("  format           : {}".format(args.format))
    print("  target prims     : {}".format(args.target_prims))
    print("  size             : {} km".format(args.size_km))
    print("  seed             : {}".format(args.seed))
    print("  animated fraction: {}".format(args.animated_fraction))
    print("  frames           : {}".format(args.frames))
    print("  with town        : {}".format(args.with_town))
    print("  with rivers      : {}".format(args.with_rivers))
    print("  wind dir/strength: {} deg / {}".format(
        args.wind_dir, args.wind_strength))
    print("  erosion iters/str: {} / {}".format(
        args.erosion_iterations, args.erosion_strength))

    start = time.time()
    paths = layers.build_scene(
        output_dir=out_dir,
        fmt=args.format,
        target_prims=args.target_prims,
        size_km=args.size_km,
        seed=args.seed,
        animated_fraction=args.animated_fraction,
        frames=args.frames,
        with_town=args.with_town,
        wind_dir_deg=args.wind_dir,
        wind_strength=args.wind_strength,
        with_rivers=args.with_rivers,
        erosion_iterations=args.erosion_iterations,
        erosion_strength=args.erosion_strength,
        road_geom=args.road_geom,
    )
    elapsed = time.time() - start

    roles = ["library", "terrain", "vegetation", "village"]
    if args.with_town:
        roles += ["town_roads", "town_buildings", "town_master"]
    if args.with_rivers:
        roles += ["hydrology"]
    roles += ["master", "heightmap", "populationmap", "moisturemap", "biomemap",
              "wetnessmap", "terrain_texture"]
    print("\nWrote layers:")
    for role in roles:
        path = paths.get(role)
        if not path:
            continue
        size = os.path.getsize(path) if os.path.exists(path) else 0
        print("  {:14s} {:>12,d} B  {}".format(role, size, path))
    print("\nMaster stage : {}".format(paths["master"]))
    print("Generation time: {:.2f} s".format(elapsed))
    return 0


if __name__ == "__main__":
    sys.exit(main())
