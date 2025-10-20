#pragma once

#include <vector>
#include "geometry.h"
#include "tgaimage.h"
#include "model.h"

#ifdef USE_RAYLIB
#include <raylib.h>
#endif

// Lighting and material properties
struct RenderMaterial {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
};

struct Light {
    vec3 position;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

// Global lighting setup
extern const RenderMaterial material;
extern const Light light;
extern const vec3 viewPos;

// Shadow mapping structures
struct ShadowMap {
    std::vector<double> depth_buffer;
    int width, height;
    mat<4,4> light_view;
    mat<4,4> light_projection;
    
    ShadowMap(int w, int h) : width(w), height(h) {
        depth_buffer.resize(w * h, -std::numeric_limits<double>::max());
    }
    
    void clear() {
        std::fill(depth_buffer.begin(), depth_buffer.end(), -std::numeric_limits<double>::max());
    }
    
    void set_depth(int x, int y, double depth) {
        if (x >= 0 && x < width && y >= 0 && y < height) {
            depth_buffer[y * width + x] = depth;
        }
    }
    
    double get_depth(int x, int y) const {
        if (x >= 0 && x < width && y >= 0 && y < height) {
            return depth_buffer[y * width + x];
        }
        return -std::numeric_limits<double>::max();
    }
};

// SSAO structures
struct SSAOParams {
    float radius = 2.0f;           // Sample radius (increased for visibility)
    float bias = 0.1f;             // Depth bias (increased)
    int kernel_size = 32;          // Number of sample points (increased)
    float intensity = 2.0f;        // SSAO intensity (increased)
    float power = 1.5f;            // Power for falloff (reduced for more linear effect)
};

struct SSAOData {
    std::vector<vec3> kernel;      // Sample kernel
    std::vector<vec3> noise;       // Random noise texture
    std::vector<double> depth_buffer; // Scene depth buffer
    int width, height;
    
    SSAOData(int w, int h) : width(w), height(h) {
        depth_buffer.resize(w * h, 0.0);
        generate_kernel();
        generate_noise();
    }
    
    void generate_kernel();
    void generate_noise();
    void update_depth_buffer(const std::vector<double>& scene_depth);
    float calculate_occlusion(int x, int y, const SSAOParams& params) const;
};

// Function declarations
vec3 calculate_phong_lighting(const vec3& worldPos, const vec3& normal, const RenderMaterial& mat, const Light& light, const vec3& viewPos);
vec3 calculate_phong_lighting_with_shadows(const vec3& worldPos, const vec3& normal, const RenderMaterial& mat, const Light& light, const vec3& viewPos, const ShadowMap& shadow_map);
vec3 calculate_phong_lighting_fast_shadows(const vec3& worldPos, const vec3& normal, const RenderMaterial& mat, const Light& light, const vec3& viewPos);
void rasterize(const vec4 clip[3], const vec3 worldPos[3], const vec3 normals[3], 
               const vec2 texCoords[3], const vec3 tangents[3], const vec3 bitangents[3],
               const ObjModel& model, std::vector<double> &zbuffer, std::vector<Color> &framebuffer, bool use_normal_mapping = true, bool use_color_texture = false);
void rasterize_simple(const vec4 clip[3], std::vector<double> &zbuffer, std::vector<Color> &framebuffer, const Color color);
void cpu_rasterize_models(const std::vector<ObjModel>& models, std::vector<Color>& framebuffer, 
                         std::vector<double>& zbuffer, const mat<4,4>& Model, 
                         bool smooth_shading = true, bool use_normal_mapping = true, bool use_color_texture = false);
void cpu_rasterize_models_with_shadows(const std::vector<ObjModel>& models, std::vector<Color>& framebuffer, 
                                      std::vector<double>& zbuffer, const mat<4,4>& Model, 
                                      const ShadowMap& shadow_map, bool smooth_shading = true, 
                                      bool use_normal_mapping = true, bool use_color_texture = false);
void cpu_rasterize_models_fast_shadows(const std::vector<ObjModel>& models, std::vector<Color>& framebuffer, 
                                      std::vector<double>& zbuffer, const mat<4,4>& Model, 
                                      bool smooth_shading = true, bool use_normal_mapping = true, bool use_color_texture = false);
void render_shadow_map(const std::vector<ObjModel>& models, ShadowMap& shadow_map, const mat<4,4>& Model);
void rasterize_shadow_depth(const vec4 clip[3], const vec3 worldPos[3], ShadowMap& shadow_map);
void rasterize_with_shadows(const vec4 clip[3], const vec3 worldPos[3], const vec3 normals[3], 
                           const vec2 texCoords[3], const vec3 tangents[3], const vec3 bitangents[3],
                           const ObjModel& model, std::vector<double> &zbuffer, std::vector<Color> &framebuffer, 
                           const ShadowMap& shadow_map, bool use_normal_mapping = true, bool use_color_texture = false);
void rasterize_fast_shadows(const vec4 clip[3], const vec3 worldPos[3], const vec3 normals[3], 
                           const vec2 texCoords[3], const vec3 tangents[3], const vec3 bitangents[3],
                           const ObjModel& model, std::vector<double> &zbuffer, std::vector<Color> &framebuffer, 
                           bool use_normal_mapping = true, bool use_color_texture = false);
std::vector<vec3> calculate_vertex_normals(const ObjModel& model);
void calculate_tangent_space(const ObjModel& model, int face_idx, vec3& tangent, vec3& bitangent);

// SSAO function declarations
void apply_ssao(std::vector<Color>& framebuffer, const std::vector<double>& depth_buffer, 
                const SSAOData& ssao_data, const SSAOParams& params, int width, int height);
void render_with_ssao(const std::vector<ObjModel>& models, std::vector<Color>& framebuffer, 
                      std::vector<double>& zbuffer, const mat<4,4>& Model, 
                      const ShadowMap& shadow_map, const SSAOData& ssao_data, const SSAOParams& ssao_params,
                      bool smooth_shading = true, bool use_normal_mapping = true, bool use_color_texture = false);

// Toon shader function declarations
vec3 calculate_toon_lighting(const vec3& worldPos, const vec3& normal, const RenderMaterial& mat, const Light& light, const vec3& viewPos);
void rasterize_toon(const vec4 clip[3], const vec3 worldPos[3], const vec3 normals[3], 
                   const vec2 texCoords[3], const vec3 tangents[3], const vec3 bitangents[3],
                   const ObjModel& model, std::vector<double> &zbuffer, std::vector<Color> &framebuffer, 
                   bool use_normal_mapping, bool use_color_texture);
void cpu_rasterize_models_toon(const std::vector<ObjModel>& models, std::vector<Color>& framebuffer, 
                               std::vector<double>& zbuffer, const mat<4,4>& Model, 
                               bool smooth_shading = true, bool use_normal_mapping = true, bool use_color_texture = false);
void render_toon_outlines(const std::vector<ObjModel>& models, std::vector<Color>& framebuffer, 
                          std::vector<double>& zbuffer, const mat<4,4>& Model, 
                          bool smooth_shading = true, bool use_normal_mapping = true, bool use_color_texture = false);
