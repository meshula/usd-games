
"""Characterize a USD stage against production-likeness targets.

Opens a stage and reports the distributions that matter for exercising
``UsdGeomBBoxCache``'s parallel paths -- schema-type histogram, native/point
instancing, prototype subtree sizes, hierarchy depth and fan-out, xform-chain
depth, a log-scale world-space bbox-diagonal histogram, time-sampled fraction,
and purpose breakdown -- then grades them PASS/WARN/FAIL against the targets
documented in README.md.  Ends with a smoke check that
``ComputeWorldBound(defaultPrim)`` returns a finite, non-empty box.

Usage:

    python3 characterize_island.py --stage ./output/island.usda
    python3 characterize_island.py --stage island.usda --json report.json
    python3 characterize_island.py --stage island.usda --compare alab.usd kitchen.usd

``--compare`` runs the *same* characterizer on other stages (e.g. the shipped
reference assets referenced in docs/performance/*.yaml) to calibrate the
targets.
"""

import argparse
import json
import math
import os
import sys

from pxr import Usd, UsdGeom


# Production-likeness targets.  See README.md for rationale.  Each entry is
# (pass_threshold, warn_threshold); grading is ">=" for these metrics.
TARGETS = {
    "instanced_fraction":      (0.70, 0.40),
    "prototype_count":         (8, 3),
    "pointinstancer_count":    (3, 1),
    "point_instance_count":    (10000, 1000),
    "bbox_decades":            (3.0, 2.0),
    "max_depth":               (6, 4),
    "time_sampled_fraction":   (0.02, 0.0001),
    "max_fanout":              (50, 10),
    # Nested point-instancing (the "city" generator's cost center): the count of
    # propagated prototypes N (merging-scene-index inputs) and the PI->PI nesting
    # depth.  These grade the imaging-stack propagation/merge path rather than
    # the bbox cache; a pure-bbox stage (the island) scores low here and vice
    # versa.  Thresholds are loose sanity gates, recalibratable like the rest.
    # pass = heavy propagation stress (the city); warn = *some* nested-PI
    # propagation is present (e.g. the island's flower beds). A scene with no
    # nested PIs grades N/A here rather than FAIL (see APPLICABILITY below).
    "propagated_prototype_count": (1000, 10),
    "max_pi_nesting_depth":       (2, 1),
}

# Applicability predicates: a metric is graded only when the structure it
# measures is actually present in the stage; otherwise it is "N/A" -- there is
# nothing to measure -- rather than a FAIL.  Each predicate keys off an
# *independent* structural fact (not the metric's own value), so a stage that
# genuinely lacks a feature (e.g. the city has no native instancing; the island
# has no nested PIs) is not penalised for it.  Metrics absent from this map are
# always applicable.  (Note: because applicability keys off *presence*, a
# regression that removes a feature entirely turns its metrics N/A rather than
# FAIL; enforcing that a given generator *must* exhibit a feature belongs in a
# per-generator assertion, not here.)
APPLICABILITY = {
    "instanced_fraction":         lambda r: r["native_instances"] > 0,
    "prototype_count":            lambda r: r["native_instances"] > 0,
    "point_instance_count":       lambda r: r["pointinstancer_count"] > 0,
    "max_pi_nesting_depth":       lambda r: r["pointinstancer_count"] > 0,
    "time_sampled_fraction":      lambda r: r.get("has_time_range", False),
    "propagated_prototype_count": lambda r: r["max_pi_nesting_depth"] >= 2,
}

# Human-readable reason shown when a metric grades N/A.
_NA_REASON = {
    "instanced_fraction":         "no native instancing",
    "prototype_count":            "no native instancing",
    "point_instance_count":       "no PointInstancers",
    "max_pi_nesting_depth":       "no PointInstancers",
    "time_sampled_fraction":      "stage authors no time range",
    "propagated_prototype_count": "no nested PointInstancers (PI-of-PI)",
}


def _applicable(metric, report):
    pred = APPLICABILITY.get(metric)
    return pred(report) if pred else True

# Cap on per-prim bbox sampling to keep the tool responsive on huge stages.
# When exceeded we stride deterministically and report the sampling.
BBOX_SAMPLE_CAP = 20000


def _grade(value, thresholds):
    pass_t, warn_t = thresholds
    if value >= pass_t:
        return "PASS"
    if value >= warn_t:
        return "WARN"
    return "FAIL"


