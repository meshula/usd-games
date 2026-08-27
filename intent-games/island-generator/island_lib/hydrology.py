
"""River hydrology: flow accumulation, a river network, and a wetness index.

The fourth guide map in the raster-backed family (``heightmap`` / ``populationmap``
/ ``moisturemap`` -> ``wetnessmap``).  It routes water over the terrain to derive
where rivers run and how wet the ground is, which then feeds the biome/moisture
model (greener valleys, riparian banks) and gives the sim a second directed curve
network alongside the roads.

Standard flow-routing pipeline, run on a coarse hydrology grid sampled from the
heightfield (rivers at island scale, and fast/deterministic in pure Python):

  1. **Priority-flood pit fill** (Barnes 2014) -- raise interior pits/flats to a
     spill level with an epsilon gradient so every land cell drains to the sea.
  2. **D8 flow direction** -- each cell drains to its steepest-descent neighbour.
  3. **Flow accumulation** -- processing cells high-to-low, each passes its
     drainage area downstream.
  4. **Channel extraction** -- cells above an accumulation threshold are river
     cells; they are traced from sources through confluences to the coast into a
     directed graph (nodes = sources/confluences/outlets, edges = channel
     polylines) with a **Strahler order** per edge (-> river width).
  5. **Wetness index** -- combines channel proximity with drainage area into a
     ``[0, 1]`` field, rasterized for bilinear ``wetness(x, y)`` lookup and fed to
     ``biome`` moisture.

Fully deterministic for a given
field: fixed iteration orders and heap tie-breaks.  Units are meters; the grid
shares the island coordinate frame (row 0 = north = +radius, col 0 = -radius).
"""

import heapq
import math
from array import array

# 8-neighbour offsets (row, col) and their world-distance multipliers.
_NB = [(-1, 0, 1.0), (1, 0, 1.0), (0, -1, 1.0), (0, 1, 1.0),
       (-1, -1, math.sqrt(2.0)), (-1, 1, math.sqrt(2.0)),
       (1, -1, math.sqrt(2.0)), (1, 1, math.sqrt(2.0))]

_FILL_EPS = 1e-3   # meters; keeps the filled DEM strictly descending (no flats)


