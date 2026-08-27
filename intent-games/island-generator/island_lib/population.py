
"""Population heat map derived from the island heightfield.

Where ``scatter.IslandField`` describes *terrain*, this module describes *where
people live*: a normalized density field in ``[0, 1]`` used by the (future) town
generator to drive road and building placement.  The signal is built by
multiplying a decorrelated fractal noise field with a smooth-stepped filter of
the terrain height -- no population under the ocean, denser around the harbour,
and fading out toward the highest elevations.

Like the terrain, the island heat map is **raster-backed**: the analytic
population formula is rasterized once into a fixed-range 16-bit grid at
construction, and ``sample(x, y)`` reads that grid with bilinear interpolation.
This makes ``populationmap.png`` the single, inspectable source of truth an
artist could hand-paint, exactly mirroring ``heightmap.png``.

``PopulationSource`` is the pluggable interface the town generator consumes;
``IslandPopulation`` is the raster-backed island heat map, and
``NoisePopulation`` is a slim field-free alternative.

Units are meters.  The heat map shares the island's coordinate frame.
"""

import math
from array import array

from .scatter import _smoothstep, _value_noise


class PopulationSource:
    """Pluggable population-density interface consumed by the town generator.

    Implementations return a normalized density in ``[0, 1]`` at a world-space
    ``(x, y)``; the town generator uses that weight to place roads and
    buildings.
    """

    def sample(self, x, y):
        """Population density in ``[0, 1]`` at world-space ``(x, y)``."""
        raise NotImplementedError


