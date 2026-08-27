
"""Draw the composition of a USD scene as an annotated text tree (a tutorial).

Given a top layer (e.g. the island benchmark's ``island.usda``), this writes a
text document with two sections, each drawn with Unicode box-drawing
characters:

  SECTION 1 -- LAYERS
      The *composition graph* of USD files.  Starting at the top layer we
      recurse through ``subLayers`` and follow the ``reference`` / ``payload``
      arcs authored in each layer to the layers they target.  This is the
      file-level structure: which layer pulls in which.

  SECTION 2 -- PRIMS
      The composed *scenegraph*.  We walk the prim hierarchy from the default
      prim (traversal naturally stops at instance boundaries), then print each
      *prototype* as its own small tree.  This is where you see how native
      instancing turns authored references into shared prototypes -- and how
      prototypes that themselves contain instances form the dependency DAG that
      ``UsdGeomBBoxCache`` walks.

Every line is annotated with single-character flags (see the legends emitted at
the top of each section).  ``--max-lines N`` caps the number of lines *per
section* so the report stays readable on large scenes.

This file is written to be read: the two traversals are deliberately explicit
 so it can serve as a worked example of the Sdf layer API
(Section 1) and the Usd instancing/prototype API (Section 2).

Usage:
    python3 composition_tree.py --stage visual-check/island.usda
    python3 composition_tree.py --stage island.usda --output tree.txt --max-lines 200
"""

import argparse
import colorsys
import os
import sys

from pxr import Sdf, Usd, UsdGeom, Gf


# A short reading guide written at the top of every report.
HOW_TO_READ = [
    "HOW TO READ THIS FILE",
    "---------------------",
    "Two sections: LAYERS (which files compose which) and PRIMS (the composed",
    "scenegraph plus the prototypes native instancing produced).",
    "",
    "In SECTION 2 an instance line looks like:",
    "    Boat_0  (Xform)  [i ...]  ->PN",
    "The [i] means it is an *instance*: it holds no geometry of its own, it",
    "SHARES a master.  '->PN' names that master.  The masters are listed lower",
    "down under 'prototypes'; each is drawn once as its own subtree.  (PN ids",
    "are assigned per scene, sorted by source asset -- so find the one you want",
    "by its '<- /Assets/...' label, not by memorizing a number.)",
    "",
    "So to study a particular master -- say the boat -- :",
    "  1. Find its master: search the 'prototypes' list for '<- /Assets/Boat'.",
    "     Suppose it is P0.  The subtree under the line 'P0 =' is the shared",
    "     master; every boat is a copy of exactly that.  The header also gives",
    "     the source asset and how many instances use it.",
    "  2. Every '->P0' elsewhere is one placement (use-site) of that master.",
    "     Search '->P0' to find them all.",
    "  3. A master flagged [* n] itself contains instances: follow its '->'",
    "     chain down (e.g. VillageBlock -> Hut -> wall) to walk the nested",
    "     prototype-dependency DAG the bbox cache resolves.  A use-site printed",
    "     as 'P14/Block_0' means it lives *inside* master P14.",
]


# Unicode box-drawing pieces used to draw the tree.
TEE = "├── "   # "|-- "  a node with siblings below it
ELL = "└── "   # "`-- "  the last node at its level
BAR = "│   "             # "|   "  vertical run in the prefix
GAP = "    "                  # "    "  blank run in the prefix


class _Truncated(Exception):
    """Raised internally to stop a section once --max-lines is reached."""


class LineSink:
    """Collects output lines and enforces the per-section line budget."""

    def __init__(self, max_lines):
        self.lines = []
        self.max_lines = max_lines  # 0 == unlimited
        self.truncated = False

    def add(self, text):
        if self.max_lines and len(self.lines) >= self.max_lines:
            self.truncated = True
            raise _Truncated()
        self.lines.append(text)


# --------------------------------------------------------------------------- #
# SECTION 1 -- the layer composition graph                                    #
# --------------------------------------------------------------------------- #

LAYER_LEGEND = [
    "  S  = subLayer arc          R = reference arc         P = payload arc",
    "  i  = instanceable (the reference generates a prototype)",
    "  xN = N such arcs collapsed into one line",
    "  ->  points at the referenced prim path inside the target layer",
    "  (cycle) = layer already visited on this branch; not re-expanded",
    "  (missing) = target layer could not be opened",
]


