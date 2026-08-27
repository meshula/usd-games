
"""Generate a standalone road-network / town scene draped on the island field.

Authors a ``/Island/Town`` department -- a ``BasisCurves`` road network with an
intersection graph and an instanced-box PointInstancer of buildings -- into a
three-layer USD scene (``roads``/``buildings`` sublayers composed into a
``town.usd*`` master).  It is a companion to ``generate_island.py``: because the
town is authored in island world coordinates and draped on the same
``IslandField``, its master can later be sublayered onto the island master as a
fourth department.

The road network is grown by the pure ``island_lib.roadnet`` citygen port and
authored as one linear basis curve per graph edge (per-class constant width),
alongside a ``Roads/Nodes`` points prim and per-curve ``startNode``/``endNode``
graph primvars -- a boid-ready traffic-sim substrate.  With ``--population
island`` the root is seeded at the most-populated point and a water gate keeps
roads on land.

The generator is fully deterministic: a given ``--seed`` (and the other
arguments) yields byte-identical ``.usda`` output across runs.

Examples:

    python3 generate_town.py --output ./out_town --population island
    python3 generate_town.py --output ./out_town --format usda --segment-limit 300
    python3 generate_town.py --population noise --seed 42
    python3 generate_town.py --highway-length 200 --street-length 100 --snap-distance 40
"""

import argparse
import os
import sys
import time

# Ensure the sibling island_lib package is importable regardless of CWD.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from island_lib import town


def _parse_args(argv=None):
    p = argparse.ArgumentParser(
        description="Generate a standalone town/road-network scene.")
    p.add_argument("--output", default="./output_town",
                   help="Output directory for the generated layers "
                        "(default: ./output_town).")
    p.add_argument("--format", choices=("usda", "usdc"), default="usdc",
                   help="Crate (usdc) or text (usda) for the generated "
                        "layers (default: usdc).")
    p.add_argument("--seed", type=int, default=1751,
                   help="RNG seed for deterministic placement (default: 1751).")
    p.add_argument("--size-km", type=float, default=2.0,
                   help="Island edge length in kilometres (default: 2.0).")
    p.add_argument("--segment-limit", type=int, default=2000,
                   help="Maximum number of road segments authored "
                        "(default: 2000).")
    p.add_argument("--population", choices=("noise", "island"), default="island",
                   help="Population source driving placement: the raster-backed "
                        "island heat map, or a field-free value-noise heat map "
                        "(default: island).")
    p.add_argument("--building-period", type=int, default=5,
                   help="Place buildings around every Nth road segment "
                        "(default: 5).")
    p.add_argument("--buildings-per-segment", type=int, default=10,
                   help="Building placement attempts per selected segment; each "
                        "is kept only if it clears roads/other buildings after "
                        "pushout (default: 10).")

    # Road-network tunables (a subset of roadnet.RoadNetworkParams).
    net = p.add_argument_group("road network")
    net.add_argument("--highway-length", type=float, default=160.0,
                     help="Target highway segment length in metres "
                          "(default: 160).")
    net.add_argument("--street-length", type=float, default=90.0,
                     help="Target street segment length in metres "
                          "(default: 90).")
    net.add_argument("--snap-distance", type=float, default=30.0,
                     help="Snap/intersection radius in metres (default: 30).")
    net.add_argument("--branch-angle-dev", type=float, default=3.0,
                     help="Branch angle deviation in degrees (default: 3).")
    net.add_argument("--straight-angle-dev", type=float, default=15.0,
                     help="Straight-continuation angle deviation in degrees "
                          "(default: 15).")
    net.add_argument("--min-intersection-dev", type=float, default=30.0,
                     help="Minimum direction difference (degrees) for an "
                          "intersection to be kept (default: 30).")
    net.add_argument("--normal-pop-threshold", type=float, default=0.15,
                     help="Population above which normal streets grow "
                          "(default: 0.15).")
    net.add_argument("--highway-pop-threshold", type=float, default=0.15,
                     help="Population above which highways branch "
                          "(default: 0.15).")
    net.add_argument("--road-drape-step", type=float, default=None,
                     help="Spacing (metres) at which each road edge is "
                          "subdivided for terrain draping (default: ~one "
                          "terrain quad).")
    net.add_argument("--road-grade-weight", type=float, default=2.0,
                     help="Terrain-following bias: how strongly arterials prefer "
                          "low-grade (isocline) continuations over steep ones "
                          "(0 = faithful citygen, pop-only; default: 2.0).")
    net.add_argument("--road-grade-cone", type=float, default=25.0,
                     help="Half-angle (degrees) the grade bias may steer a "
                          "continuation within (default: 25).")
    net.add_argument("--road-geom", choices=("curves", "ribbon", "both"),
                     default="curves",
                     help="Road geometry: boid-ready BasisCurves (default), "
                          "draped ribbon-quad meshes, or both.  Curves carry the "
                          "graph adjacency; ribbons are a visual road surface.")
    p.add_argument("--erosion-iterations", type=int, default=0,
                   help="Hydraulic-erosion iterations carved into the terrain the "
                        "roads drape on (0 = off; default: 0).")
    p.add_argument("--erosion-strength", type=float, default=1.0,
                   help="Scales the per-iteration incision rate (default: 1.0).")
    return p.parse_args(argv)