def _prim_time_samples_vary(prim):
    """True if any xformOp or extent attribute on ``prim`` has >1 time sample."""
    for attr in prim.GetAttributes():
        name = attr.GetName()
        if name == "extent" or name.startswith("xformOp:"):
            if attr.GetNumTimeSamples() > 1:
                return True
    return False


def _pi_within(stage, target_path):
    """The PointInstancer at or beneath ``target_path``, or None.

    A city building instancer's ``prototypes`` targets are library prims that
    themselves *contain* a PointInstancer -- that is the PI-of-PI nesting we want
    to detect.  Traversal uses the default predicate, so it stops at instance
    boundaries (an instanceable target contributes nothing here, matching how
    native instancing takes it off the point-instancer propagation path).
    """
    prim = stage.GetPrimAtPath(target_path)
    if not prim or not prim.IsValid():
        return None
    for p in Usd.PrimRange(prim):
        if p.IsA(UsdGeom.PointInstancer):
            return p
    return None


def _propagated_prototype_count(stage):
    """N: propagated prototypes = merging-scene-index inputs.

    Reproduces, by walking the composed stage, the count the imaging stack's
    ``UsdImagingPiPrototypePropagatingSceneIndex`` feeds into
    ``HdMergingSceneIndex``.  For each PointInstancer occurrence, every
    ``prototypes`` relationship *target* is one propagated prototype; and every
    target that itself contains a PointInstancer re-propagates that nested
    instancer's prototypes in turn.  For the city generator this closed-forms to
    ``N = D*B + B*M + D*B*M`` -- the customer's insight that N is driven by the
    ``prototypes`` relationship, independent of point count.
    """
    memo = {}

    def propagate(pi_prim):
        key = pi_prim.GetPath()
        if key in memo:
            return memo[key]
        memo[key] = 0  # cycle guard (the prototype graph is a DAG)
        total = 0
        targets = UsdGeom.PointInstancer(pi_prim).GetPrototypesRel().GetTargets()
        for t in targets:
            total += 1
            nested = _pi_within(stage, t)
            if nested is not None:
                total += propagate(nested)
        memo[key] = total
        return total

    n = 0
    for prim in Usd.PrimRange(stage.GetPseudoRoot()):
        if prim.IsA(UsdGeom.PointInstancer):
            n += propagate(prim)
    return n


def _max_pi_nesting_depth(stage):
    """Deepest PI -> (target contains PI) -> ... chain (a flat PI scene = 1)."""
    memo = {}

    def depth(pi_prim):
        key = pi_prim.GetPath()
        if key in memo:
            return memo[key]
        memo[key] = 1  # cycle guard; a PI is at least depth 1
        best = 1
        targets = UsdGeom.PointInstancer(pi_prim).GetPrototypesRel().GetTargets()
        for t in targets:
            nested = _pi_within(stage, t)
            if nested is not None:
                best = max(best, 1 + depth(nested))
        memo[key] = best
        return best

    best = 0
    for prim in Usd.PrimRange(stage.GetPseudoRoot()):
        if prim.IsA(UsdGeom.PointInstancer):
            best = max(best, depth(prim))
    return best


