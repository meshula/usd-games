
"""Town department: a road network + buildings draped on the island field.

This module authors a fourth ``/Island`` department -- ``/Island/Town`` -- as a
peer to Terrain/Vegetation/Village.  It mirrors ``layers.py``: leaf layers author
``/Island`` as an ``over`` and only the master ``def``s it, so the town composes
onto the island through sublayers with no root remapping.  A standalone
``town.usd*`` master (subLayers ``[buildings, roads]``) is also a valid openable
stage on its own.

    roads.usd*      /Island/Town/Roads/{Nodes,Highways,Streets}
    buildings.usd*  /Island/Town/Buildings -- instanced-box PointInstancer
    town.usd*       master: subLayers = [buildings, roads]

**Roads as a curve network + graph (Phase 4b).**  Roads are authored **first as
``UsdGeom.BasisCurves``** (``type = linear``, one curve per graph edge), not as
extruded ribbon meshes: the island is meant to be a traffic-sim / game substrate,
and boids can path-follow basis curves at negligible cost.  The intersection
graph is authored alongside the curves so the sim substrate is complete:

  * ``Roads/Nodes`` -- a ``UsdGeom.Points`` prim of the draped junction points
    (plus ``int64[] ids``); the shared node index space.
  * ``Roads/Highways`` / ``Roads/Streets`` -- two ``BasisCurves`` prims, one
    curve per edge of that class, each subdivided so the linear curve hugs the
    terrain; per-prim **constant** ``widths`` encode the road class.  Each curve
    carries ``int[] primvars:roadnet:startNode``/``:endNode`` (uniform =
    per-curve) indexing ``Nodes``, so edges->nodes is explicit and node->edges is
    derivable.

Extruded ribbon roads are available as an *additional* representation (Phase 4d)
behind a ``--road-geom {curves,ribbon,both}`` switch (``curves`` is the default).
``_author_ribbon_mesh`` extrudes a draped, constant-width quad strip per edge --
subdivided like the curves so it hugs the terrain -- under
``Roads/{HighwaysRibbon,StreetsRibbon}``; the curves keep the graph adjacency.

**Segment source (Phase 4a/4b).**  The segments/graph come from the pure
``island_lib.roadnet`` citygen port: ``generate_segments`` grows the
network and ``build_graph`` derives ``(nodes, edges)`` from shared endpoints.
For ``--population island`` the adapter seeds the root at the most-populated point
(``_argmax_population``) and enables ``roadnet``'s water gate, so roads stay on
populated land and off the ocean.

**Interchange types.**

  * Road graph -- ``nodes = [(x, y), ...]`` and
    ``edges = [(node_i, node_j, is_highway), ...]`` from ``roadnet.build_graph``.
  * Building -- ``(x, y, direction_deg, width, depth, height)``, produced by
    ``roadnet.generate_buildings`` (the citygen port: rejection/pushout off roads
    and each other, population-gated) via ``Building.placement()``.

The population source is reused verbatim from Phase 2
(``population.IslandPopulation`` / ``population.NoisePopulation``); this module
does not redefine it.

Determinism: every count and transform derives from a single seeded RNG or from
the segment/building index, and iteration order is fixed, so identical arguments
yield byte-identical ``.usda`` output.  Units are meters.
"""

import math
import os
import random

from pxr import Usd, UsdGeom, UsdShade, Sdf, Gf, Kind

from . import geo
from . import population as population_mod
from . import roadnet
from . import scatter


ISLAND_ROOT = "/Island"
TOWN_ROOT = ISLAND_ROOT + "/Town"
HYDRO_ROOT = ISLAND_ROOT + "/Hydrology"
DEFAULT_TCPS = 24.0

# Road widths (meters) and the small vertical lift that keeps the flat ribbon
# from z-fighting the terrain it is draped on.  Branch-length/deviation
# constants land with the Phase-4 algorithm port.
HIGHWAY_WIDTH = 12.0
STREET_WIDTH = 6.0
ROAD_OFFSET = 0.3


# --------------------------------------------------------------------------- #
# Low-level Sdf authoring helpers (mirrors layers.py)                         #
# --------------------------------------------------------------------------- #

def _over_prim(layer, path):
    spec = Sdf.CreatePrimInLayer(layer, path)
    spec.specifier = Sdf.SpecifierOver
    return spec


def _union(box_a, box_b):
    """Union of two ((minx,miny,minz),(maxx,maxy,maxz)) boxes; None is identity."""
    if box_a is None:
        return box_b
    if box_b is None:
        return box_a
    (a0, a1), (b0, b1) = box_a, box_b
    lo = tuple(min(a0[i], b0[i]) for i in range(3))
    hi = tuple(max(a1[i], b1[i]) for i in range(3))
    return (lo, hi)


