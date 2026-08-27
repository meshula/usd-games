
"""Component-model authoring for the island benchmark library.

``assetLibrary.usda`` holds the *model interfaces*
``/Assets/<Component>`` prims are tagged with ``kind`` and ``extentsHint``
The components **reference** their raw geometry from the ``geo/`` directory
(see ``geo.py``).  This mirrors a real production component: a thin model layer 
that references a separate geometry file. These geometry files may be payloads.

Two tiers of native instancing feed ``UsdGeomBBoxCache``'s 
prototype-dependency DAG:

  * **Intra-geo instancing** -- a component's geo file instances its repeated
    elements (hut walls, dock planks, foliage leaves), so a component prototype
    itself contains instances.
  * **Assembly instancing** -- ``VillageBlock`` is an assembly that references
    ``Hut`` *components* ``instanceable``, giving the deep chains such as a
    ``VillageBlock`` has Huts which have Walls.

Asset roots carry no xform ops: the department layers author a
per-instance transform on the prim that references the asset. Asset-root
ops would collide with that.  Size variety comes from those per-instance
scales; the three palm/rock/hut *variants* are distinct prims (hence distinct
prototypes) referencing the same geo.
"""

from pxr import Usd, UsdGeom, Sdf, Gf, Kind

from . import geo


ASSETS_ROOT = "/Assets"


# The catalog names every component asset the department layers can reference,
# grouped by role so scatter/layers can pick appropriately.
CATALOG = {
    "palms":     ["DecoratedPalm_0", "DecoratedPalm_1", "DecoratedPalm_2"],
    "broadleaf": ["Broadleaf"],
    "rocks":     ["Rock_0", "Rock_1", "Rock_2"],
    "foliage":   ["FoliageClump"],
    "huts":      ["Hut_0", "Hut_1", "Hut_2"],
    "blocks":    ["VillageBlock"],
    "boats":     ["Boat"],
    "docks":     ["Dock"],
    "market":    ["MarketProp"],
    "flowers":   ["Flower_0", "Flower_1", "Flower_2"],
}


# Map each component asset to the geo component it references.
GEO_FOR = {
    "DecoratedPalm_0": "palm", "DecoratedPalm_1": "palm",
    "DecoratedPalm_2": "palm",
    "Broadleaf": "broadleaf",
    "Rock_0": "rock", "Rock_1": "rock", "Rock_2": "rock",
    "FoliageClump": "foliage",
    "Hut_0": "hut", "Hut_1": "hut", "Hut_2": "hut",
    "Boat": "boat",
    "Dock": "dock",
    "MarketProp": "market",
    "Flower_0": "flower", "Flower_1": "flower", "Flower_2": "flower",
}


def asset_path(name):
    return "{}/{}".format(ASSETS_ROOT, name)


def _author_component_asset(stage, name, geo_rel):
    """Author a leaf component: an Xform model that references its geo file."""
    prim = UsdGeom.Xform.Define(stage, asset_path(name))
    prim.GetPrim().GetReferences().AddReference(geo_rel)
    comp = GEO_FOR[name]
    lo, hi = geo.component_extent(comp)
    Usd.ModelAPI(prim).SetKind(Kind.Tokens.component)
    UsdGeom.ModelAPI.Apply(prim.GetPrim()).SetExtentsHint([lo, hi])
    return prim


def _author_village_block(stage):
    """Author the assembly: instanceable references to Hut components."""
    name = CATALOG["blocks"][0]
    root = UsdGeom.Xform.Define(stage, asset_path(name))
    huts = CATALOG["huts"]
    grid = [(-6.0, -6.0), (6.0, -6.0), (-6.0, 6.0), (6.0, 6.0)]
    lo = Gf.Vec3f(1e30, 1e30, 1e30)
    hi = Gf.Vec3f(-1e30, -1e30, -1e30)
    hut_lo, hut_hi = geo.component_extent("hut")
    for i, (hx, hz) in enumerate(grid):
        hut = huts[i % len(huts)]
        block = stage.DefinePrim(Sdf.Path(asset_path(name) + "/Block_%d" % i))
        block.GetReferences().AddInternalReference(Sdf.Path(asset_path(hut)))
        block.SetInstanceable(True)
        UsdGeom.Xformable(block).AddTranslateOp().Set(Gf.Vec3d(hx, 0.0, hz))
        # Accumulate the assembly extentsHint from the placed hut bounds.
        offs = Gf.Vec3f(hx, 0.0, hz)
        blo = hut_lo + offs
        bhi = hut_hi + offs
        lo = Gf.Vec3f(min(lo[0], blo[0]), min(lo[1], blo[1]), min(lo[2], blo[2]))
        hi = Gf.Vec3f(max(hi[0], bhi[0]), max(hi[1], bhi[1]), max(hi[2], bhi[2]))
    Usd.ModelAPI(root).SetKind(Kind.Tokens.assembly)
    UsdGeom.ModelAPI.Apply(root.GetPrim()).SetExtentsHint([lo, hi])
    return root


def author_asset_library(stage, geo_rels):
    """Author ``/Assets/*`` referencing the geo files.

    ``geo_rels`` maps geo-component name -> relative reference path (from
    ``geo.author_geo_files``).  Iteration order is fixed for determinism.
    """
    UsdGeom.Scope.Define(stage, ASSETS_ROOT)

    # Leaf components (everything the DAG bottoms out on), in catalog order.
    leaf_roles = ["palms", "broadleaf", "rocks", "foliage", "huts",
                  "boats", "docks", "market", "flowers"]
    for role in leaf_roles:
        for name in CATALOG[role]:
            _author_component_asset(stage, name, geo_rels[GEO_FOR[name]])

    # Assembly last (references the huts authored above).
    _author_village_block(stage)
