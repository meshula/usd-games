
"""Assembly of the department sublayers and the composed master stage.

The layout produced here (all department layers author under the shared ``/Island``
root so they compose cleanly through sublayers) mirrors the ASWF
vfx-intent document:

    assetLibrary.usda   component models (see assets.py)
    terrain.usda        /Island/Terrain  -- unique km-scale meshes + sea plane
    vegetation.usda     /Island/Vegetation -- bulk instanceable refs + PointInstancers
    village.usda        /Island/Village  -- nested-instance blocks, docks, boats, props
    island.usda         master: subLayers = [village, vegetation, terrain]

Bulk placement (tens of thousands of instances) is authored through the ``Sdf``
layer API inside a single ``Sdf.ChangeBlock`` for speed; the low-volume,
structural pieces (terrain mesh, point instancers, boats) use the ``UsdGeom``
typed schemas for clarity.

Determinism: every count and transform derives from the seeded RNG or from the
prim's index, and iteration order is fixed, so identical arguments yield
byte-identical ``.usda`` output.
"""

import math
import os
import random
import shutil

from pxr import Usd, UsdGeom, UsdShade, Sdf, Gf, Vt, Kind

from . import assets
from . import biome as biome_mod
from . import geo
from . import hydrology as hydrology_mod
from . import population
from . import scatter


ISLAND_ROOT = "/Island"
DEFAULT_TCPS = 24.0


# --------------------------------------------------------------------------- #
# Low-level Sdf authoring helpers                                             #
# --------------------------------------------------------------------------- #

def _def_prim(layer, path, type_name="Xform", specifier=Sdf.SpecifierDef):
    spec = Sdf.CreatePrimInLayer(layer, path)
    spec.specifier = specifier
    if type_name:
        spec.typeName = type_name
    return spec


def _over_prim(layer, path):
    spec = Sdf.CreatePrimInLayer(layer, path)
    spec.specifier = Sdf.SpecifierOver
    return spec


def _author_attr(prim_spec, name, type_name, value):
    attr = Sdf.AttributeSpec(prim_spec, name, type_name)
    attr.default = value
    return attr


def _author_xform(prim_spec, translate=None, rotate_y=None, scale=None):
    """Author a translate/rotateY/scale op stack on a prim spec."""
    order = []
    if translate is not None:
        _author_attr(prim_spec, "xformOp:translate",
                     Sdf.ValueTypeNames.Double3, Gf.Vec3d(*translate))
        order.append("xformOp:translate")
    if rotate_y is not None:
        _author_attr(prim_spec, "xformOp:rotateY",
                     Sdf.ValueTypeNames.Double, float(rotate_y))
        order.append("xformOp:rotateY")
    if scale is not None:
        _author_attr(prim_spec, "xformOp:scale",
                     Sdf.ValueTypeNames.Float3, Gf.Vec3f(*scale))
        order.append("xformOp:scale")
    if order:
        _author_attr(prim_spec, "xformOpOrder",
                     Sdf.ValueTypeNames.TokenArray, Vt.TokenArray(order))
    return order


def _reference_asset(prim_spec, lib_rel_path, asset_name, instanceable=True):
    ref = Sdf.Reference(lib_rel_path, assets.asset_path(asset_name))
    prim_spec.referenceList.prependedItems.append(ref)
    if instanceable:
        prim_spec.instanceable = True


# --------------------------------------------------------------------------- #
# Terrain                                                                     #
# --------------------------------------------------------------------------- #

