
"""Raw geometry files for the component library.

Each logical component (palm, rock, hut, boat, ...) gets one geometry file in a
``geo/`` directory, demonstrating the typical pattern of splitting geometry from
a model so it can be put behind a payload. The *model* interface lives in 
``assetLibrary.usda`` (kind, ``extentsHint``, purpose) and *references* the
raw meshes authored here.

Geometry is intentionally tiny (boxes). The repeated elements (hut walls,
dock planks, foliage leaves, etc.) are authored as ``instanceable`` internal
references to an in-file ``class`` master.  That is a typical pattern
for repeated dressing. This recursive instancing structure stress-tests 
acceleration structures such as ``UsdGeomBBoxCache``

The ``class`` master is an ``Xform`` wrapping a ``Geo`` mesh. Note that it is
not a bare mesh, as an ``instanceable`` prim that references a bare Mesh would 
compose the geometry onto itself with an empty prototype subtree and draw 
nothing (that's what happened when I first wrote this tutorial ;).  Wrapping keeps 
a real gprim *inside* the prototype.
"""

import math

from pxr import Usd, UsdGeom, UsdShade, Sdf, Gf


# Directory (relative to the scene root) and file naming for geo layers.
GEO_DIR = "geo"


# ---------------------------------------------------------------------------- #
# Component materials                                                          #
# A small shared palette of UsdPreviewSurfaces, assigned to component parts by #
# name.  Because the geo files are referenced (and instanced) by the asset     #
# library, binding a material here travels with the asset -- one material per  #
# prototype, shared by every native instance -- so a handful of materials adds #
# broad visual variety for negligible cost.                                    #
# ---------------------------------------------------------------------------- #

# key -> (diffuseColor, roughness, metallic)
MATERIALS = {
    "bark":    ((0.30, 0.19, 0.10), 0.90, 0.0),   # trunks, masts, mooring
    "foliage": ((0.11, 0.40, 0.15), 0.85, 0.0),   # fronds, canopy, leaves, stems
    "stone":   ((0.44, 0.44, 0.46), 0.80, 0.0),   # rocks
    "wood":    ((0.46, 0.31, 0.17), 0.80, 0.0),   # planks, hull, hut walls
    "thatch":  ((0.66, 0.52, 0.26), 0.90, 0.0),   # roofs, market stalls
    "blossom": ((0.88, 0.30, 0.45), 0.55, 0.0),   # flower blossoms
}

# Component part name (mesh name, or instanced-element master name) -> material.
# Parts absent here (e.g. the market Proxy) are left unbound.
PART_MATERIAL = {
    "Trunk": "bark", "Mast": "bark", "Mooring": "bark",
    "Fronds": "foliage", "Canopy": "foliage", "leaf": "foliage",
    "Stem": "foliage",
    "Geom": "stone",
    "wall": "wood", "Hull": "wood", "plank": "wood",
    "Roof": "thatch", "Stall": "thatch",
    "Blossom": "blossom",
}


def geo_rel_path(name):
    return "{}/{}.usda".format(GEO_DIR, name)


# --------------------------------------------------------------------------- #
# Box mesh primitives                                                         #
# All the geometry is boxes right now as the main goal is to illustrate       #
# geometric extents, primitive hierarchy, and a vfx-intent style layer set.   #
# --------------------------------------------------------------------------- #

_BOX_FACE_COUNTS = [4, 4, 4, 4, 4, 4]
_BOX_FACE_INDICES = [
    0, 1, 2, 3,  # bottom (-Y)
    4, 7, 6, 5,  # top (+Y)
    0, 4, 5, 1,  # sides
    1, 5, 6, 2,
    2, 6, 7, 3,
    3, 7, 4, 0,
]


def _box_points(sx, sy, sz):
    """Eight corners of an axis-aligned box; base on y=0, top at y=sy."""
    hx, hz = sx * 0.5, sz * 0.5
    return [
        Gf.Vec3f(-hx, 0.0, -hz), Gf.Vec3f(hx, 0.0, -hz),
        Gf.Vec3f(hx, 0.0, hz),   Gf.Vec3f(-hx, 0.0, hz),
        Gf.Vec3f(-hx, sy, -hz),  Gf.Vec3f(hx, sy, -hz),
        Gf.Vec3f(hx, sy, hz),    Gf.Vec3f(-hx, sy, hz),
    ]


def _author_box_mesh(stage, path, size, purpose):
    """Author a tiny box Mesh with an explicit extent, polygonal (no subdiv)."""
    sx, sy, sz = size
    mesh = UsdGeom.Mesh.Define(stage, path)
    mesh.CreatePointsAttr(_box_points(sx, sy, sz))
    mesh.CreateFaceVertexCountsAttr(_BOX_FACE_COUNTS)
    mesh.CreateFaceVertexIndicesAttr(_BOX_FACE_INDICES)
    mesh.CreateSubdivisionSchemeAttr(UsdGeom.Tokens.none)
    hx, hz = sx * 0.5, sz * 0.5
    mesh.CreateExtentAttr([Gf.Vec3f(-hx, 0.0, -hz), Gf.Vec3f(hx, sy, hz)])
    if purpose and purpose != UsdGeom.Tokens.default_:
        mesh.CreatePurposeAttr(purpose)
    return mesh


