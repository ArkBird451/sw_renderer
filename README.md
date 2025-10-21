# Software Renderer

A CPU-based 3D software renderer implemented in C++ with multiple rendering modes including Phong lighting, toon shading, and interactive model viewing.

# Sample running

![alt text](<Screenshot 2025-10-21 154611.png>)

![alt text](<Screenshot 2025-10-21 1546443.png>)

![alt text](<Screenshot 2025-10-21 1547145.png>)

![alt text](<Screenshot 2025-10-21 1551516.png>)

![alt text](<Screenshot 2025-10-21 1553316.png>)

## Features

This software renderer implements CPU-based 3D rasterization with z-buffer depth testing and supports multiple rendering modes: Phong lighting with realistic ambient, diffuse, and specular reflections, toon shading with quantized lighting and silhouette outlines, and simple colored triangles for performance comparison. The renderer includes interactive model rotation controls, real-time performance timing measurement, and on-screen display of render statistics and current rendering mode.

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
./sw_renderer.exe path/to/model.obj [normal_map.tga] [color_texture.tga]
```

### Controls

- **Arrow Keys**: Rotate the model
- **Space**: Cycle through rendering modes (Phong → Colored → Toon)
- **S**: Cycle through shading modes (Flat → Smooth → Normal Mapping → Color Texture → Normal + Color)
- **H**: Toggle shadow mapping (with SSAO)
- **Close Window**: Exit the application

### On-Screen Display

- Render time in milliseconds
- Current rotation angles
- Active rendering mode and shading type
- Shadow mapping status
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

1. **Phong Lighting**: Full Phong reflection model with ambient, diffuse, and specular lighting, shadow mapping, and SSAO
2. **Toon Shader**: Cartoon-style rendering with quantized lighting levels and silhouette outlines
3. **Colored Triangles**: Simple HSV-based colored triangles without lighting calculations

## Performance

The renderer measures and displays frame timing to compare performance between different rendering modes. Phong lighting with shadow mapping typically shows higher render times due to per-pixel lighting calculations and shadow map generation.