def _layer_arcs(layer, scope_path=None):
    """Return the child arcs authored in ``layer``, de-duplicated.

    ``scope_path`` restricts collection to a prim subtree.  When we follow a
    reference into ``assetLibrary.usda</Assets/Boat>`` we scope to that prim so
    we only report what a Boat actually pulls in -- not the whole library.
    ``scope_path=None`` means the whole layer (used for the root and for layers
    reached via a subLayer arc), which is also when ``subLayers`` are listed.

    Each returned item is a dict describing one child edge:
        arc      'S' | 'R' | 'P'
        disp     what to print for the target (asset path, or "(internal)")
        prim     the referenced prim path (references/payloads) or None
        inst     True if any contributing prim spec is instanceable
        count    how many identical arcs collapsed into this line
        abs      resolved identifier of the target layer
        internal True for a same-layer reference (empty asset path)
        scope    prim path to scope the target's expansion to (or None)
    """
    children = []

    if scope_path is None:
        # subLayers are layer-global and ordered (strongest first).
        for sublayer_path in layer.subLayerPaths:
            children.append({
                "arc": "S", "disp": sublayer_path, "prim": None,
                "inst": False, "count": 1, "internal": False,
                "abs": layer.ComputeAbsolutePath(sublayer_path), "scope": None,
            })
        roots = list(layer.rootPrims)
    else:
        ps = layer.GetPrimAtPath(scope_path)
        roots = [ps] if ps else []

    # Aggregate references/payloads in the scoped subtree by
    # (arc, asset path, target prim) so many identical arcs collapse to one
    # counted line.
    agg = {}  # key -> [instanceable_any, count]

    def collect(prim_spec):
        for ref in prim_spec.referenceList.GetAppliedItems():
            _tally(prim_spec, "R", ref.assetPath, ref.primPath)
        for payload in prim_spec.payloadList.GetAppliedItems():
            _tally(prim_spec, "P", payload.assetPath, payload.primPath)
        for child in prim_spec.nameChildren:
            collect(child)

    def _tally(prim_spec, arc, asset_path, prim_path):
        # An empty asset path is an *internal* reference (same layer); we still
        # follow it so transitive edges like VillageBlock -> Hut -> geo appear.
        key = (arc, asset_path, str(prim_path) if prim_path else "")
        entry = agg.setdefault(key, [False, 0])
        entry[0] = entry[0] or bool(prim_spec.instanceable)
        entry[1] += 1

    for root_prim in roots:
        collect(root_prim)

    for (arc, asset_path, prim_path), (inst_any, count) in sorted(agg.items()):
        internal = not asset_path
        children.append({
            "arc": arc,
            "disp": "(internal)" if internal else asset_path,
            "prim": prim_path or None, "inst": inst_any, "count": count,
            "internal": internal,
            "abs": layer.identifier if internal
                   else layer.ComputeAbsolutePath(asset_path),
            "scope": prim_path or None,
        })

    return children


def _format_layer_line(node):
    """Assemble the annotation + label for one layer-arc line."""
    flags = node["arc"]
    if node["inst"]:
        flags += ",i"
    label = "[{}] {}".format(flags, node["disp"])
    if node["prim"]:
        label += "  ->{}".format(node["prim"])
    if node["count"] > 1:
        label += "  x{}".format(node["count"])
    return label


def _emit_layer(abs_path, scope, node, prefix, is_last, ancestors, sink,
                is_root):
    """Recursively emit the layer tree.

    ``abs_path`` identifies the target layer, ``scope`` the prim subtree within
    it to expand (None = whole layer).  A branch is keyed on (layer, scope) so
    the same layer reached at different prims expands independently, while true
    cycles are cut.
    """
    if is_root:
        sink.add(os.path.basename(abs_path))
    else:
        connector = ELL if is_last else TEE
        sink.add(prefix + connector + _format_layer_line(node))

    layer = Sdf.Layer.FindOrOpen(abs_path)
    child_prefix = "" if is_root else prefix + (GAP if is_last else BAR)
    if layer is None:
        sink.add(child_prefix + ELL + "(missing)")
        return

    key = (layer.identifier, str(scope) if scope else "")
    if key in ancestors:
        sink.add(child_prefix + ELL + "(cycle)")
        return

    arcs = _layer_arcs(layer, scope)
    next_ancestors = ancestors | {key}
    for i, child in enumerate(arcs):
        _emit_layer(child["abs"], child["scope"], child, child_prefix,
                    i == len(arcs) - 1, next_ancestors, sink, is_root=False)


def build_layer_section(stage, max_lines):
    sink = LineSink(max_lines)
    root_layer = stage.GetRootLayer()
    try:
        _emit_layer(root_layer.realPath or root_layer.identifier, None, None,
                    "", True, set(), sink, is_root=True)
    except _Truncated:
        pass
    return sink


# --------------------------------------------------------------------------- #
# SECTION 2 -- the composed prim + prototype tree                             #
# --------------------------------------------------------------------------- #

PRIM_LEGEND = [
    "  *  = prototype root            i = instance (-> the prototype it uses)",
    "  A/C/G = kind assembly/component/group",
    "  r/p/g = authored purpose render/proxy/guide",
    "  e  = authored extent           h = authored extentsHint",
    "  t  = time-sampled xform/extent",
    "  PI(protos=P,points=N) = PointInstancer over P prototype-rel targets,",
    "       N instance points; a target that itself contains a PI is nested PI",
    "  n  = (on a prototype) contains instances -> a node in the prototype DAG",
]

_KIND_FLAG = {"assembly": "A", "component": "C", "group": "G",
              "subcomponent": "s"}
_PURPOSE_FLAG = {"render": "r", "proxy": "p", "guide": "g"}