def _author_box_class(stage, path, size, purpose):
    """Author an abstract (class) Xform master wrapping a ``Geo`` box mesh.

    Used as the internal reference target for repeated instanceable elements.
    Being a class, it is excluded from default traversal / imaging. Only the
    concrete instances that reference these boxes will draw.
    """
    master = stage.CreateClassPrim(Sdf.Path(path))
    master.SetTypeName("Xform")
    _author_box_mesh(stage, path + "/Geo", size, purpose)
    return master


# --------------------------------------------------------------------------- #
# Component descriptions                                                      #
# --------------------------------------------------------------------------- #

_R = UsdGeom.Tokens.render
_P = UsdGeom.Tokens.proxy
_G = UsdGeom.Tokens.guide


def _mesh(name, size, offset=(0.0, 0.0, 0.0), purpose=_R):
    return {"kind": "mesh", "name": name, "size": size,
            "offset": offset, "purpose": purpose}


def _instanced(master, size, placements, purpose=_R):
    # placements: list of (offset, rotateY_degrees)
    return {"kind": "instanced", "master": master, "size": size,
            "placements": placements, "purpose": purpose}


def _foliage_placements():
    pl = []
    for k in range(6):
        ang = k * 1.0472  # 60 degrees
        pl.append(((0.15 * math.cos(ang), 0.05 * k, 0.15 * math.sin(ang)), 0.0))
    return pl


# One entry per logical component.  Sizes span cm (leaf) to m (buildings).
COMPONENTS = {
    "palm": [
        _mesh("Trunk", (0.35, 8.0, 0.35), (0.0, 0.0, 0.0)),
        _mesh("Fronds", (3.2, 1.2, 3.2), (0.0, 8.0, 0.0)),
    ],
    "broadleaf": [
        _mesh("Trunk", (0.35, 8.0, 0.35), (0.0, 0.0, 0.0)),
        _mesh("Canopy", (5.5, 4.0, 5.5), (0.0, 6.0, 0.0)),
    ],
    "rock": [
        _mesh("Geom", (1.8, 1.2, 1.6), (0.0, 0.0, 0.0)),
    ],
    "hut": [
        _instanced("wall", (4.0, 3.0, 0.25), [
            ((0.0, 0.0, -2.0), 0.0), ((0.0, 0.0, 2.0), 0.0),
            ((-2.0, 0.0, 0.0), 90.0), ((2.0, 0.0, 0.0), 90.0),
        ]),
        _mesh("Roof", (5.0, 1.0, 5.0), (0.0, 3.0, 0.0)),
    ],
    "boat": [
        _mesh("Hull", (2.2, 1.5, 6.0), (0.0, 0.0, 0.0)),
        _mesh("Mast", (0.15, 5.0, 0.15), (0.0, 0.0, 0.0)),
    ],
    "dock": [
        _instanced("plank", (2.0, 0.2, 8.0),
                   [((0.0, 0.0, k * 8.0), 0.0) for k in range(5)]),
        _mesh("Mooring", (0.1, 1.5, 0.1), (0.0, 0.0, 0.0), purpose=_G),
    ],
    "market": [
        _mesh("Stall", (2.4, 2.6, 2.4), (0.0, 0.0, 0.0)),
        _mesh("Proxy", (2.6, 2.7, 2.6), (0.0, 0.0, 0.0), purpose=_P),
    ],
    "foliage": [
        _instanced("leaf", (0.04, 0.008, 0.03), _foliage_placements()),
    ],
    # A single flower: a thin stem with a blossom on top.  Point-instanced into
    # flower beds (see layers._author_flower_beds), which are themselves
    # point-instanced -- the island's PI-of-PI (nested PointInstancer) case.
    "flower": [
        _mesh("Stem", (0.04, 0.4, 0.04), (0.0, 0.0, 0.0)),
        _mesh("Blossom", (0.28, 0.22, 0.28), (0.0, 0.4, 0.0)),
    ],
}


# --------------------------------------------------------------------------- #
# Extent aggregation                                                          #
# --------------------------------------------------------------------------- #

def _element_aabb(size, offset, rotate_y=0.0):
    """Axis-aligned bounds of a box placed at ``offset`` and rotated about Y."""
    sx, sy, sz = size
    ox, oy, oz = offset
    hx, hz = sx * 0.5, sz * 0.5
    rad = math.radians(rotate_y)
    c, s = abs(math.cos(rad)), abs(math.sin(rad))
    ehx = hx * c + hz * s
    ehz = hx * s + hz * c
    return ((ox - ehx, oy, oz - ehz), (ox + ehx, oy + sy, oz + ehz))


