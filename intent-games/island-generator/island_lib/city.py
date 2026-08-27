
"""Nested point-instancing "city" generator for the imaging-stack stress suite.

Where the island generator (see ``layers.py``) targets ``UsdGeomBBoxCache``,
this generator targets a *different* cost center: the propagation and merge that
``UsdImagingPiPrototypePropagatingSceneIndex`` + ``HdMergingSceneIndex`` perform
when a scene nests PointInstancers inside PointInstancers.  It reproduces the
structure of a customer's Solaris city, built by point-instancing modules into
buildings and buildings into districts, with districts referenced into a city:

  M modules    -- box ``Mesh`` prims in an invisible library.
  B buildings  -- ``PointInstancer`` prims, again in an invisible library, each
                  listing *all* M modules as its ``prototypes``.
  D districts   -- one prototypical district (a ``PointInstancer`` listing all B
                  buildings as its ``prototypes``) plus D-1 internal *references*
                  to it, each with only a translate override.

"Listing all M modules as its ``prototypes``" refers to the ``prototypes``
**relationship**, not to how many instances are drawn, and that distinction is
the whole reason the cost formula below works (this is the customer's diagnosed
insight): the imaging stack creates one propagated prototype per ``prototypes``
relationship *target*, regardless of whether any ``protoIndex`` ever references
it.  So the propagated-prototype count ``N`` is driven by M, B and D alone;
``modules_per_building`` and ``buildings_per_district`` (the point counts) do not
affect it.  Verified: M=100 with 10 instance points and with 400 both give the
same N.

The number of propagated prototypes -- the ``N`` that ``HdMergingSceneIndex``
ends up holding, and which population/teardown cost scales against -- is:

    N = D*B          (each district instancer over the shared building library)
      + B*M          (each building instancer over the shared module library)
      + D*B*M        (each propagated building copy re-propagating its modules)

For the defaults (M=25, B=50, D=5) that is 250 + 1250 + 6250 = 7750.

Structural details that are deliberate, not incidental:

* Multi-layer, matching the real setup: the library ``def``\\s live in a sublayer
  (``cityLibrary.usda``); the master (``city.usda``) carries library-root
  ``over``\\s with ``visibility = "invisible"`` plus the districts.  The composed
  specifier is ``def Xform`` and the composed visibility is ``invisible`` either
  way, so the composed stage matches the customer's single-file repros.
* The district references are **not** instanceable by default.  Plain references
  compose in and produce D independent sets of ordinary prims, so the districts
  multiply N through the *point-instancer* propagation path.  Marking them
  instanceable (``instanceable=True``) routes them through native instancing
  (``UsdImagingNiPrototypePropagatingSceneIndex``) instead and stops reproducing
  the point-instancer behaviour -- useful for comparison, at odds with the setup.
* Instance counts (``modules_per_building``, ``buildings_per_district``) are
  independent of M/B/D, exactly as in the real scenes, so the amount of drawn
  geometry can be varied without touching N.  ``protoIndices`` are assigned
  round-robin then shuffled, so every listed prototype is instantiated at least
  once whenever the point count allows it.

Setting a point count below its prototype count (e.g. ``modules_per_building``
below M) is legal but not a faithful reproduction: the surplus prototypes are
still listed, still propagated, and still cost their share of N -- they are
simply never instantiated.  ``generate_city.py`` warns when this happens.

Determinism: a single ``random.Random(seed)`` is consumed in a fixed order
(buildings 0..B-1, then the district), and every position derives from a prim's
index, so identical arguments yield byte-identical ``.usda`` output -- the same
guarantee the island generator makes.
"""

import os

from pxr import Usd, UsdGeom, Sdf, Gf, Kind

import random

from . import geo


# Each module is a thin slab; a building is a vertical stack of them.
MODULE_HEIGHT = 0.1
# Buildings sit on a grid with this spacing.  Module widths top out at 9.5, so
# this leaves a small gap between neighbours.
BUILDING_SPACING = 11.0

# Composed prim paths (identical whether authored in a sublayer or the master).
MODULE_LIBRARY_ROOT = "/A_Module_Library"
MODULES_CONTAINER = MODULE_LIBRARY_ROOT + "/Prototypes/modules"
BUILDING_LIBRARY_ROOT = "/B_Building_Library"
BUILDINGS_CONTAINER = BUILDING_LIBRARY_ROOT + "/Prototypes"
DISTRICT_ROOT = "/DistrictA"


def module_width(m):
    """XZ footprint of module m: 5.0, 5.5, ... 9.5, then repeating."""
    return 5.0 + (m % 10) / 2.0


def _proto_indices(n_points, n_protos, rng):
    """``protoIndices`` hitting every prototype at least once, where possible.

    Round-robin first so coverage is guaranteed, then shuffle so the ordering is
    not correlated with grid position.  When ``n_points < n_protos`` coverage is
    impossible: prototypes ``n_points..n_protos-1`` get no instances, but they
    are still listed and still propagated (see the module docstring).
    """
    idx = [k % n_protos for k in range(n_points)]
    rng.shuffle(idx)
    return idx