def characterize(stage, verbose=True):
    """Compute the full metrics dict for a stage."""
    report = {}

    default_prim = stage.GetDefaultPrim()
    root = default_prim if default_prim else stage.GetPseudoRoot()

    # ---- Traverse the composed scene (stops at instance boundaries). ----
    type_counts = {}
    depths = []
    fanouts = []
    native_instances = 0
    pointinstancers = []
    total_prims = 0
    boundable_prims = []
    renderable_prims = 0
    purpose_counts = {}
    time_sampled = 0
    time_examined = 0
    xform_chain_depths = []

    root_depth = root.GetPath().pathElementCount

    for prim in Usd.PrimRange(root):
        total_prims += 1
        tname = prim.GetTypeName() or "(untyped)"
        type_counts[tname] = type_counts.get(tname, 0) + 1

        depth = prim.GetPath().pathElementCount - root_depth
        depths.append(depth)

        n_children = len(prim.GetChildren())
        if n_children:
            fanouts.append(n_children)

        if prim.IsInstance():
            native_instances += 1
        if prim.IsA(UsdGeom.PointInstancer):
            pointinstancers.append(prim)
        if prim.IsA(UsdGeom.Imageable):
            renderable_prims += 1
        if prim.IsA(UsdGeom.Boundable) or prim.IsInstance():
            boundable_prims.append(prim)

        # Purpose (authored value only; cheap and stable).
        pattr = prim.GetAttribute("purpose")
        if pattr and pattr.HasAuthoredValue():
            pv = pattr.Get()
            purpose_counts[pv] = purpose_counts.get(pv, 0) + 1

        # xform-chain depth: xformable ancestors (incl. self) with authored ops.
        chain = 0
        walk = prim
        while walk and walk != stage.GetPseudoRoot():
            xf = UsdGeom.Xformable(walk)
            if xf and xf.GetXformOpOrderAttr().HasAuthoredValue():
                chain += 1
            walk = walk.GetParent()
        xform_chain_depths.append(chain)

    # ---- Time-sampled fraction: examine composed prims + prototype prims. ----
    def _accumulate_time(prim_range):
        nonlocal time_sampled, time_examined
        for p in prim_range:
            time_examined += 1
            if _prim_time_samples_vary(p):
                time_sampled += 1

    _accumulate_time(Usd.PrimRange(root))

    # ---- Instancing / prototype stats. ----
    prototypes = stage.GetPrototypes()
    proto_subtree_sizes = []
    for proto in prototypes:
        size = sum(1 for _ in Usd.PrimRange(proto))
        proto_subtree_sizes.append(size)
        _accumulate_time(Usd.PrimRange(proto))
        # Purpose authored inside prototypes.
        for p in Usd.PrimRange(proto):
            pattr = p.GetAttribute("purpose")
            if pattr and pattr.HasAuthoredValue():
                pv = pattr.Get()
                purpose_counts[pv] = purpose_counts.get(pv, 0) + 1

    # ---- Effective hierarchy depth (through instances into prototypes). ----
    # The composed traversal above stops at instance boundaries, so its depth
    # undercounts the real scene: a scattered instance at authored depth 4 whose
    # prototype nests 2-3 more instance levels is really ~7-8 deep.  Chase that.
    _proto_depth_memo = {}

    def _proto_eff_depth(proto_prim):
        key = proto_prim.GetPath()
        if key in _proto_depth_memo:
            return _proto_depth_memo[key]
        _proto_depth_memo[key] = 0  # cycle guard (prototype graph is a DAG)
        proto_root = proto_prim.GetPath().pathElementCount
        best = 0
        for p in Usd.PrimRange(proto_prim):
            local = p.GetPath().pathElementCount - proto_root
            extra = 0
            if p.IsInstance():
                child = p.GetPrototype()
                if child:
                    extra = 1 + _proto_eff_depth(child)
            best = max(best, local + extra)
        _proto_depth_memo[key] = best
        return best

    effective_max_depth = 0
    for prim in Usd.PrimRange(root):
        d = prim.GetPath().pathElementCount - root_depth
        if prim.IsInstance():
            child = prim.GetPrototype()
            if child:
                d += 1 + _proto_eff_depth(child)
        effective_max_depth = max(effective_max_depth, d)

    # ---- Point-instance totals. ----
    point_instance_count = 0
    for pi_prim in pointinstancers:
        ids = UsdGeom.PointInstancer(pi_prim).GetProtoIndicesAttr().Get()
        if ids is not None:
            point_instance_count += len(ids)

    # ---- BBox diagonal distribution (log-scale). ----
    cache = UsdGeom.BBoxCache(Usd.TimeCode.Default(),
                              [UsdGeom.Tokens.default_, UsdGeom.Tokens.render,
                               UsdGeom.Tokens.proxy, UsdGeom.Tokens.guide])
    n_boundable = len(boundable_prims)
    stride = 1
    if n_boundable > BBOX_SAMPLE_CAP:
        stride = math.ceil(n_boundable / BBOX_SAMPLE_CAP)
    diagonals = []
    for i in range(0, n_boundable, stride):
        prim = boundable_prims[i]
        try:
            bound = cache.ComputeWorldBound(prim)
            rng = bound.ComputeAlignedRange()
            if not rng.IsEmpty():
                diag = rng.GetSize().GetLength()
                if diag > 0.0:
                    diagonals.append(diag)
        except Exception:
            pass

    # Log-scale histogram (orders of magnitude, base 10).
    decade_hist = {}
    for d in diagonals:
        decade = int(math.floor(math.log10(d)))
        decade_hist[decade] = decade_hist.get(decade, 0) + 1
    if decade_hist:
        bbox_decades = (max(decade_hist) - min(decade_hist)) + 1
    else:
        bbox_decades = 0

    # ---- Assemble metrics. ----
    instanced_fraction = (native_instances / renderable_prims
                          if renderable_prims else 0.0)
    time_sampled_fraction = (time_sampled / time_examined
                             if time_examined else 0.0)

    report["total_prims"] = total_prims
    report["type_counts"] = type_counts
    report["native_instances"] = native_instances
    report["prototype_count"] = len(prototypes)
    report["prototype_subtree_sizes"] = sorted(proto_subtree_sizes)
    report["pointinstancer_count"] = len(pointinstancers)
    report["point_instance_count"] = point_instance_count
    report["renderable_prims"] = renderable_prims
    report["instanced_fraction"] = instanced_fraction
    report["max_depth"] = effective_max_depth
    report["authored_max_depth"] = max(depths) if depths else 0
    report["depth_histogram"] = _histogram(depths)
    report["max_fanout"] = max(fanouts) if fanouts else 0
    report["mean_fanout"] = (sum(fanouts) / len(fanouts)) if fanouts else 0.0
    report["max_xform_chain"] = max(xform_chain_depths) if xform_chain_depths else 0
    report["bbox_samples"] = len(diagonals)
    report["bbox_sampled_of"] = n_boundable
    report["bbox_stride"] = stride
    report["bbox_decade_histogram"] = {str(k): v for k, v in sorted(decade_hist.items())}
    report["bbox_decades"] = bbox_decades
    report["time_sampled_fraction"] = time_sampled_fraction
    report["time_sampled_prims"] = time_sampled
    report["purpose_counts"] = {str(k): v for k, v in purpose_counts.items()}
    report["propagated_prototype_count"] = _propagated_prototype_count(stage)
    report["max_pi_nesting_depth"] = _max_pi_nesting_depth(stage)
    report["has_time_range"] = (stage.GetEndTimeCode() > stage.GetStartTimeCode())

    # ---- Grade against targets (skipping metrics with nothing to measure). ----
    grades = {}
    for key, thresholds in TARGETS.items():
        if not _applicable(key, report):
            grades[key] = "N/A"
        else:
            grades[key] = _grade(report.get(key, 0), thresholds)
    report["grades"] = grades
    applicable = [g for g in grades.values() if g != "N/A"]
    report["overall"] = ("FAIL" if "FAIL" in applicable
                         else "WARN" if "WARN" in applicable
                         else "PASS" if applicable else "N/A")

    # ---- Smoke check. ----
    smoke = {"ok": False}
    try:
        bound = cache.ComputeWorldBound(root)
        rng = bound.ComputeAlignedRange()
        smoke["empty"] = bool(rng.IsEmpty())
        mn, mx = rng.GetMin(), rng.GetMax()
        finite = all(math.isfinite(v) for v in (mn[0], mn[1], mn[2],
                                                mx[0], mx[1], mx[2]))
        smoke["finite"] = finite
        smoke["min"] = [mn[0], mn[1], mn[2]]
        smoke["max"] = [mx[0], mx[1], mx[2]]
        smoke["ok"] = (not rng.IsEmpty()) and finite
    except Exception as e:
        smoke["error"] = str(e)
    report["smoke"] = smoke

    return report


