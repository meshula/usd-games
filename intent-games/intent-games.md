# The Games Intent

## Relation to Other Intents

- link to vfx intent
    - there's a long form document, somewhere
- link to virtual production intent
    - maybe Paolo will publish
    - key differentiator from vfx intent:
        - singular purpose - collapsed for baking
        - multiple purpose: default, proxy, render, guide

## Requirements

Purposes are not a single choice

### Runtime Requirements

- functional (non-imageable) representations
    - navigation graph
    - trigger volumes
    - physics and collision elements
    - loading portals
    - gameplay data
- purposes:
    - card
    - potato
    - Switch
    - PS5
- LOD matrixes purpose
    -  card, potato, Switch, PS5 can be switched dynamically
-  interior and exterior configurations differ
    -  open world
    -  inside a building
-  exterior configurations have traversal considerations
    -  on the ground requires high LOD availability in local region
        -  adjacent regions should have LODs incrementally more LODs available as probability of switching region increases
    -  aerial traversal requires potentially all regions loaded, not just adjacent
        - at some altitute the whole map may be visible at card/potato or may switch to a different high-altitude or planetary portal
    
### Departmental and Authoring Structure

TBD
