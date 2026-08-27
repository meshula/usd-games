
"""Procedural road-network core -- a pure-Python port of citygen-godot.

This module grows a road network with the priority-queue / local-constraints /
global-goals algorithm from the ``citygen`` project
(``citygen-godot/scripts/{city_gen,segment,math}.gd``). The algorithm is the 
sim substrate, so it must be
unit-testable under a plain ``python3`` interpreter.  ``island_lib.town`` imports
this module and authors the result to USD.

The Godot original leans on engine services that have no place in a headless,
deterministic port; each is replaced by a small local equivalent:

  * ``Physics2DServer`` circle/shape queries (used purely as a proximity index)
    -> ``_SegmentGrid``, a deterministic uniform spatial hash.
  * ``Geometry.segment_intersects_segment_2d`` / ``get_closest_point_to_segment_2d``
    -> ``_segment_intersect`` / ``_closest_point_on_segment``.
  * ``math.gd`` (``random_angle``/``min_degree_difference``/
    ``is_point_in_segment_range``) -> the ``_random_angle`` / ``_min_degree_difference``
    / ``_is_point_in_segment_range`` helpers.
  * global ``randf``/``rand_range`` -> a single ``random.Random`` threaded
    explicitly through every call that consumes randomness.

The population signal is duck-typed: any object with ``sample(x, y) -> [0, 1]``
works (``island_lib.population.PopulationSource`` and friends), so this module
does not import ``population`` either.

**Coordinate / direction convention.**  Points are 2D ``(x, y)`` float tuples in
island world coordinates -- the same frame ``scatter.terrain_grid`` maps to USD
``(x, height, y)``.  citygen's internal direction convention is preserved
verbatim (``end = start + len * (sin theta, cos theta)`` and
``direction = -atan2(dy, dx) + 90 deg``); it is self-consistent, and authoring
derives geometry from ``start``/``end`` so it never depends on the convention.

**Determinism.**  A single threaded ``random.Random``, a ``heapq`` keyed by
``(t, push_counter)`` so ties resolve to the earliest-pushed segment (matching
Godot's linear "first min-t" scan), and a fixed iteration order everywhere mean
identical ``(seed, params, population)`` yields an identical segment list --
which in turn lets ``town.py`` write byte-identical ``.usda``.

Buildings (``generate_buildings`` + ``_BuildingGrid``) land in a later sub-phase;
the public API here is ``RoadNetworkParams``, ``generate_segments``, and
``build_graph``.  Units are meters.
"""

import heapq
import math
from dataclasses import dataclass


# --------------------------------------------------------------------------- #
# Approximate float / point equality (mirrors Godot's is_equal_approx)        #
# --------------------------------------------------------------------------- #

_EPSILON = 1e-5


def _f_equal(a, b):
    """Relative float equality, mirroring Godot's ``Math::is_equal_approx``."""
    if a == b:
        return True
    tol = _EPSILON * abs(a)
    if tol < _EPSILON:
        tol = _EPSILON
    return abs(a - b) < tol


def _pt_equal(a, b):
    return _f_equal(a[0], b[0]) and _f_equal(a[1], b[1])


def _dist_sq(a, b):
    dx = a[0] - b[0]
    dy = a[1] - b[1]
    return dx * dx + dy * dy


# --------------------------------------------------------------------------- #
# 2D geometry helpers (replace Godot's Geometry.*)                            #
# --------------------------------------------------------------------------- #

def _segment_intersect(from_a, to_a, from_b, to_b):
    """Intersection point of segments A(from_a->to_a) and B(from_b->to_b).

    Faithful port of Godot's ``Geometry::segment_intersects_segment_2d``:
    returns the crossing point as ``(x, y)`` or ``None`` if the segments do not
    properly cross.
    """
    bx = to_a[0] - from_a[0]
    by = to_a[1] - from_a[1]
    cx = from_b[0] - from_a[0]
    cy = from_b[1] - from_a[1]
    dx = to_b[0] - from_a[0]
    dy = to_b[1] - from_a[1]

    ab_len = bx * bx + by * by
    if ab_len <= 0.0:
        return None
    bnx = bx / ab_len
    bny = by / ab_len
    # Rotate C and D into B's frame.
    c2x = cx * bnx + cy * bny
    c2y = cy * bnx - cx * bny
    d2x = dx * bnx + dy * bny
    d2y = dy * bnx - dx * bny

    if (c2y < 0.0 and d2y < 0.0) or (c2y >= 0.0 and d2y >= 0.0):
        return None

    ab_pos = d2x + (c2x - d2x) * d2y / (d2y - c2y)
    if ab_pos < 0.0 or ab_pos > 1.0:
        return None
    return (from_a[0] + bx * ab_pos, from_a[1] + by * ab_pos)


