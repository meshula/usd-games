"""
USD Helper Utilities
-------------------
Provides helper functions for working with USD stages, prims, and properties.
Simplifies common operations for the LOD demos.
"""

import os
from pathlib import Path
from typing import Dict, List, Tuple, Optional, Any, Union

from pxr import Usd, UsdGeom, Sdf, Gf, Tf, Vt

class UsdHelpers:
    """Helper class for USD operations"""
    
    @staticmethod
    def create_stage(output_path: Optional[str] = None) -> Usd.Stage:
        """
        Create a new USD stage
        
        Args:
            output_path: Optional path for saving the stage
            
        Returns:
            New USD stage
        """
        if output_path:
            stage = Usd.Stage.CreateNew(output_path)
        else:
            stage = Usd.Stage.CreateInMemory()
            
        # Set up stage defaults
        UsdGeom.SetStageUpAxis(stage, UsdGeom.Tokens.y)
        
        # Set meters per unit to 1.0 (Raylib uses meters)
        stage.SetMetadata("metersPerUnit", 1.0)
        
        # Set default prim
        root_prim = stage.DefinePrim("/Root", "Xform")
        stage.SetDefaultPrim(root_prim)
        
        return stage
    
    @staticmethod
    def save_stage(stage: Usd.Stage, file_path: str) -> bool:
        """
        Save a USD stage to file
        
        Args:
            stage: USD stage to save
            file_path: Output file path
            
        Returns:
            True if successful
        """
        try:
            stage.GetRootLayer().Export(file_path)
            return True
        except Exception as e:
            print(f"Error saving stage: {e}")
            return False
    
    @staticmethod
    def load_stage(file_path: str) -> Optional[Usd.Stage]:
        """
        Load a USD stage from file
        
        Args:
            file_path: USD file path
            
        Returns:
            Loaded USD stage or None if failed
        """
        try:
            return Usd.Stage.Open(file_path)
        except Exception as e:
            print(f"Error loading stage: {e}")
            return None
    
    @staticmethod
    def create_xform(stage: Usd.Stage, path: str) -> UsdGeom.Xform:
        """
        Create an Xform prim
        
        Args:
            stage: USD stage
            path: Prim path
            
        Returns:
            New Xform prim
        """
        return UsdGeom.Xform.Define(stage, path)
    
    @staticmethod
    def create_cube(stage: Usd.Stage, path: str, size: float = 1.0) -> UsdGeom.Cube:
        """
        Create a cube prim
        
        Args:
            stage: USD stage
            path: Prim path
            size: Cube size
            
        Returns:
            New Cube prim
        """
        cube = UsdGeom.Cube.Define(stage, path)
        cube.CreateSizeAttr(size)
        return cube
    
    @staticmethod
    def create_sphere(stage: Usd.Stage, path: str, radius: float = 1.0) -> UsdGeom.Sphere:
        """
        Create a sphere prim
        
        Args:
            stage: USD stage
            path: Prim path
            radius: Sphere radius
            
        Returns:
            New Sphere prim
        """
        sphere = UsdGeom.Sphere.Define(stage, path)
        sphere.CreateRadiusAttr(radius)
        return sphere
    
    @staticmethod
    def create_cylinder(stage: Usd.Stage, path: str, height: float = 1.0, radius: float = 1.0) -> UsdGeom.Cylinder:
        """
        Create a cylinder prim
        
        Args:
            stage: USD stage
            path: Prim path
            height: Cylinder height
            radius: Cylinder radius
            
        Returns:
            New Cylinder prim
        """
        cylinder = UsdGeom.Cylinder.Define(stage, path)
        cylinder.CreateHeightAttr(height)
        cylinder.CreateRadiusAttr(radius)
        return cylinder
    
    @staticmethod
    def set_transform(prim: Usd.Prim, 
                     translation: Optional[Tuple[float, float, float]] = None,
                     rotation: Optional[Tuple[float, float, float]] = None,
                     scale: Optional[Tuple[float, float, float]] = None,
                     time: Optional[float] = None):
        """
        Set transform for a prim
        
        Args:
            prim: USD prim
            translation: Optional (x, y, z) translation
            rotation: Optional (x, y, z) rotation in degrees
            scale: Optional (x, y, z) scale
            time: Optional time
        """
        xform = UsdGeom.Xformable(prim)
        
        # Use default time if not specified
        if time is None:
            time = Usd.TimeCode.Default()
        
        # Apply translation
        if translation:
            translation_op = xform.AddTranslateOp()
            translation_op.Set(Gf.Vec3d(*translation), time)
        
        # Apply rotation (in degrees, XYZ order)
        if rotation:
            rotation_op_x = xform.AddRotateXOp()
            rotation_op_y = xform.AddRotateYOp()
            rotation_op_z = xform.AddRotateZOp()
            
            rotation_op_x.Set(rotation[0], time)
            rotation_op_y.Set(rotation[1], time)
            rotation_op_z.Set(rotation[2], time)
        
        # Apply scale
        if scale:
            scale_op = xform.AddScaleOp()
            scale_op.Set(Gf.Vec3d(*scale), time)
    
    @staticmethod
    def get_world_transform(prim: Usd.Prim, time: Optional[float] = None) -> Gf.Matrix4d:
        """
        Get world transform for a prim
        
        Args:
            prim: USD prim
            time: Optional time
            
        Returns:
            World transform matrix
        """
        xform = UsdGeom.Xformable(prim)
        
        # Use default time if not specified
        if time is None:
            time = Usd.TimeCode.Default()
        
        # Get world transform
        return xform.ComputeLocalToWorldTransform(time)
    
    @staticmethod
    def get_world_position(prim: Usd.Prim, time: Optional[float] = None) -> Tuple[float, float, float]:
        """
        Get world position for a prim
        
        Args:
            prim: USD prim
            time: Optional time
            
        Returns:
            (x, y, z) world position
        """
        matrix = UsdHelpers.get_world_transform(prim, time)
        translation = matrix.ExtractTranslation()
        return (translation[0], translation[1], translation[2])
    
    @staticmethod
    def create_variant_set(prim: Usd.Prim, variant_set_name: str, variants: List[str],
                         default_variant: Optional[str] = None) -> Optional[Usd.VariantSet]:
        """
        Create a variant set with variants
        
        Args:
            prim: USD prim
            variant_set_name: Name of the variant set
            variants: List of variant names
            default_variant: Optional default variant
            
        Returns:
            Created variant set or None if failed
        """
        try:
            variant_sets = prim.GetVariantSets()
            variant_set = variant_sets.AddVariantSet(variant_set_name)
            
            # Add variants - check if they exist first using GetVariantNames
            variant_names = variant_set.GetVariantNames()
            for variant in variants:
                if variant not in variant_names:
                    variant_set.AddVariant(variant)
            
            # Set default variant if specified
            if default_variant and default_variant in variant_set.GetVariantNames():
                variant_set.SetVariantSelection(default_variant)
                
            return variant_set
        except Exception as e:
            print(f"Error creating variant set: {e}")
            return None
    
    @staticmethod
    def edit_variant(prim: Usd.Prim, variant_set_name: str, variant_name: str, 
                    callback: callable) -> bool:
        """
        Edit a variant by providing a callback function
        
        Args:
            prim: USD prim
            variant_set_name: Name of the variant set
            variant_name: Name of the variant to edit
            callback: Function to call inside the variant edit context
            
        Returns:
            True if successful
        """
        try:
            variant_sets = prim.GetVariantSets()
            variant_set = variant_sets.GetVariantSet(variant_set_name)
            
            # Check if variant exists using GetVariantNames
            if variant_name not in variant_set.GetVariantNames():
                print(f"Variant {variant_name} not found in {variant_set_name}")
                return False
            
            with variant_set.GetVariantEditContext(variant_name):
                callback()
                
            return True
        except Exception as e:
            print(f"Error editing variant: {e}")
            return False
    
    @staticmethod
    def set_variant_selection(prim: Usd.Prim, variant_set_name: str, variant_name: str) -> bool:
        """
        Set the selected variant
        
        Args:
            prim: USD prim
            variant_set_name: Name of the variant set
            variant_name: Name of the variant to select
            
        Returns:
            True if successful
        """
        try:
            variant_sets = prim.GetVariantSets()
            variant_set = variant_sets.GetVariantSet(variant_set_name)
            
            # Check if variant exists using GetVariantNames
            if variant_name not in variant_set.GetVariantNames():
                print(f"Variant {variant_name} not found in {variant_set_name}")
                return False
            
            variant_set.SetVariantSelection(variant_name)
            return True
        except Exception as e:
            print(f"Error setting variant selection: {e}")
            return False
    
    @staticmethod
    def create_payload(stage: Usd.Stage, path: str, asset_path: str, prim_path: str = "/") -> Usd.Prim:
        """
        Create a prim with a payload reference
        
        Args:
            stage: USD stage
            path: Prim path
            asset_path: Path to asset file
            prim_path: Path to target prim in asset file
            
        Returns:
            New prim with payload
        """
        prim = stage.DefinePrim(path)
        prim.GetPayloads().AddPayload(asset_path, prim_path)
        return prim
    
    @staticmethod
    def load_payload(stage: Usd.Stage, path: str) -> bool:
        """
        Load a prim's payload
        
        Args:
            stage: USD stage
            path: Prim path
            
        Returns:
            True if successful
        """
        try:
            prim = stage.GetPrimAtPath(path)
            if not prim:
                print(f"Prim {path} not found")
                return False
                
            stage.LoadPayload(prim)
            return True
        except Exception as e:
            print(f"Error loading payload: {e}")
            return False
    
    @staticmethod
    def unload_payload(stage: Usd.Stage, path: str) -> bool:
        """
        Unload a prim's payload
        
        Args:
            stage: USD stage
            path: Prim path
            
        Returns:
            True if successful
        """
        try:
            prim = stage.GetPrimAtPath(path)
            if not prim:
                print(f"Prim {path} not found")
                return False
                
            stage.UnloadPayload(prim)
            return True
        except Exception as e:
            print(f"Error unloading payload: {e}")
            return False
    
    @staticmethod
    def set_active(stage: Usd.Stage, path: str, active: bool) -> bool:
        """
        Set a prim's active state
        
        Args:
            stage: USD stage
            path: Prim path
            active: Active state
            
        Returns:
            True if successful
        """
        try:
            prim = stage.GetPrimAtPath(path)
            if not prim:
                print(f"Prim {path} not found")
                return False
                
            prim.SetActive(active)
            return True
        except Exception as e:
            print(f"Error setting active state: {e}")
            return False
    
    @staticmethod
    def get_active(stage: Usd.Stage, path: str) -> bool:
        """
        Get a prim's active state
        
        Args:
            stage: USD stage
            path: Prim path
            
        Returns:
            Active state (False if prim not found)
        """
        prim = stage.GetPrimAtPath(path)
        if not prim:
            print(f"Prim {path} not found")
            return False
            
        return prim.IsActive()
    
    @staticmethod
    def add_custom_attribute(prim: Usd.Prim, name: str, value: Any, 
                           type_name: Optional[str] = None) -> Optional[Usd.Attribute]:
        """
        Add a custom attribute to a prim
        
        Args:
            prim: USD prim
            name: Attribute name
            value: Attribute value
            type_name: Optional type name (auto-detected if not provided)
            
        Returns:
            Created attribute or None if failed
        """
        try:
            # Auto-detect type if not provided
            if type_name is None:
                if isinstance(value, bool):
                    type_name = "bool"
                elif isinstance(value, int):
                    type_name = "int"
                elif isinstance(value, float):
                    type_name = "float"
                elif isinstance(value, str):
                    type_name = "string"
                elif isinstance(value, (tuple, list)) and len(value) == 3:
                    # Assume float3 for 3-element tuples/lists
                    type_name = "float3"
                else:
                    print(f"Unsupported value type: {type(value)}")
                    return None
            
            # Create attribute
            attr = prim.CreateAttribute(name, Sdf.ValueTypeNames.Find(type_name))
            if not attr:
                print(f"Failed to create attribute: {name}")
                return None
                
            # Set value
            attr.Set(value)
            return attr
        except Exception as e:
            print(f"Error adding custom attribute: {e}")
            return None
    
    @staticmethod
    def get_attribute_value(prim: Usd.Prim, name: str, default_value: Any = None) -> Any:
        """
        Get value of an attribute
        
        Args:
            prim: USD prim
            name: Attribute name
            default_value: Default value if attribute not found
            
        Returns:
            Attribute value or default value
        """
        attr = prim.GetAttribute(name)
        if not attr:
            return default_value
            
        return attr.Get() or default_value
    
    @staticmethod
    def set_attribute_value(prim: Usd.Prim, name: str, value: Any, time: Optional[float] = None) -> bool:
        """
        Set value of an attribute
        
        Args:
            prim: USD prim
            name: Attribute name
            value: New value
            time: Optional time
            
        Returns:
            True if successful
        """
        try:
            attr = prim.GetAttribute(name)
            if not attr:
                print(f"Attribute {name} not found")
                return False
                
            if time is None:
                attr.Set(value)
            else:
                attr.Set(value, time)
                
            return True
        except Exception as e:
            print(f"Error setting attribute value: {e}")
            return False
    
    @staticmethod
    def get_distance_to_camera(stage: Usd.Stage, prim_path: str, 
                              camera_position: Tuple[float, float, float]) -> float:
        """
        Calculate distance from a prim to camera position
        
        Args:
            stage: USD stage
            prim_path: Path to the prim
            camera_position: (x, y, z) camera position
            
        Returns:
            Distance (or float('inf') if prim not found)
        """
        prim = stage.GetPrimAtPath(prim_path)
        if not prim:
            return float('inf')
            
        prim_position = UsdHelpers.get_world_position(prim)
        
        # Calculate distance
        dx = camera_position[0] - prim_position[0]
        dy = camera_position[1] - prim_position[1]
        dz = camera_position[2] - prim_position[2]
        
        return (dx * dx + dy * dy + dz * dz) ** 0.5