def _author_terrain_material(stage, mesh, tex_rel, mat_path):
    """A UsdPreviewSurface driven by the biome map, bound to ``mesh``.

    ``diffuseColor`` reads the biome-classification texture (``tex_rel``, relative
    to the layer) through a ``UsdUVTexture`` fed by the mesh ``st`` primvar, giving
    the terrain readable coarse coloring (beach/forest/dry/alpine/water).
    """
    mat = UsdShade.Material.Define(stage, mat_path)

    reader = UsdShade.Shader.Define(stage, mat_path + "/stReader")
    reader.CreateIdAttr("UsdPrimvarReader_float2")
    reader.CreateInput("varname", Sdf.ValueTypeNames.String).Set("st")
    st_out = reader.CreateOutput("result", Sdf.ValueTypeNames.Float2)

    tex = UsdShade.Shader.Define(stage, mat_path + "/BiomeTexture")
    tex.CreateIdAttr("UsdUVTexture")
    tex.CreateInput("file", Sdf.ValueTypeNames.Asset).Set(tex_rel)
    tex.CreateInput("st", Sdf.ValueTypeNames.Float2).ConnectToSource(st_out)
    tex.CreateInput("wrapS", Sdf.ValueTypeNames.Token).Set("clamp")
    tex.CreateInput("wrapT", Sdf.ValueTypeNames.Token).Set("clamp")
    tex.CreateInput("sourceColorSpace", Sdf.ValueTypeNames.Token).Set("sRGB")
    rgb_out = tex.CreateOutput("rgb", Sdf.ValueTypeNames.Float3)

    pbr = UsdShade.Shader.Define(stage, mat_path + "/PreviewSurface")
    pbr.CreateIdAttr("UsdPreviewSurface")
    pbr.CreateInput("roughness", Sdf.ValueTypeNames.Float).Set(0.85)
    pbr.CreateInput("metallic", Sdf.ValueTypeNames.Float).Set(0.0)
    pbr.CreateInput("diffuseColor", Sdf.ValueTypeNames.Color3f).ConnectToSource(
        rgb_out)
    surf_out = pbr.CreateOutput("surface", Sdf.ValueTypeNames.Token)
    mat.CreateSurfaceOutput().ConnectToSource(surf_out)

    UsdShade.MaterialBindingAPI.Apply(mesh.GetPrim())
    UsdShade.MaterialBindingAPI(mesh.GetPrim()).Bind(mat)
    return mat


def author_terrain(layer, field, resolution, frames, tcps,
                   diffuse_texture_rel=None):
    """Island heightfield + bay + sea plane on ``layer`` (a fresh Sdf.Layer).

    For the purposes of demonstration, a subset of meshes is authored *without*
    an ``extent`` so the bbox cache is forced down the points-fallback path.

    When ``diffuse_texture_rel`` is given, the island mesh also gets planar ``st``
    texture coordinates and a biome-map ``UsdPreviewSurface`` (see
    ``_author_terrain_material``) for coarse terrain coloring.
    """
    stage = Usd.Stage.Open(layer)
    _over_prim(layer, ISLAND_ROOT)
    UsdGeom.Scope.Define(stage, ISLAND_ROOT + "/Terrain")

    # Island heightfield -- large unique mesh, no authored extent (fallback).
    pts, counts, indices = scatter.terrain_grid(field, resolution)
    island = UsdGeom.Mesh.Define(stage, ISLAND_ROOT + "/Terrain/Island")
    island.CreatePointsAttr([Gf.Vec3f(*p) for p in pts])
    island.CreateFaceVertexCountsAttr(counts)
    island.CreateFaceVertexIndicesAttr(indices)
    island.CreateSubdivisionSchemeAttr(UsdGeom.Tokens.none)
    # Intentionally NO extent here -> exercises points-fallback in bboxCache.

    if diffuse_texture_rel is not None:
        # Planar top-down st matching the guide-map rasters: row 0 = north, so
        # (i, j) grid vertex -> (i/(n-1), j/(n-1)) samples the biome PNG un-flipped.
        n = resolution
        inv = 1.0 / (n - 1)
        st = UsdGeom.PrimvarsAPI(island).CreatePrimvar(
            "st", Sdf.ValueTypeNames.TexCoord2fArray, UsdGeom.Tokens.vertex)
        st.Set([Gf.Vec2f(i * inv, j * inv) for j in range(n) for i in range(n)])
        _author_terrain_material(
            stage, island, diffuse_texture_rel,
            ISLAND_ROOT + "/Terrain/Materials/BiomeSurface")

    # The ocean plane -- a big quad with an authored extent; animated tide (y).
    half = field.radius
    sea = UsdGeom.Mesh.Define(stage, ISLAND_ROOT + "/Terrain/Sea")
    sea.CreatePointsAttr([
        Gf.Vec3f(-half, 0.0, -half), Gf.Vec3f(half, 0.0, -half),
        Gf.Vec3f(half, 0.0, half), Gf.Vec3f(-half, 0.0, half),
    ])
    sea.CreateFaceVertexCountsAttr([4])
    # CCW from above so the sea surface faces +Y (see terrain_grid note).
    sea.CreateFaceVertexIndicesAttr([0, 3, 2, 1])
    sea.CreateSubdivisionSchemeAttr(UsdGeom.Tokens.none)
    sea.CreateExtentAttr([Gf.Vec3f(-half, -2.0, -half), Gf.Vec3f(half, 2.0, half)])
    sea.CreatePurposeAttr(UsdGeom.Tokens.render)
    # Tide: time-sampled translate on the sea plane.
    tide = sea.AddTranslateOp()
    for f in range(frames):
        phase = 2.0 * math.pi * f / max(1, frames)
        tide.Set(Gf.Vec3d(0.0, 0.8 * math.sin(phase), 0.0), Usd.TimeCode(f))

    stage.GetRootLayer().Save()
    return layer