def _closest_point_on_segment(p, a, b):
    """Closest point on segment ``a->b`` to point ``p`` (Godot parity)."""
    px = p[0] - a[0]
    py = p[1] - a[1]
    nx = b[0] - a[0]
    ny = b[1] - a[1]
    l2 = nx * nx + ny * ny
    if l2 < 1e-20:
        return a
    d = (nx * px + ny * py) / l2
    if d <= 0.0:
        return a
    if d >= 1.0:
        return b
    return (a[0] + nx * d, a[1] + ny * d)


# --------------------------------------------------------------------------- #
# math.gd port                                                                #
# --------------------------------------------------------------------------- #

def _random_angle(rng, limit):
    """Non-uniform random angle in ``(-limit, limit)`` (port of Math.random_angle).

    Ported verbatim against the threaded ``rng``.  The cubic accept/reject loop
    (including Godot's quirk that negative values are always accepted, since a
    negative cube is below any ``rng.random()``) is preserved for faithful
    behaviour; per-seed determinism is what matters, not bit-parity with Godot.
    """
    non_uniform_norm = limit ** 3
    val = 0.0
    while val == 0.0 or rng.random() < (val ** 3) / non_uniform_norm:
        val = rng.uniform(-limit, limit)
    return val


def _min_degree_difference(d1, d2):
    diff = math.fmod(abs(d1 - d2), 180.0)
    return min(diff, abs(diff - 180.0))


def _is_point_in_segment_range(point, seg_start, seg_end):
    vx = seg_end[0] - seg_start[0]
    vy = seg_end[1] - seg_start[1]
    dot = (point[0] - seg_start[0]) * vx + (point[1] - seg_start[1]) * vy
    return dot >= 0.0 and dot <= (vx * vx + vy * vy)


# --------------------------------------------------------------------------- #
# Segment                                                                     #
# --------------------------------------------------------------------------- #

@dataclass
class _SegmentMetadata:
    """Per-segment global-goals metadata (port of ``SegmentMetadata``)."""
    highway: bool = False
    severed: bool = False

    def clone(self):
        return _SegmentMetadata(highway=self.highway, severed=self.severed)


class _Segment:
    """A road segment (port of ``Segment``).

    ``start``/``end`` are ``(x, y)`` tuples exposed as properties; assigning to
    either bumps ``_revision`` so the cached ``direction``/``length`` recompute
    lazily -- the direct analogue of Godot's ``segment_revision`` bookkeeping.
    Link arrays hold references to other ``_Segment`` objects; identity (``is``)
    is used throughout, so ``_Segment`` intentionally has no ``__eq__``/``__hash__``.
    ``_cells`` records the grid cells the segment currently occupies for O(1)
    removal on ``split``.
    """

    __slots__ = ("_start", "_end", "t", "metadata", "links_b", "links_f",
                 "previous_segment_to_link", "_revision", "_dir_rev", "_len_rev",
                 "_dir", "_len", "_cells")

    def __init__(self, start, end, t, metadata):
        self._start = start
        self._end = end
        self.t = t
        self.metadata = metadata
        self.links_b = []
        self.links_f = []
        self.previous_segment_to_link = None
        self._revision = 0
        self._dir_rev = -1
        self._len_rev = -1
        self._dir = 0.0
        self._len = 0.0
        self._cells = None

    @property
    def start(self):
        return self._start

    @start.setter
    def start(self, v):
        self._start = v
        self._revision += 1

    @property
    def end(self):
        return self._end

    @end.setter
    def end(self, v):
        self._end = v
        self._revision += 1

    @property
    def direction(self):
        if self._dir_rev != self._revision:
            self._dir_rev = self._revision
            dx = self._end[0] - self._start[0]
            dy = self._end[1] - self._start[1]
            self._dir = math.degrees(-math.atan2(dy, dx)) + 90.0
        return self._dir

    @property
    def length(self):
        if self._len_rev != self._revision:
            self._len_rev = self._revision
            dx = self._end[0] - self._start[0]
            dy = self._end[1] - self._start[1]
            self._len = math.hypot(dx, dy)
        return self._len

    @staticmethod
    def new_using_direction(start, direction, length, t, metadata):
        rad = math.radians(direction)
        end = (start[0] + length * math.sin(rad),
               start[1] + length * math.cos(rad))
        return _Segment(start, end, t, metadata)

    def clone(self):
        return _Segment(self.start, self.end, self.t, self.metadata.clone())

    def start_is_backwards(self):
        if self.links_b:
            other = self.links_b[0]
            return _pt_equal(other.start, self.start) or \
                _pt_equal(other.end, self.start)
        elif self.links_f:
            other = self.links_f[0]
            return _pt_equal(other.start, self.end) or \
                _pt_equal(other.end, self.end)
        return False

    def intersection_with(self, other):
        point = _segment_intersect(self.start, self.end, other.start, other.end)
        if point is None:
            return None
        # Ignore intersections at either segment's endpoints -- not useful.
        if (_pt_equal(point, self.start) or _pt_equal(point, self.end) or
                _pt_equal(point, other.start) or _pt_equal(point, other.end)):
            return None
        return point

    def links_for_end_containing(self, segment):
        if any(l is segment for l in self.links_b):
            return self.links_b
        if any(l is segment for l in self.links_f):
            return self.links_f
        return None

    def setup_branch_links(self):
        if self.previous_segment_to_link is None:
            return
        # Link this new branch to every existing branch stemming from the
        # previous segment.  We append to previous.links_f only after the loop,
        # so the iteration set is stable.
        for link in self.previous_segment_to_link.links_f:
            self.links_b.append(link)
            lst = link.links_for_end_containing(self.previous_segment_to_link)
            if lst is not None:
                lst.append(self)
        self.previous_segment_to_link.links_f.append(self)
        self.links_b.append(self.previous_segment_to_link)

    def split(self, point, segment, segments, grid):
        """Split this (accepted) segment at ``point``, wiring in ``segment``.

        Mirrors ``Segment.split``: a cloned ``split_part`` takes the far half,
        this segment's ``start`` moves to ``point``, and the link arrays are
        rewired so the junction is consistent.  Unlike the Godot original -- which
        left its physics shape stale after moving ``start`` -- we keep the spatial
        index correct: this segment is removed and re-inserted with its new
        geometry, and ``split_part`` is inserted.
        """
        start_is_backwards = self.start_is_backwards()
        split_part = self.clone()
        segments.append(split_part)
        split_part.end = point
        self.start = point

        # Links are not copied by clone(); duplicate the arrays (same refs).
        split_part.links_b = list(self.links_b)
        split_part.links_f = list(self.links_f)

        if start_is_backwards:
            first_split = split_part
            second_split = self
            fix_links = split_part.links_b
        else:
            first_split = self
            second_split = split_part
            fix_links = split_part.links_f

        for link in fix_links:
            replaced = False
            for i, l in enumerate(link.links_b):
                if l is self:
                    link.links_b[i] = split_part
                    replaced = True
                    break
            if not replaced:
                for i, l in enumerate(link.links_f):
                    if l is self:
                        link.links_f[i] = split_part
                        break

        first_split.links_f = [segment, second_split]
        second_split.links_b = [segment, first_split]

        segment.links_f.append(first_split)
        segment.links_f.append(second_split)

        # Keep the spatial index consistent with the mutated geometry.
        grid.remove(self)
        grid.insert(self)
        grid.insert(split_part)