def _extent_attr(box):
    """Turn a box (or None) into the [min, max] Vec3f pair for an extent attr."""
    if box is None:
        return [Gf.Vec3f(0.0, 0.0, 0.0), Gf.Vec3f(0.0, 0.0, 0.0)]
    (lo, hi) = box
    return [Gf.Vec3f(*lo), Gf.Vec3f(*hi)]


# --------------------------------------------------------------------------- #
# Island adapter helpers                                                      #
# --------------------------------------------------------------------------- #

def _argmax_population(field, pop, samples=96):
    """Return the ``(x, y)`` of maximum population over the island square.

    A coarse deterministic grid scan; used to seed ``roadnet``'s root highway in
    a populated place (the harbour) rather than at the island's unpopulated peak.
    Cheap: samples x samples evaluations, once.
    """
    n = max(2, int(samples))
    radius = field.radius
    step = (2.0 * radius) / (n - 1)
    best_p = -1.0
    best_xy = (0.0, 0.0)
    for row in range(n):
        y = radius - row * step
        for col in range(n):
            x = -radius + col * step
            p = pop.sample(x, y)
            if p > best_p:
                best_p = p
                best_xy = (x, y)
    return best_xy


# --------------------------------------------------------------------------- #
# Roads                                                                       #
# --------------------------------------------------------------------------- #

def _drape(field, x, y):
    """2D ``(x, y)`` -> draped USD ``(x, height+ROAD_OFFSET, y)`` Vec3f."""
    h = field.height(x, y) + ROAD_OFFSET
    return Gf.Vec3f(x, h, y), (x, h, y)


def _subdivide(a, b, step):
    """Points from ``a`` to ``b`` inclusive, spaced ~``step`` (>= 2 points).

    Subdividing each edge lets a *linear* basis curve hug the terrain between its
    two graph nodes while keeping the one-curve-per-edge graph mapping intact.
    """
    dx = b[0] - a[0]
    dy = b[1] - a[1]
    length = math.hypot(dx, dy)
    k = max(1, int(math.ceil(length / step)))
    return [(a[0] + dx * (i / k), a[1] + dy * (i / k)) for i in range(k + 1)]


def _author_nodes(stage, field, path, nodes):
    """Author the shared junction points as a ``UsdGeom.Points`` prim.

    ``points`` are the draped node positions; ``ids`` is the node index space
    referenced by the per-curve ``startNode``/``endNode`` primvars.  Returns the
    extent box (or None if empty).
    """
    points_prim = UsdGeom.Points.Define(stage, path)
    positions = []
    ids = []
    box = None
    for i, (x, y) in enumerate(nodes):
        vec, pt = _drape(field, x, y)
        positions.append(vec)
        ids.append(i)
        box = _union(box, (pt, pt))
    points_prim.CreatePointsAttr(positions)
    points_prim.CreateIdsAttr(ids)
    points_prim.CreateExtentAttr(_extent_attr(box))
    return box


def _road_edge_drape(field, pts2d, over_water):
    """Deck heights (no ROAD_OFFSET) along an edge, bridging over-water runs.

    Returns ``(deck, water)``: ``deck[k]`` is draped to the terrain on land, but
    across any maximal run of ``over_water`` samples it is held on the straight
    chord between the two flanking banks -- so the road *bridges* the channel/sea
    at bank height instead of diving to the bed.  ``water[k]`` flags the bridged
    samples (used to keep the ribbon deck flat across its width there).
    """
    n = len(pts2d)
    base = [field.height(px, py) for (px, py) in pts2d]
    water = [bool(over_water(px, py)) for (px, py) in pts2d]
    deck = list(base)
    i = 0
    while i < n:
        if not water[i]:
            i += 1
            continue
        j = i
        while j < n and water[j]:
            j += 1
        left, right = i - 1, j
        if left >= 0 and right < n:
            hl, hr = base[left], base[right]
            span = right - left
            for k in range(i, j):
                deck[k] = hl + (hr - hl) * ((k - left) / span)
        elif left >= 0:                       # runs off the end -> hold bank
            for k in range(i, j):
                deck[k] = base[left]
        elif right < n:                       # runs from the start -> hold bank
            for k in range(i, j):
                deck[k] = base[right]
        i = j
    for k in range(n):
        if deck[k] < base[k]:                 # never sink below the ground
            deck[k] = base[k]
    return deck, water


