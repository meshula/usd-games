# PyRay Implementation Notes

This project uses PyRay instead of Raylib-py for rendering. PyRay is a more actively maintained Python binding for the Raylib library.

## Key Differences

When working with PyRay instead of Raylib-py, be aware of these key differences:

1. **Import Method**: Use `import pyray as rl` instead of `import raylib as rl`

2. **Color Creation**: PyRay uses `rl.Color(r, g, b, a)` instead of `Color(r, g, b, a)` 

3. **Vector Creation**: PyRay requires creating empty vectors first, then setting values:
   ```python
   # PyRay approach
   vec = rl.Vector3()
   vec.x = 1.0
   vec.y = 2.0
   vec.z = 3.0
   
   # Or directly with constructor
   vec = rl.Vector3(1.0, 2.0, 3.0)
   ```

4. **Camera Creation**: PyRay requires a different approach for camera initialization:
   ```python
   # Create camera with default values
   camera = rl.Camera3D()
   camera.position = rl.Vector3(10.0, 5.0, 10.0)
   camera.target = rl.Vector3(0.0, 0.0, 0.0)
   camera.up = rl.Vector3(0.0, 1.0, 0.0)
   camera.fovy = 60.0
   camera.projection = rl.CAMERA_PERSPECTIVE
   ```

5. **Drawing Functions**: Some drawing functions have slightly different parameter orders or requirements:
   - Cylinder drawing: `rl.draw_cylinder(start_pos, end_pos, radius, slices, color)`
   - Triangle drawing: Create Vector2 points first, then pass to draw_triangle

6. **Structures**: Structures like BoundingBox need to be created and then have their fields set:
   ```python
   bounds = rl.BoundingBox()
   bounds.min = rl.Vector3(min_x, min_y, min_z)
   bounds.max = rl.Vector3(max_x, max_y, max_z)
   ```

## Helpful Tips

1. PyRay closely follows the C API of Raylib, so the [official Raylib documentation](https://www.raylib.com/cheatsheet/cheatsheet.html) is a helpful reference.

2. When in doubt about a function's parameters, you can use Python's built-in help:
   ```python
   import pyray as rl
   help(rl.draw_cylinder)
   ```

3. For structures and objects, examine their attributes:
   ```python
   import pyray as rl
   camera = rl.Camera3D()
   dir(camera)  # Shows all attributes and methods
   ```