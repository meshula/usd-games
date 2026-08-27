
"""Seeded island/bay heightfield and density-driven placement.

All functions here are pure and deterministic: the heightfield derives from an
analytic function of (x, y) plus hash-based value noise (no per-evaluation RNG),
and placement draws from an explicitly seeded ``random.Random`` with a fixed
iteration order.  Same seed in -> same points out.

The terrain is **raster-backed**: the analytic displacement is rasterized once
into a fixed-range 16-bit heightfield grid at construction, and ``height(x, y)``
samples that grid with smooth interpolation.  This makes the terrain a single,
inspectable source of truth that can be exported as ``heightmap.png``.  The
analytic function remains available as ``_analytic_height`` for reference/tests.

Units are meters.  The island is centred on the world origin; the port bay is
carved on the +X side where the village concentrates.

Artifact export (``save_heightmap``) requires Pillow (PIL) under ``python3.11``;
it is imported lazily so importing this module stays dependency-free.
"""

import math
import random
from array import array


def _smoothstep(edge0, edge1, x):
    if edge0 == edge1:
        return 0.0 if x < edge0 else 1.0
    t = (x - edge0) / (edge1 - edge0)
    t = max(0.0, min(1.0, t))
    return t * t * (3.0 - 2.0 * t)


def _hash01(ix, iy, salt):
    """Deterministic hash of an integer lattice cell into [0, 1)."""
    h = (ix * 374761393 + iy * 668265263 + salt * 2147483647) & 0xFFFFFFFF
    h = (h ^ (h >> 13)) * 1274126177 & 0xFFFFFFFF
    h = h ^ (h >> 16)
    return (h & 0xFFFFFF) / float(0x1000000)


def _value_noise(x, y, salt):
    """Bilinearly-interpolated value noise on a unit lattice, range [0, 1)."""
    ix, iy = math.floor(x), math.floor(y)
    fx, fy = x - ix, y - iy
    ix, iy = int(ix), int(iy)
    v00 = _hash01(ix, iy, salt)
    v10 = _hash01(ix + 1, iy, salt)
    v01 = _hash01(ix, iy + 1, salt)
    v11 = _hash01(ix + 1, iy + 1, salt)
    ux = fx * fx * (3.0 - 2.0 * fx)
    uy = fy * fy * (3.0 - 2.0 * fy)
    a = v00 * (1.0 - ux) + v10 * ux
    b = v01 * (1.0 - ux) + v11 * ux
    return a * (1.0 - uy) + b * uy