def _time_varied(prim):
    """True if any xformOp or the extent attribute carries >1 time sample."""
    for attr in prim.GetAttributes():
        name = attr.GetName()
        if (name == "extent" or name.startswith("xformOp:")) and \
                attr.GetNumTimeSamples() > 1:
            return True
    return False


def _prim_flags(prim, proto_ids, is_proto_root=False):
    """Build the single-character flag list for a prim line."""
    flags = []
    # The pseudo-root (a stage with no default prim, e.g. the city master) is
    # not a real prim; querying schema attributes on "/" warns.  It carries no
    # flags anyway.
    if prim.IsPseudoRoot():
        return flags
    if is_proto_root:
        flags.append("*")
        # Mark prototypes that themselves contain instances: these are the
        # dependency edges in the prototype DAG the bbox cache resolves.
        if any(p.IsInstance() for p in Usd.PrimRange(prim)):
            flags.append("n")
    if prim.IsInstance():
        flags.append("i")

    kind = Usd.ModelAPI(prim).GetKind()
    if kind in _KIND_FLAG:
        flags.append(_KIND_FLAG[kind])

    purpose = prim.GetAttribute("purpose")
    if purpose and purpose.HasAuthoredValue():
        flags.append(_PURPOSE_FLAG.get(purpose.Get(), "?"))

    extent = prim.GetAttribute("extent")
    if extent and extent.HasAuthoredValue():
        flags.append("e")
    hint = prim.GetAttribute("extentsHint")
    if hint and hint.HasAuthoredValue():
        flags.append("h")

    if _time_varied(prim):
        flags.append("t")
    if prim.IsA(UsdGeom.PointInstancer):
        # Annotate the PI line with the count of prototype-relationship targets
        # (what drives propagation) and the count of instance points (what is
        # drawn). A target that itself contains a PI is a nested PI.
        pi = UsdGeom.PointInstancer(prim)
        n_protos = len(pi.GetPrototypesRel().GetTargets())
        indices = pi.GetProtoIndicesAttr().Get()
        n_points = len(indices) if indices is not None else 0
        flags.append("PI(protos={},points={})".format(n_protos, n_points))

    return flags


def _format_prim_line(prim, proto_ids, is_proto_root=False):
    type_name = prim.GetTypeName() or "—"
    flags = _prim_flags(prim, proto_ids, is_proto_root)
    label = "{}  ({})".format(prim.GetName() or "/", type_name)
    if flags:
        label += "  [{}]".format(" ".join(flags))
    # For an instance, name the prototype it resolves to.
    if prim.IsInstance():
        proto = prim.GetPrototype()
        if proto:
            label += "  ->{}".format(proto_ids.get(proto.GetPath(), "?"))
    return label


def _emit_prim(prim, prefix, is_last, sink, proto_ids, is_root=False,
               is_proto_root=False):
    """Recursively emit a prim subtree.  ``GetChildren`` stops at instances."""
    if is_root:
        sink.add(_format_prim_line(prim, proto_ids, is_proto_root))
    else:
        connector = ELL if is_last else TEE
        sink.add(prefix + connector +
                 _format_prim_line(prim, proto_ids, is_proto_root))

    children = prim.GetChildren()  # default predicate: stops at instances
    child_prefix = "" if is_root else prefix + (GAP if is_last else BAR)
    for i, child in enumerate(children):
        _emit_prim(child, child_prefix, i == len(children) - 1, sink,
                   proto_ids, is_root=False)


def _prototype_summary(proto):
    """What a prototype *is*: (source label, use count, example use-site).

    The source label is read from one instance's authored reference target
    (e.g. ``/Assets/Boat``, or an internal path like ``/hut/wall``) so an
    otherwise-opaque ``/__Prototype_12`` becomes recognizable.
    """
    instances = proto.GetInstances()
    count = len(instances)
    source = None
    example = None
    if instances:
        example = instances[0].GetPath()
        ref_list = instances[0].GetMetadata("references")
        if ref_list:
            items = ref_list.GetAppliedItems()
            if items and items[0].primPath:
                source = str(items[0].primPath)
    return source, count, example


def _use_site_label(path, proto_ids):
    """Render an instance path, rewriting a prototype prefix as its ``PN`` id.

    A use-site inside another prototype (e.g. ``/__Prototype_18/Block_0``)
    becomes ``P14/Block_0`` -- stable across runs and, better, it names the
    DAG *parent* the master is used from.
    """
    s = str(path)
    for proto_path, pid in proto_ids.items():
        prefix = str(proto_path)
        if s == prefix:
            return pid
        if s.startswith(prefix + "/"):
            return pid + s[len(prefix):]
    return s


