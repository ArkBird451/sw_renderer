#include <limits>
#include <algorithm>
#include <chrono>
#include <iostream>
#include "geometry.h"
#include "model.h"
#include "tgaimage.h"
#include "viewer.h"
#include "rasterizer.h"

#ifdef USE_RAYLIB
#include <raylib.h>
#endif

mat<4,4> ModelView, Viewport, Perspective;

// Rendering modes
enum RenderingMode {
    PHONG_LIGHTING,
    COLORED_TRIANGLES,
    TOON_SHADER
};

enum ShadingMode {
    FLAT_SHADING,
    SMOOTH_SHADING,
    NORMAL_MAPPING,
    COLOR_TEXTURE,
    NORMAL_AND_COLOR
};

RenderingMode current_mode = PHONG_LIGHTING;
ShadingMode current_shading = SMOOTH_SHADING;
bool use_shadow_mapping = true;  // Enabled by default
bool use_fast_shadows = true;  // Use simplified shadow calculation
ShadowMap shadow_map(512, 512);  // Shadow map resolution (width, height)
SSAOData ssao_data(800, 800);  // SSAO data for screen resolution (integrated with shadow mapping)
SSAOParams ssao_params;  // SSAO parameters
double last_angleX = -999, last_angleY = -999;  // Track rotation changes


void lookat(const vec3 eye, const vec3 center, const vec3 up) {
    vec3 z = normalized(eye - center);          // forward (camera space +Z points backward)
    vec3 x = normalized(cross(up, z));          // right
    vec3 y = cross(z, x);                       // true up

    mat<4,4> rotation = {{{x.x, x.y, x.z, 0}, {y.x, y.y, y.z, 0}, {z.x, z.y, z.z, 0}, {0, 0, 0, 1}}};
    mat<4,4> translation = {{{1, 0, 0, -eye.x}, {0, 1, 0, -eye.y}, {0, 0, 1, -eye.z}, {0, 0, 0, 1}}};
    ModelView = rotation * translation;
}

void perspective_fov(const double fov_degrees) {
    constexpr double Pi = 3.14159265358979323846;
    const double f = 1.0 / std::tan((fov_degrees * Pi / 180.0) * 0.5);
    Perspective = {{{1,0,0,0}, {0,1,0,0}, {0,0,1,0}, {0,0,-1.0/f,1}}};
}

void viewport(const int x, const int y, const int w, const int h) {
    Viewport = {{{w/2., 0, 0, x+w/2.}, {0, h/2., 0, y+h/2.}, {0,0,1,0}, {0,0,0,1}}};
}

Color hsv_to_rgb(double hue, double saturation = 1.0, double value = 1.0) {
    // Normalize hue to [0, 360)
    hue = fmod(hue, 360.0);
    if (hue < 0) hue += 360.0;
    
    // HSV to RGB conversion
    double h = hue / 60.0;
    int sector = (int)h;
    double f = h - sector;
    double p = 0.0, q = 1.0 - f, t = f;
    
    double r, g, b;
    switch (sector % 6) {
        case 0: r = 1.0; g = t; b = 0.0; break;
        case 1: r = q; g = 1.0; b = 0.0; break;
        case 2: r = 0.0; g = 1.0; b = t; break;
        case 3: r = 0.0; g = q; b = 1.0; break;
        case 4: r = t; g = 0.0; b = 1.0; break;
        case 5: r = 1.0; g = 0.0; b = q; break;
        default: r = 1.0; g = 0.0; b = 0.0; break;
    }
    
    // Apply saturation and value
    r = r * saturation * value;
    g = g * saturation * value;
    b = b * saturation * value;
    
    Color color;
    color.r = (unsigned char)(r * 255);  // Red
    color.g = (unsigned char)(g * 255);  // Green  
    color.b = (unsigned char)(b * 255);  // Blue
    color.a = 255;
    
    return color;
}


void cpu_rasterize_colored_triangles(const std::vector<ObjModel>& models, std::vector<Color>& framebuffer, 
                                    std::vector<double>& zbuffer, const mat<4,4>& Model) {
    // -- CPU rasterization with simple colored triangles
    for (const auto &model : models) {
        for (int i=0; i<model.nfaces(); i++) {
            vec4 clip[3];
            
            for (int d : {0,1,2}) {
                vec3 v = model.vert(i, d);
                clip[d] = Perspective * ModelView * Model * vec4{v.x, v.y, v.z, 1.};
            }
            
            // Use simple HSV color cycling for each triangle
            double hue = (i * 0.618033988749895) * 360.0; // Golden ratio for good distribution
            Color triangle_color = hsv_to_rgb(hue);
            
            // Simple rasterization without lighting
            rasterize_simple(clip, zbuffer, framebuffer, triangle_color);
        }
    }
}