def _stack(n, step):
    """n positions stacked vertically, ``step`` apart in Y, starting at 0."""
    return [Gf.Vec3f(0.0, k * step, 0.0) for k in range(n)]


def _grid(n, spacing):
    """n positions on a roughly square XZ grid centred on the origin."""
    side = max(1, int(n ** 0.5 + 0.999))
    out = []
    for k in range(n):
        x = (k % side - (side - 1) / 2.0) * spacing
        z = (k // side - (side - 1) / 2.0) * spacing
        out.append(Gf.Vec3f(x, 0.0, z))
    return out


def _author_city_point_instancer(stage, path, targets, positions, indices,
                                  draw_mode, extent, kind=Kind.Tokens.group):
    """Author one city ``PointInstancer`` over ``targets``.

    Unlike ``layers._author_point_instancer`` (which builds a ``/Prototypes``
    scope of *instanceable* references to library assets), the city instancers
    are non-instanceable and their ``prototypes`` relationship points straight at
    existing library prims -- which are themselves ``PointInstancer``\\s at the
    building tier.  That PI-of-PI nesting is the whole point of this generator.
    """
    pi = UsdGeom.PointInstancer.Define(stage, path)
    prim = pi.GetPrim()
    if kind:
        Usd.ModelAPI(prim).SetKind(kind)
    if extent is not None:
        lo, hi = extent
        pi.CreateExtentAttr([Gf.Vec3f(*lo), Gf.Vec3f(*hi)])
    if draw_mode and draw_mode != "default":
        api = UsdGeom.ModelAPI.Apply(prim)
        api.CreateModelApplyDrawModeAttr(True)
        api.CreateModelDrawModeAttr(draw_mode)
    pi.CreatePositionsAttr(list(positions))
    pi.CreateProtoIndicesAttr(list(indices))
    pi.CreateIdsAttr([k for k in range(len(indices))])
    pi.CreatePrototypesRel().SetTargets([Sdf.Path(t) for t in targets])
    return pi


def _author_module_library(stage, modules, draw_mode):
    """Author ``/A_Module_Library/Prototypes/modules`` with M box meshes.

    Returns the list of absolute module prim paths (the shared prototype list
    every building instancer targets).
    """
    UsdGeom.Xform.Define(stage, MODULE_LIBRARY_ROOT)
    UsdGeom.Xform.Define(stage, MODULE_LIBRARY_ROOT + "/Prototypes")
    UsdGeom.Xform.Define(stage, MODULES_CONTAINER)
    targets = []
    for m in range(modules):
        w = module_width(m)
        path = "{}/building_module{}".format(MODULES_CONTAINER, m)
        # Reuse the island generator's box mesh so the two suites author boxes
        # identically.  No purpose (default) matches the customer's modules.
        geo._author_box_mesh(stage, path, (w, MODULE_HEIGHT, w),
                             UsdGeom.Tokens.default_)
        if draw_mode and draw_mode != "default":
            api = UsdGeom.ModelAPI.Apply(stage.GetPrimAtPath(Sdf.Path(path)))
            api.CreateModelApplyDrawModeAttr(True)
            api.CreateModelDrawModeAttr(draw_mode)
        targets.append(path)
    return targets


def _author_building_library(stage, modules, buildings, module_targets,
                             modules_per_building, draw_mode, rng):
    """Author ``/B_Building_Library`` -- B instancers over the shared modules.

    Returns ``(building_targets, bld_half, bld_height)`` for the district tier.
    """
    UsdGeom.Xform.Define(stage, BUILDING_LIBRARY_ROOT)
    UsdGeom.Xform.Define(stage, BUILDINGS_CONTAINER)

    # A building is a vertical stack of module slabs, MODULE_HEIGHT apart.
    bld_pos = _stack(modules_per_building, MODULE_HEIGHT)
    bld_half = max(module_width(m) for m in range(modules)) / 2.0
    bld_height = modules_per_building * MODULE_HEIGHT
    bld_extent = ((-bld_half, 0.0, -bld_half),
                  (bld_half, bld_height, bld_half))

    building_targets = []
    for b in range(buildings):
        bpath = "{}/building_{}".format(BUILDINGS_CONTAINER, b)
        bx = UsdGeom.Xform.Define(stage, bpath)
        Usd.ModelAPI(bx).SetKind(Kind.Tokens.group)
        # Consume the shared RNG once per building, in index order (see the
        # determinism note in the module docstring).
        idx = _proto_indices(modules_per_building, modules, rng)
        _author_city_point_instancer(
            stage, bpath + "/instancer", module_targets, bld_pos, idx,
            draw_mode, bld_extent)
        building_targets.append(bpath)
    return building_targets, bld_half, bld_height


def _author_districts(stage, buildings, districts, building_targets,
                      buildings_per_district, bld_half, bld_height,
                      instanceable, rng):
    """Author the prototypical ``/DistrictA`` plus its D-1 references."""
    dst_pos = _grid(buildings_per_district, BUILDING_SPACING)
    dst_half = max((abs(q) for p in dst_pos for q in (p[0], p[2])),
                   default=0.0) + bld_half
    dst_extent = ((-dst_half, 0.0, -dst_half),
                  (dst_half, bld_height, dst_half))

    district = UsdGeom.Xform.Define(stage, DISTRICT_ROOT)
    Usd.ModelAPI(district).SetKind(Kind.Tokens.group)
    # No draw mode on the district instancer: in the real scenes only the
    # modules and the building instancers carry one.  Consume the RNG once, last.
    dst_idx = _proto_indices(buildings_per_district, buildings, rng)
    _author_city_point_instancer(
        stage, DISTRICT_ROOT + "/instancer", building_targets, dst_pos,
        dst_idx, "default", dst_extent)

    # D-1 references to the prototypical district, in a line along X.  Spaced by
    # the district's own footprint plus one building gap so they sit side by side
    # however large they are.
    dst_spacing = 2.0 * dst_half + BUILDING_SPACING
    for d in range(1, districts):
        rpath = "/DistrictA_{}".format(d)
        prim = stage.DefinePrim(Sdf.Path(rpath))  # typeless; type comes via ref
        prim.GetReferences().AddInternalReference(Sdf.Path(DISTRICT_ROOT))
        if instanceable:
            prim.SetInstanceable(True)
        UsdGeom.Xformable(prim).AddTranslateOp().Set(
            Gf.Vec3d(d * dst_spacing, 0.0, 0.0))
    return dst_half


def _set_stage_metadata(stage, frames):
    UsdGeom.SetStageUpAxis(stage, UsdGeom.Tokens.y)
    UsdGeom.SetStageMetersPerUnit(stage, 1.0)
    stage.SetTimeCodesPerSecond(24.0)
    stage.SetFramesPerSecond(24.0)
    stage.SetStartTimeCode(0)
    stage.SetEndTimeCode(max(0, frames - 1))


def build_city_scene(output_dir, fmt="usdc", modules=25, buildings=50,
                     districts=5, modules_per_building=1000,
                     buildings_per_district=100, instanceable=False,
                     draw_mode="bounds", seed=0, frames=0):
    """Generate the nested-PI city scene into ``output_dir``.

    Returns a dict with the authored layer paths (keys ``library`` and
    ``master``) plus the propagated-prototype count ``N`` and structural counts,
    so callers can print and cross-check the cost model.
    """
    if fmt not in ("usda", "usdc"):
        raise ValueError("format must be 'usda' or 'usdc'")
    if draw_mode not in ("bounds", "cards", "origin", "default"):
        raise ValueError("draw_mode must be bounds/cards/origin/default")
    os.makedirs(output_dir, exist_ok=True)

    lib_name = "cityLibrary.usda"     # always usda: human-legible library
    master_name = "city.{}".format(fmt)
    paths = {"library": os.path.join(output_dir, lib_name),
             "master": os.path.join(output_dir, master_name)}

    rng = random.Random(seed)

    # 1) Library sublayer: module meshes + building instancers.  Consumes the
    #    RNG B times (buildings 0..B-1), matching the district tier's ordering.
    lib_layer = Sdf.Layer.CreateNew(paths["library"])
    lib_stage = Usd.Stage.Open(lib_layer)
    module_targets = _author_module_library(lib_stage, modules, draw_mode)
    building_targets, bld_half, bld_height = _author_building_library(
        lib_stage, modules, buildings, module_targets, modules_per_building,
        draw_mode, rng)
    lib_stage.GetRootLayer().Save()

    # 2) Master: sublayer the library, drop invisible overs on the library
    #    roots, then author the districts.  Consumes the RNG once, last.
    master_layer = Sdf.Layer.CreateNew(paths["master"])
    master_layer.subLayerPaths.append(lib_name)
    master_stage = Usd.Stage.Open(master_layer)
    for lib_root in (MODULE_LIBRARY_ROOT, BUILDING_LIBRARY_ROOT):
        over = master_stage.GetPrimAtPath(Sdf.Path(lib_root))
        UsdGeom.Imageable(over).CreateVisibilityAttr(UsdGeom.Tokens.invisible)
    _author_districts(master_stage, buildings, districts, building_targets,
                      buildings_per_district, bld_half, bld_height,
                      instanceable, rng)
    _set_stage_metadata(master_stage, frames)
    master_stage.GetRootLayer().Save()

    n = districts * buildings + buildings * modules \
        + districts * buildings * modules
    result = dict(paths)
    result.update({
        "modules": modules,
        "buildings": buildings,
        "districts": districts,
        "modules_per_building": modules_per_building,
        "buildings_per_district": buildings_per_district,
        "instanceable": instanceable,
        "authored_pairs": districts * buildings + buildings * modules,
        "N": n,
    })
    return result
