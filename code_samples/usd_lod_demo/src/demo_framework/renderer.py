"""
Renderer
--------
Provides abstraction for rendering USD scenes using Raylib.
Simplifies the representation of USD elements for visualization.
"""

import pyray as rl
from typing import List, Dict, Tuple, Optional, Union

# Type aliases
Position = Tuple[float, float, float]
Size = Tuple[float, float, float]
ColorType = Tuple[int, int, int, int]

class Renderer:
    """Renderer class that abstracts the rendering of USD elements"""
    
    def __init__(self, app):
        self.app = app
        
        # Define colors for different LOD levels
        self.lod_colors = {
            "high": rl.Color(0, 150, 255, 255),      # Blue
            "medium": rl.Color(0, 200, 100, 255),    # Green
            "low": rl.Color(255, 200, 0, 255),       # Yellow
            "minimal": rl.Color(255, 100, 0, 255)    # Orange
        }
        
        # Define colors for different subsystems
        self.system_colors = {
            "visual": rl.Color(200, 50, 200, 255),       # Purple
            "behavior": rl.Color(50, 150, 200, 255),     # Light Blue
            "physics": rl.Color(200, 50, 50, 255),       # Red
            "audio": rl.Color(50, 200, 200, 255),        # Teal
            "gameplay": rl.Color(200, 150, 50, 255),     # Gold
            "default": rl.Color(150, 150, 150, 255)      # Gray
        }
    
    def draw_box(self, position: Position, size: Size, color: Union[rl.Color, str, ColorType], 
                wire: bool = False, label: Optional[str] = None):
        """
        Draw a box (cube) representing a USD prim
        
        Args:
            position: (x, y, z) position
            size: (width, height, depth) size
            color: Color to use (string name, RGB tuple, or raylib Color)
            wire: Whether to draw as wireframe
            label: Optional text label to display
        """
        # Convert string color to raylib Color
        if isinstance(color, str):
            if color in self.lod_colors:
                color = self.lod_colors[color]
            elif color in self.system_colors:
                color = self.system_colors[color]
            else:
                print(f"Warning: Unknown color name '{color}', using default color")
                color = self.system_colors["default"]
        
        # Convert tuple color to raylib Color
        elif isinstance(color, tuple):
            if len(color) == 3:
                color = rl.Color(color[0], color[1], color[2], 255)
            else:
                color = rl.Color(color[0], color[1], color[2], color[3])
        
        # Create position and size as Vector3
        pos = rl.Vector3(position[0], position[1], position[2])
        sz = rl.Vector3(size[0], size[1], size[2])
        
        # Draw box
        if wire:
            rl.draw_cube_wires(pos, sz.x, sz.y, sz.z, color)
        else:
            rl.draw_cube(pos, sz.x, sz.y, sz.z, color)
            # Also draw wireframe in black for better visibility
            rl.draw_cube_wires(pos, sz.x, sz.y, sz.z, rl.Color(0, 0, 0, 100))
        
        # Draw label if provided
        if label:
            # Calculate screen position
            screen_pos = rl.get_world_to_screen(pos, self.app.camera)
            # Draw text
            rl.draw_text(label, int(screen_pos.x), int(screen_pos.y), 20, rl.Color(255, 255, 255, 255))
    
    def draw_sphere(self, position: Position, radius: float, color: Union[rl.Color, str, ColorType],
                   wire: bool = False, label: Optional[str] = None):
        """
        Draw a sphere representing a USD prim
        
        Args:
            position: (x, y, z) position
            radius: Sphere radius
            color: Color to use (string name, RGB tuple, or raylib Color)
            wire: Whether to draw as wireframe
            label: Optional text label to display
        """
        # Convert string color to raylib Color
        if isinstance(color, str):
            if color in self.lod_colors:
                color = self.lod_colors[color]
            elif color in self.system_colors:
                color = self.system_colors[color]
            else:
                print(f"Warning: Unknown color name '{color}', using default color")
                color = self.system_colors["default"]
        
        # Convert tuple color to raylib Color
        elif isinstance(color, tuple):
            if len(color) == 3:
                color = rl.Color(color[0], color[1], color[2], 255)
            else:
                color = rl.Color(color[0], color[1], color[2], color[3])
        
        # Create position as Vector3
        pos = rl.Vector3(position[0], position[1], position[2])
        
        # Draw sphere
        if wire:
            rl.draw_sphere_wires(pos, radius, 8, 8, color)
        else:
            rl.draw_sphere(pos, radius, color)
            # Also draw wireframe in black for better visibility
            rl.draw_sphere_wires(pos, radius, 8, 8, rl.Color(0, 0, 0, 100))
        
        # Draw label if provided
        if label:
            # Calculate screen position
            screen_pos = rl.get_world_to_screen(pos, self.app.camera)
            # Draw text
            rl.draw_text(label, int(screen_pos.x), int(screen_pos.y), 20, rl.Color(255, 255, 255, 255))
    
    def draw_cylinder(self, position: Position, radius: float, height: float, color: Union[rl.Color, str, ColorType],
                     wire: bool = False, label: Optional[str] = None):
        """
        Draw a cylinder representing a USD prim
        
        Args:
            position: (x, y, z) position of the base center
            radius: Cylinder radius
            height: Cylinder height
            color: Color to use (string name, RGB tuple, or raylib Color)
            wire: Whether to draw as wireframe
            label: Optional text label to display
        """
        # Convert string color to raylib Color
        if isinstance(color, str):
            if color in self.lod_colors:
                color = self.lod_colors[color]
            elif color in self.system_colors:
                color = self.system_colors[color]
            else:
                print(f"Warning: Unknown color name '{color}', using default color")
                color = self.system_colors["default"]
        
        # Convert tuple color to raylib Color
        elif isinstance(color, tuple):
            if len(color) == 3:
                color = rl.Color(color[0], color[1], color[2], 255)
            else:
                color = rl.Color(color[0], color[1], color[2], color[3])
        
        # Create position as Vector3
        pos = rl.Vector3(position[0], position[1], position[2])
        
        # PyRay requires a different approach for drawing cylinders
        if wire:
            # There's no direct cylinder wire drawing in PyRay
            # So we'll approximate with multiple circle wires
            segments = 8
            for i in range(segments + 1):
                y = position[1] + (height * i / segments)
                center = rl.Vector3(position[0], y, position[2])
                rl.draw_circle_3d(center, radius, rl.Vector3(0, 1, 0), 0, color)
            
            # Connect circles with lines
            for angle in range(0, 360, 45):
                rad_angle = angle * 0.0174533  # Convert to radians
                x_offset = radius * rl.cos(rad_angle)
                z_offset = radius * rl.sin(rad_angle)
                
                start = rl.Vector3(pos.x + x_offset, pos.y, pos.z + z_offset)
                end = rl.Vector3(pos.x + x_offset, pos.y + height, pos.z + z_offset)
                
                rl.draw_line_3d(start, end, color)
        else:
            # In PyRay we need to provide start and end positions for cylinder
            start_pos = rl.Vector3(position[0], position[1], position[2])
            end_pos = rl.Vector3(position[0], position[1] + height, position[2])
            rl.draw_cylinder(start_pos, end_pos, radius, 16, 1, color)
            
            # Also draw wireframe for better visibility
            # Approximate wireframe for the cylinder
            segments = 8
            for i in range(segments + 1):
                y = position[1] + (height * i / segments)
                center = rl.Vector3(position[0], y, position[2])
                rl.draw_circle_3d(center, radius, rl.Vector3(0, 1, 0), 0, rl.Color(0, 0, 0, 100))
            
            # Connect circles with lines
            for angle in range(0, 360, 45):
                rad_angle = angle * 0.0174533  # Convert to radians
                x_offset = radius * rl.cos(rad_angle)
                z_offset = radius * rl.sin(rad_angle)
                
                start = rl.Vector3(pos.x + x_offset, pos.y, pos.z + z_offset)
                end = rl.Vector3(pos.x + x_offset, pos.y + height, pos.z + z_offset)
                
                rl.draw_line_3d(start, end, rl.Color(0, 0, 0, 100))
        
        # Draw label if provided
        if label:
            # Calculate screen position for center of cylinder
            center_pos = rl.Vector3(pos.x, pos.y + height/2, pos.z)
            screen_pos = rl.get_world_to_screen(center_pos, self.app.camera)
            # Draw text
            rl.draw_text(label, int(screen_pos.x), int(screen_pos.y), 20, rl.Color(255, 255, 255, 255))
    
    def draw_line_3d(self, start_pos: Position, end_pos: Position, color: Union[rl.Color, str, ColorType]):
        """
        Draw a 3D line
        
        Args:
            start_pos: (x, y, z) start position
            end_pos: (x, y, z) end position
            color: Color to use
        """
        # Convert string color to raylib Color
        if isinstance(color, str):
            if color in self.lod_colors:
                color = self.lod_colors[color]
            elif color in self.system_colors:
                color = self.system_colors[color]
            else:
                print(f"Warning: Unknown color name '{color}', using default color")
                color = self.system_colors["default"]
        
        # Convert tuple color to raylib Color
        elif isinstance(color, tuple):
            if len(color) == 3:
                color = rl.Color(color[0], color[1], color[2], 255)
            else:
                color = rl.Color(color[0], color[1], color[2], color[3])
        
        # Create Vector3 positions
        start = rl.Vector3(start_pos[0], start_pos[1], start_pos[2])
        end = rl.Vector3(end_pos[0], end_pos[1], end_pos[2])
        
        # Draw line
        rl.draw_line_3d(start, end, color)
    
    def draw_grid(self, slices: int, spacing: float):
        """Draw a grid on the XZ plane"""
        rl.draw_grid(slices, spacing)
    
    def draw_bounding_box(self, min_point: Position, max_point: Position, color: Union[rl.Color, str, ColorType]):
        """
        Draw a bounding box from min and max points
        
        Args:
            min_point: (x, y, z) minimum point
            max_point: (x, y, z) maximum point
            color: Color to use
        """
        # Convert string color to raylib Color
        if isinstance(color, str):
            if color in self.lod_colors:
                color = self.lod_colors[color]
            elif color in self.system_colors:
                color = self.system_colors[color]
            else:
                print(f"Warning: Unknown color name '{color}', using default color")
                color = self.system_colors["default"]
        
        # Convert tuple color to raylib Color
        elif isinstance(color, tuple):
            if len(color) == 3:
                color = rl.Color(color[0], color[1], color[2], 255)
            else:
                color = rl.Color(color[0], color[1], color[2], color[3])
        
        # Create bounding box - PyRay uses a different structure
        min_vec = rl.Vector3(min_point[0], min_point[1], min_point[2])
        max_vec = rl.Vector3(max_point[0], max_point[1], max_point[2])
        bounds = rl.BoundingBox()
        bounds.min = min_vec
        bounds.max = max_vec
        
        # Draw bounding box
        rl.draw_bounding_box(bounds, color)
    
    def draw_text_3d(self, position: Position, text: str, font_size: int = 20, 
                    color: Union[rl.Color, str, ColorType] = (255, 255, 255, 255)):
        """
        Draw text at a 3D position
        
        Args:
            position: (x, y, z) position
            text: Text to display
            font_size: Font size
            color: Text color
        """
        # Convert string color to raylib Color
        if isinstance(color, str):
            if color in self.lod_colors:
                color = self.lod_colors[color]
            elif color in self.system_colors:
                color = self.system_colors[color]
            else:
                print(f"Warning: Unknown color name '{color}', using default color")
                color = self.system_colors["default"]
        
        # Convert tuple color to raylib Color
        elif isinstance(color, tuple):
            if len(color) == 3:
                color = rl.Color(color[0], color[1], color[2], 255)
            else:
                color = rl.Color(color[0], color[1], color[2], color[3])
        
        # Calculate screen position
        pos_3d = rl.Vector3(position[0], position[1], position[2])
        screen_pos = rl.get_world_to_screen(pos_3d, self.app.camera)
        
        # Draw text
        rl.draw_text(text, int(screen_pos.x), int(screen_pos.y), font_size, color)
    
    def draw_lod_indicator(self, position: Position, lod_level: str, size: float = 0.5):
        """
        Draw an indicator showing the current LOD level
        
        Args:
            position: (x, y, z) position
            lod_level: LOD level name (high, medium, low, minimal)
            size: Size of the indicator
        """
        # Position slightly above the main object
        pos = (position[0], position[1] + size * 2, position[2])
        
        # Get color for the LOD level
        color = self.lod_colors.get(lod_level.lower(), self.lod_colors["high"])
        
        # Draw a small sphere
        self.draw_sphere(pos, size * 0.3, color, wire=False)
        
        # Draw text
        text_pos = (pos[0], pos[1] + size * 0.5, pos[2])
        self.draw_text_3d(text_pos, lod_level.upper(), 16, (255, 255, 255, 255))
    
    def draw_system_indicator(self, position: Position, system_name: str, 
                             size: float = 0.5, offset_x: float = 0.0):
        """
        Draw an indicator showing the system type
        
        Args:
            position: (x, y, z) position
            system_name: System name (visual, behavior, physics, etc.)
            size: Size of the indicator
            offset_x: Horizontal offset for positioning multiple indicators
        """
        # Position with offset
        pos = (position[0] + offset_x, position[1] + size * 3, position[2])
        
        # Get color for the system
        color = self.system_colors.get(system_name.lower(), self.system_colors["default"])
        
        # Draw a small cube
        self.draw_box(pos, (size * 0.4, size * 0.4, size * 0.4), color, wire=False)
        
        # Draw text
        text_pos = (pos[0], pos[1] + size * 0.3, pos[2])
        self.draw_text_3d(text_pos, system_name.upper(), 14, (255, 255, 255, 255))