def build_prim_section(stage, max_lines):
    sink = LineSink(max_lines)

    # Summarize every prototype, then assign short ids (P0, P1, ...) in a
    # *stable, meaningful* order: sorted by source asset (then path).  This
    # keeps the ids reproducible across runs -- unlike raw GetPrototypes()
    # order -- and groups related prototypes together.
    prototypes = stage.GetPrototypes()
    summaries = {p.GetPath(): _prototype_summary(p) for p in prototypes}
    prototypes = sorted(
        prototypes,
        key=lambda p: (summaries[p.GetPath()][0] or "~", str(p.GetPath())))
    proto_ids = {p.GetPath(): "P{}".format(i)
                 for i, p in enumerate(prototypes)}

    root = stage.GetDefaultPrim() or stage.GetPseudoRoot()
    try:
        sink.add("composed scenegraph (from default prim {}):".format(
            root.GetPath()))
        _emit_prim(root, "", True, sink, proto_ids, is_root=True)

        if prototypes:
            sink.add("")
            sink.add("prototypes ({}):".format(len(prototypes)))
            for proto in prototypes:
                sink.add("")
                # Header line names the prototype (the "master") and shows what
                # it stands for -- source asset, use count, and one use-site so
                # the reader can cross-check against SECTION-2 '->PN' lines.
                pid = proto_ids[proto.GetPath()]
                source, count, example = summaries[proto.GetPath()]
                header = "{} = {}".format(pid, proto.GetPath())
                extra = []
                if source:
                    extra.append("<- {}".format(source))
                extra.append("{} instance{}".format(count,
                                                     "" if count == 1 else "s"))
                if example is not None:
                    extra.append("e.g. {}".format(
                        _use_site_label(example, proto_ids)))
                sink.add("{}   ({})".format(header, "; ".join(extra)))
                _emit_prim(proto, "", True, sink, proto_ids,
                           is_root=True, is_proto_root=True)
    except _Truncated:
        pass

    return sink


# --------------------------------------------------------------------------- #
# Driver                                                                      #
# --------------------------------------------------------------------------- #

def _write_section(out, title, legend, sink):
    out.write("=" * 72 + "\n")
    out.write(title + "\n")
    out.write("=" * 72 + "\n")
    out.write("legend:\n")
    for line in legend:
        out.write(line + "\n")
    out.write("\n")
    for line in sink.lines:
        out.write(line + "\n")
    if sink.truncated:
        out.write("... [truncated at {} lines; raise --max-lines for more]\n"
                  .format(sink.max_lines))
    out.write("\n")


# =========================================================================== #
# Render plan emitter                                                         #
#                                                                             #
# A machine-parseable dump of the prototype DAG, designed for a from-scratch  #
# renderer (see docs/rendering-from-the-dag.md). It is deliberately the *DAG*,
# not the flattened scene: the consumer walks/memoizes it to produce world    #
# transforms and bounds -- the same traversal UsdGeomBBoxCache performs, so it #
# is where bbox-caching algorithms can be prototyped and profiled.            #
#                                                                             #
# Format (line-oriented, whitespace-tokenized, same spirit as .lglcap):       #
#   # labgl renderplan v2                                                     #
#   # protos=P roots=R curves=C points=Pt                                     #
#   upaxis Y                                                                  #
#   proto <id> <nverts> <ntris> <nchildren> <purpose>   (purpose = r|p|g|d)   #
#     v <x y z nx ny nz r g b>            x nverts   (proto-local, flat-shaded)#
#     f <a b c>                           x ntris                             #
#     child <childId> <m0..m15>           x nchildren (proto-local, col-major)#
#     aabb <minx miny minz maxx maxy maxz>          (proto-local)             #
#   root <protoId> <m0..m15>              x R         (world, col-major)       #
#   curve <class> <r g b> <width> <startNode> <endNode> <nverts> <x y z...>   #  v2
#   point <class> <r g b> <id> <x> <y> <z>                                    #  v2
#                                                                             #
# v2 is a graceful superset of v1: hero (unique, non-instanced) meshes -- the #
# terrain, sea plane, ribbon roads and river water -- ride the existing       #
# proto/root records as single-use protos with an identity root, so a v1      #
# consumer draws them unchanged.  Only the road/river graph -- curve (per     #
# BasisCurves curve) and point (per Points node) records -- is v2-only; a v1  #
# consumer skips those unknown record types.  Curve/point coordinates are     #
# world-space (the prim's local-to-world is already baked in, matching how    #
# root placements bake the PI/instance transforms), so a consumer draws them  #
# directly: line strips for curve, GL_POINTS/billboards for point.  The       #
# road/river graph adjacency travels with each curve via startNode/endNode,   #
# indexing into the emitted point set (default -1 when unauthored).           #
#                                                                             #
# All matrices are column-major float[16] usable directly as a GL model       #
# matrix: we flatten GfMatrix4d in row-major order, which equals the          #
# column-major storage of its transpose (USD row-vector -> GL column-vector). #
# =========================================================================== #

_PURPOSE_CHAR = {UsdGeom.Tokens.render: "r", UsdGeom.Tokens.proxy: "p",
                 UsdGeom.Tokens.guide: "g", UsdGeom.Tokens.default_: "d"}


def _mat16(m):
    """GfMatrix4d -> column-major float[16] (see format note above)."""
    return [m[i][j] for i in range(4) for j in range(4)]