def _author_curve_set(stage, field, path, nodes, edges, width, drape_step,
                      over_water):
    """Author one ``BasisCurves`` prim: one linear curve per edge of a class.

    Each edge is subdivided and draped so the curve follows the terrain (and
    *bridges* over water via ``_road_edge_drape`` -- the sim path stays on the
    deck, never in the river); every curve shares the class ``width`` (constant
    interpolation).  Per-curve ``int[] primvars:roadnet:startNode``/``:endNode``
    (uniform) index ``nodes``.  Returns the extent box.
    """
    curves = UsdGeom.BasisCurves.Define(stage, path)
    curves.CreateTypeAttr(UsdGeom.Tokens.linear)
    curves.CreateWrapAttr(UsdGeom.Tokens.nonperiodic)

    points = []
    counts = []
    start_nodes = []
    end_nodes = []
    box = None
    for (i, j, _is_highway) in edges:
        pts2d = _subdivide(nodes[i], nodes[j], drape_step)
        deck, _water = _road_edge_drape(field, pts2d, over_water)
        counts.append(len(pts2d))
        for k, (px, py) in enumerate(pts2d):
            h = deck[k] + ROAD_OFFSET
            points.append(Gf.Vec3f(px, h, py))
            box = _union(box, ((px, h, py), (px, h, py)))
        start_nodes.append(i)
        end_nodes.append(j)

    curves.CreatePointsAttr(points)
    curves.CreateCurveVertexCountsAttr(counts)
    curves.CreateWidthsAttr([width])
    curves.SetWidthsInterpolation(UsdGeom.Tokens.constant)
    curves.CreateExtentAttr(_extent_attr(box))

    primvars = UsdGeom.PrimvarsAPI(curves)
    start_pv = primvars.CreatePrimvar(
        "roadnet:startNode", Sdf.ValueTypeNames.IntArray, UsdGeom.Tokens.uniform)
    start_pv.Set(start_nodes)
    end_pv = primvars.CreatePrimvar(
        "roadnet:endNode", Sdf.ValueTypeNames.IntArray, UsdGeom.Tokens.uniform)
    end_pv.Set(end_nodes)
    return box


def _author_road_material(stage, path):
    """A simple asphalt ``UsdPreviewSurface`` placeholder bound to road ribbons."""
    mat = UsdShade.Material.Define(stage, path)
    pbr = UsdShade.Shader.Define(stage, path + "/PreviewSurface")
    pbr.CreateIdAttr("UsdPreviewSurface")
    pbr.CreateInput("diffuseColor", Sdf.ValueTypeNames.Color3f).Set(
        Gf.Vec3f(0.09, 0.09, 0.10))
    pbr.CreateInput("roughness", Sdf.ValueTypeNames.Float).Set(0.9)
    pbr.CreateInput("metallic", Sdf.ValueTypeNames.Float).Set(0.0)
    mat.CreateSurfaceOutput().ConnectToSource(
        pbr.CreateOutput("surface", Sdf.ValueTypeNames.Token))
    return mat


def _author_road_ribbon(stage, field, path, node_a, node_b, width, drape_step,
                        over_water, material):
    """Author one road edge as its own draped ribbon ``UsdGeom.Mesh``.

    Separate per-edge meshes (like the river water) so an artist can shape-merge
    or replace individual spans.  The strip conforms to the terrain across its
    width on land, and over water it holds a **flat bridge deck** at the bridged
    centreline height (``_road_edge_drape``) so the road spans the channel/sea
    instead of draping into it.  ``st`` runs ``u`` 0->1 across, ``v`` 0->1 along
    the length (arc length), ready for a road material.  Returns the extent box.
    """
    pts = _subdivide(node_a, node_b, drape_step)
    n = len(pts)
    if n < 2:
        return None
    hw = width * 0.5
    deck, water = _road_edge_drape(field, pts, over_water)

    centre = [Gf.Vec3f(pts[k][0], deck[k] + ROAD_OFFSET, pts[k][1])
              for k in range(n)]
    cum = [0.0]
    for k in range(1, n):
        cum.append(cum[-1] + (centre[k] - centre[k - 1]).GetLength())
    total = cum[-1] or 1.0

    points = []
    sts = []
    box = None
    for m in range(n):
        if m == 0:
            tx, ty = pts[1][0] - pts[0][0], pts[1][1] - pts[0][1]
        elif m == n - 1:
            tx, ty = pts[m][0] - pts[m - 1][0], pts[m][1] - pts[m - 1][1]
        else:
            tx, ty = pts[m + 1][0] - pts[m - 1][0], pts[m + 1][1] - pts[m - 1][1]
        tl = math.hypot(tx, ty) or 1.0
        nx, ny = -ty / tl, tx / tl
        lx, ly = pts[m][0] + nx * hw, pts[m][1] + ny * hw
        rx, ry = pts[m][0] - nx * hw, pts[m][1] - ny * hw
        if water[m]:
            hy = deck[m] + ROAD_OFFSET        # flat deck across the bridge
            lh = rh = hy
        else:
            lh = field.height(lx, ly) + ROAD_OFFSET   # conform to terrain
            rh = field.height(rx, ry) + ROAD_OFFSET
        points.append(Gf.Vec3f(lx, lh, ly))
        points.append(Gf.Vec3f(rx, rh, ry))
        v = cum[m] / total
        sts.append(Gf.Vec2f(0.0, v))
        sts.append(Gf.Vec2f(1.0, v))
        box = _union(box, ((lx, lh, ly), (lx, lh, ly)))
        box = _union(box, ((rx, rh, ry), (rx, rh, ry)))

    counts = []
    indices = []
    for m in range(n - 1):
        lm = 2 * m
        rm = lm + 1
        ln = 2 * (m + 1)
        rn = ln + 1
        counts.append(4)
        indices.extend((lm, ln, rn, rm))      # CCW from +Y (matches terrain_grid)

    mesh = UsdGeom.Mesh.Define(stage, path)
    mesh.CreatePointsAttr(points)
    mesh.CreateFaceVertexCountsAttr(counts)
    mesh.CreateFaceVertexIndicesAttr(indices)
    mesh.CreateSubdivisionSchemeAttr(UsdGeom.Tokens.none)
    mesh.CreateExtentAttr(_extent_attr(box))
    UsdGeom.PrimvarsAPI(mesh).CreatePrimvar(
        "st", Sdf.ValueTypeNames.TexCoord2fArray, UsdGeom.Tokens.vertex).Set(sts)
    UsdShade.MaterialBindingAPI.Apply(mesh.GetPrim())
    UsdShade.MaterialBindingAPI(mesh.GetPrim()).Bind(material)
    return box