# --------------------------------------------------------------------------- #
# Spatial hash (replaces the Physics2DServer proximity index)                 #
# --------------------------------------------------------------------------- #

class _SegmentGrid:
    """Deterministic uniform spatial hash over accepted segments.

    Each segment is indexed into every cell its axis-aligned bounding box
    overlaps (a conservative superset of the cells the segment passes through),
    so ``query_near`` never misses a candidate -- the exact geometric tests in
    ``_local_constraints`` do the real filtering.  Candidate order is fully
    deterministic: cells are visited in sorted ``(cx, cy)`` order and segments in
    per-cell insertion order, with first-seen dedupe.
    """

    def __init__(self, cell_size):
        self.cell_size = float(cell_size)
        self.cells = {}

    def _cell_range(self, minx, miny, maxx, maxy):
        cs = self.cell_size
        c0x = int(math.floor(minx / cs))
        c1x = int(math.floor(maxx / cs))
        c0y = int(math.floor(miny / cs))
        c1y = int(math.floor(maxy / cs))
        return c0x, c1x, c0y, c1y

    def _cells_for_seg(self, seg):
        (x0, y0) = seg.start
        (x1, y1) = seg.end
        minx, maxx = (x0, x1) if x0 <= x1 else (x1, x0)
        miny, maxy = (y0, y1) if y0 <= y1 else (y1, y0)
        c0x, c1x, c0y, c1y = self._cell_range(minx, miny, maxx, maxy)
        keys = []
        for cx in range(c0x, c1x + 1):
            for cy in range(c0y, c1y + 1):
                keys.append((cx, cy))
        return keys

    def insert(self, seg):
        keys = self._cells_for_seg(seg)
        seg._cells = keys
        for k in keys:
            self.cells.setdefault(k, []).append(seg)

    def remove(self, seg):
        if seg._cells is None:
            return
        for k in seg._cells:
            lst = self.cells.get(k)
            if lst:
                for i, s in enumerate(lst):
                    if s is seg:
                        del lst[i]
                        break
        seg._cells = None

    def query_near(self, center, radius):
        c0x, c1x, c0y, c1y = self._cell_range(
            center[0] - radius, center[1] - radius,
            center[0] + radius, center[1] + radius)
        out = []
        seen = set()
        for cx in range(c0x, c1x + 1):
            for cy in range(c0y, c1y + 1):
                lst = self.cells.get((cx, cy))
                if not lst:
                    continue
                for seg in lst:
                    key = id(seg)
                    if key not in seen:
                        seen.add(key)
                        out.append(seg)
        return out