def _proto_color(index):
    """Deterministic, well-separated RGB per prototype id (golden-ratio hue)."""
    h = (index * 0.6180339887) % 1.0
    r, g, b = colorsys.hsv_to_rgb(h, 0.55, 0.95)
    return (r, g, b)


def _append_mesh(mesh_prim, proto, xf, color, verts, tris, aabb):
    """Triangulate a Mesh into proto-local, flat-shaded verts + tri indices."""
    mesh = UsdGeom.Mesh(mesh_prim)
    pts = mesh.GetPointsAttr().Get()
    counts = mesh.GetFaceVertexCountsAttr().Get()
    idxs = mesh.GetFaceVertexIndicesAttr().Get()
    if not pts or not counts or not idxs:
        return
    # Transform points into prototype-local space (proto root is identity).
    m = xf.GetLocalToWorldTransform(mesh_prim)
    tp = [m.Transform(Gf.Vec3f(p[0], p[1], p[2])) for p in pts]

    fv = 0
    for fc in counts:
        if fc < 3:
            fv += fc
            continue
        face = [idxs[fv + k] for k in range(fc)]
        p0, p1, p2 = tp[face[0]], tp[face[1]], tp[face[2]]
        n = Gf.Vec3f((p1 - p0) ^ (p2 - p0))   # geometric normal (flat)
        ln = n.GetLength()
        n = n / ln if ln > 1e-9 else Gf.Vec3f(0, 1, 0)
        base = len(verts)
        for k in range(fc):
            p = tp[face[k]]
            verts.append((p[0], p[1], p[2], n[0], n[1], n[2],
                          color[0], color[1], color[2]))
            aabb[0] = [min(aabb[0][i], p[i]) for i in range(3)]
            aabb[1] = [max(aabb[1][i], p[i]) for i in range(3)]
        for k in range(1, fc - 1):
            tris.append((base, base + k, base + k + 1))
        fv += fc


def _extract_proto(stage, proto, id_of, xf):
    """Merged geometry + local AABB + child-instance edges for one prototype."""
    color = _proto_color(id_of[proto.GetPath()])
    verts, tris, children = [], [], []
    aabb = [[1e30, 1e30, 1e30], [-1e30, -1e30, -1e30]]
    purpose = "r"
    for prim in Usd.PrimRange(proto):   # default predicate: stops at instances
        if prim == proto:
            continue
        if prim.IsInstance():
            child = prim.GetPrototype()
            if child and child.GetPath() in id_of:
                lm = xf.GetLocalToWorldTransform(prim)  # proto-local
                children.append((id_of[child.GetPath()], _mat16(lm)))
            continue
        if prim.IsA(UsdGeom.Mesh):
            _append_mesh(prim, proto, xf, color, verts, tris, aabb)
            pa = prim.GetAttribute("purpose")
            if pa and pa.HasAuthoredValue():
                purpose = _PURPOSE_CHAR.get(pa.Get(), purpose)
    if not verts:
        aabb = [[0, 0, 0], [0, 0, 0]]
    return {"verts": verts, "tris": tris, "children": children,
            "aabb": aabb, "purpose": purpose}


def _compose_point(pos, orient, scale):
    """Point-instancer per-point transform -> column-major float[16]."""
    m = Gf.Matrix4d(1.0)
    if scale is not None:
        m = Gf.Matrix4d(scale[0], 0, 0, 0, 0, scale[1], 0, 0,
                        0, 0, scale[2], 0, 0, 0, 0, 1)
    if orient is not None:
        r = Gf.Matrix4d(1.0)
        r.SetRotate(Gf.Quatd(orient.GetReal(), Gf.Vec3d(orient.GetImaginary())))
        m = m * r
    t = Gf.Matrix4d(1.0)
    t.SetTranslate(Gf.Vec3d(pos[0], pos[1], pos[2]))
    return m * t   # scale/rotate in local, then translate (row-vector order)


def _resolve_target_id(stage, target_path, id_of, proto_data, xf):
    """Proto id for one of a PointInstancer's ``prototypes``-rel targets.

    Native path (the island): an *instanceable* target resolves to its Usd
    prototype's id, exactly as before.  Additive path (the city): a non-instance
    target is a prim shared through the ``prototypes`` *relationship* rather than
    native instancing, so it has no Usd prototype -- synthesize a proto for it so
    nested PIs appear in the plan.  The additive branch never fires for the
    island, whose PI targets are all instanceable, so its plan is unchanged.
    """
    prim = stage.GetPrimAtPath(target_path)
    if prim and prim.IsInstance():
        cproto = prim.GetPrototype()
        return id_of.get(cproto.GetPath()) if cproto else None
    return _synth_target_proto(stage, target_path, id_of, proto_data, xf)


