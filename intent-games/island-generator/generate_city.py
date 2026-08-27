
"""Generate a nested point-instancing "city" scene.

The city is built by point-instancing M modules into B buildings and B buildings
into D districts, with D-1 districts *referenced* from a prototypical one.  It is
a companion to ``generate_island.py``: the island stresses ``UsdGeomBBoxCache``,
while the city stresses the imaging stack's PI-prototype propagation and merge
(``UsdImagingPiPrototypePropagatingSceneIndex`` + ``HdMergingSceneIndex``).

The number of propagated prototypes -- the cost the propagation/merge scales
against -- is driven by the ``prototypes`` *relationship*, not by point count:

    N = D*B + B*M + D*B*M

so ``--modules-per-building`` and ``--buildings-per-district`` do not affect it.
See ``island_lib/city.py`` for the full rationale.

Examples:

    python3 generate_city.py --output ./out_city                 # N = 7750
    python3 generate_city.py --output ./out_city -B 100          # N = 15,500
    python3 generate_city.py --output ./out_city -M 3 -B 4 -D 2  # N = 44
"""

import argparse
import os
import sys
import time

# Ensure the sibling island_lib package is importable regardless of CWD.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from island_lib import city


def _parse_args(argv=None):
    p = argparse.ArgumentParser(
        description="Generate a nested point-instancing city scene.")
    p.add_argument("--output", default="./output_city",
                   help="Output directory for the generated layers "
                        "(default: ./output_city).")
    p.add_argument("--format", choices=("usda", "usdc"), default="usdc",
                   help="Crate (usdc) or text (usda) for the master layer "
                        "(default: usdc). The library is always written usda.")
    p.add_argument("-M", "--modules", type=int, default=25,
                   help="Mesh prims in the module library. Every building "
                        "instancer lists the entire library as its prototypes, "
                        "even prototypes it never instances (default 25).")
    p.add_argument("-B", "--buildings", type=int, default=50,
                   help="PointInstancer prims in the building library. Every "
                        "district instancer lists the entire library as its "
                        "prototypes (default 50).")
    p.add_argument("-D", "--districts", type=int, default=5,
                   help="districts: 1 def + D-1 references to it (default 5).")
    p.add_argument("--modules-per-building", type=int, default=1000,
                   help="module *instances* each building draws (its point "
                        "count). Not the distinct module count -- that is -M. "
                        "Default 1000.")
    p.add_argument("--buildings-per-district", type=int, default=100,
                   help="building *instances* each district draws (its point "
                        "count). Not the distinct building count -- that is -B. "
                        "Default 100.")
    p.add_argument("--instanceable", action="store_true",
                   help="mark the district references instanceable, routing "
                        "them through native instancing instead (changes what "
                        "is tested -- see island_lib/city.py).")
    p.add_argument("--draw-mode", default="bounds",
                   choices=("bounds", "cards", "origin", "default"),
                   help="model:drawMode on the modules and building instancers, "
                        "via GeomModelAPI. The real scenes author 'bounds' (the "
                        "default here); 'default' authors no draw mode at all.")
    p.add_argument("--seed", type=int, default=0,
                   help="seed for protoIndices (default 0, deterministic).")
    return p.parse_args(argv)


def _warn_subprototype(args):
    """Warn when a point count is below the prototype count it targets."""
    for what, points, protos, flag in (
            ("building", args.modules_per_building, args.modules,
             "--modules-per-building"),
            ("district", args.buildings_per_district, args.buildings,
             "--buildings-per-district")):
        if points < protos:
            print("warning: {}={} is below the {} prototypes each {} instancer "
                  "targets, so {} of them will be propagated but never "
                  "instanced. N is unaffected; the real scenes have "
                  "points >> prototypes.".format(
                      flag, points, protos, what, protos - points),
                  file=sys.stderr)


def main(argv=None):
    args = _parse_args(argv)
    for name, val in (("--modules", args.modules),
                      ("--buildings", args.buildings),
                      ("--districts", args.districts),
                      ("--modules-per-building", args.modules_per_building),
                      ("--buildings-per-district", args.buildings_per_district)):
        if val <= 0:
            print("error: {} must be positive".format(name), file=sys.stderr)
            return 2

    _warn_subprototype(args)

    out_dir = os.path.abspath(args.output)
    print("Generating city scene:")
    print("  output              : {}".format(out_dir))
    print("  format              : {}".format(args.format))
    print("  modules (M)         : {}".format(args.modules))
    print("  buildings (B)       : {}".format(args.buildings))
    print("  districts (D)       : {}".format(args.districts))
    print("  modules/building    : {}".format(args.modules_per_building))
    print("  buildings/district  : {}".format(args.buildings_per_district))
    print("  instanceable        : {}".format(args.instanceable))
    print("  draw mode           : {}".format(args.draw_mode))
    print("  seed                : {}".format(args.seed))

    start = time.time()
    result = city.build_city_scene(
        output_dir=out_dir,
        fmt=args.format,
        modules=args.modules,
        buildings=args.buildings,
        districts=args.districts,
        modules_per_building=args.modules_per_building,
        buildings_per_district=args.buildings_per_district,
        instanceable=args.instanceable,
        draw_mode=args.draw_mode,
        seed=args.seed,
    )
    elapsed = time.time() - start

    print("\nWrote layers:")
    for role in ("library", "master"):
        path = result[role]
        size = os.path.getsize(path) if os.path.exists(path) else 0
        print("  {:8s} {:>12,d} B  {}".format(role, size, path))

    M, B, D = args.modules, args.buildings, args.districts
    print("\nPropagated prototypes (merging-scene-index inputs):")
    print("  authored pairs        = D*B + B*M = {} + {} = {}".format(
        D * B, B * M, result["authored_pairs"]))
    print("  propagated prototypes = + D*B*M ({}) = {}".format(
        D * B * M, result["N"]))
    if args.instanceable:
        print("  NOTE: --instanceable routes the districts through native "
              "instancing;\n        the formula above no longer applies.")
    print("\nMaster stage : {}".format(result["master"]))
    print("Generation time: {:.2f} s".format(elapsed))
    return 0


if __name__ == "__main__":
    sys.exit(main())