# --------------------------------------------------------------------------- #
# Parameters                                                                  #
# --------------------------------------------------------------------------- #

@dataclass
class RoadNetworkParams:
    """All citygen tunables, with island-appropriate defaults.

    Lengths and distances are in meters, tuned for the 2 km island (radius
    1000 m) rather than citygen's ~world-unit demo scale.  The two population
    thresholds are the key island deviation: citygen uses ``0.5``, but the island
    population heat map rarely exceeds ``0.5`` off the harbour, so a faithful
    ``0.5`` would grow almost nothing under ``--population island``.  The default
    ``0.15`` matches the Phase-3 placeholder gate, yielding a real network on the
    island while staying tunable.
    """
    segment_limit: int = 2000
    highway_length: float = 160.0
    street_length: float = 90.0
    snap_distance: float = 30.0
    branch_angle_dev: float = 3.0
    straight_angle_dev: float = 15.0
    min_intersection_dev: float = 30.0
    default_branch_prob: float = 0.4
    highway_branch_prob: float = 0.05
    normal_pop_threshold: float = 0.15
    highway_pop_threshold: float = 0.15
    normal_branch_delay_from_highway: int = 5
    # Spatial-hash cell size; ``None`` -> derived from the segment lengths.
    grid_cell_size: float = None
    # Junction merge tolerance (meters) used by ``build_graph``.
    node_merge_eps: float = 1.0

    # --- Island adaptations (default to faithful citygen behaviour) --------- #
    # Where the seed highway is planted.  citygen uses the origin; on the island
    # the origin is the (unpopulated) peak, so the island adapter passes a
    # populated coastal point instead.
    root_origin: tuple = (0.0, 0.0)
    # If not ``None``, a candidate segment whose *end* samples population at or
    # below this value is rejected.  Because ``IslandPopulation`` is exactly 0
    # over water, a small positive value keeps every road class -- highways
    # included -- on populated land and off the ocean.  ``None`` disables the
    # gate, reproducing citygen's unconditional highway growth.
    water_pop_threshold: float = None

    # --- Terrain-following (grade) bias ------------------------------------- #
    # When > 0 (and a ``terrain`` is supplied to ``generate_segments``), the
    # straight *continuation* direction is chosen from a fan of candidates across
    # the straight-deviation cone by ``score = population - grade_weight * slope``
    # (slope = |dh| / length).  This biases arterials to follow isoclines
    # (minimise climbing) while population still pulls them toward density; the
    # +/-90 deg branches stay rectilinear, acting as the connectors that traverse
    # elevation.  ``0.0`` disables the bias, reproducing citygen's pop-only choice.
    grade_weight: float = 0.0
    # Half-angle (deg) of the fan the grade bias steers within.  A wider cone than
    # ``straight_angle_dev`` lets arterials bend to hug contours (and opens more
    # branch opportunities) without making the pop-only path wigglier.  ``None``
    # -> fall back to ``straight_angle_dev``.
    grade_cone_dev: float = None
    # Number of candidate directions sampled across [-grade_cone_dev, +cone]
    # (odd -> the straight/0-offset direction is always included).
    grade_fan_samples: int = 7

    # --- Buildings (4c) ----------------------------------------------------- #
    # Place buildings around every Nth segment; each of ``count`` attempts
    # scatters a candidate within ``max_building_distance`` of the segment
    # midpoint and keeps it only if the rejection/pushout loop clears roads and
    # other buildings.  Sizes are island-scaled (citygen's ~80-150 unit diagonals
    # belong to its 300-400 unit streets; the island's streets are ~90 m).
    building_segment_period: int = 5
    building_count_per_segment: int = 10
    building_placement_attempts: int = 3
    max_building_distance: float = 60.0
    building_diagonal_min: float = 6.0     # half-diagonal (centre -> corner), m
    building_diagonal_max: float = 14.0
    building_aspect_min: float = 0.5
    building_aspect_max: float = 2.0
    building_height_min: float = 4.0
    building_height_max: float = 16.0
    # Effective road width (m) buildings must clear; sized to clear highways.
    road_collider_width: float = 12.0
    # Buildings are dropped where population is at or below this (0 over water).
    # ``None`` -> fall back to ``normal_pop_threshold``.
    building_pop_threshold: float = None

    def __post_init__(self):
        if self.grid_cell_size is None:
            self.grid_cell_size = max(self.highway_length, self.street_length)
        if self.grade_cone_dev is None:
            self.grade_cone_dev = self.straight_angle_dev
        if self.building_pop_threshold is None:
            self.building_pop_threshold = self.normal_pop_threshold


# --------------------------------------------------------------------------- #
# Local constraints                                                            #
# --------------------------------------------------------------------------- #

