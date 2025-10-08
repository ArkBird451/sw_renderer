#include "rasterizer.h"
#include <algorithm>
#include <cmath>

// External matrix variables from main.cpp
extern mat<4,4> ModelView, Viewport, Perspective;

// Global lighting setup
const Material material = {
    {0.1f, 0.1f, 0.1f},    // ambient
    {0.7f, 0.7f, 0.7f},    // diffuse
    {1.0f, 1.0f, 1.0f},     // specular
    32.0f                   // shininess
};

const Light light = {
    {1.0f, 1.0f, 1.0f},     // position
    {0.2f, 0.2f, 0.2f},     // ambient
    {0.8f, 0.8f, 0.8f},     // diffuse
    {1.0f, 1.0f, 1.0f}       // specular
};

const vec3 viewPos = {0.0f, 0.0f, 2.0f};  // camera position for specular calculation

vec3 calculate_phong_lighting(const vec3& worldPos, const vec3& normal, const Material& mat, const Light& light, const vec3& viewPos) {
    // Normalize vectors
    vec3 norm = normalized(normal);
    vec3 lightDir = normalized(light.position - worldPos);
    vec3 viewDir = normalized(viewPos - worldPos);
    vec3 reflectDir = normalized(2.0f * dot(norm, lightDir) * norm - lightDir);
    
    // Ambient component
    vec3 ambient = {mat.ambient.x * light.ambient.x, mat.ambient.y * light.ambient.y, mat.ambient.z * light.ambient.z};
    
    // Diffuse component
    float diff = std::max(0.0f, (float)dot(norm, lightDir));
    vec3 diffuse = {mat.diffuse.x * light.diffuse.x * diff, mat.diffuse.y * light.diffuse.y * diff, mat.diffuse.z * light.diffuse.z * diff};
    
    // Specular component
    float spec = std::pow(std::max(0.0f, (float)dot(viewDir, reflectDir)), mat.shininess);
    vec3 specular = {mat.specular.x * light.specular.x * spec, mat.specular.y * light.specular.y * spec, mat.specular.z * light.specular.z * spec};
    
    // Combine all components
    vec3 result = ambient + diffuse + specular;
    
    // Clamp values to [0,1]
    result.x = std::min(1.0f, std::max(0.0f, (float)result.x));
    result.y = std::min(1.0f, std::max(0.0f, (float)result.y));
    result.z = std::min(1.0f, std::max(0.0f, (float)result.z));
    
    return result;
}

void rasterize(const vec4 clip[3], const vec3 worldPos[3], const vec3 normals[3], 
               std::vector<double> &zbuffer, TGAImage &framebuffer) {
    vec4 ndc[3]    = { clip[0]/clip[0].w, clip[1]/clip[1].w, clip[2]/clip[2].w };                // normalized device coordinates
    vec2 screen[3] = { (Viewport*ndc[0]).xy(), (Viewport*ndc[1]).xy(), (Viewport*ndc[2]).xy() }; // screen coordinates

    mat<3,3> ABC = {{ {screen[0].x, screen[0].y, 1.}, {screen[1].x, screen[1].y, 1.}, {screen[2].x, screen[2].y, 1.} }};
    if (ABC.det()<1) return; // backface culling + discarding triangles that cover less than a pixel

    auto [bbminx,bbmaxx] = std::minmax({screen[0].x, screen[1].x, screen[2].x}); // bounding box for the triangle
    auto [bbminy,bbmaxy] = std::minmax({screen[0].y, screen[1].y, screen[2].y}); // defined by its top left and bottom right corners

    #pragma omp parallel for
    for (int y=std::max<int>(bbminy, 0); y<=std::min<int>(bbmaxy, framebuffer.height()-1); y++) { // clip the bounding box by the screen
        for (int x=std::max<int>(bbminx, 0); x<=std::min<int>(bbmaxx, framebuffer.width()-1); x++) {
            vec3 bc = ABC.invert_transpose() * vec3{static_cast<double>(x), static_cast<double>(y), 1.}; // barycentric coordinates of {x,y} w.r.t the triangle
            if (bc.x<0 || bc.y<0 || bc.z<0) continue;                                                    // negative barycentric coordinate => the pixel is outside the triangle
            double z = bc * vec3{ ndc[0].z, ndc[1].z, ndc[2].z };
            if (z <= zbuffer[x+y*framebuffer.width()]) continue;
            zbuffer[x+y*framebuffer.width()] = z;
            
            // Interpolate world position and normal using barycentric coordinates
            vec3 worldPos_interp = bc.x * worldPos[0] + bc.y * worldPos[1] + bc.z * worldPos[2];
            vec3 normal_interp = bc.x * normals[0] + bc.y * normals[1] + bc.z * normals[2];
            
            // Calculate Phong lighting
            vec3 lighting = calculate_phong_lighting(worldPos_interp, normal_interp, material, light, viewPos);
            
            // Convert to TGAColor
            TGAColor color;
            color[0] = (unsigned char)(lighting.x * 255);
            color[1] = (unsigned char)(lighting.y * 255);
            color[2] = (unsigned char)(lighting.z * 255);
            color.bytespp = 3;
            
            framebuffer.set(x, y, color);
        }
    }
}

