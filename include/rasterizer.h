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

// Function declarations
vec3 calculate_phong_lighting(const vec3& worldPos, const vec3& normal, const Material& mat, const Light& light, const vec3& viewPos);
void rasterize(const vec4 clip[3], const vec3 worldPos[3], const vec3 normals[3], 
               std::vector<double> &zbuffer, TGAImage &framebuffer);
void rasterize_simple(const vec4 clip[3], std::vector<double> &zbuffer, TGAImage &framebuffer, const TGAColor color);
void cpu_rasterize_models(const std::vector<Model>& models, TGAImage& framebuffer, 
                         std::vector<double>& zbuffer, const mat<4,4>& Model);