def _local_constraints(seg, grid, params, segments):
    """Adjust ``seg`` to fit its neighbourhood; return whether to accept it.

    Port of ``local_constraints`` + the three action classes.  Only
    already-accepted segments (those in ``grid``) are considered.  The winning
    action is selected by priority -- intersection (4) > snap (3) >
    intersection-in-radius (2) -- then applied, possibly mutating ``seg`` and the
    grid (via ``split``).
    """
    action = None            # (kind, other, point)
    action_priority = 0
    prev_int_dist_sq = None

    center = ((seg.start[0] + seg.end[0]) * 0.5,
              (seg.start[1] + seg.end[1]) * 0.5)
    radius = seg.length * 0.5 + params.snap_distance
    snap_sq = params.snap_distance * params.snap_distance

    for other in grid.query_near(center, radius):
        # Intersection check (priority 4).
        if action_priority <= 4:
            inter = seg.intersection_with(other)
            if inter is not None:
                d2 = _dist_sq(seg.start, inter)
                if prev_int_dist_sq is None or d2 < prev_int_dist_sq:
                    prev_int_dist_sq = d2
                    action_priority = 4
                    action = ("intersect", other, inter)

        # Snap to an existing endpoint within radius (priority 3).
        if action_priority <= 3:
            if _dist_sq(seg.end, other.end) <= snap_sq:
                action_priority = 3
                action = ("snap", other, other.end)

        # Intersection within radius (priority 2).
        if action_priority <= 2:
            if _is_point_in_segment_range(seg.end, other.start, other.end):
                inter = _closest_point_on_segment(seg.end, other.start, other.end)
                if _dist_sq(seg.end, inter) < snap_sq:
                    action_priority = 2
                    action = ("radius", other, inter)

    if action is None:
        return True

    kind, other, point = action
    if kind == "intersect":
        if _min_degree_difference(other.direction, seg.direction) < \
                params.min_intersection_dev:
            return False
        other.split(point, seg, segments, grid)
        seg.end = point
        seg.metadata.severed = True
        return True

    if kind == "snap":
        seg.end = point
        seg.metadata.severed = True
        # Update the links at the far end of ``other``.
        links = other.links_f if other.start_is_backwards() else other.links_b
        for link in links:
            if ((_pt_equal(link.start, seg.end) and _pt_equal(link.end, seg.start)) or
                    (_pt_equal(link.start, seg.start) and _pt_equal(link.end, seg.end))):
                return False  # duplicate line already exists
        for link in links:
            lst = link.links_for_end_containing(other)
            if lst is not None:
                lst.append(seg)
            seg.links_f.append(link)
        links.append(seg)
        seg.links_f.append(other)
        return True

    # kind == "radius"
    seg.end = point
    seg.metadata.severed = True
    if _min_degree_difference(other.direction, seg.direction) < \
            params.min_intersection_dev:
        return False
    other.split(point, seg, segments, grid)
    return True


# --------------------------------------------------------------------------- #
# Global goals                                                                #
# --------------------------------------------------------------------------- #

def _sample_population(pop, start, end):
    return (pop.sample(start[0], start[1]) + pop.sample(end[0], end[1])) * 0.5