def component_extent(name, scale=1.0):
    """Union AABB of a component's geometry, uniformly scaled.  (min, max)."""
    lo = [1e30, 1e30, 1e30]
    hi = [-1e30, -1e30, -1e30]
    for el in COMPONENTS[name]:
        if el["kind"] == "mesh":
            placements = [(el["offset"], 0.0)]
        else:
            placements = el["placements"]
        for offset, ry in placements:
            elo, ehi = _element_aabb(el["size"], offset, ry)
            for i in range(3):
                lo[i] = min(lo[i], elo[i])
                hi[i] = max(hi[i], ehi[i])
    lo = Gf.Vec3f(*[v * scale for v in lo])
    hi = Gf.Vec3f(*[v * scale for v in hi])
    return lo, hi


# --------------------------------------------------------------------------- #
# Authoring                                                                   #
# --------------------------------------------------------------------------- #

def _author_preview_material(stage, path, key):
    """Author a UsdPreviewSurface for palette ``key`` at ``path``; return it."""
    rgb, roughness, metallic = MATERIALS[key]
    mat = UsdShade.Material.Define(stage, path)
    pbr = UsdShade.Shader.Define(stage, path + "/PreviewSurface")
    pbr.CreateIdAttr("UsdPreviewSurface")
    pbr.CreateInput("diffuseColor", Sdf.ValueTypeNames.Color3f).Set(
        Gf.Vec3f(*rgb))
    pbr.CreateInput("roughness", Sdf.ValueTypeNames.Float).Set(roughness)
    pbr.CreateInput("metallic", Sdf.ValueTypeNames.Float).Set(metallic)
    mat.CreateSurfaceOutput().ConnectToSource(
        pbr.CreateOutput("surface", Sdf.ValueTypeNames.Token))
    return mat


def _bind(stage, mesh_prim, container_path, key, cache):
    """Bind ``mesh_prim`` to palette ``key``, authoring the material once per
    ``container_path`` (kept inside the same prim subtree so the binding stays
    valid when the component is referenced/instanced)."""
    ck = (container_path, key)
    mat = cache.get(ck)
    if mat is None:
        mat = _author_preview_material(
            stage, container_path + "/Materials/" + key, key)
        cache[ck] = mat
    UsdShade.MaterialBindingAPI.Apply(mesh_prim)
    UsdShade.MaterialBindingAPI(mesh_prim).Bind(mat)


def _author_component(stage, name):
    """Author a single geo layer's default-prim subtree for ``name``."""
    root_path = "/{}".format(name)
    root = UsdGeom.Xform.Define(stage, root_path)
    mat_cache = {}
    for el in COMPONENTS[name]:
        if el["kind"] == "mesh":
            ox, oy, oz = el["offset"]
            mesh = _author_box_mesh(
                stage, "{}/{}".format(root_path, el["name"]),
                el["size"], el["purpose"])
            if (ox, oy, oz) != (0.0, 0.0, 0.0):
                mesh.AddTranslateOp().Set(Gf.Vec3d(ox, oy, oz))
            key = PART_MATERIAL.get(el["name"])
            if key:
                _bind(stage, mesh.GetPrim(), root_path, key, mat_cache)
        else:  # instanced group
            master_path = "{}/{}".format(root_path, el["master"])
            _author_box_class(stage, master_path, el["size"], el["purpose"])
            key = PART_MATERIAL.get(el["master"])
            if key:
                # Material lives inside the master subtree so the binding is
                # self-contained within the (internally-instanced) prototype.
                geo_prim = stage.GetPrimAtPath(master_path + "/Geo")
                _bind(stage, geo_prim, master_path, key, mat_cache)
            for i, (offset, ry) in enumerate(el["placements"]):
                ipath = "{}/{}_{}".format(root_path, el["master"], i)
                inst = stage.DefinePrim(Sdf.Path(ipath))
                inst.GetReferences().AddInternalReference(Sdf.Path(master_path))
                inst.SetInstanceable(True)
                xf = UsdGeom.Xformable(inst)
                xf.AddTranslateOp().Set(Gf.Vec3d(*offset))
                if ry:
                    xf.AddRotateYOp().Set(ry)
    stage.SetDefaultPrim(root.GetPrim())
    return root


def author_geo_files(output_dir):
    """Write every ``geo/<name>.usda`` under ``output_dir``.

    Returns a dict mapping component name -> relative reference path (e.g.
    ``geo/palm.usda``).  Fully deterministic.
    """
    import os
    geo_dir = os.path.join(output_dir, GEO_DIR)
    os.makedirs(geo_dir, exist_ok=True)
    rels = {}
    for name in COMPONENTS:
        path = os.path.join(geo_dir, "{}.usda".format(name))
        layer = Sdf.Layer.CreateNew(path)
        stage = Usd.Stage.Open(layer)
        _author_component(stage, name)
        stage.GetRootLayer().Save()
        rels[name] = geo_rel_path(name)
    return rels
