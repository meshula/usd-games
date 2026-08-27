
"""Moisture + biome guide map derived from terrain and a prevailing wind.

Where ``scatter.IslandField`` describes *terrain* and ``population`` describes
*where people live*, this module describes *what grows where*.  It adds the third
guide map in the same raster-backed family (``heightmap.png`` / ``populationmap.png``
-> ``moisturemap.png`` / ``biomemap.png``): a normalized moisture field and a
discrete biome classification the vegetation scatter reads to avoid odd
juxtapositions (lush palms on a dry ridge, etc.).

**Moisture model** (``[0, 1]``), the standard lightweight climate stack for this
scale -- no explicit river hydrology yet:

  * **Elevation dryness** -- drier with height (and true alpine is barren).
  * **Orographic wind** -- the windward side (toward ``wind_dir_deg``) is wetter;
    the leeward opposite is a rain shadow.  ``wind_strength`` sets the asymmetry.
  * **Noise** -- a decorrelated fractal breaks up the bands.

``moisture(x, y) = clamp01( (1 - elev_dryness*e) * (1 + wind_strength*windwardness)
* noise )`` where ``e`` is normalized elevation and ``windwardness`` in ``[-1, 1]``
is the projection of the position onto the windward direction.

**Biome** -- a Whittaker-style lookup on ``(elevation, moisture)``: water, beach,
wet forest, lowland, dry scrub, montane, dry slope, alpine.  Each biome carries a
vegetation profile (per-role placement weights, a size multiplier, and a display
colour) consumed by ``layers.author_vegetation``.

Like the other guide maps, the analytic moisture is rasterized once into a
fixed-range 16-bit grid at construction and ``moisture(x, y)`` reads it with
bilinear interpolation, so the grid, ``moisturemap.png``, and ``moisture()`` all
agree.  ``biome_at``/``role_weight``/``size_scale`` derive from the terrain height
and that moisture, so they need no separate raster.

Artifact export requires Pillow (PIL); it is imported lazily so importing this 
module stays otherwise dependency-free.  Units are meters; the map shares
the island coordinate frame.  Deterministic for a given field/seed and wind.
"""

import math
from array import array

from .scatter import _smoothstep, _value_noise


# --------------------------------------------------------------------------- #
# Biome table                                                                 #
# --------------------------------------------------------------------------- #
# Each biome lists per-role vegetation weights (multipliers in [0, 1] applied to
# the terrain's base vegetation density for the Palm / Rock / Foliage scatter
# groups), a (min, max) size multiplier, and an RGB colour for biomemap.png.
# The role keys match the scatter groups in layers.author_vegetation:
#   'palms'   -> palms + broadleaf trees   'rocks' -> rocks   'foliage' -> shrubs

(WATER, BEACH, WET_FOREST, LOWLAND, DRY_SCRUB, MONTANE, DRY_SLOPE, ALPINE,
 RIPARIAN) = range(9)

BIOMES = [
    # id           name          palms rocks folg  size(min,max)   colour
    {"name": "water",      "roles": {"palms": 0.0, "rocks": 0.0, "foliage": 0.0},
     "size": (1.0, 1.0), "color": (28, 52, 104)},
    {"name": "beach",      "roles": {"palms": 0.7, "rocks": 0.1, "foliage": 0.2},
     "size": (0.9, 1.1), "color": (214, 198, 138)},
    {"name": "wet_forest", "roles": {"palms": 1.0, "rocks": 0.05, "foliage": 1.0},
     "size": (1.15, 1.45), "color": (26, 122, 46)},
    {"name": "lowland",    "roles": {"palms": 0.6, "rocks": 0.15, "foliage": 0.6},
     "size": (0.95, 1.15), "color": (74, 156, 68)},
    {"name": "dry_scrub",  "roles": {"palms": 0.15, "rocks": 0.7, "foliage": 0.3},
     "size": (0.6, 0.85), "color": (166, 158, 86)},
    {"name": "montane",    "roles": {"palms": 0.25, "rocks": 0.5, "foliage": 0.7},
     "size": (0.8, 1.0), "color": (58, 110, 74)},
    {"name": "dry_slope",  "roles": {"palms": 0.1, "rocks": 0.8, "foliage": 0.25},
     "size": (0.6, 0.8), "color": (132, 120, 78)},
    {"name": "alpine",     "roles": {"palms": 0.0, "rocks": 0.7, "foliage": 0.1},
     "size": (0.5, 0.7), "color": (196, 200, 205)},
    # Riparian corridor: lush banks along rivers (strong low/mid wetness).
    {"name": "riparian",   "roles": {"palms": 1.0, "rocks": 0.05, "foliage": 1.0},
     "size": (1.25, 1.6), "color": (18, 92, 88)},
]