def _global_goals(rng, previous, pop, params, terrain=None):
    """Spawn candidate follow-on segments (port of ``global_goals_generate``).

    When ``terrain`` is supplied and ``params.grade_weight > 0``, the straight
    *continuation* direction is chosen from a fan of candidates across the
    straight-deviation cone to minimise ``grade_weight * slope - population``
    (i.e. follow isoclines while still favouring density).  Otherwise the choice
    is citygen's pop-only pick between the straight and one random direction.  The
    +/-90 deg branches are unchanged either way -- they are the rectilinear
    connectors that traverse elevation.
    """
    new_branches = []
    if not previous.metadata.severed:
        prev_dir = previous.direction
        prev_end = previous.end
        prev_len = previous.length
        prev_meta = previous.metadata

        def seg_continue(direction):
            # NB: shares ``prev_meta`` with the previous segment, exactly as the
            # Godot ``segment_continue`` does (no metadata clone).
            return _Segment.new_using_direction(
                prev_end, direction, prev_len, 0, prev_meta)

        def seg_branch(direction):
            t = params.normal_branch_delay_from_highway \
                if prev_meta.highway else 0
            return _Segment.new_using_direction(
                prev_end, direction, params.street_length, t,
                _SegmentMetadata())

        grade_mode = terrain is not None and params.grade_weight > 0.0

        def _slope(seg):
            h0 = terrain.height(seg.start[0], seg.start[1])
            h1 = terrain.height(seg.end[0], seg.end[1])
            return abs(h1 - h0) / seg.length if seg.length > 1e-9 else 0.0

        def _best_continue():
            """Fan of continuation dirs; pick max ``pop - grade_weight*slope``.

            Returns ``(chosen_segment, its_population)``.  The fan spans
            ``[-straight_dev, +straight_dev]``; an odd sample count includes the
            straight (0-offset) direction, so with ``grade_weight -> 0`` this
            degrades to the plain straight continuation.
            """
            sdev = params.grade_cone_dev
            m = max(2, params.grade_fan_samples)
            best = None
            for i in range(m):
                off = -sdev + (2.0 * sdev) * i / (m - 1)
                cand = seg_continue(prev_dir + off)
                p = _sample_population(pop, cand.start, cand.end)
                score = p - params.grade_weight * _slope(cand)
                if best is None or score > best[0]:
                    best = (score, p, cand)
            return best[2], best[1]

        continue_straight = seg_continue(prev_dir)
        straight_pop = _sample_population(
            pop, continue_straight.start, continue_straight.end)

        if prev_meta.highway:
            if grade_mode:
                chosen, road_pop = _best_continue()
                new_branches.append(chosen)
            else:
                random_straight = seg_continue(
                    prev_dir + _random_angle(rng, params.straight_angle_dev))
                random_pop = _sample_population(
                    pop, random_straight.start, random_straight.end)
                if random_pop > straight_pop:
                    new_branches.append(random_straight)
                    road_pop = random_pop
                else:
                    new_branches.append(continue_straight)
                    road_pop = straight_pop
            if road_pop > params.highway_pop_threshold:
                if rng.random() < params.highway_branch_prob:
                    new_branches.append(seg_continue(
                        prev_dir - 90.0 + _random_angle(
                            rng, params.branch_angle_dev)))
                elif rng.random() < params.highway_branch_prob:
                    new_branches.append(seg_continue(
                        prev_dir + 90.0 + _random_angle(
                            rng, params.branch_angle_dev)))
        elif straight_pop > params.normal_pop_threshold:
            if grade_mode:
                chosen, _ = _best_continue()
                new_branches.append(chosen)
            else:
                new_branches.append(continue_straight)

        if straight_pop > params.normal_pop_threshold:
            if rng.random() < params.default_branch_prob:
                new_branches.append(seg_branch(
                    prev_dir - 90.0 + _random_angle(
                        rng, params.branch_angle_dev)))
            elif rng.random() < params.default_branch_prob:
                new_branches.append(seg_branch(
                    prev_dir + 90.0 + _random_angle(
                        rng, params.branch_angle_dev)))

    for branch in new_branches:
        branch.previous_segment_to_link = previous
    return new_branches


# --------------------------------------------------------------------------- #
# Segment generation                                                          #
# --------------------------------------------------------------------------- #

def generate_segments(rng, pop, params, terrain=None):
    """Grow and return the list of accepted ``_Segment`` objects.

    ``rng`` is a ``random.Random``; ``pop`` is any ``sample(x, y) -> [0, 1]``
    source; ``params`` is a ``RoadNetworkParams``.  ``terrain`` (optional, any
    object with ``height(x, y)``) enables the grade bias when
    ``params.grade_weight > 0``.  The priority queue is a ``heapq`` keyed by
    ``(t, push_counter)`` so equal-``t`` segments pop in push order, reproducing
    Godot's linear "first min-t" scan.
    """
    segments = []
    grid = _SegmentGrid(params.grid_cell_size)
    heap = []
    push = 0

    ox, oy = params.root_origin
    root_meta = _SegmentMetadata(highway=True)
    root = _Segment((ox, oy), (ox + params.highway_length, oy), 0, root_meta)

    opposite = root.clone()
    opposite.end = (ox - params.highway_length, opposite.end[1])
    opposite.links_b.append(root)
    root.links_b.append(opposite)

    heapq.heappush(heap, (root.t, push, root))
    push += 1
    heapq.heappush(heap, (opposite.t, push, opposite))
    push += 1

    water_thresh = params.water_pop_threshold
    while heap and len(segments) < params.segment_limit:
        _, _, seg = heapq.heappop(heap)
        # Island gate: drop candidates whose frontier endpoint is over water
        # (population 0).  Disabled when ``water_pop_threshold`` is None.
        if water_thresh is not None and \
                pop.sample(seg.end[0], seg.end[1]) <= water_thresh:
            continue
        if _local_constraints(seg, grid, params, segments):
            seg.setup_branch_links()
            grid.insert(seg)
            segments.append(seg)
            for new_seg in _global_goals(rng, seg, pop, params, terrain):
                new_seg.t = seg.t + 1 + new_seg.t
                heapq.heappush(heap, (new_seg.t, push, new_seg))
                push += 1

    return segments


# --------------------------------------------------------------------------- #
# Graph extraction                                                            #
# --------------------------------------------------------------------------- #

def build_graph(segments, eps=1.0):
    """Derive the intersection graph from segment endpoints.

    Returns ``(nodes, edges)`` where ``nodes = [(x, y), ...]`` are deduped
    junction points (endpoints quantized to an ``eps``-grid so coincident
    junctions merge) and ``edges = [(node_i, node_j, is_highway), ...]``, one per
    non-degenerate segment.  Adjacency is derived purely from shared endpoints,
    independent of the internal ``links_*`` bookkeeping, so it stays valid
    regardless of how the segments were grown.
    """
    nodes = []
    index = {}

    def node_for(pt):
        key = (int(round(pt[0] / eps)), int(round(pt[1] / eps)))
        idx = index.get(key)
        if idx is None:
            idx = len(nodes)
            index[key] = idx
            nodes.append((pt[0], pt[1]))
        return idx

    edges = []
    for seg in segments:
        i = node_for(seg.start)
        j = node_for(seg.end)
        if i == j:
            continue  # degenerate (zero-length after snapping)
        edges.append((i, j, bool(seg.metadata.highway)))

    return nodes, edges