# --------------------------------------------------------------------------- #
# Vegetation                                                                  #
# --------------------------------------------------------------------------- #

def _region_index(x, y, field, grid):
    half = field.radius
    u = int((x + half) / (2.0 * half) * grid)
    v = int((y + half) / (2.0 * half) * grid)
    u = max(0, min(grid - 1, u))
    v = max(0, min(grid - 1, v))
    return u, v


def _region_center(u, v, field, grid):
    half = field.radius
    cell = (2.0 * half) / grid
    return (-half + (u + 0.5) * cell, -half + (v + 0.5) * cell)


def _author_point_instancer(stage, path, proto_asset_names, lib_rel_path,
                            points, field_y_offset=0.0, animated=False,
                            frames=0):
    """Author a PointInstancer referencing library assets as prototypes."""
    pi = UsdGeom.PointInstancer.Define(stage, path)
    proto_container = path + "/Prototypes"
    UsdGeom.Scope.Define(stage, proto_container)
    targets = []
    for i, name in enumerate(proto_asset_names):
        ppath = "{}/proto_{}".format(proto_container, i)
        proto = UsdGeom.Xform.Define(stage, ppath)
        proto.GetPrim().GetReferences().AddReference(
            lib_rel_path, assets.asset_path(name))
        proto.GetPrim().SetInstanceable(True)
        targets.append(Sdf.Path(ppath))
    pi.CreatePrototypesRel().SetTargets(targets)

    positions = [Gf.Vec3f(x, h + field_y_offset, y) for (x, y, h) in points]
    n = len(points)
    # Deterministic proto assignment by index.
    proto_indices = [i % len(targets) for i in range(n)]
    pi.CreatePositionsAttr(positions)
    pi.CreateProtoIndicesAttr(proto_indices)

    if animated and frames > 0:
        # Time-sample positions with a small deterministic bob/drift.
        pos_attr = pi.GetPositionsAttr()
        for f in range(frames):
            phase = 2.0 * math.pi * f / frames
            shifted = [
                Gf.Vec3f(p[0], p[1] + 0.05 * math.sin(phase + 0.01 * k), p[2])
                for k, p in enumerate(positions)
            ]
            pos_attr.Set(Vt.Vec3fArray(shifted), Usd.TimeCode(f))
    return pi


def _role_weight(field, biome, role):
    """Placement-weight callable ``(x, y) -> [0, 1]`` for a scatter ``role``.

    Combines the terrain's base vegetation density with the biome's per-role
    multiplier (``None`` biome -> terrain density only), so each species group
    concentrates in the biomes it belongs to.
    """
    if biome is None:
        return field.vegetation_density

    def weight(x, y):
        return field.vegetation_density(x, y) * biome.role_weight(role, x, y)
    return weight