def _synth_target_proto(stage, target_path, id_of, proto_data, xf):
    """Ensure (memoized) a synthetic proto for a non-instance PI target.

    A target that itself contains a PointInstancer becomes a DAG internal node
    (its children are the nested PI's point placements); a target with only
    gprims becomes a leaf proto (its merged geometry).  Returns the proto id, or
    None if the target is missing/empty.  ``proto_data`` and ``id_of`` are grown
    in place, with ids assigned after the native prototypes.
    """
    if target_path in id_of:
        return id_of[target_path]
    prim = stage.GetPrimAtPath(target_path)
    if not prim or not prim.IsValid():
        return None
    nested_pi = None
    for p in Usd.PrimRange(prim):
        if p.IsA(UsdGeom.PointInstancer):
            nested_pi = p
            break
    pid = len(proto_data)
    id_of[target_path] = pid       # reserve the id before recursing (cycle-safe)
    proto_data.append(None)        # placeholder slot keeps ids stable
    color = _proto_color(pid)
    if nested_pi is not None:
        children = _expand_pi_children(stage, nested_pi, prim, id_of,
                                       proto_data, xf)
        proto_data[pid] = {"verts": [], "tris": [], "children": children,
                           "aabb": [[0, 0, 0], [0, 0, 0]], "purpose": "r"}
    else:
        verts, tris, aabb, purpose = _extract_leaf_geometry(prim, xf, color)
        proto_data[pid] = {"verts": verts, "tris": tris, "children": [],
                           "aabb": aabb, "purpose": purpose}
    return pid


def _extract_leaf_geometry(root_prim, xf, color):
    """Merge the meshes under a non-prototype prim into proto-local geometry."""
    verts, tris = [], []
    aabb = [[1e30, 1e30, 1e30], [-1e30, -1e30, -1e30]]
    purpose = "r"
    for p in Usd.PrimRange(root_prim):
        if p.IsA(UsdGeom.Mesh):
            _append_mesh(p, root_prim, xf, color, verts, tris, aabb)
            pa = p.GetAttribute("purpose")
            if pa and pa.HasAuthoredValue():
                purpose = _PURPOSE_CHAR.get(pa.Get(), purpose)
    if not verts:
        aabb = [[0, 0, 0], [0, 0, 0]]
    return verts, tris, aabb, purpose


def _expand_pi_children(stage, pi_prim, root_prim, id_of, proto_data, xf):
    """Child edges for a synthetic DAG node: the nested PI's point placements,
    each transform expressed proto-local to ``root_prim``."""
    pi = UsdGeom.PointInstancer(pi_prim)
    root_world = xf.GetLocalToWorldTransform(root_prim)
    pi_local = xf.GetLocalToWorldTransform(pi_prim) * root_world.GetInverse()
    targets = pi.GetPrototypesRel().GetTargets()
    target_ids = [_resolve_target_id(stage, t, id_of, proto_data, xf)
                  for t in targets]
    positions = pi.GetPositionsAttr().Get()
    proto_indices = pi.GetProtoIndicesAttr().Get()
    if positions is None or proto_indices is None:
        return []
    orientations = pi.GetOrientationsAttr().Get()
    scales = pi.GetScalesAttr().Get()
    children = []
    for i in range(len(positions)):
        pi_idx = proto_indices[i]
        if pi_idx >= len(target_ids):
            continue
        cid = target_ids[pi_idx]
        if cid is None:
            continue
        o = orientations[i] if orientations else None
        s = scales[i] if scales else None
        m = _compose_point(positions[i], o, s) * pi_local
        children.append((cid, _mat16(m)))
    return children


def _expand_pointinstancer(stage, pi_prim, id_of, proto_data, xf):
    """Each point becomes a scene root of the referenced prototype."""
    pi = UsdGeom.PointInstancer(pi_prim)
    world = xf.GetLocalToWorldTransform(pi_prim)
    targets = pi.GetPrototypesRel().GetTargets()
    target_ids = [_resolve_target_id(stage, t, id_of, proto_data, xf)
                  for t in targets]

    positions = pi.GetPositionsAttr().Get()
    proto_indices = pi.GetProtoIndicesAttr().Get()
    if positions is None or proto_indices is None:
        return []
    orientations = pi.GetOrientationsAttr().Get()
    scales = pi.GetScalesAttr().Get()

    roots = []
    for i in range(len(positions)):
        pi_idx = proto_indices[i]
        if pi_idx >= len(target_ids):
            continue
        pid = target_ids[pi_idx]
        if pid is None:
            continue
        o = orientations[i] if orientations else None
        s = scales[i] if scales else None
        m = _compose_point(positions[i], o, s) * world
        roots.append((pid, _mat16(m)))
    return roots


# --------------------------------------------------------------------------- #
# v2: curve / point graph records (road + river networks)                     #
# --------------------------------------------------------------------------- #

# Stable, well-separated colours per curve/point class (grey fallback below).
_CLASS_COLOR = {
    "highway": (0.92, 0.58, 0.20),   # arterial orange
    "street":  (0.62, 0.62, 0.66),   # asphalt grey
    "river":   (0.20, 0.52, 0.90),   # water blue
    "road":    (0.85, 0.75, 0.35),   # road-node amber
}


def _class_color(cls):
    """Stable RGB for a curve/point class token (grey fallback)."""
    return _CLASS_COLOR.get(cls, (0.80, 0.80, 0.80))