# --------------------------------------------------------------------------- #
# Buildings                                                                   #
# --------------------------------------------------------------------------- #

@dataclass
class Building:
    """A footprint placed near a road (port of ``building.gd``).

    ``center`` is the 2D ``(x, y)`` centre; ``direction`` is the source segment's
    citygen-convention direction (deg); ``aspect_ratio`` and ``diagonal`` define a
    rectangle inscribed in a circle of radius ``diagonal`` (its corners lie on
    that circle, so ``diagonal`` is also the circumscribed radius used by the
    circle-approximate overlap test).  ``height`` is synthesised for the 3D
    authoring (citygen buildings are 2D).
    """
    center: tuple
    direction: float
    aspect_ratio: float
    diagonal: float
    height: float = 0.0

    def _half_extents(self):
        """(half-length along ``direction``, half-length perpendicular), meters."""
        ad = math.atan(self.aspect_ratio)
        return self.diagonal * math.cos(ad), self.diagonal * math.sin(ad)

    def corners(self):
        """The four 2D corners (faithful ``generate_corners`` formula)."""
        ad = math.degrees(math.atan(self.aspect_ratio))
        cx, cy = self.center
        out = []
        for ang in (ad + self.direction, -ad + self.direction,
                    180.0 + ad + self.direction, 180.0 - ad + self.direction):
            r = math.radians(ang)
            out.append((cx + self.diagonal * math.sin(r),
                        cy + self.diagonal * math.cos(r)))
        return out

    def radius(self):
        """Circumscribed radius used by the circle-approximate overlap test."""
        return self.diagonal

    def placement(self):
        """Authoring tuple ``(x, y, direction_deg, width, depth, height)``.

        ``direction_deg`` is the standard ``atan2`` world angle of the box's
        principal axis (``90 - citygen_direction``); ``width`` spans that axis and
        ``depth`` the perpendicular.  Consumed by ``town.author_buildings``.
        """
        a, b = self._half_extents()
        return (self.center[0], self.center[1], 90.0 - self.direction,
                2.0 * a, 2.0 * b, self.height)


class _BuildingGrid:
    """Deterministic uniform spatial hash over placed building footprints.

    Analogous to ``_SegmentGrid``: each building is indexed into the cells its
    bounding box (centre +/- radius) overlaps; ``query_near`` returns candidates
    for the exact circle test.
    """

    def __init__(self, cell_size):
        self.cell_size = float(cell_size)
        self.cells = {}

    def _cell_range(self, minx, miny, maxx, maxy):
        cs = self.cell_size
        return (int(math.floor(minx / cs)), int(math.floor(maxx / cs)),
                int(math.floor(miny / cs)), int(math.floor(maxy / cs)))

    def insert(self, b):
        r = b.radius()
        cx, cy = b.center
        c0x, c1x, c0y, c1y = self._cell_range(cx - r, cy - r, cx + r, cy + r)
        for gx in range(c0x, c1x + 1):
            for gy in range(c0y, c1y + 1):
                self.cells.setdefault((gx, gy), []).append(b)

    def query_near(self, center, radius):
        c0x, c1x, c0y, c1y = self._cell_range(
            center[0] - radius, center[1] - radius,
            center[0] + radius, center[1] + radius)
        out = []
        seen = set()
        for gx in range(c0x, c1x + 1):
            for gy in range(c0y, c1y + 1):
                for b in self.cells.get((gx, gy), ()):
                    key = id(b)
                    if key not in seen:
                        seen.add(key)
                        out.append(b)
        return out


def _building_contacts(b, bgrid, seg_grid, params):
    """Circle-approximate contacts of ``b`` with placed buildings and roads.

    Returns the list of contact points (other building centres / closest points
    on nearby road segments) that ``b`` currently overlaps -- the analogue of the
    Godot ``collide_shape`` results driving the pushout.
    """
    contacts = []
    r = b.radius()
    road_half = params.road_collider_width * 0.5

    for other in bgrid.query_near(b.center, r + params.building_diagonal_max):
        if other is b:
            continue
        dx = b.center[0] - other.center[0]
        dy = b.center[1] - other.center[1]
        if (dx * dx + dy * dy) < (r + other.radius()) ** 2:
            contacts.append(other.center)

    for seg in seg_grid.query_near(b.center, r + road_half):
        cp = _closest_point_on_segment(b.center, seg.start, seg.end)
        dx = b.center[0] - cp[0]
        dy = b.center[1] - cp[1]
        if (dx * dx + dy * dy) < (r + road_half) ** 2:
            contacts.append(cp)

    return contacts