def author_roads(layer, field, nodes, edges, drape_step, road_geom="curves",
                 over_water=None):
    """Author the road network + graph under ``/Island/Town/Roads`` on ``layer``.

    Always authors ``Roads/Nodes`` (junction ``Points``, the graph's node index
    space).  ``road_geom`` selects the road geometry:

      * ``curves`` (default) -- ``Roads/Highways`` / ``Roads/Streets`` as
        ``BasisCurves`` (one linear, draped/bridged curve per edge, per-prim
        constant width, ``startNode``/``endNode`` graph primvars).  The boid-ready
        substrate; carries the graph adjacency.
      * ``ribbon`` -- ``Roads/Ribbon/{Highway,Street}_<k>``, a **separate** draped
        ribbon ``Mesh`` per edge (for artist shape-merge), with ``st`` and an
        asphalt material.
      * ``both`` -- authors curves and ribbons.

    ``over_water(x, y) -> bool`` flags where a road must bridge instead of drape
    (river channels / sea); defaults to ``field.is_water``.  ``nodes``/``edges``
    come from ``roadnet.build_graph``.  Returns the union extent box.
    """
    if road_geom not in ("curves", "ribbon", "both"):
        raise ValueError("road_geom must be 'curves', 'ribbon' or 'both'")
    if over_water is None:
        over_water = field.is_water

    stage = Usd.Stage.Open(layer)
    _over_prim(layer, ISLAND_ROOT)
    UsdGeom.Scope.Define(stage, TOWN_ROOT)
    UsdGeom.Scope.Define(stage, TOWN_ROOT + "/Roads")

    highways = [e for e in edges if e[2]]
    streets = [e for e in edges if not e[2]]

    box = None
    box = _union(box, _author_nodes(
        stage, field, TOWN_ROOT + "/Roads/Nodes", nodes))

    if road_geom in ("curves", "both"):
        box = _union(box, _author_curve_set(
            stage, field, TOWN_ROOT + "/Roads/Highways", nodes, highways,
            HIGHWAY_WIDTH, drape_step, over_water))
        box = _union(box, _author_curve_set(
            stage, field, TOWN_ROOT + "/Roads/Streets", nodes, streets,
            STREET_WIDTH, drape_step, over_water))

    if road_geom in ("ribbon", "both"):
        UsdGeom.Scope.Define(stage, TOWN_ROOT + "/Roads/Ribbon")
        road_mat = _author_road_material(
            stage, TOWN_ROOT + "/Roads/Ribbon/RoadMaterial")
        for cls, cls_edges, cls_width in (("Highway", highways, HIGHWAY_WIDTH),
                                          ("Street", streets, STREET_WIDTH)):
            for k, (i, j, _h) in enumerate(cls_edges):
                box = _union(box, _author_road_ribbon(
                    stage, field,
                    "{}/Roads/Ribbon/{}_{}".format(TOWN_ROOT, cls, k),
                    nodes[i], nodes[j], cls_width, drape_step, over_water,
                    road_mat))

    stage.GetRootLayer().Save()
    return box


# --------------------------------------------------------------------------- #
# Rivers (hydrology curve network)                                            #
# --------------------------------------------------------------------------- #