def _curve_class(prim):
    """Map a BasisCurves prim to a stable class token by name/path."""
    n = prim.GetName().lower()
    if "highway" in n:
        return "highway"
    if "street" in n:
        return "street"
    p = str(prim.GetPath()).lower()
    if "hydro" in p or "river" in p or "channel" in n:
        return "river"
    return "curve"


def _point_class(prim):
    """Map a Points prim (graph nodes) to a stable class token."""
    p = str(prim.GetPath()).lower()
    if "hydro" in p or "river" in p:
        return "river"
    return "road"


def _graph_primvar(prim, name):
    """Read a road/river adjacency primvar (roadnet: or hydro: namespace)."""
    for ns in ("roadnet", "hydro"):
        a = prim.GetAttribute("primvars:{}:{}".format(ns, name))
        if a and a.HasAuthoredValue():
            return a.Get()
    return None


def _extract_curves(prim, xf):
    """One world-space curve record per curve in a BasisCurves prim.

    Points are baked to world (matching the mesh/root path); per-curve width
    comes from the ``widths`` primvar (constant -> shared, uniform -> per-curve)
    and the graph adjacency from ``primvars:{roadnet,hydro}:{start,end}Node``
    (default -1 when unauthored).
    """
    bc = UsdGeom.BasisCurves(prim)
    pts = bc.GetPointsAttr().Get()
    counts = bc.GetCurveVertexCountsAttr().Get()
    if not pts or not counts:
        return []
    m = xf.GetLocalToWorldTransform(prim)
    widths = bc.GetWidthsAttr().Get()
    shared_w = (bc.GetWidthsInterpolation() == UsdGeom.Tokens.constant or
                (widths is not None and len(widths) == 1))
    starts = _graph_primvar(prim, "startNode")
    ends = _graph_primvar(prim, "endNode")
    cls = _curve_class(prim)
    color = _class_color(cls)

    records = []
    off = 0
    for i, nv in enumerate(counts):
        coords = []
        for k in range(nv):
            wp = m.Transform(Gf.Vec3f(pts[off + k][0], pts[off + k][1],
                                      pts[off + k][2]))
            coords.extend((wp[0], wp[1], wp[2]))
        off += nv
        if widths:
            w = widths[0] if shared_w else (
                widths[i] if i < len(widths) else widths[-1])
        else:
            w = 1.0
        sn = int(starts[i]) if starts is not None and i < len(starts) else -1
        en = int(ends[i]) if ends is not None and i < len(ends) else -1
        records.append((cls, color, float(w), sn, en, nv, coords))
    return records


def _extract_points(prim, xf):
    """One world-space point record per point in a Points prim."""
    pts_geom = UsdGeom.Points(prim)
    positions = pts_geom.GetPointsAttr().Get()
    if not positions:
        return []
    ids = pts_geom.GetIdsAttr().Get()
    m = xf.GetLocalToWorldTransform(prim)
    cls = _point_class(prim)
    color = _class_color(cls)
    records = []
    for i in range(len(positions)):
        wp = m.Transform(Gf.Vec3f(positions[i][0], positions[i][1],
                                  positions[i][2]))
        pid = int(ids[i]) if ids is not None and i < len(ids) else i
        records.append((cls, color, pid, wp[0], wp[1], wp[2]))
    return records