def _histogram(values):
    hist = {}
    for v in values:
        hist[v] = hist.get(v, 0) + 1
    return {str(k): hist[k] for k in sorted(hist)}


def _print_report(name, report):
    print("=" * 72)
    print("Stage: {}".format(name))
    print("=" * 72)
    print("Total prims (composed) : {:,}".format(report["total_prims"]))
    print("Renderable (Imageable) : {:,}".format(report["renderable_prims"]))
    print()
    print("Prim count by schema type:")
    for tname, cnt in sorted(report["type_counts"].items(),
                             key=lambda kv: (-kv[1], kv[0])):
        print("  {:24s} {:>10,}".format(tname, cnt))
    print()
    print("Instancing:")
    print("  native instances       : {:,}".format(report["native_instances"]))
    print("  instanced fraction      : {:.1%}".format(report["instanced_fraction"]))
    print("  prototypes              : {}".format(report["prototype_count"]))
    sizes = report["prototype_subtree_sizes"]
    if sizes:
        print("  prototype subtree sizes : min={} median={} max={}".format(
            sizes[0], sizes[len(sizes) // 2], sizes[-1]))
    print("  point instancers        : {}".format(report["pointinstancer_count"]))
    print("  point instances (total) : {:,}".format(report["point_instance_count"]))
    print("  max PI nesting depth    : {}".format(
        report["max_pi_nesting_depth"]))
    print("  propagated prototypes N : {:,}  (merging-scene-index inputs)".format(
        report["propagated_prototype_count"]))
    print()
    print("Hierarchy:")
    print("  max depth (effective)   : {}  (authored {})".format(
        report["max_depth"], report["authored_max_depth"]))
    print("  max fan-out             : {}".format(report["max_fanout"]))
    print("  mean fan-out            : {:.1f}".format(report["mean_fanout"]))
    print("  max xform-chain depth   : {}".format(report["max_xform_chain"]))
    print()
    print("World-space bbox diagonal (log10 decades):")
    if report["bbox_stride"] > 1:
        print("  (sampled {:,} of {:,} boundable prims, stride {})".format(
            report["bbox_samples"], report["bbox_sampled_of"],
            report["bbox_stride"]))
    for decade, cnt in sorted(report["bbox_decade_histogram"].items(),
                              key=lambda kv: int(kv[0])):
        lo = 10.0 ** int(decade)
        print("  [1e{:>3} .. 1e{:>3}) m : {:>10,}".format(
            decade, int(decade) + 1, cnt))
    print("  decades spanned         : {}".format(report["bbox_decades"]))
    print()
    print("Animation & purpose:")
    print("  time-sampled prims      : {:,} ({:.2%})".format(
        report["time_sampled_prims"], report["time_sampled_fraction"]))
    if report["purpose_counts"]:
        print("  authored purposes       : {}".format(
            ", ".join("{}={}".format(k, v)
                      for k, v in sorted(report["purpose_counts"].items()))))
    print()
    print("Production-likeness grades:")
    for key, thresholds in TARGETS.items():
        grade = report["grades"][key]
        val = report.get(key)
        val_str = "{:.3f}".format(val) if isinstance(val, float) else str(val)
        if grade == "N/A":
            print("  {:24s} {:>10} : N/A   (nothing to measure: {})".format(
                key, val_str, _NA_REASON.get(key, "not applicable")))
        else:
            print("  {:24s} {:>10} : {}  (pass>={}, warn>={})".format(
                key, val_str, grade, thresholds[0], thresholds[1]))
    print()
    print("  OVERALL: {}  (over applicable metrics; N/A excluded)".format(
        report["overall"]))
    print()
    smoke = report["smoke"]
    if smoke.get("ok"):
        print("Smoke bbox: OK  min={} max={}".format(
            [round(v, 2) for v in smoke["min"]],
            [round(v, 2) for v in smoke["max"]]))
    else:
        print("Smoke bbox: FAILED ({})".format(
            smoke.get("error", "empty or non-finite box")))
    print()


def _parse_args(argv=None):
    p = argparse.ArgumentParser(
        description="Characterize a USD stage vs production-likeness targets.")
    p.add_argument("--stage", required=True, help="Primary stage to open.")
    p.add_argument("--json", help="Optional path to write the JSON report.")
    p.add_argument("--compare", nargs="*", default=[],
                   help="Additional stages to characterize for calibration.")
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

    report = characterize(stage)
    _print_report(args.stage, report)

    all_reports = {args.stage: report}
    for other in args.compare:
        if not os.path.exists(other):
            print("warning: --compare stage not found, skipping: {}".format(other),
                  file=sys.stderr)
            continue
        ostage = Usd.Stage.Open(other)
        if not ostage:
            print("warning: failed to open --compare stage: {}".format(other),
                  file=sys.stderr)
            continue
        oreport = characterize(ostage)
        _print_report(other, oreport)
        all_reports[other] = oreport

    if args.json:
        with open(args.json, "w") as f:
            json.dump(all_reports if args.compare else report, f, indent=2)
        print("Wrote JSON report: {}".format(args.json))

    # Exit non-zero if the primary stage fails the targets or the smoke check.
    if report["overall"] == "FAIL" or not report["smoke"].get("ok"):
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