def _author_river_channels(stage, field, path, edges):
    """Author the river channels as one draped ``BasisCurves`` prim.

    One linear curve per river edge, following the channel polyline (already at
    hydrology-grid spacing, so it hugs the terrain) draped to the terrain.  Each
    curve is directed downstream (first point = upstream node, last = outlet).
    Per-curve **uniform** ``widths`` carry the Strahler-order width, and
    ``int[] primvars:hydro:{startNode,endNode,order}`` (uniform = per-curve) carry
    the directed graph adjacency + stream order.  Returns the extent box.
    """
    curves = UsdGeom.BasisCurves.Define(stage, path)
    curves.CreateTypeAttr(UsdGeom.Tokens.linear)
    curves.CreateWrapAttr(UsdGeom.Tokens.nonperiodic)
    points = []
    counts = []
    widths = []
    start_nodes = []
    end_nodes = []
    orders = []
    box = None
    for e in edges:
        pts = e["points"]
        if len(pts) < 2:
            continue
        counts.append(len(pts))
        for (px, py) in pts:
            vec, pt = _drape(field, px, py)
            points.append(vec)
            box = _union(box, (pt, pt))
        widths.append(e["width"])
        start_nodes.append(e["u"])
        end_nodes.append(e["v"])
        orders.append(e["order"])

    curves.CreatePointsAttr(points)
    curves.CreateCurveVertexCountsAttr(counts)
    curves.CreateWidthsAttr(widths)
    curves.SetWidthsInterpolation(UsdGeom.Tokens.uniform)
    curves.CreateExtentAttr(_extent_attr(box))

    primvars = UsdGeom.PrimvarsAPI(curves)
    for name, vals in (("hydro:startNode", start_nodes),
                       ("hydro:endNode", end_nodes),
                       ("hydro:order", orders)):
        pv = primvars.CreatePrimvar(name, Sdf.ValueTypeNames.IntArray,
                                    UsdGeom.Tokens.uniform)
        pv.Set(vals)
    return box


def _author_water_material(stage, path):
    """A simple translucent-blue ``UsdPreviewSurface`` placeholder for water.

    Bound to the river ribbons so they read as water in usdview; the per-ribbon
    ``st`` (0->1 downhill) is authored so a flowing-water shader can replace this
    with a scrolling/animated surface later.
    """
    mat = UsdShade.Material.Define(stage, path)
    pbr = UsdShade.Shader.Define(stage, path + "/PreviewSurface")
    pbr.CreateIdAttr("UsdPreviewSurface")
    pbr.CreateInput("diffuseColor", Sdf.ValueTypeNames.Color3f).Set(
        Gf.Vec3f(0.04, 0.20, 0.35))
    pbr.CreateInput("roughness", Sdf.ValueTypeNames.Float).Set(0.15)
    pbr.CreateInput("metallic", Sdf.ValueTypeNames.Float).Set(0.0)
    pbr.CreateInput("opacity", Sdf.ValueTypeNames.Float).Set(0.6)
    surf = pbr.CreateOutput("surface", Sdf.ValueTypeNames.Token)
    mat.CreateSurfaceOutput().ConnectToSource(surf)
    return mat


