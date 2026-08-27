
"""Deterministic grid hydraulic erosion (stream-power incision + thermal talus).

Closes the terrain <-> water feedback loop: water concentrates where the terrain
routes it, and the concentrated flow carves the terrain, which routes more water
there next iteration.  Iterating this a few dozen times turns the smooth analytic
dome into one with incised valleys -- so the ``hydrology`` rivers settle into real
valleys and the roads/vegetation follow the new relief.

Each iteration, on a working heightfield grid:

  1. **Priority-flood pit fill** (for routing only) so every cell drains.
  2. **D8 flow direction + slope** to the steepest-descent neighbour.
  3. **Flow accumulation** (drainage area).
  4. **Stream-power incision**: lower each land cell by
     ``K * (A/Amax)^m * S^n``, capped to a fraction of the drop to its downstream
     neighbour so the channel never inverts.  High drainage area x slope -> deep
     incision, i.e. valleys; headward growth over iterations makes them dendritic.
  5. **Thermal / talus smoothing**: where the slope to the lowest neighbour
     exceeds a repose angle, move a little material downhill.  Removes
     incision-induced spikes and builds natural talus slopes.

Operates on a flat ``list`` of
elevations (row-major, ``n x n``); unit-testable under plain ``python3`` and fully
deterministic (fixed iteration orders, heap tie-broken by an insertion counter).
Ocean cells (``elev <= sea_level``) are held fixed, so the coastline/bay are
carved from the land side only.  Units are meters.
"""

import heapq
import math

_NB = [(-1, 0, 1.0), (1, 0, 1.0), (0, -1, 1.0), (0, 1, 1.0),
       (-1, -1, math.sqrt(2.0)), (-1, 1, math.sqrt(2.0)),
       (1, -1, math.sqrt(2.0)), (1, 1, math.sqrt(2.0))]

_FILL_EPS = 1e-3


def _priority_flood(elev, n):
    """Return a pit-filled copy of ``elev`` (Barnes priority-flood + epsilon)."""
    filled = list(elev)
    visited = bytearray(n * n)
    heap = []
    push = 0
    for c in range(n):
        for i in (c, (n - 1) * n + c):
            if not visited[i]:
                visited[i] = 1
                heapq.heappush(heap, (elev[i], push, i)); push += 1
    for r in range(n):
        for i in (r * n, r * n + n - 1):
            if not visited[i]:
                visited[i] = 1
                heapq.heappush(heap, (elev[i], push, i)); push += 1
    while heap:
        e, _, i = heapq.heappop(heap)
        r, c = divmod(i, n)
        for dr, dc, _d in _NB:
            nr, nc = r + dr, c + dc
            if nr < 0 or nr >= n or nc < 0 or nc >= n:
                continue
            j = nr * n + nc
            if visited[j]:
                continue
            visited[j] = 1
            ne = elev[j]
            if ne <= e:
                ne = e + _FILL_EPS
            filled[j] = ne
            heapq.heappush(heap, (ne, push, j)); push += 1
    return filled


def _flow_dirs(filled, n, step):
    """D8 steepest-descent neighbour + slope for each cell (on ``filled``)."""
    flowdir = [-1] * (n * n)
    slope = [0.0] * (n * n)
    for r in range(n):
        for c in range(n):
            i = r * n + c
            fi = filled[i]
            best = 0.0
            bj = -1
            for dr, dc, dist in _NB:
                nr, nc = r + dr, c + dc
                if nr < 0 or nr >= n or nc < 0 or nc >= n:
                    continue
                j = nr * n + nc
                s = (fi - filled[j]) / (dist * step)
                if s > best:
                    best = s
                    bj = j
            flowdir[i] = bj
            slope[i] = best
    return flowdir, slope


def _accumulate(filled, flowdir, n):
    acc = [1.0] * (n * n)
    order = sorted(range(n * n), key=lambda i: filled[i], reverse=True)
    for i in order:
        j = flowdir[i]
        if j >= 0:
            acc[j] += acc[i]
    return acc


