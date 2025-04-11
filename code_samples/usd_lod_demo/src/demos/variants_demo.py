"""
Variants Demo
------------
Demonstrates LOD using USD variants as described in Chapter 3.2.
Shows how variants can be used to switch between different detail levels 
of entire subsystems.
"""

import os
from pathlib import Path
from typing import Dict, List, Tuple, Optional

import pyray as rl

from pxr import Usd, UsdGeom, Sdf, Gf, Tf

from demo_framework.app import Demo
from usd_utils.usd_helpers import UsdHelpers

class VariantsDemo(Demo):
    """Demonstrates LOD using USD variants"""
    
    def __init__(self, app):
        super().__init__(app)
        self.stage = None
        self.enemy_prim = None
        self.camera_distance = 10.0
        self.current_lod = "high"
        self.auto_lod = True
        
        # Define LOD distance thresholds
        self.lod_thresholds = {
            "high": 5.0,
            "medium": 10.0,
            "low": 20.0
        }
        
    def initialize(self):
        """Initialize the demo"""
        if self.initialized:
            return
            
        print("Initializing Variants Demo")
        
        # Create assets directory if it doesn't exist
        assets_dir = self.app.assets_path / "enemy"
        assets_dir.mkdir(exist_ok=True, parents=True)
        
        # Create USD stage
        self.stage = UsdHelpers.create_stage()
        
        # Create root prim
        root = UsdHelpers.create_xform(self.stage, "/Root")
        
        # Create enemy prim with LOD variants
        self.create_enemy()
        
        # Initialize camera position
        self.app.camera.position = rl.Vector3(0.0, 5.0, 10.0)
        self.app.camera.target = rl.Vector3(0.0, 2.0, 0.0)
        
        self.initialized = True
    
    def create_enemy(self):
        """Create enemy prim with LOD variants"""
        # Create enemy prim
        enemy = UsdHelpers.create_xform(self.stage, "/Root/Enemy")
        self.enemy_prim = enemy.GetPrim()
        
        # Add custom attributes
        UsdHelpers.add_custom_attribute(self.enemy_prim, "sparkle:entity:id", "goblin_01", "string")
        
        # Create LOD variant set
        UsdHelpers.create_variant_set(
            self.enemy_prim,
            "complexityLOD",
            ["high", "medium", "low"],
            "high"
        )
        
        # Define high LOD variant
        def create_high_lod():
            # Appearance - High detail mesh
            appearance = UsdHelpers.create_xform(self.stage, "/Root/Enemy/Appearance")
            high_mesh = UsdHelpers.create_cube(self.stage, "/Root/Enemy/Appearance/HighDetailMesh", 2.0)
            
            # Add custom attributes to indicate this is high detail
            UsdHelpers.add_custom_attribute(high_mesh.GetPrim(), "polygons", 10000, "int")
            
            # Animation - Full skeleton
            animation = UsdHelpers.create_xform(self.stage, "/Root/Enemy/Animation")
            skeleton = UsdHelpers.create_xform(self.stage, "/Root/Enemy/Animation/FullSkeleton")
            UsdHelpers.add_custom_attribute(skeleton.GetPrim(), "joints", 80, "int")
            
            # Behavior - Full AI behavior
            behavior = UsdHelpers.create_xform(self.stage, "/Root/Enemy/Behavior")
            ai = UsdHelpers.create_xform(self.stage, "/Root/Enemy/Behavior/FullBehaviorTree")
            UsdHelpers.add_custom_attribute(ai.GetPrim(), "sparkle:ai:type", "complex", "string")
            UsdHelpers.add_custom_attribute(ai.GetPrim(), "sparkle:ai:maxDecisionDepth", 5, "int")
            UsdHelpers.add_custom_attribute(ai.GetPrim(), "sparkle:ai:patrolPath", "/Level/Paths/ComplexPatrol", "string")
            
            # Physics - Detailed physics
            physics = UsdHelpers.create_xform(self.stage, "/Root/Enemy/Physics")
            detailed_physics = UsdHelpers.create_xform(self.stage, "/Root/Enemy/Physics/DetailedPhysics")
            UsdHelpers.add_custom_attribute(detailed_physics.GetPrim(), "sparkle:physics:collisionType", "perBone", "string")
            UsdHelpers.add_custom_attribute(detailed_physics.GetPrim(), "sparkle:physics:collisionMesh", "/Enemy/Appearance/HighDetailMesh", "string")
        
        # Define medium LOD variant
        def create_medium_lod():
            # Appearance - Medium detail mesh
            appearance = UsdHelpers.create_xform(self.stage, "/Root/Enemy/Appearance")
            medium_mesh = UsdHelpers.create_cube(self.stage, "/Root/Enemy/Appearance/MediumDetailMesh", 1.9)
            
            # Add custom attributes to indicate this is medium detail
            UsdHelpers.add_custom_attribute(medium_mesh.GetPrim(), "polygons", 3000, "int")
            
            # Animation - Simplified skeleton
            animation = UsdHelpers.create_xform(self.stage, "/Root/Enemy/Animation")
            skeleton = UsdHelpers.create_xform(self.stage, "/Root/Enemy/Animation/MediumSkeleton")
            UsdHelpers.add_custom_attribute(skeleton.GetPrim(), "joints", 30, "int")
            
            # Behavior - Reduced AI behavior
            behavior = UsdHelpers.create_xform(self.stage, "/Root/Enemy/Behavior")
            ai = UsdHelpers.create_xform(self.stage, "/Root/Enemy/Behavior/SimplifiedBehaviorTree")
            UsdHelpers.add_custom_attribute(ai.GetPrim(), "sparkle:ai:type", "basic", "string")
            UsdHelpers.add_custom_attribute(ai.GetPrim(), "sparkle:ai:maxDecisionDepth", 3, "int")
            UsdHelpers.add_custom_attribute(ai.GetPrim(), "sparkle:ai:patrolPath", "/Level/Paths/SimplePatrol", "string")
            
            # Physics - Simplified physics
            physics = UsdHelpers.create_xform(self.stage, "/Root/Enemy/Physics")
            simplified_physics = UsdHelpers.create_xform(self.stage, "/Root/Enemy/Physics/SimplifiedPhysics")
            UsdHelpers.add_custom_attribute(simplified_physics.GetPrim(), "sparkle:physics:collisionType", "capsule", "string")
            UsdHelpers.add_custom_attribute(simplified_physics.GetPrim(), "sparkle:physics:collisionMesh", "/Enemy/Appearance/MediumDetailMesh", "string")
        
        # Define low LOD variant
        def create_low_lod():
            # Appearance - Low detail mesh
            appearance = UsdHelpers.create_xform(self.stage, "/Root/Enemy/Appearance")
            low_mesh = UsdHelpers.create_cube(self.stage, "/Root/Enemy/Appearance/LowDetailMesh", 1.8)
            
            # Add custom attributes to indicate this is low detail
            UsdHelpers.add_custom_attribute(low_mesh.GetPrim(), "polygons", 500, "int")
            
            # Animation - Minimal skeleton
            animation = UsdHelpers.create_xform(self.stage, "/Root/Enemy/Animation")
            skeleton = UsdHelpers.create_xform(self.stage, "/Root/Enemy/Animation/LowSkeleton")
            UsdHelpers.add_custom_attribute(skeleton.GetPrim(), "joints", 10, "int")
            
            # Behavior - Minimal AI behavior
            behavior = UsdHelpers.create_xform(self.stage, "/Root/Enemy/Behavior")
            ai = UsdHelpers.create_xform(self.stage, "/Root/Enemy/Behavior/MinimalBehaviorTree")
            UsdHelpers.add_custom_attribute(ai.GetPrim(), "sparkle:ai:type", "stationary", "string")
            UsdHelpers.add_custom_attribute(ai.GetPrim(), "sparkle:ai:maxDecisionDepth", 1, "int")
            
            # Physics - Minimal physics
            physics = UsdHelpers.create_xform(self.stage, "/Root/Enemy/Physics")
            minimal_physics = UsdHelpers.create_xform(self.stage, "/Root/Enemy/Physics/MinimalPhysics")
            UsdHelpers.add_custom_attribute(minimal_physics.GetPrim(), "sparkle:physics:collisionType", "simple", "string")
        
        # Create variants
        UsdHelpers.edit_variant(self.enemy_prim, "complexityLOD", "high", create_high_lod)
        UsdHelpers.edit_variant(self.enemy_prim, "complexityLOD", "medium", create_medium_lod)
        UsdHelpers.edit_variant(self.enemy_prim, "complexityLOD", "low", create_low_lod)
        
        # Set initial variant selection
        UsdHelpers.set_variant_selection(self.enemy_prim, "complexityLOD", "high")
        
    def update(self, delta_time: float):
        """Update the demo state"""
        # Calculate distance from camera to enemy
        camera_pos = (
            self.app.camera.position.x,
            self.app.camera.position.y,
            self.app.camera.position.z
        )
        self.camera_distance = UsdHelpers.get_distance_to_camera(
            self.stage, 
            "/Root/Enemy", 
            camera_pos
        )
        
        # Update LOD based on distance if auto LOD is enabled
        if self.auto_lod:
            if self.camera_distance < self.lod_thresholds["high"]:
                new_lod = "high"
            elif self.camera_distance < self.lod_thresholds["medium"]:
                new_lod = "medium"
            else:
                new_lod = "low"
                
            if new_lod != self.current_lod:
                self.current_lod = new_lod
                UsdHelpers.set_variant_selection(self.enemy_prim, "complexityLOD", new_lod)
    
    def render(self):
        """Render the demo scene"""
        # Draw ground plane
        self.renderer.draw_grid(20, 1.0)
        
        # Get current variant selection
        variant_sets = self.enemy_prim.GetVariantSets()
        variant_set = variant_sets.GetVariantSet("complexityLOD")
        variant_selection = variant_set.GetVariantSelection()
        
        # Default to "high" if no variant is selected
        if not variant_selection:
            variant_selection = "high"
            print(f"No variant selected, defaulting to {variant_selection}")
        
        # Draw enemy representation based on current LOD
        position = UsdHelpers.get_world_position(self.enemy_prim)
        
        # Draw base cube representing the enemy
        self.renderer.draw_box(
            position,
            (2.0, 4.0, 2.0),
            self.renderer.lod_colors[variant_selection],
            wire=False,
            label=f"Enemy ({variant_selection.upper()})"
        )
        
        # Draw LOD indicator
        self.renderer.draw_lod_indicator(
            (position[0], position[1] + 2.5, position[2]),
            variant_selection
        )
        
        # Draw subsystems based on current LOD
        self._render_subsystems(variant_selection)
    
    def _render_subsystems(self, lod: str):
        """Render visual representations of subsystems based on LOD"""
        # Calculate base position from enemy prim
        position = UsdHelpers.get_world_position(self.enemy_prim)
        base_x, base_y, base_z = position
        
        # Draw appearance subsystem
        if lod == "high":
            self.renderer.draw_text_3d(
                (base_x, base_y + 3.5, base_z),
                "Polygons: 10,000",
                14
            )
            # Indicate high detail mesh
            self.renderer.draw_box(
                (base_x, base_y, base_z),
                (2.0, 4.0, 2.0),
                (0, 150, 255, 100),
                wire=True
            )
        elif lod == "medium":
            self.renderer.draw_text_3d(
                (base_x, base_y + 3.5, base_z),
                "Polygons: 3,000",
                14
            )
            # Indicate medium detail mesh
            self.renderer.draw_box(
                (base_x, base_y, base_z),
                (1.9, 3.9, 1.9),
                (0, 200, 100, 100),
                wire=True
            )
        elif lod == "low":
            self.renderer.draw_text_3d(
                (base_x, base_y + 3.5, base_z),
                "Polygons: 500",
                14
            )
            # Indicate low detail mesh
            self.renderer.draw_box(
                (base_x, base_y, base_z),
                (1.8, 3.8, 1.8),
                (255, 200, 0, 100),
                wire=True
            )
        
        # Draw animation subsystem
        anim_x = base_x - 3.0
        self.renderer.draw_cylinder(
            (anim_x, base_y, base_z),
            0.5,
            3.0,
            self.renderer.system_colors["behavior"],
            wire=False,
            label="Animation"
        )
        
        if lod == "high":
            self.renderer.draw_text_3d(
                (anim_x, base_y + 3.5, base_z),
                "Joints: 80",
                14
            )
        elif lod == "medium":
            self.renderer.draw_text_3d(
                (anim_x, base_y + 3.5, base_z),
                "Joints: 30",
                14
            )
        elif lod == "low":
            self.renderer.draw_text_3d(
                (anim_x, base_y + 3.5, base_z),
                "Joints: 10",
                14
            )
        
        # Draw behavior subsystem
        ai_x = base_x + 3.0
        self.renderer.draw_sphere(
            (ai_x, base_y + 1.5, base_z),
            1.0,
            self.renderer.system_colors["physics"],
            wire=False,
            label="AI"
        )
        
        if lod == "high":
            self.renderer.draw_text_3d(
                (ai_x, base_y + 3.0, base_z),
                "Complex AI",
                14
            )
            self.renderer.draw_text_3d(
                (ai_x, base_y + 3.5, base_z),
                "Depth: 5",
                14
            )
        elif lod == "medium":
            self.renderer.draw_text_3d(
                (ai_x, base_y + 3.0, base_z),
                "Basic AI",
                14
            )
            self.renderer.draw_text_3d(
                (ai_x, base_y + 3.5, base_z),
                "Depth: 3",
                14
            )
        elif lod == "low":
            self.renderer.draw_text_3d(
                (ai_x, base_y + 3.0, base_z),
                "Stationary AI",
                14
            )
            self.renderer.draw_text_3d(
                (ai_x, base_y + 3.5, base_z),
                "Depth: 1",
                14
            )
        
        # Draw physics subsystem
        physics_z = base_z + 3.0
        self.renderer.draw_box(
            (base_x, base_y + 1.0, physics_z),
            (1.0, 2.0, 1.0),
            self.renderer.system_colors["visual"],
            wire=False,
            label="Physics"
        )
        
        if lod == "high":
            self.renderer.draw_text_3d(
                (base_x, base_y + 3.5, physics_z),
                "Collision: perBone",
                14
            )
            # Draw detailed collision
            self.renderer.draw_box(
                (base_x, base_y + 1.0, physics_z),
                (1.1, 2.1, 1.1),
                (200, 50, 50, 100),
                wire=True
            )
        elif lod == "medium":
            self.renderer.draw_text_3d(
                (base_x, base_y + 3.5, physics_z),
                "Collision: capsule",
                14
            )
            # Draw simplified collision
            self.renderer.draw_cylinder(
                (base_x, base_y, physics_z),
                0.6,
                3.0,
                (200, 50, 50, 100),
                wire=True
            )
        elif lod == "low":
            self.renderer.draw_text_3d(
                (base_x, base_y + 3.5, physics_z),
                "Collision: simple",
                14
            )
            # Draw simple collision
            self.renderer.draw_sphere(
                (base_x, base_y + 1.0, physics_z),
                1.0,
                (200, 50, 50, 100),
                wire=True
            )
        
        # Draw camera distance information
        self.renderer.draw_text_3d(
            (base_x, base_y + 5.0, base_z),
            f"Camera Distance: {self.camera_distance:.1f}m",
            16
        )
    
    def render_ui(self):
        """Render UI elements for the demo"""
        # Get panel content area
        x, y, width, height = self.ui.get_panel_content_area()
        
        # Create control panel
        self.ui.begin_panel("LOD Controls", x + 10, y + 10, 300, 350)
        
        # Display current LOD
        self.ui.label(f"Current LOD: {self.current_lod.upper()}", x + 20, y + 40)
        self.ui.label(f"Camera Distance: {self.camera_distance:.1f}m", x + 20, y + 70)
        
        # Add LOD threshold sliders
        y_offset = 110
        for lod_name in ["high", "medium"]:
            threshold = self.ui.slider_with_label(
                f"{lod_name.capitalize()} LOD Threshold",
                f"threshold_{lod_name}",
                x + 20, y + y_offset,
                260,
                1.0, 30.0,
                self.lod_thresholds[lod_name]
            )
            self.lod_thresholds[lod_name] = threshold
            y_offset += 60
        
        # Add auto LOD toggle
        auto_lod = self.ui.checkbox(
            "Auto LOD",
            "auto_lod",
            x + 20, y + y_offset,
            self.auto_lod
        )
        self.auto_lod = auto_lod
        y_offset += 40
        
        # If auto LOD is disabled, add manual LOD selection
        if not self.auto_lod:
            self.ui.label("Manual LOD Selection:", x + 20, y + y_offset)
            y_offset += 30
            
            for lod_name in ["high", "medium", "low"]:
                is_selected = self.current_lod == lod_name
                if self.ui.button(
                    lod_name.upper(),
                    x + 20 + (["high", "medium", "low"].index(lod_name) * 90),
                    y + y_offset,
                    80, 30,
                    is_selected
                ):
                    self.current_lod = lod_name
                    UsdHelpers.set_variant_selection(self.enemy_prim, "complexityLOD", lod_name)
        
        # End panel
        self.ui.end_panel()
    
    def cleanup(self):
        """Clean up resources"""
        print("Cleaning up Variants Demo")
        self.stage = None
        self.enemy_prim = None