def _author_river_water(stage, field, edges, water_material,
                        width_scale=3.0, min_width=8.0):
    """Author a draped water ribbon per river edge under ``Rivers/Water``.

    Each edge becomes its **own** ``UsdGeom.Mesh`` (a bunch of separate ribbons,
    so a flowing-water material can animate each independently): a constant-width
    quad strip extruded tangent to the draped channel polyline, wide enough
    (``max(min_width, order_width * width_scale)``) to intersect the incised
    channel.  ``st`` runs ``u`` 0->1 across the width and ``v`` 0->1 down the
    downhill (upstream->outlet) direction by fractional arc length, ready for a
    scrolling water shader.  Returns the union extent box.
    """
    UsdGeom.Scope.Define(stage, HYDRO_ROOT + "/Rivers/Water")
    box = None
    for k, e in enumerate(edges):
        pts = e["points"]
        n = len(pts)
        if n < 2:
            continue
        hw = 0.5 * max(min_width, e["width"] * width_scale)

        # Draped centreline vertices first, for the downhill arc-length param.
        centre = [_drape(field, px, py)[0] for (px, py) in pts]
        cum = [0.0]
        for i in range(1, n):
            cum.append(cum[-1] + (centre[i] - centre[i - 1]).GetLength())
        total = cum[-1] or 1.0

        points = []
        sts = []
        ebox = None
        for m in range(n):
            if m == 0:
                tx, ty = pts[1][0] - pts[0][0], pts[1][1] - pts[0][1]
            elif m == n - 1:
                tx, ty = pts[m][0] - pts[m - 1][0], pts[m][1] - pts[m - 1][1]
            else:
                tx, ty = pts[m + 1][0] - pts[m - 1][0], pts[m + 1][1] - pts[m - 1][1]
            tl = math.hypot(tx, ty) or 1.0
            nx, ny = -ty / tl, tx / tl
            lv, lp = _drape(field, pts[m][0] + nx * hw, pts[m][1] + ny * hw)
            rv, rp = _drape(field, pts[m][0] - nx * hw, pts[m][1] - ny * hw)
            points.append(lv)
            points.append(rv)
            v = cum[m] / total
            sts.append(Gf.Vec2f(0.0, v))
            sts.append(Gf.Vec2f(1.0, v))
            ebox = _union(ebox, (lp, lp))
            ebox = _union(ebox, (rp, rp))

        counts = []
        indices = []
        for m in range(n - 1):
            lm = 2 * m
            rm = lm + 1
            ln = 2 * (m + 1)
            rn = ln + 1
            counts.append(4)
            indices.extend((lm, ln, rn, rm))   # CCW from +Y (matches terrain_grid)

        mesh = UsdGeom.Mesh.Define(
            stage, HYDRO_ROOT + "/Rivers/Water/River_{}".format(k))
        mesh.CreatePointsAttr(points)
        mesh.CreateFaceVertexCountsAttr(counts)
        mesh.CreateFaceVertexIndicesAttr(indices)
        mesh.CreateSubdivisionSchemeAttr(UsdGeom.Tokens.none)
        mesh.CreateExtentAttr(_extent_attr(ebox))
        UsdGeom.PrimvarsAPI(mesh).CreatePrimvar(
            "st", Sdf.ValueTypeNames.TexCoord2fArray,
            UsdGeom.Tokens.vertex).Set(sts)
        UsdShade.MaterialBindingAPI.Apply(mesh.GetPrim())
        UsdShade.MaterialBindingAPI(mesh.GetPrim()).Bind(water_material)
        box = _union(box, ebox)
    return box


def author_rivers(layer, field, nodes, edges, water=True):
    """Author the river network under ``/Island/Hydrology/Rivers`` on ``layer``.

    ``Rivers/Nodes`` -- junction ``Points`` (sources / confluences / coastal
    outlets), the shared node index space; ``Rivers/Channels`` -- the directed
    ``BasisCurves`` river network.  When ``water`` is set, ``Rivers/Water`` gets a
    separate draped ribbon ``Mesh`` per edge (the flowing-water surface, with
    downhill ``st``).  ``nodes``/``edges`` come from
    ``hydrology.HydrologyMap.rivers()``.  Returns the union extent box.
    """
    stage = Usd.Stage.Open(layer)
    _over_prim(layer, ISLAND_ROOT)
    UsdGeom.Scope.Define(stage, HYDRO_ROOT)
    UsdGeom.Scope.Define(stage, HYDRO_ROOT + "/Rivers")

    box = None
    box = _union(box, _author_nodes(
        stage, field, HYDRO_ROOT + "/Rivers/Nodes", nodes))
    box = _union(box, _author_river_channels(
        stage, field, HYDRO_ROOT + "/Rivers/Channels", edges))
    if water:
        water_mat = _author_water_material(
            stage, HYDRO_ROOT + "/Rivers/Water/WaterMaterial")
        box = _union(box, _author_river_water(
            stage, field, edges, water_mat))

    stage.GetRootLayer().Save()
    return box


# --------------------------------------------------------------------------- #
# Buildings                                                                   #
# --------------------------------------------------------------------------- #

def _author_unit_box_proto(stage, path):
    """A 1x1x1 box Mesh (base on y=0), in the style of ``geo._author_box_mesh``.

    Point-instanced and scaled per building, so a unit box keeps the per-instance
    ``scales`` == ``(width, height, depth)``.
    """
    mesh = UsdGeom.Mesh.Define(stage, path)
    mesh.CreatePointsAttr(geo._box_points(1.0, 1.0, 1.0))
    mesh.CreateFaceVertexCountsAttr(geo._BOX_FACE_COUNTS)
    mesh.CreateFaceVertexIndicesAttr(geo._BOX_FACE_INDICES)
    mesh.CreateSubdivisionSchemeAttr(UsdGeom.Tokens.none)
    mesh.CreateExtentAttr([Gf.Vec3f(-0.5, 0.0, -0.5), Gf.Vec3f(0.5, 1.0, 0.5)])
    return mesh