def author_vegetation(layer, field, lib_rel_path, counts, seed,
                      animated_fraction, frames, grid=6, cluster_size=128,
                      biome=None, exclude=None):
    """Bulk native-instance scatter + several PointInstancers.

    ``counts`` is a dict with keys 'palms', 'rocks', 'foliage' giving the number
    of native instances of each role.  Instances are grouped into a
    Region/Cluster hierarchy to give production-like depth and fan-out and to
    stress the thread-local UsdGeomXformCache.

    ``biome`` (optional ``biome.BiomeMap``) biases placement by biome -- each
    species group is weighted by ``biome.role_weight`` so it lands in appropriate
    biomes, and per-instance size is scaled by ``biome.size_scale`` (larger where
    wetter).  ``exclude`` (optional ``(x, y) -> bool``) prunes placements off
    road/building footprints.
    """
    stage = Usd.Stage.Open(layer)
    _over_prim(layer, ISLAND_ROOT)
    UsdGeom.Scope.Define(stage, ISLAND_ROOT + "/Vegetation")

    veg_root = ISLAND_ROOT + "/Vegetation"

    # Draw placements up front (deterministic).  Each role is weighted by its
    # biome affinity so palms cluster in wet lowland/beach, rocks on dry/alpine
    # slopes, etc., and pruned off roads/buildings via ``exclude``.
    rng = random.Random(seed ^ 0x5EED)
    palm_pts = field.scatter_land(rng, counts["palms"], density_power=1.2,
                                  weight=_role_weight(field, biome, "palms"),
                                  reject=exclude)
    rock_pts = field.scatter_land(rng, counts["rocks"], density_power=0.6,
                                  weight=_role_weight(field, biome, "rocks"),
                                  reject=exclude)
    foliage_pts = field.scatter_land(rng, counts["foliage"], density_power=1.6,
                                     weight=_role_weight(field, biome, "foliage"),
                                     reject=exclude)

    palms = assets.CATALOG["palms"] + assets.CATALOG["broadleaf"]
    rocks = assets.CATALOG["rocks"]
    foliage = assets.CATALOG["foliage"]

    # Decide which instances animate (sway) to hit the requested fraction.
    total = len(palm_pts) + len(rock_pts) + len(foliage_pts)
    n_anim = int(total * animated_fraction)
    anim_stride = max(1, total // max(1, n_anim)) if n_anim else 0

    # Assign every placement to a Region/Cluster bucket keyed by location.
    def _bucketize(points, kind):
        buckets = {}
        for idx, (x, y, h) in enumerate(points):
            u, v = _region_index(x, y, field, grid)
            buckets.setdefault((u, v), []).append((idx, x, y, h))
        return buckets

    all_groups = [
        ("Palm", palm_pts, palms),
        ("Rock", rock_pts, rocks),
        ("Foliage", foliage_pts, foliage),
    ]

    global_index = 0
    with Sdf.ChangeBlock():
        # Ensure the Vegetation scope spec exists in this ChangeBlock context.
        for kind, points, choices in all_groups:
            buckets = _bucketize(points, kind)
            for (u, v) in sorted(buckets.keys()):
                cx, cy = _region_center(u, v, field, grid)
                region_path = "{}/{}_Region_{}_{}".format(veg_root, kind, u, v)
                rspec = _def_prim(layer, region_path)
                _author_xform(rspec, translate=(cx, 0.0, cy))
                items = buckets[(u, v)]
                for local_i, (idx, x, y, h) in enumerate(items):
                    cluster = local_i // cluster_size
                    cpath = "{}/Cluster_{}".format(region_path, cluster)
                    if local_i % cluster_size == 0:
                        _def_prim(layer, cpath)
                    asset_name = choices[idx % len(choices)]
                    ipath = "{}/{}_{}".format(cpath, kind, idx)
                    ispec = _def_prim(layer, ipath)
                    _reference_asset(ispec, lib_rel_path, asset_name)
                    # Residual transform relative to the region center.
                    ry = (idx * 37) % 360
                    s = 0.7 + ((idx * 13) % 60) / 100.0
                    if biome is not None:
                        s *= biome.size_scale(x, y)
                    order = _author_xform(
                        ispec,
                        translate=(x - cx, h, y - cy),
                        rotate_y=ry, scale=(s, s, s))
                    # A fraction of instances carry an explicit purpose so
                    # purpose flows from the instancing prim into the prototype.
                    if idx % 11 == 0:
                        _author_attr(ispec, "purpose",
                                     Sdf.ValueTypeNames.Token,
                                     UsdGeom.Tokens.proxy)
                    # Sway animation on a deterministic subset.
                    if anim_stride and (global_index % anim_stride == 0):
                        base = ry
                        for f in range(frames):
                            ph = 2.0 * math.pi * f / max(1, frames)
                            val = base + 6.0 * math.sin(ph + 0.05 * idx)
                            layer.SetTimeSample(
                                ispec.path.AppendProperty("xformOp:rotateY"),
                                f, val)
                    global_index += 1

    # PointInstancers -- authored outside the ChangeBlock for clarity.
    pi_rng = random.Random(seed ^ 0x1CE)
    grass_pts = field.scatter_land(pi_rng, counts.get("grass", 30000),
                                   density_power=0.8,
                                   weight=_role_weight(field, biome, "foliage"),
                                   reject=exclude)
    pebble_pts = field.scatter_beach(pi_rng, counts.get("pebbles", 15000),
                                     reject=exclude)
    shell_pts = field.scatter_beach(pi_rng, counts.get("shells", 8000),
                                    band=(0.0, 4.0), reject=exclude)

    _author_point_instancer(
        stage, veg_root + "/GrassInstancer",
        ["FoliageClump"], lib_rel_path, grass_pts)
    _author_point_instancer(
        stage, veg_root + "/PebbleInstancer",
        assets.CATALOG["rocks"], lib_rel_path, pebble_pts)
    _author_point_instancer(
        stage, veg_root + "/ShellInstancer",
        assets.CATALOG["foliage"], lib_rel_path, shell_pts)

    stage.GetRootLayer().Save()
    return layer


# --------------------------------------------------------------------------- #
# Village                                                                     #
# --------------------------------------------------------------------------- #

def _author_flower_beds(stage, village_root, field, lib_rel_path, seed,
                        n_bed_types=3, n_flowers_per_bed=12, n_beds=8):
    """Nested PointInstancers: a bed instances flowers; a bed-instancer beds.

    This is the island's PI-of-PI (nested PointInstancer) case -- the same shape
    the city generator stresses: an outer PointInstancer whose ``prototypes``
    targets are *non-instanceable* prims that themselves *contain* a
    PointInstancer. The bed prototypes live under an invisible ``Prototypes``
    scope so they draw only via the instancer, not at the origin (mirroring the
    city's invisible library). Deterministic for a given ``seed``.
    """
    fb_root = village_root + "/FlowerBeds"
    UsdGeom.Scope.Define(stage, fb_root)
    protos = fb_root + "/Prototypes"
    proto_scope = UsdGeom.Scope.Define(stage, protos)
    UsdGeom.Imageable(proto_scope).CreateVisibilityAttr(UsdGeom.Tokens.invisible)

    rng = random.Random(seed ^ 0xF10E)
    flowers = assets.CATALOG["flowers"]

    # Bed prototypes: each is an Xform containing a flower PointInstancer.  Left
    # non-instanceable so the nested PI stays visible to bbox/scene-index walks.
    bed_targets = []
    for k in range(n_bed_types):
        bed_path = "{}/bed_{}".format(protos, k)
        UsdGeom.Xform.Define(stage, bed_path)
        pts = [(rng.uniform(-1.0, 1.0), rng.uniform(-1.0, 1.0), 0.0)
               for _ in range(n_flowers_per_bed)]
        _author_point_instancer(stage, bed_path + "/flowers", flowers,
                                lib_rel_path, pts)
        bed_targets.append(bed_path)

    # Outer instancer: scatter beds on the dry land ringing the bay (the shops).
    bx, by = field.bay_center
    positions, indices = [], []
    placed, attempts = 0, 0
    while placed < n_beds and attempts < n_beds * 200:
        attempts += 1
        ang = rng.uniform(0.0, 2.0 * math.pi)
        rad = field.bay_radius * rng.uniform(1.0, 1.3)
        x = bx + rad * math.cos(ang)
        y = by + rad * math.sin(ang)
        h = field.height(x, y)
        if h <= field.SEA_LEVEL:
            continue
        positions.append(Gf.Vec3f(x, h, y))
        indices.append(placed % len(bed_targets))
        placed += 1

    bed_pi = UsdGeom.PointInstancer.Define(stage, fb_root + "/BedInstancer")
    bed_pi.CreatePositionsAttr(positions)
    bed_pi.CreateProtoIndicesAttr(indices)
    bed_pi.CreateIdsAttr([i for i in range(len(indices))])
    bed_pi.CreatePrototypesRel().SetTargets([Sdf.Path(t) for t in bed_targets])
    return fb_root


def author_village(layer, field, lib_rel_path, seed, frames, tcps,
                   n_blocks=24, n_boats=40, n_docks=6, n_market=60):
    """Nested-instance blocks, docks, animated boats, and market props.

    Concentrated in and around the port bay.  Boats are time-sampled (bob +
    drift) to exercise the time-sampled xform path.
    """
    stage = Usd.Stage.Open(layer)
    _over_prim(layer, ISLAND_ROOT)
    UsdGeom.Scope.Define(stage, ISLAND_ROOT + "/Village")
    village = ISLAND_ROOT + "/Village"

    rng = random.Random(seed ^ 0x1111)
    bx, by = field.bay_center

    # Village blocks (nested instances) on the dry land ringing the bay.
    blocks_root = village + "/Blocks"
    _def_prim(layer, blocks_root)
    placed = 0
    attempts = 0
    with Sdf.ChangeBlock():
        while placed < n_blocks and attempts < n_blocks * 200:
            attempts += 1
            ang = rng.uniform(0.0, 2.0 * math.pi)
            rad = field.bay_radius * rng.uniform(1.0, 1.6)
            x = bx + rad * math.cos(ang)
            y = by + rad * math.sin(ang)
            h = field.height(x, y)
            if h <= field.SEA_LEVEL:
                continue
            path = "{}/Block_{}".format(blocks_root, placed)
            spec = _def_prim(layer, path)
            _reference_asset(spec, lib_rel_path, "VillageBlock")
            _author_xform(spec, translate=(x, h, y),
                          rotate_y=(placed * 41) % 360)
            placed += 1

    # Market props ringing the landward side of the bay (never on water).
    market_root = village + "/Market"
    _def_prim(layer, market_root)
    placed = 0
    attempts = 0
    with Sdf.ChangeBlock():
        while placed < n_market and attempts < n_market * 200:
            attempts += 1
            ang = rng.uniform(0.0, 2.0 * math.pi)
            rad = field.bay_radius * rng.uniform(0.95, 1.25)
            x = bx + rad * math.cos(ang)
            y = by + rad * math.sin(ang)
            h = field.height(x, y)
            if h <= field.SEA_LEVEL:
                continue
            path = "{}/Stall_{}".format(market_root, placed)
            spec = _def_prim(layer, path)
            _reference_asset(spec, lib_rel_path, "MarketProp")
            _author_xform(spec, translate=(x, h, y),
                          rotate_y=(placed * 53) % 360)
            placed += 1

    # Docks reaching into the bay water (guide-purpose planks live inside).
    docks_root = village + "/Docks"
    UsdGeom.Scope.Define(stage, docks_root)
    for i in range(n_docks):
        ang = (2.0 * math.pi * i / n_docks)
        rad = field.bay_radius * 0.55
        x = bx + rad * math.cos(ang)
        y = by + rad * math.sin(ang)
        dock = UsdGeom.Xform.Define(stage, "{}/Dock_{}".format(docks_root, i))
        dock.GetPrim().GetReferences().AddReference(
            lib_rel_path, assets.asset_path("Dock"))
        dock.GetPrim().SetInstanceable(True)
        dock.AddTranslateOp().Set(Gf.Vec3d(x, 0.5, y))
        dock.AddRotateYOp().Set(math.degrees(ang))

    # Boats: animated bob + drift on the bay water.
    boats_root = village + "/Boats"
    UsdGeom.Scope.Define(stage, boats_root)
    boat_pts = field.scatter_bay_water(rng, n_boats)
    for i, (x, y, h) in enumerate(boat_pts):
        boat = UsdGeom.Xform.Define(stage, "{}/Boat_{}".format(boats_root, i))
        boat.GetPrim().GetReferences().AddReference(
            lib_rel_path, assets.asset_path("Boat"))
        boat.GetPrim().SetInstanceable(True)
        t = boat.AddTranslateOp()
        r = boat.AddRotateYOp()
        for f in range(frames):
            ph = 2.0 * math.pi * f / max(1, frames)
            bob = 0.3 * math.sin(ph + 0.4 * i)
            drift = 1.5 * math.sin(ph * 0.5 + 0.2 * i)
            t.Set(Gf.Vec3d(x + drift, 0.4 + bob, y), Usd.TimeCode(f))
            r.Set((i * 29 + 10.0 * math.sin(ph)) % 360, Usd.TimeCode(f))

    # Flower beds in front of the shops: nested PointInstancers (PI-of-PI).
    _author_flower_beds(stage, village, field, lib_rel_path, seed)

    stage.GetRootLayer().Save()
    return layer


# --------------------------------------------------------------------------- #
# Master assembly                                                             #
# --------------------------------------------------------------------------- #

def author_master(layer, sublayer_rel_paths, frames, tcps):
    """Compose the department sublayers into the master island stage."""
    stage = Usd.Stage.Open(layer)
    # subLayers are strongest-first; village overrides vegetation overrides
    # terrain where they touch the same prim.
    layer.subLayerPaths.clear()
    for p in sublayer_rel_paths:
        layer.subLayerPaths.append(p)

    island = UsdGeom.Xform.Define(stage, ISLAND_ROOT)
    Usd.ModelAPI(island).SetKind(Kind.Tokens.assembly)
    stage.SetDefaultPrim(island.GetPrim())

    UsdGeom.SetStageUpAxis(stage, UsdGeom.Tokens.y)
    UsdGeom.SetStageMetersPerUnit(stage, 1.0)
    stage.SetTimeCodesPerSecond(tcps)
    stage.SetFramesPerSecond(tcps)
    stage.SetStartTimeCode(0)
    stage.SetEndTimeCode(max(0, frames - 1))
    stage.GetRootLayer().Save()
    return layer


# --------------------------------------------------------------------------- #
# Top-level orchestration                                                     #
# --------------------------------------------------------------------------- #

def _plan_counts(target_prims):
    """Split the native-instance budget across roles, plus PI point budgets."""
    palms = int(target_prims * 0.55)
    rocks = int(target_prims * 0.15)
    foliage = target_prims - palms - rocks
    # PointInstancer point counts scale gently with the target.
    scale = max(1.0, target_prims / 50000.0)
    return {
        "palms": palms,
        "rocks": rocks,
        "foliage": foliage,
        "grass": int(30000 * scale),
        "pebbles": int(15000 * scale),
        "shells": int(8000 * scale),
    }


def _terrain_resolution(size_km):
    # ~3 m per quad along each axis so the mesh resolves the (eroded) relief
    # without the aliasing a coarse grid shows; capped for sanity.
    res = int(size_km * 180)
    return max(96, min(res, 576))


def build_scene(output_dir, fmt="usdc", target_prims=50000, size_km=2.0,
                seed=1751, animated_fraction=0.1, frames=96, with_town=False,
                wind_dir_deg=315.0, wind_strength=0.6, with_rivers=False,
                erosion_iterations=0, erosion_strength=1.0,
                road_geom="curves"):
    """Generate the full island scene into ``output_dir``.

    ``erosion_iterations > 0`` runs hydraulic erosion on the heightfield first, so
    the terrain has incised valleys that the rivers settle into and the roads and
    vegetation follow.  It reshapes the single ``IslandField`` everything samples.

    A river hydrology (``hydrology.HydrologyMap``) is always computed from the
    heightfield -- flow accumulation, a river network, and a wetness index
    exported as ``wetnessmap.png`` -- and its wetness feeds the biome moisture so
    valleys and riverbanks green up (and a riparian biome appears near water).
    When ``with_rivers`` is set, the river curve network is authored as an
    ``/Island/Hydrology`` department and vegetation is pruned off the channels.

    Vegetation placement is biased by a moisture/biome guide map
    (``biome.BiomeMap``) derived from the terrain, a prevailing wind
    (``wind_dir_deg`` / ``wind_strength``), and the hydrology wetness: the
    windward/wetter side and river valleys are lush (larger, denser, forest
    species), the leeward and high ground drier (sparser, smaller, scrub/rock),
    exported as ``biomemap.png`` / ``moisturemap.png``.

    When ``with_town`` is set, the ``/Island/Town`` department (roads + buildings,
    via ``town.build_town_scene`` with ``--population island``) is generated into
    the same directory and its ``town.usd*`` master is inserted into the island
    master's sublayers as ``[village, town, vegetation, terrain]`` -- a fourth
    peer department.  Because the town is authored in island world coordinates on
    the same seed/size ``IslandField``, it composes with no root remapping (the
    town master's own ``[buildings, roads]`` sublayers are pulled in recursively).
    Vegetation is then pruned off the road/building footprints.

    Returns a dict of role -> absolute path for the layers written.
    """
    if fmt not in ("usda", "usdc"):
        raise ValueError("format must be 'usda' or 'usdc'")
    os.makedirs(output_dir, exist_ok=True)

    field = scatter.IslandField(size_km=size_km, seed=seed,
                                erosion_iterations=erosion_iterations,
                                erosion_strength=erosion_strength)
    counts = _plan_counts(target_prims)
    tcps = DEFAULT_TCPS

    lib_name = "assetLibrary.usda"          # always usda: small + human-legible
    terrain_name = "terrain.{}".format(fmt)
    veg_name = "vegetation.{}".format(fmt)
    village_name = "village.{}".format(fmt)
    master_name = "island.{}".format(fmt)

    paths = {k: os.path.join(output_dir, n) for k, n in {
        "library": lib_name, "terrain": terrain_name, "vegetation": veg_name,
        "village": village_name, "master": master_name,
        "heightmap": "heightmap.png",
        "populationmap": "populationmap.png",
        "moisturemap": "moisturemap.png",
        "biomemap": "biomemap.png",
        "wetnessmap": "wetnessmap.png"}.items()}

    # Export the rasterized terrain as an inspectable 16-bit heightmap artifact.
    field.save_heightmap(paths["heightmap"])

    # Derive the population heat map from the heightmap and export it likewise.
    pop = population.IslandPopulation(field)
    pop.save_populationmap(paths["populationmap"])

    # Route water over the terrain: rivers + a wetness index (always computed;
    # feeds the biome moisture below).  Export the wetness artifact.
    hydro = hydrology_mod.HydrologyMap(field)
    hydro.save_wetnessmap(paths["wetnessmap"])

    # Derive the moisture/biome guide map from terrain + wind + hydrology wetness.
    biome = biome_mod.BiomeMap(field, wind_dir_deg=wind_dir_deg,
                               wind_strength=wind_strength, hydro=hydro)
    biome.save_moisturemap(paths["moisturemap"])
    biome.save_biomemap(paths["biomemap"])

    # Copy the biome map into a mat/ folder (mirrors geo/) as the terrain's
    # diffuse texture, referenced by the terrain material below.
    mat_dir = os.path.join(output_dir, "mat")
    os.makedirs(mat_dir, exist_ok=True)
    terrain_tex_rel = os.path.join("mat", "terrain_biome.png")
    shutil.copyfile(paths["biomemap"], os.path.join(output_dir, terrain_tex_rel))
    paths["terrain_texture"] = os.path.join(output_dir, terrain_tex_rel)

    # 1) Geometry files (geo/<component>.usda) + asset library referencing them.
    geo_rels = geo.author_geo_files(output_dir)
    lib_layer = Sdf.Layer.CreateNew(paths["library"])
    lib_stage = Usd.Stage.Open(lib_layer)
    assets.author_asset_library(lib_stage, geo_rels)
    lib_stage.GetRootLayer().Save()

    # Department layers reference the library by relative path.
    lib_rel = lib_name

    # 2) Terrain (with a biome-map UsdPreviewSurface for coarse coloring).
    terrain_layer = Sdf.Layer.CreateNew(paths["terrain"])
    author_terrain(terrain_layer, field, _terrain_resolution(size_km),
                   frames, tcps, diffuse_texture_rel=terrain_tex_rel)

    # 3) Optional curve-network departments (town roads/buildings, rivers).
    # Authored before vegetation so their footprints can prune the scatter.
    # Sublayer order: [village, (town), (hydrology), vegetation, terrain].
    sublayers = [village_name]
    excludes = []
    if with_town or with_rivers:
        # Imported lazily so the common path stays light and the import graph
        # stays acyclic.
        from . import town
        from . import roadnet

    if with_town:
        town_name = "town.{}".format(fmt)
        # Roads must bridge (not drape into) rivers/sea: flag points near a river
        # channel or below sea level so road curves/ribbons hold a bank-height
        # span there.  Rivers exist as data whether or not their geometry is
        # authored, so bridging is always river-aware in the integrated scene.
        rnodes_b, redges_b = hydro.rivers()
        rpts_b = []
        redgelist_b = []
        for e in redges_b:
            b = len(rpts_b)
            rpts_b.extend(e["points"])
            for kk in range(len(e["points"]) - 1):
                redgelist_b.append((b + kk, b + kk + 1, False))
        near_river = roadnet.build_exclusion(
            rpts_b, redgelist_b, (), road_clearance=9.0)

        def road_over_water(x, y):
            return field.is_water(x, y) or near_river(x, y)

        town_paths = town.build_town_scene(
            output_dir, fmt=fmt, seed=seed, population="island",
            size_km=size_km, field=field, over_water=road_over_water,
            road_geom=road_geom)
        sublayers.append(town_name)
        paths["town_master"] = town_paths["master"]
        paths["town_roads"] = town_paths["roads"]
        paths["town_buildings"] = town_paths["buildings"]
        # Prune vegetation off the road/building footprints.
        excludes.append(roadnet.build_exclusion(
            town_paths["nodes"], town_paths["edges"],
            town_paths["building_footprints"],
            road_clearance=town.HIGHWAY_WIDTH * 0.5 + 3.0,
            building_clearance=2.0))

    if with_rivers:
        hydro_name = "hydrology.{}".format(fmt)
        paths["hydrology"] = os.path.join(output_dir, hydro_name)
        rnodes, redges = hydro.rivers()
        hydro_layer = Sdf.Layer.CreateNew(paths["hydrology"])
        town.author_rivers(hydro_layer, field, rnodes, redges)
        hydro_layer.Save()
        sublayers.append(hydro_name)
        # Prune vegetation out of the channels.  Treat each consecutive pair of
        # channel-polyline points as a segment so pruning follows the curve.
        rpts = []
        redgelist = []
        for e in redges:
            base = len(rpts)
            rpts.extend(e["points"])
            for k in range(len(e["points"]) - 1):
                redgelist.append((base + k, base + k + 1, False))
        excludes.append(roadnet.build_exclusion(
            rpts, redgelist, (), road_clearance=6.0))

    sublayers += [veg_name, terrain_name]

    # Combine any exclusion predicates (a point is excluded if any says so).
    if not excludes:
        veg_exclude = None
    elif len(excludes) == 1:
        veg_exclude = excludes[0]
    else:
        def veg_exclude(x, y, _es=tuple(excludes)):
            return any(e(x, y) for e in _es)

    # 4) Vegetation (biome-biased, pruned off any road/building/river footprints).
    veg_layer = Sdf.Layer.CreateNew(paths["vegetation"])
    author_vegetation(veg_layer, field, lib_rel, counts, seed,
                      animated_fraction, frames, biome=biome,
                      exclude=veg_exclude)

    # 5) Village.
    village_layer = Sdf.Layer.CreateNew(paths["village"])
    author_village(village_layer, field, lib_rel, seed, frames, tcps)

    # 6) Master.
    master_layer = Sdf.Layer.CreateNew(paths["master"])
    author_master(master_layer, sublayers, frames, tcps)

    return paths