def _accumulate_mfd(filled, n, step, p=1.2):
    """Multiple-flow-direction accumulation (Freeman/Quinn).

    Each cell distributes its drainage to *all* lower neighbours, weighted by
    ``slope**p``, rather than the single steepest one.  This spreads flow
    laterally and removes the D8 8-direction bias that carves straight radial
    rays into a smooth dome, giving smoother, more natural valleys.
    """
    acc = [1.0] * (n * n)
    order = sorted(range(n * n), key=lambda i: filled[i], reverse=True)
    for i in order:
        fi = filled[i]
        r, c = divmod(i, n)
        targets = []
        tot = 0.0
        for dr, dc, dist in _NB:
            nr, nc = r + dr, c + dc
            if nr < 0 or nr >= n or nc < 0 or nc >= n:
                continue
            j = nr * n + nc
            dh = fi - filled[j]
            if dh > 0.0:
                w = (dh / (dist * step)) ** p
                targets.append((j, w))
                tot += w
        if tot > 0.0:
            share = acc[i]
            for j, w in targets:
                acc[j] += share * (w / tot)
    return acc


def _thermal(h, n, step, sea, talus_tan, rate):
    """One thermal/talus relaxation pass (deltas computed then applied)."""
    delta = [0.0] * (n * n)
    for r in range(n):
        for c in range(n):
            i = r * n + c
            if h[i] <= sea:
                continue
            hi = h[i]
            best = 0.0
            bj = -1
            bdist = 1.0
            for dr, dc, dist in _NB:
                nr, nc = r + dr, c + dc
                if nr < 0 or nr >= n or nc < 0 or nc >= n:
                    continue
                j = nr * n + nc
                d = hi - h[j]
                if d > best:
                    best = d
                    bj = j
                    bdist = dist
            if bj < 0:
                continue
            thresh = talus_tan * bdist * step
            if best > thresh:
                move = rate * (best - thresh) * 0.5
                delta[i] -= move
                delta[bj] += move
    for i in range(n * n):
        h[i] += delta[i]
    return h


def erode_grid(elev, n, step, sea_level, iterations,
               strength=1.0, m_exp=0.5, n_exp=1.0, carve_cap=0.5,
               talus_tan=0.55, thermal_rate=0.5, thermal_iterations=2):
    """Erode a flat ``n x n`` heightfield ``elev`` and return the eroded copy.

    ``step`` is the grid spacing (meters); ``sea_level`` cells and below are held
    fixed.  ``strength`` scales the per-iteration incision constant ``K`` (the
    main knob).  ``thermal_iterations`` thermal/talus relaxation passes run after
    each incision step -- more passes diffuse the D8 channels laterally, softening
    the radial streaking a smooth dome produces.  The remaining parameters follow
    the module docstring.  Input is not mutated.
    """
    h = list(elev)
    if iterations <= 0:
        return h
    K = 6.0 * strength
    for _ in range(int(iterations)):
        filled = _priority_flood(h, n)
        flowdir, slope = _flow_dirs(filled, n, step)
        # MFD accumulation for the incision drainage term (smooth, no radial
        # rays); the steepest-descent flowdir/slope drive the incision cap.
        acc = _accumulate_mfd(filled, n, step)

        amax = 1.0
        for i in range(n * n):
            if h[i] > sea_level and acc[i] > amax:
                amax = acc[i]

        newh = list(h)
        for i in range(n * n):
            if h[i] <= sea_level:
                continue
            j = flowdir[i]
            if j < 0:
                continue
            a = acc[i] / amax
            s = slope[i]
            if s <= 0.0:
                continue
            dz = K * (a ** m_exp) * (s ** n_exp)
            drop = h[i] - h[j]
            if drop <= 0.0:
                continue
            cap = carve_cap * drop
            if dz > cap:
                dz = cap
            newh[i] = h[i] - dz
        h = newh
        for _t in range(max(0, int(thermal_iterations))):
            _thermal(h, n, step, sea_level, talus_tan, thermal_rate)
    return h
