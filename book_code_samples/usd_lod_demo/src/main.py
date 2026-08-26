#!/usr/bin/env python3
"""
USD LOD Demo - Main Entry Point
-------------------------------
This application demonstrates advanced LOD techniques using USD composition
as detailed in Chapter 3.2: "Advanced LOD Through Composition"
"""

import sys
import os
from pathlib import Path

# Add src directory to path
sys.path.append(str(Path(__file__).parent))

from demo_framework.app import DemoApplication
from demos import (
    variants_demo,
    payloads_demo,
    active_demo,
    behavioral_lod_demo,
    physics_lod_demo,
    castle_lod_demo
)

def main():
    """Main entry point for the application"""
    print("USD LOD Demo - Starting")
    
    # Create demo application
    app = DemoApplication(
        title="USD LOD Demo",
        width=1280,
        height=720
    )
    
    # Register demos
    # For now, only register the variants demo since it's the only one we've implemented
    app.register_demo("Variants", variants_demo.VariantsDemo)
    
    # These will be uncommented as we implement each demo
    # app.register_demo("Payloads", payloads_demo.PayloadsDemo)
    # app.register_demo("Active Metadata", active_demo.ActiveDemo)
    # app.register_demo("Behavioral LOD", behavioral_lod_demo.BehavioralLodDemo)
    # app.register_demo("Physics LOD", physics_lod_demo.PhysicsLodDemo)
    # app.register_demo("Castle LOD", castle_lod_demo.CastleLodDemo)
    
    # Run application
    app.run()
    
    print("USD LOD Demo - Exiting")

if __name__ == "__main__":
    main()