void rasterize_simple(const vec4 clip[3], std::vector<double> &zbuffer, TGAImage &framebuffer, const TGAColor color) {
    vec4 ndc[3]    = { clip[0]/clip[0].w, clip[1]/clip[1].w, clip[2]/clip[2].w };                // normalized device coordinates
    vec2 screen[3] = { (Viewport*ndc[0]).xy(), (Viewport*ndc[1]).xy(), (Viewport*ndc[2]).xy() }; // screen coordinates

    mat<3,3> ABC = {{ {screen[0].x, screen[0].y, 1.}, {screen[1].x, screen[1].y, 1.}, {screen[2].x, screen[2].y, 1.} }};
    if (ABC.det()<1) return; // backface culling + discarding triangles that cover less than a pixel

    auto [bbminx,bbmaxx] = std::minmax({screen[0].x, screen[1].x, screen[2].x}); // bounding box for the triangle
    auto [bbminy,bbmaxy] = std::minmax({screen[0].y, screen[1].y, screen[2].y}); // defined by its top left and bottom right corners
#pragma omp parallel for
    for (int x=std::max<int>(bbminx, 0); x<=std::min<int>(bbmaxx, framebuffer.width()-1); x++) { // clip the bounding box by the screen
        for (int y=std::max<int>(bbminy, 0); y<=std::min<int>(bbmaxy, framebuffer.height()-1); y++) {
            vec3 bc = ABC.invert_transpose() * vec3{static_cast<double>(x), static_cast<double>(y), 1.}; // barycentric coordinates of {x,y} w.r.t the triangle
            if (bc.x<0 || bc.y<0 || bc.z<0) continue;                                                    // negative barycentric coordinate => the pixel is outside the triangle
            double z = bc * vec3{ ndc[0].z, ndc[1].z, ndc[2].z };
            if (z <= zbuffer[x+y*framebuffer.width()]) continue;
            zbuffer[x+y*framebuffer.width()] = z;
            
            framebuffer.set(x, y, color);
        }
    }
}

void cpu_rasterize_models(const std::vector<Model>& models, TGAImage& framebuffer, 
                         std::vector<double>& zbuffer, const mat<4,4>& Model) {
    // -- CPU rasterization of all loaded models
    for (const auto &model : models) {
        for (int i=0; i<model.nfaces(); i++) {
            vec4 clip[3];
            vec3 worldPos[3];
            vec3 normals[3];
            
            for (int d : {0,1,2}) {
                vec3 v = model.vert(i, d);
                worldPos[d] = v;  // Store world position before transformation
                clip[d] = Perspective * ModelView * Model * vec4{v.x, v.y, v.z, 1.};
            }
            
            // Calculate face normal
            vec3 edge1 = worldPos[1] - worldPos[0];
            vec3 edge2 = worldPos[2] - worldPos[0];
            vec3 faceNormal = normalized(cross(edge1, edge2));
            
            // Use the same normal for all vertices (flat shading)
            for (int d : {0,1,2}) {
                normals[d] = faceNormal;
            }
            
            rasterize(clip, worldPos, normals, zbuffer, framebuffer);
        }
    }
}
