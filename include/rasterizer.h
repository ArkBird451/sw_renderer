#pragma once

#include <vector>
#include "geometry.h"
#include "tgaimage.h"
#include "model.h"

// Lighting and material properties
struct Material {
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
extern const Material material;
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

// Function declarations
vec3 calculate_phong_lighting(const vec3& worldPos, const vec3& normal, const Material& mat, const Light& light, const vec3& viewPos);
vec3 calculate_phong_lighting_with_shadows(const vec3& worldPos, const vec3& normal, const Material& mat, const Light& light, const vec3& viewPos, const ShadowMap& shadow_map);
vec3 calculate_phong_lighting_fast_shadows(const vec3& worldPos, const vec3& normal, const Material& mat, const Light& light, const vec3& viewPos);
void rasterize(const vec4 clip[3], const vec3 worldPos[3], const vec3 normals[3], 
               const vec2 texCoords[3], const vec3 tangents[3], const vec3 bitangents[3],
               const Model& model, std::vector<double> &zbuffer, TGAImage &framebuffer, bool use_normal_mapping = true, bool use_color_texture = false);
void rasterize_simple(const vec4 clip[3], std::vector<double> &zbuffer, TGAImage &framebuffer, const TGAColor color);
void cpu_rasterize_models(const std::vector<Model>& models, TGAImage& framebuffer, 
                         std::vector<double>& zbuffer, const mat<4,4>& Model, 
                         bool smooth_shading = true, bool use_normal_mapping = true, bool use_color_texture = false);
void cpu_rasterize_models_with_shadows(const std::vector<Model>& models, TGAImage& framebuffer, 
                                      std::vector<double>& zbuffer, const mat<4,4>& Model, 
                                      const ShadowMap& shadow_map, bool smooth_shading = true, 
                                      bool use_normal_mapping = true, bool use_color_texture = false);
void cpu_rasterize_models_fast_shadows(const std::vector<Model>& models, TGAImage& framebuffer, 
                                      std::vector<double>& zbuffer, const mat<4,4>& Model, 
                                      bool smooth_shading = true, bool use_normal_mapping = true, bool use_color_texture = false);
void render_shadow_map(const std::vector<Model>& models, ShadowMap& shadow_map, const mat<4,4>& Model);
void rasterize_shadow_depth(const vec4 clip[3], const vec3 worldPos[3], ShadowMap& shadow_map);
void rasterize_with_shadows(const vec4 clip[3], const vec3 worldPos[3], const vec3 normals[3], 
                           const vec2 texCoords[3], const vec3 tangents[3], const vec3 bitangents[3],
                           const Model& model, std::vector<double> &zbuffer, TGAImage &framebuffer, 
                           const ShadowMap& shadow_map, bool use_normal_mapping = true, bool use_color_texture = false);
void rasterize_fast_shadows(const vec4 clip[3], const vec3 worldPos[3], const vec3 normals[3], 
                           const vec2 texCoords[3], const vec3 tangents[3], const vec3 bitangents[3],
                           const Model& model, std::vector<double> &zbuffer, TGAImage &framebuffer, 
                           bool use_normal_mapping = true, bool use_color_texture = false);
std::vector<vec3> calculate_vertex_normals(const Model& model);
void calculate_tangent_space(const Model& model, int face_idx, vec3& tangent, vec3& bitangent);
