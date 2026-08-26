# USD LOD Demo

An interactive demonstration of advanced Level of Detail (LOD) techniques using USD composition.

## Overview

This project demonstrates concepts from "Chapter 3.2: Advanced LOD Through Composition," showcasing how USD's composition system can be leveraged for comprehensive LOD management across various game systems, not just geometry.

Instead of relying on Hydra rendering, this demo uses Python with Raylib to visualize USD scenes through simplified representations, making the LOD concepts clear and accessible.

## Features

- Interactive demonstrations of multiple LOD techniques:
  - Variant-based LOD
  - Payload-based LOD
  - Active metadata LOD
  - Behavioral LOD
  - Physics LOD
  - Complete castle demo with multiple systems

- Simple visualization using Raylib:
  - Clear representation of different LOD states
  - Color-coded systems and components
  - Real-time feedback on LOD changes

- Interactive controls:
  - Camera movement to test distance-based LOD
  - Manual LOD parameter adjustment
  - Demo scenario selection

## Requirements

- Python 3.7 or newer
- OpenUSD (via OpenEXR Core)
- Raylib (Python bindings)
- Additional dependencies specified in requirements.txt

## Installation

1. Clone this repository:
   ```
   git clone https://github.com/yourusername/usd_lod_demo.git
   cd usd_lod_demo
   ```

2. Create a virtual environment (recommended):
   ```
   python -m venv venv
   source venv/bin/activate  # On Windows: venv\Scripts\activate
   ```

3. Install dependencies:
   ```
   pip install -r requirements.txt
   ```

## Usage

Run the main application:
```
python src/main.py
```

### Demo Navigation

- Use mouse to select demos from the left panel
- Control the camera:
  - WASD: Move camera position
  - Arrow keys: Rotate camera
  - Q/E: Move camera up/down
  - Right mouse button + drag: Look around
- Use UI controls to:
  - Adjust LOD thresholds and parameters
  - Toggle automatic LOD
  - Manually select LOD levels
  - View diagnostic information

### Demo-Specific Controls

#### Variants Demo
- Adjust distance thresholds for LOD transitions
- Toggle automatic LOD selection based on distance
- Manually select specific LOD variant when auto mode is disabled
- Observe how different subsystems (geometry, AI, physics) change with LOD

#### Payloads Demo
- See dynamic loading/unloading of content as you move the camera
- Adjust distance thresholds for payload loading
- Toggle payload loading for specific components

#### Active Metadata Demo
- Observe subsystems being enabled/disabled based on distance
- Toggle activation of specific subsystems
- Adjust activation thresholds

#### Additional Demos
Each demo has specific controls that will be displayed in the UI panel

## Demo Descriptions

### Variant-Based LOD
Demonstrates how variants can be used to switch between different detail levels of entire subsystems.

### Payload-Based LOD
Shows dynamic loading/unloading of content using USD payloads based on distance.

### Active Metadata LOD
Displays toggling of subsystems using the USD 'active' metadata.

### Behavioral LOD
Demonstrates LOD for AI/behavior systems using USD composition.

### Physics LOD
Shows different levels of physics simulation based on distance.

### Castle Demo
A comprehensive example showing LOD across multiple systems in a castle scenario.

## Project Structure

```
usd_lod_demo/
├── assets/            # USD files and other assets
├── src/               # Source code
│   ├── demo_framework/  # Core framework
│   ├── demos/           # Individual demo implementations
│   ├── usd_utils/       # USD helper utilities
│   └── main.py          # Entry point
├── plan.md            # Project plan
├── status.md          # Current project status
└── requirements.txt   # Dependencies
```

## Development

See [plan.md](plan.md) for development roadmap and details.
See [status.md](status.md) for current project status.

## License

[Specify license]

## Acknowledgments

Based on concepts from "Chapter 3.2: Advanced LOD Through Composition"