def author_buildings(layer, field, buildings):
    """Instanced-box buildings as a PointInstancer under ``/Island/Town``.

    One unit-box prototype (``.../Buildings/Prototypes/proto_0``); per-instance
    ``positions`` are draped to the terrain, ``orientations`` rotate about Y by
    the segment direction, and ``scales`` carry ``(width, height, depth)``.
    Returns the instancer extent box (or None if there are no buildings).
    """
    stage = Usd.Stage.Open(layer)
    _over_prim(layer, ISLAND_ROOT)
    UsdGeom.Scope.Define(stage, TOWN_ROOT)

    pi = UsdGeom.PointInstancer.Define(stage, TOWN_ROOT + "/Buildings")
    proto_container = TOWN_ROOT + "/Buildings/Prototypes"
    UsdGeom.Scope.Define(stage, proto_container)
    proto0 = proto_container + "/proto_0"
    _author_unit_box_proto(stage, proto0)
    pi.CreatePrototypesRel().SetTargets([Sdf.Path(proto0)])

    positions = []
    orientations = []
    scales = []
    proto_indices = []
    box = None
    for (x, y, direction_deg, width, depth, height) in buildings:
        h = field.height(x, y)
        positions.append(Gf.Vec3f(x, h, y))
        # Quaternion about +Y by the segment direction.
        quat = Gf.Rotation(Gf.Vec3d(0.0, 1.0, 0.0), direction_deg).GetQuat()
        im = quat.GetImaginary()
        orientations.append(
            Gf.Quath(quat.GetReal(), im[0], im[1], im[2]))
        scales.append(Gf.Vec3f(width, height, depth))
        proto_indices.append(0)
        # Conservative per-instance AABB: horizontal half-extent covers any Y
        # rotation of the width x depth footprint; vertical spans [0, height].
        r = 0.5 * math.hypot(width, depth)
        box = _union(box, ((x - r, h, y - r), (x + r, h + height, y + r)))

    pi.CreatePositionsAttr(positions)
    pi.CreateOrientationsAttr(orientations)
    pi.CreateScalesAttr(scales)
    pi.CreateProtoIndicesAttr(proto_indices)
    pi.CreateExtentAttr(_extent_attr(box))

    stage.GetRootLayer().Save()
    return box


# --------------------------------------------------------------------------- #
# Master assembly                                                             #
# --------------------------------------------------------------------------- #

def author_town_master(layer, sublayer_rel_paths, extent):
    """Compose the town sublayers into a standalone openable ``town`` master.

    Mirrors ``layers.author_master``: subLayers strongest-first, ``/Island``
    ``def``d as a ``Kind.assembly`` default prim, Y-up / metersPerUnit 1.0 / tcps.
    A top-level ``extentsHint`` is authored on ``/Island/Town`` via
    ``UsdGeom.ModelAPI`` from the caller-supplied ``extent`` box.
    """
    stage = Usd.Stage.Open(layer)
    layer.subLayerPaths.clear()
    for p in sublayer_rel_paths:
        layer.subLayerPaths.append(p)

    island = UsdGeom.Xform.Define(stage, ISLAND_ROOT)
    Usd.ModelAPI(island).SetKind(Kind.Tokens.assembly)
    stage.SetDefaultPrim(island.GetPrim())

    UsdGeom.SetStageUpAxis(stage, UsdGeom.Tokens.y)
    UsdGeom.SetStageMetersPerUnit(stage, 1.0)
    stage.SetTimeCodesPerSecond(DEFAULT_TCPS)
    stage.SetFramesPerSecond(DEFAULT_TCPS)

    # The sublayers define /Island/Town; author its extentsHint here as an over.
    town_prim = stage.GetPrimAtPath(TOWN_ROOT)
    if town_prim and town_prim.IsValid():
        UsdGeom.ModelAPI.Apply(town_prim).SetExtentsHint(_extent_attr(extent))

    stage.GetRootLayer().Save()
    return layer


# --------------------------------------------------------------------------- #
# Top-level orchestration                                                     #
# --------------------------------------------------------------------------- #

def _default_drape_step(field, size_km):
    """Road-curve drape spacing (~15 m): fine enough to follow gentle relief
    without tripling curve vertex counts along with the denser terrain mesh."""
    res = max(32, min(int(size_km * 60), 192))
    return (2.0 * field.radius) / res