def build_render_plan(stage, path):
    """Write the prototype-DAG render plan for ``stage`` to ``path``."""
    protos = sorted(stage.GetPrototypes(), key=lambda p: str(p.GetPath()))
    id_of = {p.GetPath(): i for i, p in enumerate(protos)}
    xf = UsdGeom.XformCache(Usd.TimeCode.Default())

    # Native prototypes fill ids 0..N-1; synthetic PI-target protos (city) get
    # ids appended after these as _expand_pointinstancer discovers them below.
    proto_data = [_extract_proto(stage, p, id_of, xf) for p in protos]

    # Scene roots: native instances in the main tree; PointInstancers expand
    # point-by-point. Prune PI subtrees so their /Prototypes/proto_i instances
    # are not mistaken for scene placements.
    roots = []
    curves = []      # v2: per-curve BasisCurves records (road/river graph)
    points = []      # v2: per-point Points records (graph nodes)
    default = stage.GetDefaultPrim() or stage.GetPseudoRoot()
    it = iter(Usd.PrimRange(default))
    for prim in it:
        if prim.IsA(UsdGeom.PointInstancer):
            roots.extend(_expand_pointinstancer(stage, prim, id_of, proto_data,
                                                xf))
            it.PruneChildren()
            continue
        if prim.IsInstance():
            proto = prim.GetPrototype()
            if proto and proto.GetPath() in id_of:
                roots.append((id_of[proto.GetPath()],
                              _mat16(xf.GetLocalToWorldTransform(prim))))
            continue
        if prim.IsA(UsdGeom.Mesh):
            # Hero (unique, non-instanced) mesh reached in the main tree: emit a
            # single-use proto placed by an identity root (its geometry is
            # already baked to world space).  This is the terrain, sea plane,
            # ribbon roads and river water -- geometry the instancing DAG omits
            # because it is neither a native prototype nor a PI point.  We reuse
            # _extract_leaf_geometry, which merges the prim's meshes, then prune
            # so its (already-merged) descendants are not re-emitted.
            pid = len(proto_data)
            verts, tris, aabb, purpose = _extract_leaf_geometry(
                prim, xf, _proto_color(pid))
            it.PruneChildren()
            if not verts:                 # skip degenerate/empty meshes
                continue
            proto_data.append({"verts": verts, "tris": tris, "children": [],
                               "aabb": aabb, "purpose": purpose})
            roots.append((pid, _mat16(Gf.Matrix4d(1.0))))
            continue
        if prim.IsA(UsdGeom.BasisCurves):
            curves.extend(_extract_curves(prim, xf))
            continue
        if prim.IsA(UsdGeom.Points):
            points.extend(_extract_points(prim, xf))

    up = UsdGeom.GetStageUpAxis(stage)
    with open(path, "w") as f:
        f.write("# labgl renderplan v2\n")
        f.write("# protos={} roots={} curves={} points={}\n".format(
            len(proto_data), len(roots), len(curves), len(points)))
        f.write("upaxis {}\n".format(up))
        for pid, d in enumerate(proto_data):
            f.write("proto {} {} {} {} {}\n".format(
                pid, len(d["verts"]), len(d["tris"]),
                len(d["children"]), d["purpose"]))
            for v in d["verts"]:
                f.write("v {:.6g} {:.6g} {:.6g} {:.4g} {:.4g} {:.4g} "
                        "{:.4g} {:.4g} {:.4g}\n".format(*v))
            for t in d["tris"]:
                f.write("f {} {} {}\n".format(*t))
            for (cid, m) in d["children"]:
                f.write("child {} {}\n".format(
                    cid, " ".join("{:.7g}".format(x) for x in m)))
            a = d["aabb"]
            f.write("aabb {:.6g} {:.6g} {:.6g} {:.6g} {:.6g} {:.6g}\n".format(
                a[0][0], a[0][1], a[0][2], a[1][0], a[1][1], a[1][2]))
            f.write("endproto\n")
        for (pid, m) in roots:
            f.write("root {} {}\n".format(
                pid, " ".join("{:.7g}".format(x) for x in m)))
        for (cls, color, w, sn, en, nv, coords) in curves:
            f.write("curve {} {:.4g} {:.4g} {:.4g} {:.6g} {} {} {} {}\n".format(
                cls, color[0], color[1], color[2], w, sn, en, nv,
                " ".join("{:.6g}".format(x) for x in coords)))
        for (cls, color, pid, x, y, z) in points:
            f.write("point {} {:.4g} {:.4g} {:.4g} {} {:.6g} {:.6g} {:.6g}\n"
                    .format(cls, color[0], color[1], color[2], pid, x, y, z))
    return len(proto_data), len(roots), len(curves), len(points)


def _parse_args(argv=None):
    p = argparse.ArgumentParser(
        description="Draw a USD scene's composition as an annotated text tree.")
    p.add_argument("--stage", required=True, help="Top layer to open.")
    p.add_argument("--output", default="./composition_tree.txt",
                   help="Text file to write (default: ./composition_tree.txt).")
    p.add_argument("--max-lines", type=int, default=0,
                   help="Max lines per section (0 = unlimited).")
    p.add_argument("--render-plan", default=None,
                   help="Also write a machine-parseable render plan (the "
                        "prototype DAG + geometry) to this path.")
    return p.parse_args(argv)


def main(argv=None):
    args = _parse_args(argv)
    if not os.path.exists(args.stage):
        print("error: stage not found: {}".format(args.stage), file=sys.stderr)
        return 2

    stage = Usd.Stage.Open(args.stage)
    if not stage:
        print("error: failed to open stage: {}".format(args.stage),
              file=sys.stderr)
        return 2

    layer_sink = build_layer_section(stage, args.max_lines)
    prim_sink = build_prim_section(stage, args.max_lines)

    with open(args.output, "w") as out:
        out.write("composition tree for: {}\n\n".format(args.stage))
        for line in HOW_TO_READ:
            out.write(line + "\n")
        out.write("\n")
        _write_section(out, "SECTION 1 -- LAYERS (composition graph)",
                       LAYER_LEGEND, layer_sink)
        _write_section(out, "SECTION 2 -- PRIMS (scenegraph + prototypes)",
                       PRIM_LEGEND, prim_sink)

    print("Wrote {} ({} layer lines, {} prim lines){}".format(
        args.output, len(layer_sink.lines), len(prim_sink.lines),
        "" if not (layer_sink.truncated or prim_sink.truncated)
        else "  [truncated -- see --max-lines]"))

    if args.render_plan:
        nprotos, nroots, ncurves, npoints = build_render_plan(
            stage, args.render_plan)
        print("Wrote {} ({} prototypes, {} scene roots, {} curves, {} points)"
              .format(args.render_plan, nprotos, nroots, ncurves, npoints))
    return 0


if __name__ == "__main__":
    sys.exit(main())
