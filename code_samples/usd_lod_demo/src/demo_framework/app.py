"""
Demo Application Framework
-------------------------
Provides the base application structure for all demos, handling:
- Window and rendering setup
- Demo selection UI
- Camera controls
- Integration with USD stage
"""

import os
import sys
from pathlib import Path
from typing import Dict, List, Tuple, Optional, Any, Union, Type

# Use pyray instead of raylib
import pyray as rl

from .renderer import Renderer
from .ui import UI

# Base class for all demos
class Demo:
    """Base class for all demos"""
    
    def __init__(self, app: 'DemoApplication'):
        self.app = app
        self.renderer = app.renderer
        self.ui = app.ui
        self.initialized = False
        
    def initialize(self):
        """Initialize the demo (load USD files, etc.)"""
        self.initialized = True
        
    def update(self, delta_time: float):
        """Update the demo state"""
        pass
        
    def render(self):
        """Render the demo scene"""
        pass
        
    def render_ui(self):
        """Render the demo UI elements"""
        pass
        
    def cleanup(self):
        """Clean up resources"""
        pass


class DemoApplication:
    """Main application framework for USD LOD demos"""
    
    def __init__(self, title: str, width: int = 1280, height: int = 720):
        self.title = title
        self.width = width
        self.height = height
        self.running = False
        self.demos: Dict[str, Type[Demo]] = {}
        self.current_demo: Optional[Demo] = None
        self.current_demo_name: Optional[str] = None
        
        # Assets path
        self.assets_path = Path(__file__).parent.parent.parent / "assets"
        
        # Initialize raylib
        rl.init_window(width, height, title)
        rl.set_target_fps(60)
        
        # Initialize camera - PyRay uses a different approach
        # Create a camera with position, target, up vector, fov, and projection type
        self.camera = rl.Camera3D()
        self.camera.position = rl.Vector3(10.0, 5.0, 10.0)
        self.camera.target = rl.Vector3(0.0, 0.0, 0.0)
        self.camera.up = rl.Vector3(0.0, 1.0, 0.0)
        self.camera.fovy = 60.0
        self.camera.projection = rl.CAMERA_PERSPECTIVE
        
        # Initialize renderer and UI
        self.renderer = Renderer(self)
        self.ui = UI(self)
        
    def register_demo(self, name: str, demo_class: Type[Demo]):
        """Register a demo with the application"""
        self.demos[name] = demo_class
        
    def select_demo(self, name: str):
        """Select and initialize a demo by name"""
        if name not in self.demos:
            print(f"Demo '{name}' not found")
            return
            
        # Clean up current demo if exists
        if self.current_demo:
            self.current_demo.cleanup()
            
        # Create and initialize new demo
        demo_class = self.demos[name]
        self.current_demo = demo_class(self)
        self.current_demo_name = name
        
        print(f"Initializing demo: {name}")
        self.current_demo.initialize()
        
    def run(self):
        """Run the application main loop"""
        self.running = True
        
        # Select first demo if available
        if self.demos and not self.current_demo:
            first_demo_name = next(iter(self.demos))
            self.select_demo(first_demo_name)
        
        # Main loop
        while not rl.window_should_close() and self.running:
            # Update
            delta_time = rl.get_frame_time()
            self._update(delta_time)
            
            # Draw
            rl.begin_drawing()
            rl.clear_background(rl.Color(245, 245, 245, 255))
            
            # 3D scene
            rl.begin_mode_3d(self.camera)
            self._render_3d()
            rl.end_mode_3d()
            
            # 2D UI
            self._render_ui()
            
            rl.end_drawing()
            
        # Cleanup
        if self.current_demo:
            self.current_demo.cleanup()
            
        rl.close_window()
        
    def _update(self, delta_time: float):
        """Update application state"""
        # Update camera
        self._update_camera(delta_time)
        
        # Update UI
        self.ui.update(delta_time)
        
        # Update current demo
        if self.current_demo:
            self.current_demo.update(delta_time)
            
    def _update_camera(self, delta_time: float):
        """Update camera position based on input"""
        # Camera controls: WASD for movement, mouse for look
        speed = 10.0 * delta_time
        
        # Forward/backward
        if rl.is_key_down(rl.KEY_W):
            self.camera.position.z -= speed
            self.camera.target.z -= speed
        if rl.is_key_down(rl.KEY_S):
            self.camera.position.z += speed
            self.camera.target.z += speed
            
        # Left/right
        if rl.is_key_down(rl.KEY_A):
            self.camera.position.x -= speed
            self.camera.target.x -= speed
        if rl.is_key_down(rl.KEY_D):
            self.camera.position.x += speed
            self.camera.target.x += speed
            
        # Mouse look (when right button is held)
        if rl.is_mouse_button_down(rl.MOUSE_BUTTON_RIGHT):
            mouse_delta = rl.get_mouse_delta()
            # TODO: Implement proper mouse camera control
            
    def _render_3d(self):
        """Render 3D scene"""
        # Draw grid
        rl.draw_grid(20, 1.0)
        
        # Draw current demo
        if self.current_demo:
            self.current_demo.render()
        
    def _render_ui(self):
        """Render UI elements"""
        # Draw UI framework
        self.ui.begin_frame()
        
        # Demo selection UI
        self._render_demo_selection()
        
        # Draw current demo UI
        if self.current_demo:
            self.current_demo.render_ui()
            
        # Draw debug info
        self._render_debug_info()
        
        # End UI rendering
        self.ui.end_frame()
        
    def _render_demo_selection(self):
        """Render demo selection UI"""
        self.ui.begin_panel("Demos", 10, 10, 200, 30 + len(self.demos) * 30)
        
        for i, demo_name in enumerate(self.demos.keys()):
            is_selected = demo_name == self.current_demo_name
            if self.ui.button(demo_name, 10, 40 + i * 30, 180, 25, is_selected):
                self.select_demo(demo_name)
                
        self.ui.end_panel()
        
    def _render_debug_info(self):
        """Render debug information"""
        rl.draw_fps(self.width - 100, 10)
        
        # Camera position
        pos_text = f"Pos: ({self.camera.position.x:.1f}, {self.camera.position.y:.1f}, {self.camera.position.z:.1f})"
        rl.draw_text(pos_text, 10, self.height - 30, 20, rl.Color(50, 50, 50, 255))