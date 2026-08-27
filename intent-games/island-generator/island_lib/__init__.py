"""Generation helpers for the tropical-island bbox benchmark scene.

The interfaces are structured to be small and importable so the pieces 
can be unit-tested in isolation, and reused elsewhere

  geo     -- raw geometry files (geo/<component>.usda), including intra-geo
             instancing of repeated elements (hut walls, dock planks, leaves).
  assets  -- component-model authoring: /Assets/* referencing the geo files,
             plus the VillageBlock assembly, feeding the bbox-cache
             prototype-dependency DAG.
  scatter -- seeded, deterministic island/bay heightfield plus density-driven
             placement of scattered assets and point-instancer points.
  layers  -- sublayer/master assembly, purpose assignment, extentsHint, and
             time-sampled animation.
  city    -- a second, standalone generator: a nested point-instancing "city"
             (modules -> buildings -> districts) that stresses the imaging
             stack's PI-prototype propagation / merge, a different cost center
             from the bbox cache the island targets.
  population -- raster-backed population heat map (populationmap.png), the town
             generator's placement signal.
  erosion -- deterministic grid hydraulic erosion (stream-power incision +
             thermal); optionally carves valleys into the IslandField heightmap
             so rivers/roads/vegetation follow the new relief.
  hydrology -- flow-accumulation river network + wetness index (wetnessmap.png)
             from the heightfield; feeds biome moisture and authors a directed
             river curve network (/Island/Hydrology).
  biome   -- raster-backed moisture + biome guide map (moisturemap.png /
             biomemap.png) from terrain, a prevailing wind, and hydrology
             wetness; drives biome-appropriate vegetation density/size/species.
  roadnet -- pure-Python citygen road-network core: grows a segment
             network + intersection graph from a population source; unit-testable
             under plain python3.  Consumed by town.
  town    -- a third, standalone generator: a road network + buildings authored
             as an /Island/Town department, draped on the island field and gated
             on the population heat map (reusing island_lib.population).  Roads
             are authored as BasisCurves + an intersection graph via roadnet.

Everything is fully deterministic for a given seed so that every generated
island is reproducible and usda files are byte-identical between runs.
"""

from . import geo
from . import assets
from . import scatter
from . import erosion
from . import population
from . import hydrology
from . import biome
from . import layers
from . import city
from . import roadnet
from . import town

__all__ = ["geo", "assets", "scatter", "erosion", "population", "hydrology",
           "biome", "layers", "city", "roadnet", "town"]