def main(argv=None):
    args = _parse_args(argv)
    for name, val in (("--segment-limit", args.segment_limit),
                      ("--building-period", args.building_period),
                      ("--buildings-per-segment", args.buildings_per_segment)):
        if val <= 0:
            print("error: {} must be positive".format(name), file=sys.stderr)
            return 2

    out_dir = os.path.abspath(args.output)
    print("Generating town scene:")
    print("  output               : {}".format(out_dir))
    print("  format               : {}".format(args.format))
    print("  seed                 : {}".format(args.seed))
    print("  size                 : {} km".format(args.size_km))
    print("  segment limit        : {}".format(args.segment_limit))
    print("  population           : {}".format(args.population))
    print("  building period      : {}".format(args.building_period))
    print("  buildings/segment    : {}".format(args.buildings_per_segment))
    print("  highway/street length: {} / {} m".format(
        args.highway_length, args.street_length))
    print("  snap distance        : {} m".format(args.snap_distance))
    print("  pop thresholds       : normal {} / highway {}".format(
        args.normal_pop_threshold, args.highway_pop_threshold))
    print("  road drape step      : {}".format(
        "auto" if args.road_drape_step is None else args.road_drape_step))
    print("  grade weight / cone  : {} / {} deg".format(
        args.road_grade_weight, args.road_grade_cone))
    print("  road geometry        : {}".format(args.road_geom))

    start = time.time()
    result = town.build_town_scene(
        output_dir=out_dir,
        fmt=args.format,
        seed=args.seed,
        population=args.population,
        size_km=args.size_km,
        segment_limit=args.segment_limit,
        building_period=args.building_period,
        buildings_per_segment=args.buildings_per_segment,
        highway_length=args.highway_length,
        street_length=args.street_length,
        snap_distance=args.snap_distance,
        branch_angle_dev=args.branch_angle_dev,
        straight_angle_dev=args.straight_angle_dev,
        min_intersection_dev=args.min_intersection_dev,
        normal_pop_threshold=args.normal_pop_threshold,
        highway_pop_threshold=args.highway_pop_threshold,
        road_drape_step=args.road_drape_step,
        grade_weight=args.road_grade_weight,
        grade_cone_dev=args.road_grade_cone,
        road_geom=args.road_geom,
        erosion_iterations=args.erosion_iterations,
        erosion_strength=args.erosion_strength,
    )
    elapsed = time.time() - start

    print("\nWrote layers:")
    for role in ("roads", "buildings", "master"):
        path = result[role]
        size = os.path.getsize(path) if os.path.exists(path) else 0
        print("  {:10s} {:>12,d} B  {}".format(role, size, path))
    print("\nSegments : {}".format(result["n_segments"]))
    print("Graph    : {} nodes, {} edges "
          "({} highways, {} streets)".format(
              result["n_nodes"], result["n_edges"],
              result["n_highways"], result["n_streets"]))
    print("Buildings: {}".format(result["n_buildings"]))
    print("Master stage : {}".format(result["master"]))
    print("Generation time: {:.2f} s".format(elapsed))
    return 0


if __name__ == "__main__":
    sys.exit(main())