class IslandPopulation(PopulationSource):
    """Raster-backed island population heat map derived from ``IslandField``.

    The density is a decorrelated fractal noise field gated by a smooth-stepped
    filter of the terrain height (empty under the ocean, ramping up off the
    shoreline, fading out toward the peaks) and boosted around the harbour bay.
    That analytic signal (``_analytic_population``) is rasterized once into a
    fixed-range 16-bit grid at construction; ``sample(x, y)`` reads the
    dequantized grid with bilinear interpolation, so the grid,
    ``populationmap.png``, and ``sample()`` all agree exactly.
    """

    # Population is already normalized, so the 16-bit quantization spans the
    # full [0, 1] density range.
    POP_MIN = 0.0
    POP_MAX = 1.0

    def __init__(self, field, resolution=None, seed_offset=91021,
                 interp="bilinear"):
        if interp != "bilinear":
            raise ValueError("interp must be 'bilinear'")
        self.field = field
        self.interp = interp
        self.seed_offset = int(seed_offset)
        # Align 1:1 with the heightmap raster by default so edits to the terrain
        # and the heat map stay registered.
        if resolution is None:
            resolution = field.heightmap_resolution
        self.resolution = int(resolution)

        # Shaping parameters (meters), tuned for a 2 km island with a 180 m
        # peak.  See _analytic_population for how each is used.
        self.shore_band = 20.0        # ramp up over the first 20 m off the shore
        self.alpine_low = 80.0        # density gone by here on the way up...
        self.alpine_high = 160.0      # ...starting to fade from here
        self.harbour_radius = field.bay_radius * 2.0
        self.harbour_boost = 1.5
        self.noise_off = 12345.0      # decorrelate the heat-map noise from relief

        self._build_raster()

    # ------------------------------------------------------------------ #
    # Analytic source                                                    #
    # ------------------------------------------------------------------ #

    def _fractal(self, x, y):
        """4-octave value noise on the field's frequency, range [0, 1).

        Reuses ``field._noise_scale`` for the same feature size as the terrain
        but offsets the salt by ``seed_offset`` so the heat map is decorrelated
        from relief while staying deterministic for the field's seed.
        """
        field = self.field
        n = 0.0
        amp = 1.0
        freq = 1.0
        total = 0.0
        for octave in range(4):
            n += amp * _value_noise(x * field._noise_scale * freq,
                                    y * field._noise_scale * freq,
                                    field.seed + self.seed_offset + octave)
            total += amp
            amp *= 0.5
            freq *= 2.0
        return n / total  # [0, 1)

    def _analytic_population(self, x, y):
        """Analytic population density in ``[0, 1]`` at (x, y).

        This is the source used to fill the raster; ``sample()`` reads the
        rasterized grid instead.
        """
        field = self.field
        h = field.height(x, y)
        if h <= field.SEA_LEVEL:
            return 0.0  # NO population under the ocean
        # Smooth-stepped filter of the heightmap: ramp up off the shoreline,
        # fade out toward the peaks.
        coastal = _smoothstep(0.0, self.shore_band, h)
        alpine = _smoothstep(self.alpine_high, self.alpine_low, h)
        elev_filter = coastal * alpine
        # Decorrelated fractal value-noise breaks up the density.
        noise = self._fractal(x + self.noise_off, y - self.noise_off)
        # Higher density around the harbour bay.
        bx, by = field.bay_center
        bd = ((x - bx) ** 2 + (y - by) ** 2) ** 0.5
        harbour = 1.0 + self.harbour_boost * _smoothstep(
            self.harbour_radius, 0.0, bd)
        p = noise * elev_filter * harbour
        if p < 0.0:
            return 0.0
        if p > 1.0:
            return 1.0
        return p

    # ------------------------------------------------------------------ #
    # Raster build + quantization                                        #
    # ------------------------------------------------------------------ #

    def _build_raster(self):
        """Fill ``self._p16`` from ``_analytic_population`` over the island.

        Same grid convention as ``IslandField._build_raster``: ``n x n``, row 0
        is north (world ``y = +radius``), column 0 is ``x = -radius``,
        ``step = 2*radius/(n-1)``.  The dequantized grid is the canonical
        sampling source so ``sample()`` and ``populationmap.png`` agree.
        """
        n = self.resolution
        if n < 2:
            raise ValueError("resolution must be >= 2")
        radius = self.field.radius
        span = self.POP_MAX - self.POP_MIN
        inv_span = 65535.0 / span
        p16 = array('H', bytes(2 * n * n))
        step = (2.0 * radius) / (n - 1)
        analytic = self._analytic_population
        lo = self.POP_MIN
        for row in range(n):
            # Row 0 = north = +radius; increasing row moves south (-y).
            y = radius - row * step
            base = row * n
            for col in range(n):
                x = -radius + col * step
                p = analytic(x, y)
                q = int(round((p - lo) * inv_span))
                if q < 0:
                    q = 0
                elif q > 65535:
                    q = 65535
                p16[base + col] = q
        self._p16 = p16

    def _dequant(self, q):
        """16-bit code -> density (inverse of the quantization above)."""
        return self.POP_MIN + (q / 65535.0) * (self.POP_MAX - self.POP_MIN)

    def _grid_value(self, col, row):
        """Dequantized density at integer grid indices (clamped to bounds)."""
        n = self.resolution
        if col < 0:
            col = 0
        elif col >= n:
            col = n - 1
        if row < 0:
            row = 0
        elif row >= n:
            row = n - 1
        return self._dequant(self._p16[row * n + col])

    def sample(self, x, y):
        """Population density in ``[0, 1]`` at world-space ``(x, y)``.

        Reads the rasterized heat map with bilinear interpolation.  Queries at
        or outside the island square clamp to the boundary texel.
        """
        n = self.resolution
        step = (2.0 * self.field.radius) / (n - 1)
        # World -> fractional grid coords.  Column runs +x; row runs -y (row 0
        # is north = +radius), matching _build_raster.
        gc = (x + self.field.radius) / step
        gr = (self.field.radius - y) / step
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

    def save_populationmap(self, path):
        """Write the rasterized heat map to ``path`` as a 16-bit PNG.

        Uses Pillow's ``I;16`` mode straight from the packed grid bytes.  Pillow
        is imported lazily so importing this module stays dependency-free for
        callers that never export an artifact.
        """
        from PIL import Image
        n = self.resolution
        im = Image.frombytes("I;16", (n, n), self._p16.tobytes())
        im.save(path, format="PNG")
        return path


class NoisePopulation(PopulationSource):
    """Field-free population heat map: a fractal over value noise.

    A slim standalone source that realizes the pluggable abstraction without a
    terrain field or raster.  Deterministic for a given ``seed``; ``sample()``
    returns a density in ``[0, 1)``.
    """

    def __init__(self, seed=1751, scale=0.001, octaves=4):
        self.seed = int(seed)
        self.scale = float(scale)
        self.octaves = int(octaves)

    def sample(self, x, y):
        n = 0.0
        amp = 1.0
        freq = 1.0
        total = 0.0
        for octave in range(self.octaves):
            n += amp * _value_noise(x * self.scale * freq,
                                    y * self.scale * freq,
                                    self.seed + octave)
            total += amp
            amp *= 0.5
            freq *= 2.0
        return n / total  # [0, 1)