def generate_buildings(rng, segments, pop, params, seg_grid=None):
    """Place buildings around the road network (port of ``generate_buildings``).

    Every ``building_segment_period``-th segment gets
    ``building_count_per_segment`` attempts: each scatters a candidate within
    ``max_building_distance`` of the segment midpoint, then runs the
    rejection/pushout loop (``building_placement_attempts`` tries) that moves the
    candidate away from any road or already-placed building it overlaps.  A
    surviving candidate must also sample population above
    ``building_pop_threshold`` (0 over water), keeping buildings on populated land.

    Returns a list of ``Building`` (authoring via ``Building.placement()``).  A
    ``seg_grid`` may be supplied to reuse an existing index; otherwise one is
    built from ``segments``.
    """
    if seg_grid is None:
        seg_grid = _SegmentGrid(params.grid_cell_size)
        for s in segments:
            seg_grid.insert(s)

    cell = max(2.0 * params.building_diagonal_max, params.road_collider_width)
    bgrid = _BuildingGrid(cell)

    period = max(1, params.building_segment_period)
    attempts = max(1, params.building_placement_attempts)
    buildings = []

    for i in range(0, len(segments), period):
        seg = segments[i]
        mx = (seg.start[0] + seg.end[0]) * 0.5
        my = (seg.start[1] + seg.end[1]) * 0.5
        direction = seg.direction

        for _ in range(params.building_count_per_segment):
            ang = rng.random() * 360.0
            rad = rng.random() * params.max_building_distance
            cx = mx + rad * math.sin(math.radians(ang))
            cy = my + rad * math.cos(math.radians(ang))
            b = Building(
                center=(cx, cy),
                direction=direction,
                aspect_ratio=rng.uniform(params.building_aspect_min,
                                         params.building_aspect_max),
                diagonal=rng.uniform(params.building_diagonal_min,
                                     params.building_diagonal_max),
                height=rng.uniform(params.building_height_min,
                                   params.building_height_max))

            placed = False
            for it in range(attempts):
                contacts = _building_contacts(b, bgrid, seg_grid, params)
                if not contacts:
                    placed = True
                    break
                if it == attempts - 1:
                    break
                # Push away from every contact (snapshot centre for all terms).
                ox, oy = b.center
                px, py = ox, oy
                for (contx, conty) in contacts:
                    px += ox - contx
                    py += oy - conty
                b.center = (px, py)

            if not placed:
                continue
            if pop.sample(b.center[0], b.center[1]) <= params.building_pop_threshold:
                continue
            bgrid.insert(b)
            buildings.append(b)

    return buildings


# --------------------------------------------------------------------------- #
# Exclusion mask                                                              #
# --------------------------------------------------------------------------- #

def build_exclusion(nodes, edges, buildings=(), road_clearance=8.0,
                    building_clearance=3.0, grid_cell_size=None):
    """Build a ``reject(x, y) -> bool`` predicate for road/building footprints.

    ``nodes``/``edges`` are a road graph (as from ``build_graph``); ``buildings``
    is an iterable of ``(x, y, radius)`` footprint circles.  The returned
    predicate is True when ``(x, y)`` lies within ``road_clearance`` of any road
    edge or within ``building_clearance`` of any building footprint -- suitable
    as the ``reject`` argument to ``IslandField.scatter_land`` so vegetation is
    pruned off roads and buildings.  Pure and deterministic; uses the same
    spatial hashes as the generator, so queries stay cheap on large networks.
    """
    cell = grid_cell_size
    if cell is None:
        cell = max(64.0, road_clearance * 4.0)
    seg_grid = _SegmentGrid(cell)
    for (i, j, _hw) in edges:
        seg = _Segment(nodes[i], nodes[j], 0, _SegmentMetadata())
        seg_grid.insert(seg)

    bgrid = _BuildingGrid(cell)
    max_b_radius = 0.0
    for (bx, by, br) in buildings:
        # Reuse Building purely as a radius-carrying footprint (radius() == diagonal).
        bgrid.insert(Building(center=(bx, by), direction=0.0,
                              aspect_ratio=1.0, diagonal=br))
        if br > max_b_radius:
            max_b_radius = br

    road_sq = road_clearance * road_clearance

    def reject(x, y):
        for seg in seg_grid.query_near((x, y), road_clearance):
            cp = _closest_point_on_segment((x, y), seg.start, seg.end)
            dx = x - cp[0]
            dy = y - cp[1]
            if dx * dx + dy * dy < road_sq:
                return True
        if max_b_radius > 0.0:
            for b in bgrid.query_near((x, y), building_clearance + max_b_radius):
                dx = x - b.center[0]
                dy = y - b.center[1]
                lim = b.radius() + building_clearance
                if dx * dx + dy * dy < lim * lim:
                    return True
        return False

    return reject