class HydrologyMap:
    """Flow accumulation, river network, and wetness over an ``IslandField``.

    ``resolution`` is the hydrology grid size (coarser than the heightmap so
    rivers are island-scaled).  ``river_area_frac`` sets the channel threshold as
    a fraction of the largest drainage area.  ``wetness_falloff_m`` is the
    distance over which channel proximity wetness decays.
    """

    WET_MIN = 0.0
    WET_MAX = 1.0

    def __init__(self, field, resolution=256, river_area_frac=0.08,
                 wetness_falloff_m=70.0, min_river_width=2.0,
                 width_per_order=3.0):
        self.field = field
        self.n = int(resolution)
        if self.n < 4:
            raise ValueError("resolution must be >= 4")
        self.radius = field.radius
        self.step = (2.0 * self.radius) / (self.n - 1)
        self.river_area_frac = float(river_area_frac)
        self.wetness_falloff_m = float(wetness_falloff_m)
        self.min_river_width = float(min_river_width)
        self.width_per_order = float(width_per_order)

        self._sample_elevation()
        self._priority_flood()
        self._flow_directions()
        self._accumulate()
        self._extract_rivers()
        self._build_wetness()

    # ------------------------------------------------------------------ #
    # Grid <-> world                                                     #
    # ------------------------------------------------------------------ #

    def _cell_center(self, r, c):
        return (-self.radius + c * self.step, self.radius - r * self.step)

    def _sample_elevation(self):
        n = self.n
        elev = [0.0] * (n * n)
        h = self.field.height
        for r in range(n):
            y = self.radius - r * self.step
            base = r * n
            for c in range(n):
                x = -self.radius + c * self.step
                elev[base + c] = h(x, y)
        self.elev = elev
        self.sea = self.field.SEA_LEVEL

    # ------------------------------------------------------------------ #
    # 1) Priority-flood pit fill                                         #
    # ------------------------------------------------------------------ #

    def _priority_flood(self):
        n = self.n
        elev = self.elev
        filled = list(elev)
        visited = bytearray(n * n)
        heap = []
        push = 0
        # Seed with the grid boundary (all ocean around the centered island).
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
                    ne = e + _FILL_EPS   # raise pit/flat to keep descending
                filled[j] = ne
                heapq.heappush(heap, (ne, push, j)); push += 1
        self.filled = filled

    # ------------------------------------------------------------------ #
    # 2) D8 flow direction                                               #
    # ------------------------------------------------------------------ #

    def _flow_directions(self):
        n = self.n
        filled = self.filled
        step = self.step
        flowdir = [-1] * (n * n)
        for r in range(n):
            for c in range(n):
                i = r * n + c
                fi = filled[i]
                best_slope = 0.0
                best_j = -1
                for dr, dc, dist in _NB:
                    nr, nc = r + dr, c + dc
                    if nr < 0 or nr >= n or nc < 0 or nc >= n:
                        continue
                    j = nr * n + nc
                    drop = fi - filled[j]
                    if drop <= 0.0:
                        continue
                    slope = drop / (dist * step)
                    if slope > best_slope:
                        best_slope = slope
                        best_j = j
                flowdir[i] = best_j
        self.flowdir = flowdir

    # ------------------------------------------------------------------ #
    # 3) Flow accumulation                                               #
    # ------------------------------------------------------------------ #

    def _accumulate(self):
        n = self.n
        filled = self.filled
        flowdir = self.flowdir
        acc = [1.0] * (n * n)
        # Process high -> low so upstream is done before downstream.
        order = sorted(range(n * n), key=lambda i: filled[i], reverse=True)
        for i in order:
            j = flowdir[i]
            if j >= 0:
                acc[j] += acc[i]
        self.acc = acc
        self._proc_order = order   # reused for Strahler topological pass

    # ------------------------------------------------------------------ #
    # 4) Channel extraction -> river graph                               #
    # ------------------------------------------------------------------ #

    def _extract_rivers(self):
        n = self.n
        elev = self.elev
        acc = self.acc
        flowdir = self.flowdir
        sea = self.sea

        land_max_acc = 0.0
        for i in range(n * n):
            if elev[i] > sea and acc[i] > land_max_acc:
                land_max_acc = acc[i]
        threshold = max(2.0, self.river_area_frac * land_max_acc)
        self.channel_threshold = threshold

        channel = bytearray(n * n)
        for i in range(n * n):
            if elev[i] > sea and acc[i] >= threshold:
                channel[i] = 1
        self.channel = channel

        # Downstream-in-channel inflow counts + node classification.
        inflow = [0] * (n * n)
        for i in range(n * n):
            if not channel[i]:
                continue
            j = flowdir[i]
            if j >= 0 and channel[j]:
                inflow[j] += 1

        def is_outlet(i):
            j = flowdir[i]
            return j < 0 or not channel[j]

        is_node = bytearray(n * n)
        for i in range(n * n):
            if not channel[i]:
                continue
            if inflow[i] != 1 or is_outlet(i):
                is_node[i] = 1

        # Trace edges: from each node, walk downstream to the next node.
        node_index = {}
        nodes = []

        def node_id(i):
            idx = node_index.get(i)
            if idx is None:
                idx = len(nodes)
                node_index[i] = idx
                nodes.append(self._cell_center(*divmod(i, n)))
            return idx

        edges = []   # dicts: {u, v, cells, points}
        for s in range(n * n):
            if not (channel[s] and is_node[s]):
                continue
            j = flowdir[s]
            if j < 0 or not channel[j]:
                continue   # terminal outlet node: no outgoing channel edge
            cells = [s]
            cur = j
            while True:
                cells.append(cur)
                if is_node[cur] or is_outlet(cur):
                    break
                cur = flowdir[cur]
            u = node_id(s)
            v = node_id(cur)
            pts = [self._cell_center(*divmod(ci, n)) for ci in cells]
            edges.append({"u": u, "v": v, "cells": cells, "points": pts})

        self._strahler(edges)
        self.nodes = nodes
        self.edges = edges

    def _strahler(self, edges):
        """Assign Strahler order + width to each edge (upstream -> downstream)."""
        n = self.n
        filled = self.filled
        incoming = {}
        for idx, e in enumerate(edges):
            incoming.setdefault(e["v"], []).append(idx)
        # Upstream nodes sit higher; process edges by upstream-node elevation
        # descending so tributaries are ordered before the edge they feed.
        def up_elev(idx):
            e = edges[idx]
            ci = e["cells"][0]
            return filled[ci]
        for idx in sorted(range(len(edges)), key=up_elev, reverse=True):
            u = edges[idx]["u"]
            ins = incoming.get(u, ())
            if not ins:
                order = 1
            else:
                os = [edges[k]["order"] for k in ins]
                mx = max(os)
                order = mx + 1 if os.count(mx) >= 2 else mx
            edges[idx]["order"] = order
            edges[idx]["width"] = self.min_river_width + \
                (order - 1) * self.width_per_order

    def rivers(self):
        """Return ``(nodes, edges)``.

        ``nodes = [(x, y), ...]`` are junction points (sources / confluences /
        coastal outlets).  ``edges`` are dicts ``{u, v, points, order, width}``
        directed downstream (``u`` upstream node -> ``v`` downstream), where
        ``points`` is the draped channel polyline in 2D world coords.
        """
        return self.nodes, self.edges

    # ------------------------------------------------------------------ #
    # 5) Wetness index                                                    #
    # ------------------------------------------------------------------ #

    def _build_wetness(self):
        n = self.n
        elev = self.elev
        acc = self.acc
        channel = self.channel
        step = self.step
        sea = self.sea

        # Multi-source Dijkstra distance (meters) to the nearest channel cell.
        INF = float("inf")
        dist = [INF] * (n * n)
        heap = []
        push = 0
        for i in range(n * n):
            if channel[i]:
                dist[i] = 0.0
                heapq.heappush(heap, (0.0, push, i)); push += 1
        while heap:
            d, _, i = heapq.heappop(heap)
            if d > dist[i]:
                continue
            r, c = divmod(i, n)
            for dr, dc, dd in _NB:
                nr, nc = r + dr, c + dc
                if nr < 0 or nr >= n or nc < 0 or nc >= n:
                    continue
                j = nr * n + nc
                nd = d + dd * step
                if nd < dist[j]:
                    dist[j] = nd
                    heapq.heappush(heap, (nd, push, j)); push += 1

        falloff = max(1.0, self.wetness_falloff_m)
        log_max = math.log(1.0 + max(acc)) or 1.0
        w = [0.0] * (n * n)
        for i in range(n * n):
            if elev[i] <= sea:
                w[i] = 0.0
                continue
            prox = math.exp(-dist[i] / falloff)          # near-channel wetness
            valley = math.log(1.0 + acc[i]) / log_max     # drainage-area wetness
            val = 0.6 * prox + 0.4 * valley
            w[i] = 0.0 if val < 0.0 else (1.0 if val > 1.0 else val)

        # Quantize to a 16-bit raster for bilinear sampling.
        inv_span = 65535.0 / (self.WET_MAX - self.WET_MIN)
        w16 = array('H', bytes(2 * n * n))
        for i in range(n * n):
            q = int(round((w[i] - self.WET_MIN) * inv_span))
            w16[i] = 0 if q < 0 else (65535 if q > 65535 else q)
        self._w16 = w16

    def _dequant(self, q):
        return self.WET_MIN + (q / 65535.0) * (self.WET_MAX - self.WET_MIN)

    def _grid_value(self, col, row):
        n = self.n
        if col < 0:
            col = 0
        elif col >= n:
            col = n - 1
        if row < 0:
            row = 0
        elif row >= n:
            row = n - 1
        return self._dequant(self._w16[row * n + col])

    def wetness(self, x, y):
        """Wetness in ``[0, 1]`` at world-space (x, y) (bilinear raster read)."""
        n = self.n
        step = self.step
        gc = (x + self.radius) / step
        gr = (self.radius - y) / step
        c0 = int(math.floor(gc))
        r0 = int(math.floor(gr))
        fc = gc - c0
        fr = gr - r0
        v00 = self._grid_value(c0, r0)
        v10 = self._grid_value(c0 + 1, r0)
        v01 = self._grid_value(c0, r0 + 1)
        v11 = self._grid_value(c0 + 1, r0 + 1)
        a = v00 * (1.0 - fc) + v10 * fc
        b = v01 * (1.0 - fc) + v11 * fc
        return a * (1.0 - fr) + b * fr

    # ------------------------------------------------------------------ #
    # Artifact export                                                    #
    # ------------------------------------------------------------------ #

    def save_wetnessmap(self, path):
        """Write the wetness raster to ``path`` as a 16-bit PNG."""
        from PIL import Image
        n = self.n
        im = Image.frombytes("I;16", (n, n), self._w16.tobytes())
        im.save(path, format="PNG")
        return path
