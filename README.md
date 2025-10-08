# Software Renderer

A CPU-based 3D software renderer implemented in C++ with Phong lighting and interactive model viewing.

![alt text](<Screenshot 2025-10-08 180450.png>)

![alt text](<Screenshot2 2025-10-08 180513.png>)

## Features

This software renderer implements CPU-based 3D rasterization with z-buffer depth testing and supports two rendering modes: Phong lighting with realistic ambient, diffuse, and specular reflections, and simple colored triangles for performance comparison. The renderer includes interactive model rotation controls, real-time performance timing measurement, and on-screen display of render statistics and current rendering mode.

## Requirements

- CMake 3.20+
- C++17 compatible compiler
- Raylib (automatically fetched via CMake)

## Building

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Debug
```

## Usage

```bash
./sw_renderer.exe path/to/model.obj
```

### Controls

- **Arrow Keys**: Rotate the model
- **Space**: Switch between Phong lighting and colored triangles
- **Close Window**: Exit the application

### On-Screen Display

- Render time in milliseconds
- Current rotation angles
- Active rendering mode
- Control instructions

## Project Structure

```
include/
├── geometry.h      # Vector and matrix math
├── model.h         # 3D model loading
├── rasterizer.h    # Rendering functions
├── tgaimage.h      # Image handling
└── viewer.h        # Window management

source/
├── main.cpp        # Application logic
├── model.cpp       # Model implementation
├── rasterizer.cpp  # Rendering implementation
├── tgaimage.cpp    # Image implementation
└── viewer.cpp      # Window implementation
```

## Rendering Modes

1. **Phong Lighting**: Full Phong reflection model with ambient, diffuse, and specular lighting
2. **Colored Triangles**: Simple HSV-based colored triangles without lighting calculations

## Performance

The renderer measures and displays frame timing to compare performance between different rendering modes. Phong lighting typically shows higher render times due to per-pixel lighting calculations.