BIOME_NAMES = [b["name"] for b in BIOMES]


class BiomeMap:
    """Raster-backed moisture field + biome classification over an ``IslandField``.

    ``wind_dir_deg`` is the direction the prevailing wind blows *from*, measured
    counter-clockwise from +x in the map plane; the windward (wetter) side of the
    island faces toward it and the leeward (drier) side is opposite.
    ``wind_strength`` in ``[0, 1]`` scales the wet/dry asymmetry.
    """

    MOIST_MIN = 0.0
    MOIST_MAX = 1.0

    def __init__(self, field, wind_dir_deg=315.0, wind_strength=0.6,
                 resolution=None, seed_offset=57291, hydro=None,
                 riparian_boost=0.8, riparian_wetness=0.7):
        self.field = field
        self.wind_dir_deg = float(wind_dir_deg)
        self.wind_strength = max(0.0, min(1.0, float(wind_strength)))
        self.seed_offset = int(seed_offset)
        # Optional hydrology (wetness index); when present it greens valleys and
        # riverbanks and enables the riparian biome.
        self.hydro = hydro
        self.riparian_boost = float(riparian_boost)
        self.riparian_wetness = float(riparian_wetness)
        # Windward unit vector (toward the wind source = wetter side).
        rad = math.radians(self.wind_dir_deg)
        self._wx = math.cos(rad)
        self._wy = math.sin(rad)

        # Shaping parameters (meters / normalized), tuned for the 2 km island.
        self.peak = field.peak_height
        self.elev_dryness = 0.6       # how strongly elevation dries the air out
        self.alpine_norm = 0.72       # normalized elevation where alpine begins
        self.shore_band = 6.0         # <= this height (and dry land) reads beach
        self.riparian_elev = 0.5      # riparian only below this normalized elev

        if resolution is None:
            resolution = field.heightmap_resolution
        self.resolution = int(resolution)
        self._build_raster()

    # ------------------------------------------------------------------ #
    # Analytic source                                                    #
    # ------------------------------------------------------------------ #

    def _fractal(self, x, y):
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

    def _windwardness(self, x, y):
        """Projection of (x, y) onto the windward direction, clamped [-1, 1].

        +1 on the windward (wet) shore, -1 on the leeward (dry) shore, ~0 through
        the middle -- normalized by the island radius so it is a smooth gradient
        rather than a hard hemisphere split.
        """
        w = (x * self._wx + y * self._wy) / self.field.radius
        if w < -1.0:
            return -1.0
        if w > 1.0:
            return 1.0
        return w

    def _analytic_moisture(self, x, y):
        """Analytic moisture in ``[0, 1]`` at (x, y); source for the raster."""
        field = self.field
        h = field.height(x, y)
        if h <= field.SEA_LEVEL:
            return 0.0
        e = h / self.peak
        if e < 0.0:
            e = 0.0
        elif e > 1.0:
            e = 1.0
        dry = 1.0 - self.elev_dryness * e                 # drier with height
        wind = 1.0 + self.wind_strength * self._windwardness(x, y)
        noise = 0.85 + 0.3 * self._fractal(x - 3000.0, y + 3000.0)
        m = dry * wind * noise
        # Rivers/valleys add moisture (riparian greening) when hydrology is on.
        if self.hydro is not None:
            m *= 1.0 + self.riparian_boost * self.hydro.wetness(x, y)
        if m < 0.0:
            return 0.0
        if m > 1.0:
            return 1.0
        return m

    # ------------------------------------------------------------------ #
    # Raster build + quantization                                        #
    # ------------------------------------------------------------------ #

    def _build_raster(self):
        n = self.resolution
        if n < 2:
            raise ValueError("resolution must be >= 2")
        radius = self.field.radius
        span = self.MOIST_MAX - self.MOIST_MIN
        inv_span = 65535.0 / span
        m16 = array('H', bytes(2 * n * n))
        step = (2.0 * radius) / (n - 1)
        analytic = self._analytic_moisture
        lo = self.MOIST_MIN
        for row in range(n):
            y = radius - row * step
            base = row * n
            for col in range(n):
                x = -radius + col * step
                q = int(round((analytic(x, y) - lo) * inv_span))
                if q < 0:
                    q = 0
                elif q > 65535:
                    q = 65535
                m16[base + col] = q
        self._m16 = m16

    def _dequant(self, q):
        return self.MOIST_MIN + (q / 65535.0) * (self.MOIST_MAX - self.MOIST_MIN)

    def _grid_value(self, col, row):
        n = self.resolution
        if col < 0:
            col = 0
        elif col >= n:
            col = n - 1
        if row < 0:
            row = 0
        elif row >= n:
            row = n - 1
        return self._dequant(self._m16[row * n + col])

    def moisture(self, x, y):
        """Moisture in ``[0, 1]`` at world-space (x, y) (bilinear raster read)."""
        n = self.resolution
        step = (2.0 * self.field.radius) / (n - 1)
        gc = (x + self.field.radius) / step
        gr = (self.field.radius - y) / step
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
    # Biome classification + vegetation profile                          #
    # ------------------------------------------------------------------ #

    def biome_at(self, x, y):
        """Discrete biome id (see the module constants) at (x, y)."""
        field = self.field
        h = field.height(x, y)
        if h <= field.SEA_LEVEL:
            return WATER
        if h <= self.shore_band:
            return BEACH
        e = h / self.peak
        m = self.moisture(x, y)
        # Riparian corridor: strong wetness on low/mid ground near rivers.
        if (self.hydro is not None and e < self.riparian_elev and
                self.hydro.wetness(x, y) >= self.riparian_wetness):
            return RIPARIAN
        if e >= self.alpine_norm:
            return ALPINE
        if e < 0.35:                       # lowland band
            if m > 0.55:
                return WET_FOREST
            if m > 0.3:
                return LOWLAND
            return DRY_SCRUB
        # mid-elevation band
        if m > 0.45:
            return MONTANE
        return DRY_SLOPE

    def role_weight(self, role, x, y):
        """Placement-weight multiplier in ``[0, 1]`` for a scatter role at (x, y).

        Multiplies the terrain's base vegetation density so each species group
        concentrates in its appropriate biomes (palms in wet lowland/beach, rocks
        on dry/alpine slopes, etc.), shifting the species mix by locale.
        """
        return BIOMES[self.biome_at(x, y)]["roles"].get(role, 0.0)

    def size_scale(self, x, y):
        """Per-instance size multiplier at (x, y): larger where it is wetter."""
        b = BIOMES[self.biome_at(x, y)]
        lo, hi = b["size"]
        m = self.moisture(x, y)
        return lo + (hi - lo) * m

    # ------------------------------------------------------------------ #
    # Artifact export                                                    #
    # ------------------------------------------------------------------ #

    def save_moisturemap(self, path):
        """Write the moisture raster to ``path`` as a 16-bit PNG."""
        from PIL import Image
        n = self.resolution
        im = Image.frombytes("I;16", (n, n), self._m16.tobytes())
        im.save(path, format="PNG")
        return path

    def save_biomemap(self, path):
        """Write a colour-coded biome classification to ``path`` as an RGB PNG."""
        from PIL import Image
        n = self.resolution
        radius = self.field.radius
        step = (2.0 * radius) / (n - 1)
        colors = [b["color"] for b in BIOMES]
        buf = bytearray(3 * n * n)
        for row in range(n):
            y = radius - row * step
            base = 3 * row * n
            for col in range(n):
                x = -radius + col * step
                r, g, b = colors[self.biome_at(x, y)]
                o = base + 3 * col
                buf[o] = r
                buf[o + 1] = g
                buf[o + 2] = b
        im = Image.frombytes("RGB", (n, n), bytes(buf))
        im.save(path, format="PNG")
        return path