class IslandField:
    """Raster-backed tropical-island terrain with a carved port bay.

    The profile is a radial dome with fractal noise; the bay is a circular
    depression on the +X shore pushed below sea level so boats/docks have water.
    That analytic displacement (``_analytic_height``) is rasterized once into a
    fixed-range 16-bit heightfield at construction; ``height(x, y)`` samples the
    dequantized grid with bilinear (default) or bicubic interpolation, so the
    grid, ``heightmap.png``, and ``height()`` all agree exactly.
    """

    SEA_LEVEL = 0.0

    # Fixed physical range for the 16-bit quantization.  Sea level (0.0) always
    # maps to the same pixel value, so heightmaps are comparable across seeds.
    # The span covers the bay floor (-bay_floor) down to the peak plus relief
    # headroom; heights outside [HEIGHT_MIN, HEIGHT_MAX] are clamped.
    HEIGHT_MIN = -40.0
    HEIGHT_MAX = 240.0

    def __init__(self, size_km=2.0, seed=1751,
                 heightmap_resolution=1024, interp="bilinear",
                 erosion_iterations=0, erosion_strength=1.0,
                 erosion_resolution=384):
        self.size = size_km * 1000.0
        self.radius = self.size * 0.5
        self.seed = int(seed)
        # Island dome parameters, all in meters.
        self.peak_height = 180.0
        # Bay centre out on the +X shore, straddling the coastline so the bay
        # opens to the ocean rather than forming an inland lake.
        self.bay_center = (self.radius * 0.85, 0.0)
        self.bay_radius = self.radius * 0.30
        # Bay floor sits this many metres *below* sea level; the terrain is
        # lerped toward it so the bay is represented by water regardless of the
        # underlying dome height.
        self.bay_floor = 15.0
        # Noise frequency: a few hundred metres per feature.
        self._noise_scale = 6.0 / self.size

        # Rasterize the analytic displacement into a fixed-range 16-bit grid.
        if interp not in ("bilinear", "bicubic"):
            raise ValueError("interp must be 'bilinear' or 'bicubic'")
        self.interp = interp
        self.heightmap_resolution = int(heightmap_resolution)
        self.erosion_iterations = int(erosion_iterations)
        self.erosion_strength = float(erosion_strength)
        self.erosion_resolution = int(erosion_resolution)
        self._build_raster()
        if self.erosion_iterations > 0:
            self._apply_erosion()

    # ------------------------------------------------------------------ #
    # Raster build + quantization                                        #
    # ------------------------------------------------------------------ #

    def _build_raster(self):
        """Fill ``self._h16`` from ``_analytic_height`` over the island square.

        The grid spans ``[-radius, radius]`` on each axis.  Orientation: row 0
        is north (world ``y = +radius``) at the top; column 0 is
        ``x = -radius``.  This matches ``terrain_grid``'s x->USD-x, y->USD-z
        convention (image top = +z/north).  The dequantized grid is the
        canonical sampling source for ``height()`` so it agrees with the PNG.
        """
        n = self.heightmap_resolution
        if n < 2:
            raise ValueError("heightmap_resolution must be >= 2")
        span = self.HEIGHT_MAX - self.HEIGHT_MIN
        inv_span = 65535.0 / span
        h16 = array('H', bytes(2 * n * n))
        # Map grid index -> world coord: i in [0, n-1] -> [-radius, radius].
        step = (2.0 * self.radius) / (n - 1)
        analytic = self._analytic_height
        lo = self.HEIGHT_MIN
        for row in range(n):
            # Row 0 = north = +radius; increasing row moves south (-y).
            y = self.radius - row * step
            base = row * n
            for col in range(n):
                x = -self.radius + col * step
                h = analytic(x, y)
                q = int(round((h - lo) * inv_span))
                if q < 0:
                    q = 0
                elif q > 65535:
                    q = 65535
                h16[base + col] = q
        self._h16 = h16

    def _apply_erosion(self):
        """Carve valleys into the heightmap raster with hydraulic erosion.

        Erosion runs on a coarse working grid (``erosion_resolution``); the
        resulting per-cell height *delta* is bilinearly upsampled and added to the
        full-resolution heightmap.  Adding the delta (rather than replacing the
        raster) preserves the fine analytic noise while introducing the broad
        carved valleys, so ``height()``/``heightmap.png`` and every downstream
        consumer (terrain mesh, population, hydrology rivers, biome, town draping,
        vegetation) reflect the eroded relief.  Ocean cells are held fixed.
        """
        from . import erosion
        n = self.erosion_resolution
        radius = self.radius
        cstep = (2.0 * radius) / (n - 1)
        # Sample the (just-built) raster into the coarse working grid.
        elev0 = [0.0] * (n * n)
        for r in range(n):
            y = radius - r * cstep
            base = r * n
            for c in range(n):
                elev0[base + c] = self.height(-radius + c * cstep, y)
        eroded = erosion.erode_grid(
            elev0, n, cstep, self.SEA_LEVEL, self.erosion_iterations,
            strength=self.erosion_strength)
        delta = [eroded[i] - elev0[i] for i in range(n * n)]

        def sample_delta(x, y):
            gc = (x + radius) / cstep
            gr = (radius - y) / cstep
            c0 = int(math.floor(gc))
            r0 = int(math.floor(gr))
            fc = gc - c0
            fr = gr - r0

            def g(col, row):
                if col < 0:
                    col = 0
                elif col >= n:
                    col = n - 1
                if row < 0:
                    row = 0
                elif row >= n:
                    row = n - 1
                return delta[row * n + col]
            a = g(c0, r0) * (1.0 - fc) + g(c0 + 1, r0) * fc
            b = g(c0, r0 + 1) * (1.0 - fc) + g(c0 + 1, r0 + 1) * fc
            return a * (1.0 - fr) + b * fr

        # Add the upsampled delta back into the full-resolution 16-bit raster.
        hm = self.heightmap_resolution
        hstep = (2.0 * radius) / (hm - 1)
        lo = self.HEIGHT_MIN
        inv_span = 65535.0 / (self.HEIGHT_MAX - self.HEIGHT_MIN)
        h16 = self._h16
        for row in range(hm):
            y = radius - row * hstep
            base = row * hm
            for col in range(hm):
                x = -radius + col * hstep
                h = self._dequant(h16[base + col]) + sample_delta(x, y)
                q = int(round((h - lo) * inv_span))
                if q < 0:
                    q = 0
                elif q > 65535:
                    q = 65535
                h16[base + col] = q

    def _dequant(self, q):
        """16-bit code -> meters (inverse of the quantization in _build_raster)."""
        return self.HEIGHT_MIN + (q / 65535.0) * (self.HEIGHT_MAX - self.HEIGHT_MIN)

    def _fractal(self, x, y):
        n = 0.0
        amp = 1.0
        freq = 1.0
        total = 0.0
        for octave in range(4):
            n += amp * _value_noise(x * self._noise_scale * freq,
                                    y * self._noise_scale * freq,
                                    self.seed + octave)
            total += amp
            amp *= 0.5
            freq *= 2.0
        return n / total  # [0, 1)

    def _analytic_height(self, x, y):
        """Analytic world-space terrain height at (x, y), meters.

        This is the source used to fill the raster; it is retained for
        reference/tests.  ``height()`` samples the rasterized grid instead.
        """
        r = math.hypot(x, y)
        # Radial dome: full height at centre, tapering to shore.
        dome = self.peak_height * _smoothstep(self.radius, self.radius * 0.15, r)
        # Break up the dome with fractal noise, more inland than at the shore.
        relief = (self._fractal(x, y) - 0.5) * 60.0
        relief *= _smoothstep(self.radius, self.radius * 0.4, r)
        h = dome + relief
        # Carve the port bay: lerp the terrain toward the (sub-sea) bay floor,
        # full strength at the bay centre, fading to natural terrain at the rim.
        bx, by = self.bay_center
        bd = math.hypot(x - bx, y - by)
        t = _smoothstep(self.bay_radius, 0.0, bd)
        return h * (1.0 - t) + (-self.bay_floor) * t

    def _grid_value(self, col, row):
        """Dequantized height at integer grid indices (clamped to bounds)."""
        n = self.heightmap_resolution
        if col < 0:
            col = 0
        elif col >= n:
            col = n - 1
        if row < 0:
            row = 0
        elif row >= n:
            row = n - 1
        return self._dequant(self._h16[row * n + col])

    def height(self, x, y):
        """World-space terrain height at (x, y), meters.

        Samples the rasterized heightfield with the configured interpolation.
        Queries at or outside the island square clamp to the boundary texel, so
        the shore/ocean classification stays sane.
        """
        n = self.heightmap_resolution
        step = (2.0 * self.radius) / (n - 1)
        # World -> fractional grid coords.  Column runs +x; row runs -y (row 0
        # is north = +radius), matching _build_raster.
        gc = (x + self.radius) / step
        gr = (self.radius - y) / step

        if self.interp == "bicubic":
            return self._sample_bicubic(gc, gr)
        return self._sample_bilinear(gc, gr)

    def _sample_bilinear(self, gc, gr):
        c0 = math.floor(gc)
        r0 = math.floor(gr)
        fc = gc - c0
        fr = gr - r0
        c0 = int(c0)
        r0 = int(r0)
        v00 = self._grid_value(c0, r0)
        v10 = self._grid_value(c0 + 1, r0)
        v01 = self._grid_value(c0, r0 + 1)
        v11 = self._grid_value(c0 + 1, r0 + 1)
        a = v00 * (1.0 - fc) + v10 * fc
        b = v01 * (1.0 - fc) + v11 * fc
        return a * (1.0 - fr) + b * fr

    @staticmethod
    def _catmull_rom(p0, p1, p2, p3, t):
        t2 = t * t
        t3 = t2 * t
        return 0.5 * (
            (2.0 * p1)
            + (-p0 + p2) * t
            + (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t2
            + (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * t3)

    def _sample_bicubic(self, gc, gr):
        c1 = int(math.floor(gc))
        r1 = int(math.floor(gr))
        fc = gc - c1
        fr = gr - r1
        cols = (c1 - 1, c1, c1 + 1, c1 + 2)
        rowvals = []
        for r in (r1 - 1, r1, r1 + 1, r1 + 2):
            p = [self._grid_value(c, r) for c in cols]
            rowvals.append(self._catmull_rom(p[0], p[1], p[2], p[3], fc))
        return self._catmull_rom(rowvals[0], rowvals[1], rowvals[2], rowvals[3], fr)

    def save_heightmap(self, path):
        """Write the rasterized heightfield to ``path`` as a 16-bit PNG.

        Uses Pillow's ``I;16`` mode straight from the packed grid bytes (no
        per-pixel loop).  Pillow is imported lazily so importing this module
        stays dependency-free for callers that never export an artifact.
        """
        from PIL import Image
        n = self.heightmap_resolution
        im = Image.frombytes("I;16", (n, n), self._h16.tobytes())
        im.save(path, format="PNG")
        return path

    def is_water(self, x, y):
        return self.height(x, y) <= self.SEA_LEVEL

    def in_bay(self, x, y):
        bx, by = self.bay_center
        return math.hypot(x - bx, y - by) <= self.bay_radius

    def vegetation_density(self, x, y):
        """Placement weight in [0, 1]: dense inland, thinning to the water."""
        h = self.height(x, y)
        if h <= self.SEA_LEVEL:
            return 0.0
        # Ramp up from the beach (0 m) to a lush band, then thin on the peak.
        beach = _smoothstep(0.0, 18.0, h)
        alpine = _smoothstep(160.0, 100.0, h)
        clumps = 0.4 + 0.6 * self._fractal(x + 5000.0, y - 5000.0)
        # Keep the bay itself clear of trees.
        bx, by = self.bay_center
        bd = math.hypot(x - bx, y - by)
        bay_clear = _smoothstep(self.bay_radius * 0.6, self.bay_radius, bd)
        return beach * alpine * clumps * bay_clear

    # ------------------------------------------------------------------ #
    # Placement helpers                                                  #
    # ------------------------------------------------------------------ #

    def scatter_land(self, rng, count, density_power=1.0, margin=0.9,
                     weight=None, reject=None):
        """Rejection-sample ``count`` (x, y, h) points weighted by veg density.

        ``rng`` is a ``random.Random``; iteration is deterministic.  Returns a
        list of (x, y, height) tuples on dry land.

        ``weight`` overrides the placement-density source: a callable
        ``(x, y) -> [0, 1]`` (default ``self.vegetation_density``), letting a
        caller bias placement by a biome/moisture field.  ``reject`` is an
        optional exclusion predicate ``(x, y) -> bool``; points for which it
        returns True are discarded (e.g. footprints of roads/buildings).
        """
        if weight is None:
            weight = self.vegetation_density
        pts = []
        reach = self.radius * margin
        attempts = 0
        max_attempts = count * 60
        while len(pts) < count and attempts < max_attempts:
            attempts += 1
            x = rng.uniform(-reach, reach)
            y = rng.uniform(-reach, reach)
            d = weight(x, y)
            if d <= 0.0:
                continue
            if reject is not None and reject(x, y):
                continue
            if rng.random() <= d ** density_power:
                pts.append((x, y, self.height(x, y)))
        return pts

    def scatter_bay_water(self, rng, count):
        """Sample ``count`` (x, y, h) points on the open water of the bay."""
        pts = []
        bx, by = self.bay_center
        attempts = 0
        max_attempts = count * 60
        while len(pts) < count and attempts < max_attempts:
            attempts += 1
            ang = rng.uniform(0.0, 2.0 * math.pi)
            rad = self.bay_radius * math.sqrt(rng.random())
            x = bx + rad * math.cos(ang)
            y = by + rad * math.sin(ang)
            if self.is_water(x, y):
                pts.append((x, y, self.SEA_LEVEL))
        return pts

    def scatter_beach(self, rng, count, band=(0.0, 8.0), reject=None):
        """Sample ``count`` points in the shoreline height band (shells etc.).

        ``reject`` is an optional exclusion predicate ``(x, y) -> bool`` (points
        for which it returns True are discarded), e.g. road/building footprints.
        """
        pts = []
        reach = self.radius * 0.98
        lo, hi = band
        attempts = 0
        max_attempts = count * 80
        while len(pts) < count and attempts < max_attempts:
            attempts += 1
            x = rng.uniform(-reach, reach)
            y = rng.uniform(-reach, reach)
            h = self.height(x, y)
            if lo < h <= hi:
                if reject is not None and reject(x, y):
                    continue
                pts.append((x, y, h))
        return pts


def terrain_grid(field, resolution):
    """Build a heightfield mesh grid over the island bounds.

    Returns (points, faceVertexCounts, faceVertexIndices) for a
    ``resolution`` x ``resolution`` quad mesh spanning the island square.
    """
    n = resolution
    half = field.radius
    step = (2.0 * half) / (n - 1)
    points = []
    for j in range(n):
        y = -half + j * step
        for i in range(n):
            x = -half + i * step
            points.append((x, field.height(x, y), y))
    counts = []
    indices = []
    for j in range(n - 1):
        for i in range(n - 1):
            a = j * n + i
            b = a + 1
            c = a + n + 1
            d = a + n
            counts.append(4)
            # Wind CCW as seen from above (+Y) so the surface faces up under
            # USD's default rightHanded orientation: (i,j)->(i,j+1)->(i+1,j+1)
            # ->(i+1,j).  The naive (a,b,c,d) order faces -Y and is culled from
            # the top.
            indices.extend((a, d, c, b))
    return points, counts, indices
