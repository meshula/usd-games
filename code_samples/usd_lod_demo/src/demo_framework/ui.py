"""
UI Module
---------
Provides a simple UI system for the demos built on top of Raylib.
"""

import pyray as rl
from typing import Optional, Tuple, Dict, List, Callable

class UI:
    """Simple UI system for the demos"""
    
    def __init__(self, app):
        self.app = app
        self.font_size = 20
        self.padding = 10
        self.active_panels = []
        self.panel_stack = []
        
        # UI colors
        self.colors = {
            "panel_bg": rl.Color(30, 30, 30, 200),
            "panel_border": rl.Color(100, 100, 100, 255),
            "panel_title": rl.Color(255, 255, 255, 255),
            "button": rl.Color(50, 50, 50, 255),
            "button_hover": rl.Color(70, 70, 70, 255),
            "button_active": rl.Color(100, 100, 255, 255),
            "button_text": rl.Color(255, 255, 255, 255),
            "slider_bg": rl.Color(40, 40, 40, 255),
            "slider_knob": rl.Color(100, 100, 255, 255),
            "slider_active": rl.Color(120, 120, 255, 255),
            "slider_text": rl.Color(255, 255, 255, 255),
            "label": rl.Color(255, 255, 255, 255),
            "dropdown_bg": rl.Color(40, 40, 40, 255),
            "dropdown_border": rl.Color(100, 100, 100, 255),
            "dropdown_item": rl.Color(50, 50, 50, 255),
            "dropdown_item_hover": rl.Color(70, 70, 70, 255),
            "dropdown_text": rl.Color(255, 255, 255, 255),
        }
        
        # Track UI state
        self.hot_item = None
        self.active_item = None
        self.mouse_pos = (0, 0)
        self.was_mouse_pressed = False
        
        # For sliders
        self.dragging_slider = None
        self.slider_values = {}
        
        # For dropdowns
        self.open_dropdown = None
        
    def begin_frame(self):
        """Begin a new UI frame"""
        self.active_panels = []
        self.mouse_pos = (rl.get_mouse_x(), rl.get_mouse_y())
        self.hot_item = None
        
        # Check if mouse was pressed this frame
        mouse_pressed = rl.is_mouse_button_pressed(rl.MOUSE_BUTTON_LEFT)
        if mouse_pressed:
            self.active_item = self.hot_item
            self.was_mouse_pressed = True
        elif rl.is_mouse_button_released(rl.MOUSE_BUTTON_LEFT):
            self.was_mouse_pressed = False
            if self.dragging_slider:
                self.dragging_slider = None
    
    def end_frame(self):
        """End the current UI frame"""
        # If mouse was released, clear active item
        if rl.is_mouse_button_released(rl.MOUSE_BUTTON_LEFT):
            self.active_item = None
    
    def begin_panel(self, title: str, x: int, y: int, width: int, height: int) -> Tuple[int, int, int, int]:
        """
        Begin a UI panel
        
        Args:
            title: Panel title
            x, y: Panel position
            width, height: Panel size
            
        Returns:
            Tuple of (x, y, width, height) for the content area
        """
        # Draw panel background
        rl.draw_rectangle(x, y, width, height, self.colors["panel_bg"])
        rl.draw_rectangle_lines(x, y, width, height, self.colors["panel_border"])
        
        # Draw title
        rl.draw_text(title, x + 10, y + 5, self.font_size, self.colors["panel_title"])
        
        # Calculate content area
        content_x = x + self.padding
        content_y = y + self.padding + self.font_size + 5
        content_width = width - (self.padding * 2)
        content_height = height - (self.padding * 2) - self.font_size - 5
        
        # Add to active panels
        self.active_panels.append((x, y, width, height))
        
        # Push to panel stack
        self.panel_stack.append((content_x, content_y, content_width, content_height))
        
        return content_x, content_y, content_width, content_height
    
    def end_panel(self):
        """End the current UI panel"""
        if self.panel_stack:
            self.panel_stack.pop()
    
    def get_panel_content_area(self) -> Tuple[int, int, int, int]:
        """Get the content area of the current panel"""
        if not self.panel_stack:
            # Default to full screen if no panel
            return 0, 0, self.app.width, self.app.height
            
        return self.panel_stack[-1]
    
    def is_point_in_rect(self, x: int, y: int, rect_x: int, rect_y: int, 
                         rect_width: int, rect_height: int) -> bool:
        """Check if a point is inside a rectangle"""
        return (x >= rect_x and x <= rect_x + rect_width and 
                y >= rect_y and y <= rect_y + rect_height)
    
    def button(self, label: str, x: int, y: int, width: int, height: int, 
              is_selected: bool = False) -> bool:
        """
        Draw a button and return True if clicked
        
        Args:
            label: Button text
            x, y: Button position
            width, height: Button size
            is_selected: Whether the button is in selected state
            
        Returns:
            True if the button was clicked
        """
        # Generate a unique ID for this button
        button_id = f"button_{label}_{x}_{y}"
        
        # Check if mouse is over the button
        mouse_over = self.is_point_in_rect(
            self.mouse_pos[0], self.mouse_pos[1], 
            x, y, width, height
        )
        
        # Set hot item if mouse is over
        if mouse_over:
            self.hot_item = button_id
            
        # Determine button color
        color = self.colors["button"]
        if is_selected:
            color = self.colors["button_active"]
        elif self.hot_item == button_id:
            color = self.colors["button_hover"]
        
        # Draw button
        rl.draw_rectangle(x, y, width, height, color)
        rl.draw_rectangle_lines(x, y, width, height, self.colors["panel_border"])
        
        # Calculate text position to center in button
        text_width = rl.measure_text(label, self.font_size)
        text_x = x + (width - text_width) // 2
        text_y = y + (height - self.font_size) // 2
        
        # Draw text
        rl.draw_text(label, text_x, text_y, self.font_size, self.colors["button_text"])
        
        # Return true if button was clicked
        return mouse_over and rl.is_mouse_button_released(rl.MOUSE_BUTTON_LEFT)
    
    def label(self, text: str, x: int, y: int, font_size: Optional[int] = None):
        """
        Draw a text label
        
        Args:
            text: Label text
            x, y: Label position
            font_size: Optional custom font size
        """
        size = font_size if font_size is not None else self.font_size
        rl.draw_text(text, x, y, size, self.colors["label"])
    
    def slider(self, id_str: str, x: int, y: int, width: int, height: int, 
              min_val: float, max_val: float, default_val: Optional[float] = None) -> float:
        """
        Draw a slider and return its value
        
        Args:
            id_str: Unique identifier for this slider
            x, y: Slider position
            width, height: Slider size
            min_val, max_val: Value range
            default_val: Optional default value
            
        Returns:
            Current slider value
        """
        # Initialize slider value if not exists
        if id_str not in self.slider_values:
            self.slider_values[id_str] = default_val if default_val is not None else min_val
        
        # Get current value
        value = self.slider_values[id_str]
        
        # Calculate slider parameters
        slider_id = f"slider_{id_str}"
        normalized_value = (value - min_val) / (max_val - min_val)
        knob_pos = int(x + normalized_value * width)
        knob_radius = height
        
        # Check if mouse is over the knob
        mouse_over_knob = self.is_point_in_rect(
            self.mouse_pos[0], self.mouse_pos[1],
            knob_pos - knob_radius, y - knob_radius // 2,
            knob_radius * 2, knob_radius * 2
        )
        
        # Set hot item if mouse is over
        if mouse_over_knob:
            self.hot_item = slider_id
        
        # Update drag state
        if self.active_item == slider_id and rl.is_mouse_button_down(rl.MOUSE_BUTTON_LEFT):
            self.dragging_slider = slider_id
            
        # Update value if dragging
        if self.dragging_slider == slider_id:
            mouse_x = self.mouse_pos[0]
            normalized_pos = max(0, min(1, (mouse_x - x) / width))
            self.slider_values[id_str] = min_val + normalized_pos * (max_val - min_val)
        
        # Draw slider background
        rl.draw_rectangle(x, y, width, height, self.colors["slider_bg"])
        
        # Draw slider progress
        rl.draw_rectangle(x, y, int(normalized_value * width), height, 
                        self.colors["slider_active" if self.dragging_slider == slider_id else "slider_knob"])
        
        # Draw knob
        knob_color = self.colors["slider_active"] if self.dragging_slider == slider_id else self.colors["slider_knob"]
        rl.draw_circle(knob_pos, y + height // 2, knob_radius, knob_color)
        
        return self.slider_values[id_str]
    
    def slider(self, id_str: str, x: int, y: int, width: int, height: int, 
              min_val: float, max_val: float, default_val: Optional[float] = None) -> float:
        """
        Draw a slider and return its value
        
        Args:
            id_str: Unique identifier for this slider
            x, y: Slider position
            width, height: Slider size
            min_val, max_val: Value range
            default_val: Optional default value
            
        Returns:
            Current slider value
        """
        # Initialize slider value if not exists
        if id_str not in self.slider_values:
            self.slider_values[id_str] = default_val if default_val is not None else min_val
        
        # Get current value
        value = self.slider_values[id_str]
        
        # Calculate slider parameters
        slider_id = f"slider_{id_str}"
        normalized_value = (value - min_val) / (max_val - min_val)
        knob_pos = int(x + normalized_value * width)
        knob_radius = height
        
        # Check if mouse is over the knob
        mouse_over_knob = self.is_point_in_rect(
            self.mouse_pos[0], self.mouse_pos[1],
            knob_pos - knob_radius, y - knob_radius // 2,
            knob_radius * 2, knob_radius * 2
        )
        
        # Set hot item if mouse is over
        if mouse_over_knob:
            self.hot_item = slider_id
        
        # Update drag state
        if self.active_item == slider_id and rl.is_mouse_button_down(rl.MOUSE_BUTTON_LEFT):
            self.dragging_slider = slider_id
            
        # Update value if dragging
        if self.dragging_slider == slider_id:
            mouse_x = self.mouse_pos[0]
            normalized_pos = max(0, min(1, (mouse_x - x) / width))
            self.slider_values[id_str] = min_val + normalized_pos * (max_val - min_val)
        
        # Draw slider background
        rl.draw_rectangle(x, y, width, height, self.colors["slider_bg"])
        
        # Draw slider progress
        rl.draw_rectangle(x, y, int(normalized_value * width), height, 
                        self.colors["slider_active" if self.dragging_slider == slider_id else "slider_knob"])
        
        # Draw knob
        knob_color = self.colors["slider_active"] if self.dragging_slider == slider_id else self.colors["slider_knob"]
        rl.draw_circle(knob_pos, y + height // 2, knob_radius, knob_color)
        
        return self.slider_values[id_str]
    
    def slider_with_label(self, label: str, id_str: str, x: int, y: int, width: int, 
                         min_val: float, max_val: float, default_val: Optional[float] = None, 
                         format_str: str = "{:.1f}") -> float:
        """
        Draw a slider with a label and value display
        
        Args:
            label: Label text
            id_str: Unique identifier for this slider
            x, y: Position
            width: Width of the entire widget
            min_val, max_val: Value range
            default_val: Optional default value
            format_str: Format string for the value display
            
        Returns:
            Current slider value
        """
        # Draw label
        self.label(label, x, y)
        y += self.font_size + 5
        
        # Calculate slider dimensions
        slider_width = width - 60  # Reserve space for value display
        slider_height = 20
        
        # Draw slider
        value = self.slider(id_str, x, y, slider_width, slider_height, min_val, max_val, default_val)
        
        # Draw value
        value_text = format_str.format(value)
        rl.draw_text(value_text, x + slider_width + 10, y, self.font_size, self.colors["slider_text"])
        
        return value
    
    def dropdown(self, id_str: str, x: int, y: int, width: int, height: int, 
                options: List[str], selected_index: int = 0) -> int:
        """
        Draw a dropdown and return the selected index
        
        Args:
            id_str: Unique identifier for this dropdown
            x, y: Position
            width, height: Size
            options: List of dropdown options
            selected_index: Initially selected index
            
        Returns:
            Currently selected index
        """
        dropdown_id = f"dropdown_{id_str}"
        
        # Store selected index
        if not hasattr(self, 'dropdown_selections'):
            self.dropdown_selections = {}
        
        if dropdown_id not in self.dropdown_selections:
            self.dropdown_selections[dropdown_id] = selected_index
        
        # Get current selection
        current_selection = self.dropdown_selections[dropdown_id]
        selected_text = options[current_selection] if options else "No options"
        
        # Check if mouse is over the dropdown
        mouse_over = self.is_point_in_rect(
            self.mouse_pos[0], self.mouse_pos[1],
            x, y, width, height
        )
        
        # Set hot item if mouse is over
        if mouse_over:
            self.hot_item = dropdown_id
        
        # Draw dropdown box
        rl.draw_rectangle(x, y, width, height, self.colors["dropdown_bg"])
        rl.draw_rectangle_lines(x, y, width, height, self.colors["dropdown_border"])
        
        # Draw selected text
        text_y = y + (height - self.font_size) // 2
        rl.draw_text(selected_text, x + 10, text_y, self.font_size, self.colors["dropdown_text"])
        
        # Draw dropdown arrow - PyRay uses Vector2 differently
        arrow_x = x + width - 20
        arrow_y = y + height // 2
        
        # Create Vector2 points for triangle
        v1 = rl.Vector2(arrow_x, arrow_y - 5)
        v2 = rl.Vector2(arrow_x + 10, arrow_y - 5)
        v3 = rl.Vector2(arrow_x + 5, arrow_y + 5)
        
        # Draw triangle
        rl.draw_triangle(v1, v2, v3, self.colors["dropdown_text"])
        
        # Handle dropdown opening/closing
        if mouse_over and rl.is_mouse_button_released(rl.MOUSE_BUTTON_LEFT):
            if self.open_dropdown == dropdown_id:
                self.open_dropdown = None
            else:
                self.open_dropdown = dropdown_id
        
        # Draw dropdown list if open
        if self.open_dropdown == dropdown_id and options:
            dropdown_height = len(options) * (self.font_size + 10)
            rl.draw_rectangle(x, y + height, width, dropdown_height, self.colors["dropdown_bg"])
            rl.draw_rectangle_lines(x, y + height, width, dropdown_height, self.colors["dropdown_border"])
            
            # Draw options
            for i, option in enumerate(options):
                option_y = y + height + i * (self.font_size + 10)
                option_rect = (x, option_y, width, self.font_size + 10)
                
                # Check if mouse is over this option
                mouse_over_option = self.is_point_in_rect(
                    self.mouse_pos[0], self.mouse_pos[1],
                    x, option_y, width, self.font_size + 10
                )
                
                # Draw option background
                if mouse_over_option:
                    rl.draw_rectangle(x, option_y, width, self.font_size + 10, self.colors["dropdown_item_hover"])
                elif i == current_selection:
                    rl.draw_rectangle(x, option_y, width, self.font_size + 10, self.colors["dropdown_item"])
                
                # Draw option text
                rl.draw_text(option, x + 10, option_y + 5, self.font_size, self.colors["dropdown_text"])
                
                # Handle option selection
                if mouse_over_option and rl.is_mouse_button_released(rl.MOUSE_BUTTON_LEFT):
                    self.dropdown_selections[dropdown_id] = i
                    self.open_dropdown = None
        
        return self.dropdown_selections[dropdown_id]
    
    def dropdown_with_label(self, label: str, id_str: str, x: int, y: int, width: int, 
                           options: List[str], selected_index: int = 0) -> int:
        """
        Draw a dropdown with a label
        
        Args:
            label: Label text
            id_str: Unique identifier for this dropdown
            x, y: Position
            width: Width of the entire widget
            options: List of dropdown options
            selected_index: Initially selected index
            
        Returns:
            Currently selected index
        """
        # Draw label
        self.label(label, x, y)
        y += self.font_size + 5
        
        # Draw dropdown
        return self.dropdown(id_str, x, y, width, 30, options, selected_index)
    
    def checkbox(self, label: str, id_str: str, x: int, y: int, checked: bool = False) -> bool:
        """
        Draw a checkbox and return its state
        
        Args:
            label: Checkbox label
            id_str: Unique identifier for this checkbox
            x, y: Position
            checked: Initial state
            
        Returns:
            Current checkbox state (checked or not)
        """
        checkbox_id = f"checkbox_{id_str}"
        
        # Store checkbox state
        if not hasattr(self, 'checkbox_states'):
            self.checkbox_states = {}
        
        if checkbox_id not in self.checkbox_states:
            self.checkbox_states[checkbox_id] = checked
        
        # Get current state
        is_checked = self.checkbox_states[checkbox_id]
        
        # Calculate dimensions
        box_size = self.font_size
        
        # Check if mouse is over the checkbox
        mouse_over = self.is_point_in_rect(
            self.mouse_pos[0], self.mouse_pos[1],
            x, y, box_size + self.font_size * len(label), box_size
        )
        
        # Set hot item if mouse is over
        if mouse_over:
            self.hot_item = checkbox_id
        
        # Handle click
        if mouse_over and rl.is_mouse_button_released(rl.MOUSE_BUTTON_LEFT):
            self.checkbox_states[checkbox_id] = not is_checked
            is_checked = self.checkbox_states[checkbox_id]
        
        # Draw checkbox
        if is_checked:
            rl.draw_rectangle(x, y, box_size, box_size, self.colors["button_active"])
        else:
            rl.draw_rectangle(x, y, box_size, box_size, self.colors["button"])
        
        rl.draw_rectangle_lines(x, y, box_size, box_size, self.colors["panel_border"])
        
        # Draw checkmark if checked
        if is_checked:
            # Simple checkmark
            rl.draw_line(x + 3, y + box_size // 2, 
                        x + box_size // 2 - 2, y + box_size - 3, 
                        self.colors["button_text"])
            rl.draw_line(x + box_size // 2 - 2, y + box_size - 3, 
                        x + box_size - 3, y + 3, 
                        self.colors["button_text"])
        
        # Draw label
        rl.draw_text(label, x + box_size + 5, y + (box_size - self.font_size) // 2, 
                   self.font_size, self.colors["label"])
        
        return is_checked
    
    def update(self, delta_time: float):
        """Update UI state"""
        pass  # All updates are handled in begin_frame and individual widgets