void render_frame(const std::vector<ObjModel>& models, std::vector<Color>& framebuffer, std::vector<double>& zbuffer, 
                 double angleX, double angleY, double& render_time_ms) {
    // Start timing
    auto start_time = std::chrono::high_resolution_clock::now();
    
    const int width = 800;  // Fixed width
    const int height = 800;  // Fixed height
    
    // -- Clear framebuffer and z-buffer FIRST to eliminate ghosting
    std::fill(zbuffer.begin(), zbuffer.end(), -std::numeric_limits<double>::max());
    std::fill(framebuffer.begin(), framebuffer.end(), Color{30, 30, 30, 255});
    
    // -- Update rotation from input, then build model rotation matrices (Y then X)
    const double cy = std::cos(angleY), sy = std::sin(angleY);
    const double cx = std::cos(angleX), sx = std::sin(angleX);
    mat<4,4> RotY = {{{ cy, 0, sy, 0}, {0, 1, 0, 0}, {-sy, 0, cy, 0}, {0, 0, 0, 1}}};
    mat<4,4> RotX = {{{ 1, 0, 0, 0}, {0, cx, -sx, 0}, {0, sx, cx, 0}, {0, 0, 0, 1}}};
    mat<4,4> Model = RotY * RotX;

    // -- Setup shadow mapping if enabled (only regenerate when rotation changes)
    if (use_shadow_mapping && current_mode == PHONG_LIGHTING) {
        // Check if rotation has changed significantly
        bool rotation_changed = (std::abs(angleX - last_angleX) > 0.01 || std::abs(angleY - last_angleY) > 0.01);
        
        if (rotation_changed) {
            // Setup light view and projection matrices
            vec3 light_pos = light.position;
            vec3 light_target = {0, 0, 0};
            vec3 light_up = {0, 1, 0};
            
            // Create light view matrix (look at from light position)
            vec3 z = normalized(light_pos - light_target);
            vec3 x = normalized(cross(light_up, z));
            vec3 y = cross(z, x);
            shadow_map.light_view = {{{x.x, x.y, x.z, -dot(x, light_pos)},
                                     {y.x, y.y, y.z, -dot(y, light_pos)},
                                     {z.x, z.y, z.z, -dot(z, light_pos)},
                                     {0, 0, 0, 1}}};
            
            // Create light projection matrix (orthographic for directional light)
            double near_plane = 0.1;
            double far_plane = 50.0;
            double left = -10.0, right = 10.0;
            double bottom = -10.0, top = 10.0;
            shadow_map.light_projection = {{{2.0/(right-left), 0, 0, -(right+left)/(right-left)},
                                           {0, 2.0/(top-bottom), 0, -(top+bottom)/(top-bottom)},
                                           {0, 0, -2.0/(far_plane-near_plane), -(far_plane+near_plane)/(far_plane-near_plane)},
                                           {0, 0, 0, 1}}};
            
            // Render shadow map
            render_shadow_map(models, shadow_map, Model);
            
            // Update last rotation angles
            last_angleX = angleX;
            last_angleY = angleY;
        }
    }
    
    // -- CPU rasterization of all loaded models
    if (current_mode == PHONG_LIGHTING) {
        bool use_smooth_shading = (current_shading == SMOOTH_SHADING || current_shading == NORMAL_MAPPING || current_shading == COLOR_TEXTURE || current_shading == NORMAL_AND_COLOR);
        bool use_normal_mapping = (current_shading == NORMAL_MAPPING || current_shading == NORMAL_AND_COLOR);
        bool use_color_texture = (current_shading == COLOR_TEXTURE || current_shading == NORMAL_AND_COLOR);
        
        if (use_shadow_mapping) {
            // Use shadow mapping with integrated SSAO
            render_with_ssao(models, framebuffer, zbuffer, Model, shadow_map, ssao_data, ssao_params, use_smooth_shading, use_normal_mapping, use_color_texture);
        } else {
            // Use regular rendering without shadows
            cpu_rasterize_models(models, framebuffer, zbuffer, Model, use_smooth_shading, use_normal_mapping, use_color_texture);
        }
    } else if (current_mode == TOON_SHADER) {
        // Render with toon shader including outlines
        bool use_smooth_shading = (current_shading == SMOOTH_SHADING || current_shading == NORMAL_MAPPING || current_shading == COLOR_TEXTURE || current_shading == NORMAL_AND_COLOR);
        bool use_normal_mapping = (current_shading == NORMAL_MAPPING || current_shading == NORMAL_AND_COLOR);
        bool use_color_texture = (current_shading == COLOR_TEXTURE || current_shading == NORMAL_AND_COLOR);
        render_toon_outlines(models, framebuffer, zbuffer, Model, use_smooth_shading, use_normal_mapping, use_color_texture);
    } else {
        cpu_rasterize_colored_triangles(models, framebuffer, zbuffer, Model);
    }

    // Present with timing information
    const char* mode_name;
    switch (current_mode) {
        case PHONG_LIGHTING: mode_name = "Phong Lighting"; break;
        case COLORED_TRIANGLES: mode_name = "Colored Triangles"; break;
        case TOON_SHADER: mode_name = "Toon Shader"; break;
        default: mode_name = "Unknown"; break;
    }
    const char* shading_name;
    switch (current_shading) {
        case FLAT_SHADING: shading_name = "Flat"; break;
        case SMOOTH_SHADING: shading_name = "Smooth"; break;
        case NORMAL_MAPPING: shading_name = "Normal Mapping"; break;
        case COLOR_TEXTURE: shading_name = "Color Texture"; break;
        case NORMAL_AND_COLOR: shading_name = "Normal + Color"; break;
    }
    const char* normal_mapping_status = (current_shading == NORMAL_MAPPING || current_shading == NORMAL_AND_COLOR) ? "ON" : "OFF";
    const char* shadow_status = use_shadow_mapping ? "ON" : "OFF";
    
    // Direct Raylib rendering (included in timing)
    BeginDrawing();
    ClearBackground(BLACK);
    
    // Convert framebuffer to RGBA format for efficient texture upload (with Y-flip)
    static std::vector<unsigned char> rgba_buffer(width * height * 4);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int src_index = y * width + x;
            int dst_index = (height - 1 - y) * width + x;  // Flip Y coordinate
            rgba_buffer[dst_index * 4 + 0] = framebuffer[src_index].r;     // Red
            rgba_buffer[dst_index * 4 + 1] = framebuffer[src_index].g;     // Green
            rgba_buffer[dst_index * 4 + 2] = framebuffer[src_index].b;     // Blue
            rgba_buffer[dst_index * 4 + 3] = framebuffer[src_index].a;     // Alpha
        }
    }
    
    // Create and update texture efficiently
    static Texture2D framebuffer_texture = {0};
    if (framebuffer_texture.id == 0) {
        Image img = {0};
        img.data = rgba_buffer.data();
        img.width = width;
        img.height = height;
        img.mipmaps = 1;
        img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
        framebuffer_texture = LoadTextureFromImage(img);
    } else {
        UpdateTexture(framebuffer_texture, rgba_buffer.data());
    }
    
    // Draw the entire framebuffer as a single texture (much faster)
    DrawTexture(framebuffer_texture, 0, 0, WHITE);
    
    // Display timing information
    char timing_text[256];
    snprintf(timing_text, sizeof(timing_text), "Render Time: %.2f ms", render_time_ms);
    DrawText(timing_text, 10, 10, 20, GREEN);
    
    char angle_text[256];
    snprintf(angle_text, sizeof(angle_text), "Angle X: %.2f, Y: %.2f", angleX, angleY);
    DrawText(angle_text, 10, 35, 18, YELLOW);
    
    char mode_text[256];
    snprintf(mode_text, sizeof(mode_text), "Mode: %s", mode_name);
    DrawText(mode_text, 10, 58, 18, BLUE);
    
    char shading_text[256];
    snprintf(shading_text, sizeof(shading_text), "Shading: %s", shading_name);
    DrawText(shading_text, 10, 81, 18, PURPLE);
    
    char normal_text[256];
    snprintf(normal_text, sizeof(normal_text), "Normal Mapping: %s", normal_mapping_status);
    DrawText(normal_text, 10, 104, 18, ORANGE);
    
    char shadow_text[256];
    snprintf(shadow_text, sizeof(shadow_text), "Shadow Mapping: %s", shadow_status);
    DrawText(shadow_text, 10, 127, 18, MAGENTA);
    
    DrawText("Arrow keys: rotate | Space: mode (Phong/Colored/Toon) | S: cycle shading | H: toggle shadows", 10, 150, 16, WHITE);
    
    EndDrawing();
    
    // End timing (after all display operations)
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    render_time_ms = duration.count() / 1000.0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " obj/model.obj [normal_map.tga] [color_texture.tga]" << std::endl;
        return 1;
    }

    constexpr int width  = 800;    // output image size
    constexpr int height = 800;
    constexpr vec3    eye{-1,0,2}; // camera position
    constexpr vec3 center{0,0,0};  // camera look-at target
    constexpr vec3     up{0,1,0};  // camera up vector

    lookat(eye, center, up);                              // build the ModelView   matrix
    perspective_fov(60.0);                                // build the Perspective matrix (FOV-based)
    viewport(width/16, height/16, width*7/8, height*7/8); // build the Viewport    matrix

    // Load models once
    std::vector<ObjModel> models;
    models.reserve(argc-1);
    if (argc >= 4) {
        // Load model with normal map and color texture
        models.emplace_back(argv[1], argv[2], argv[3]);
        std::cout << "Loading model with normal map and color texture: " << argv[1] << " + " << argv[2] << " + " << argv[3] << std::endl;
    } else if (argc >= 3) {
        // Load model with normal map
        models.emplace_back(argv[1], argv[2]);
        std::cout << "Loading model with normal map: " << argv[1] << " + " << argv[2] << std::endl;
    } else {
        // Load model without textures
        models.emplace_back(argv[1]);
        std::cout << "Loading model without textures: " << argv[1] << std::endl;
    }
    
    // Check if any model has normal mapping
    bool any_model_has_normal = false;
    for (const auto& model : models) {
        if (model.has_normal()) {
            any_model_has_normal = true;
            break;
        }
    }

    // Initialize viewer
    if (!viewer_init(width, height, "sw_renderer - interactive")) {
        std::cerr << "Viewer not available. Rebuild with USE_RAYLIB enabled." << std::endl;
        return 1;
    }

    // Direct Raylib pixel buffer
    std::vector<Color> framebuffer(width * height, BLACK);
    std::vector<double> zbuffer(width*height, -std::numeric_limits<double>::max());

    double angleY = 0.0;
    double angleX = 0.0;
    
    // Timing variables
    double render_time_ms = 0.0;
    
    // ==== Main render loop ====
    while (!viewer_should_close()) {
        const double dt = 1.0/60.0; // viewer is vsynced to 60 FPS; keys sampled each loop
        const double speed = 1.5; // radians/sec
        
        bool rotation_occurred = false;
        if (viewer_key_down(ViewerKey_Right)) { angleY += speed*dt; rotation_occurred = true; }
        if (viewer_key_down(ViewerKey_Left))  { angleY -= speed*dt; rotation_occurred = true; }
        if (viewer_key_down(ViewerKey_Up))    { angleX += speed*dt; rotation_occurred = true; }
        if (viewer_key_down(ViewerKey_Down))  { angleX -= speed*dt; rotation_occurred = true; }
        
        // Check for mode switching (Space key)
        static bool space_pressed = false;
        if (viewer_key_down(ViewerKey_Space)) {
            if (!space_pressed) {
                current_mode = (RenderingMode)((current_mode + 1) % 3); // Cycle through all 3 modes
                space_pressed = true;
            }
        } else {
            space_pressed = false;
        }
        
        // Check for shading mode cycling (S key)
        static bool s_pressed = false;
        if (viewer_key_down(ViewerKey_S)) {
            if (!s_pressed) {
                current_shading = (ShadingMode)((current_shading + 1) % 5);
                s_pressed = true;
            }
        } else {
            s_pressed = false;
        }
        
        // Check for shadow mapping toggle (H key) - now includes SSAO
        static bool h_pressed = false;
        if (viewer_key_down(ViewerKey_H)) {
            if (!h_pressed) {
                use_shadow_mapping = !use_shadow_mapping;
                h_pressed = true;
            }
        } else {
            h_pressed = false;
        }

        render_frame(models, framebuffer, zbuffer, angleX, angleY, render_time_ms);
    }
    viewer_shutdown();
    return 0;
    
}