def build_town_scene(output_dir, fmt="usdc", seed=1751, population="island",
                     size_km=2.0, segment_limit=2000, building_period=5,
                     buildings_per_segment=10, highway_length=160.0,
                     street_length=90.0, snap_distance=30.0,
                     branch_angle_dev=3.0, straight_angle_dev=15.0,
                     min_intersection_dev=30.0, normal_pop_threshold=0.15,
                     highway_pop_threshold=0.15, road_drape_step=None,
                     grade_weight=2.0, grade_cone_dev=25.0,
                     road_geom="curves", erosion_iterations=0,
                     erosion_strength=1.0, field=None, over_water=None):
    """Generate the standalone town scene into ``output_dir``.

    Grows the road network with ``roadnet`` (citygen port), derives the graph,
    and authors it as draped ``BasisCurves`` + a ``Nodes`` points prim, plus an
    instanced-box PointInstancer of buildings lined along the segments.

    ``field`` may be a pre-built ``IslandField`` to drape on (so an integrating
    caller shares one eroded field); otherwise one is constructed from
    ``size_km``/``seed`` and the ``erosion_*`` parameters.

    Returns a dict with the ``roads``/``buildings``/``master`` absolute paths and
    the ``n_segments``/``n_buildings``/``n_nodes``/``n_edges``/``n_highways``/
    ``n_streets`` counts.
    """
    if fmt not in ("usda", "usdc"):
        raise ValueError("format must be 'usda' or 'usdc'")
    if road_geom not in ("curves", "ribbon", "both"):
        raise ValueError("road_geom must be 'curves', 'ribbon' or 'both'")
    os.makedirs(output_dir, exist_ok=True)

    # The field drives terrain draping regardless of which population source is
    # chosen.  Reuse a caller-supplied field (e.g. an already-eroded one) or
    # build one from size/seed + erosion parameters.
    if field is None:
        field = scatter.IslandField(size_km=size_km, seed=seed,
                                    erosion_iterations=erosion_iterations,
                                    erosion_strength=erosion_strength)
    if population == "island":
        pop = population_mod.IslandPopulation(field)
    elif population == "noise":
        pop = population_mod.NoisePopulation(seed)
    else:
        raise ValueError("population must be 'noise' or 'island'")

    # Build the road-network parameters.  For --population island the peak/origin
    # is unpopulated, so seed the root at the most-populated point and enable the
    # water gate (roadnet rejects segments whose frontier is over water, where
    # IslandPopulation is exactly 0).  For --population noise the origin is
    # populated and there is no water, so keep the faithful citygen defaults.
    params = roadnet.RoadNetworkParams(
        segment_limit=segment_limit,
        highway_length=highway_length,
        street_length=street_length,
        snap_distance=snap_distance,
        branch_angle_dev=branch_angle_dev,
        straight_angle_dev=straight_angle_dev,
        min_intersection_dev=min_intersection_dev,
        normal_pop_threshold=normal_pop_threshold,
        highway_pop_threshold=highway_pop_threshold,
        grade_weight=grade_weight,
        grade_cone_dev=grade_cone_dev,
        building_segment_period=building_period,
        building_count_per_segment=buildings_per_segment,
    )
    if population == "island":
        params.root_origin = _argmax_population(field, pop)
        params.water_pop_threshold = 0.0

    # Terrain-following (grade) bias: passing the field lets roadnet steer
    # arterials along isoclines when grade_weight > 0.  Roads always drape on the
    # same field, so the bias applies to both population sources.
    rng = random.Random(seed)
    segments = roadnet.generate_segments(rng, pop, params, terrain=field)
    nodes, edges = roadnet.build_graph(segments, params.node_merge_eps)

    # Buildings via the citygen generate_buildings port (rejection/pushout off
    # roads and each other, population-gated).  A separate deterministic stream
    # keeps building placement stable when the road generator is retuned.
    build_rng = random.Random(seed + 1)
    buildings = [b.placement()
                 for b in roadnet.generate_buildings(build_rng, segments, pop,
                                                      params)]

    if road_drape_step is None:
        road_drape_step = _default_drape_step(field, size_km)

    roads_name = "roads.{}".format(fmt)
    buildings_name = "buildings.{}".format(fmt)
    master_name = "town.{}".format(fmt)
    paths = {
        "roads": os.path.join(output_dir, roads_name),
        "buildings": os.path.join(output_dir, buildings_name),
        "master": os.path.join(output_dir, master_name),
    }

    roads_layer = Sdf.Layer.CreateNew(paths["roads"])
    roads_box = author_roads(roads_layer, field, nodes, edges, road_drape_step,
                             road_geom=road_geom, over_water=over_water)

    buildings_layer = Sdf.Layer.CreateNew(paths["buildings"])
    buildings_box = author_buildings(buildings_layer, field, buildings)

    master_layer = Sdf.Layer.CreateNew(paths["master"])
    author_town_master(master_layer, [buildings_name, roads_name],
                       _union(roads_box, buildings_box))

    n_highways = sum(1 for e in edges if e[2])
    paths["n_segments"] = len(segments)
    paths["n_buildings"] = len(buildings)
    paths["n_nodes"] = len(nodes)
    paths["n_edges"] = len(edges)
    paths["n_highways"] = n_highways
    paths["n_streets"] = len(edges) - n_highways
    # Geometry for downstream consumers (e.g. vegetation pruning): the road
    # graph and building footprint circles (x, y, radius).
    paths["nodes"] = nodes
    paths["edges"] = edges
    paths["building_footprints"] = [
        (bx, by, 0.5 * math.hypot(bw, bd))
        for (bx, by, _dir, bw, bd, _h) in buildings]
    return paths
