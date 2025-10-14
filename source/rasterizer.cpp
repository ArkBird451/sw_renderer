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

vec3 calculate_phong_lighting_with_shadows(const vec3& worldPos, const vec3& normal, const Material& mat, const Light& light, const vec3& viewPos, const ShadowMap& shadow_map) {
    // Calculate shadow factor
    double shadow_factor = 1.0;
    
    // Transform world position to light space
    vec4 light_pos = shadow_map.light_projection * shadow_map.light_view * vec4{worldPos.x, worldPos.y, worldPos.z, 1.0};
    
    if (light_pos.w > 0) {
        // Perspective divide
        vec3 light_ndc = {light_pos.x / light_pos.w, light_pos.y / light_pos.w, light_pos.z / light_pos.w};
        
        // Early exit if outside shadow map bounds
        if (light_ndc.x < -1.0 || light_ndc.x > 1.0 || light_ndc.y < -1.0 || light_ndc.y > 1.0 || light_ndc.z < -1.0 || light_ndc.z > 1.0) {
            shadow_factor = 0.3; // Assume shadowed if outside bounds
        } else {
            // Convert to shadow map coordinates
            int shadow_x = (int)((light_ndc.x + 1.0) * 0.5 * shadow_map.width);
            int shadow_y = (int)((light_ndc.y + 1.0) * 0.5 * shadow_map.height);
            
            // Get depth from shadow map
            double shadow_depth = shadow_map.get_depth(shadow_x, shadow_y);
            double current_depth = light_ndc.z;
            
            // Add bias to prevent shadow acne
            double bias = 0.001;
            if (current_depth - bias > shadow_depth) {
                shadow_factor = 0.3; // Shadow attenuation
            }
        }
    }
    
    // Calculate Phong lighting
    vec3 ambient = {mat.ambient.x * light.ambient.x, mat.ambient.y * light.ambient.y, mat.ambient.z * light.ambient.z};
    
    vec3 light_dir = normalized(light.position - worldPos);
    double diff = std::max(0.0, dot(normal, light_dir));
    vec3 diffuse = {mat.diffuse.x * light.diffuse.x * diff, mat.diffuse.y * light.diffuse.y * diff, mat.diffuse.z * light.diffuse.z * diff};
    
    vec3 view_dir = normalized(viewPos - worldPos);
    vec3 reflect_dir = normalized(2.0 * dot(light_dir, normal) * normal - light_dir);
    double spec = std::pow(std::max(0.0, dot(view_dir, reflect_dir)), mat.shininess);
    vec3 specular = {mat.specular.x * light.specular.x * spec, mat.specular.y * light.specular.y * spec, mat.specular.z * light.specular.z * spec};
    
    vec3 result = ambient + shadow_factor * (diffuse + specular);
    result.x = std::min(1.0f, std::max(0.0f, (float)result.x));
    result.y = std::min(1.0f, std::max(0.0f, (float)result.y));
    result.z = std::min(1.0f, std::max(0.0f, (float)result.z));
    
    return result;
}

vec3 calculate_phong_lighting_fast_shadows(const vec3& worldPos, const vec3& normal, const Material& mat, const Light& light, const vec3& viewPos) {
    // Fast shadow calculation based on distance from light
    vec3 light_dir = normalized(light.position - worldPos);
    double distance_to_light = norm(light.position - worldPos);
    
    // Simple shadow factor based on distance (closer = brighter)
    double shadow_factor = std::min(1.0, 1.0 / (1.0 + distance_to_light * 0.1));
    
    // Calculate Phong lighting
    vec3 ambient = {mat.ambient.x * light.ambient.x, mat.ambient.y * light.ambient.y, mat.ambient.z * light.ambient.z};
    
    double diff = std::max(0.0, dot(normal, light_dir));
    vec3 diffuse = {mat.diffuse.x * light.diffuse.x * diff, mat.diffuse.y * light.diffuse.y * diff, mat.diffuse.z * light.diffuse.z * diff};
    
    vec3 view_dir = normalized(viewPos - worldPos);
    vec3 reflect_dir = normalized(2.0 * dot(light_dir, normal) * normal - light_dir);
    double spec = std::pow(std::max(0.0, dot(view_dir, reflect_dir)), mat.shininess);
    vec3 specular = {mat.specular.x * light.specular.x * spec, mat.specular.y * light.specular.y * spec, mat.specular.z * light.specular.z * spec};
    
    vec3 result = ambient + shadow_factor * (diffuse + specular);
    result.x = std::min(1.0f, std::max(0.0f, (float)result.x));
    result.y = std::min(1.0f, std::max(0.0f, (float)result.y));
    result.z = std::min(1.0f, std::max(0.0f, (float)result.z));
    
    return result;
}

void rasterize(const vec4 clip[3], const vec3 worldPos[3], const vec3 normals[3], 
               const vec2 texCoords[3], const vec3 tangents[3], const vec3 bitangents[3],
               const Model& model, std::vector<double> &zbuffer, TGAImage &framebuffer, bool use_normal_mapping, bool use_color_texture) {
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
            
            // Interpolate world position, normal, and UV coordinates using barycentric coordinates
            vec3 worldPos_interp = bc.x * worldPos[0] + bc.y * worldPos[1] + bc.z * worldPos[2];
            vec3 normal_interp = bc.x * normals[0] + bc.y * normals[1] + bc.z * normals[2];
            vec2 uv_interp = bc.x * texCoords[0] + bc.y * texCoords[1] + bc.z * texCoords[2];
            
            // Sample normal map if available and enabled
            vec3 final_normal = normal_interp;
            if (use_normal_mapping && model.has_normal()) {
                vec3 normal_map_sample = model.normal(uv_interp);
                
            // Interpolate tangent and bitangent vectors
            vec3 tangent_interp = bc.x * tangents[0] + bc.y * tangents[1] + bc.z * tangents[2];
            vec3 bitangent_interp = bc.x * bitangents[0] + bc.y * bitangents[1] + bc.z * bitangents[2];
            
            // Normalize interpolated vectors
            tangent_interp = normalized(tangent_interp);
            bitangent_interp = normalized(bitangent_interp);
            
            // Transform normal from tangent space to world space using TBN matrix
            mat<3,3> TBN = {{{tangent_interp.x, bitangent_interp.x, normal_interp.x},
                            {tangent_interp.y, bitangent_interp.y, normal_interp.y},
                            {tangent_interp.z, bitangent_interp.z, normal_interp.z}}};
            final_normal = normalized(TBN * normal_map_sample);
            }
            
            // Calculate Phong lighting with final normal
            vec3 lighting = calculate_phong_lighting(worldPos_interp, final_normal, material, light, viewPos);
            
            // Apply color texture if enabled
            vec3 final_color = lighting;
            if (use_color_texture && model.has_color()) {
                vec3 texture_color = model.color(uv_interp);
                // Use texture color as base material color, then apply lighting
                final_color.x = texture_color.x * lighting.x;
                final_color.y = texture_color.y * lighting.y;
                final_color.z = texture_color.z * lighting.z;
            }
            
            // Convert to TGAColor
            TGAColor color;
            color[0] = (unsigned char)(final_color.x * 255);
            color[1] = (unsigned char)(final_color.y * 255);
            color[2] = (unsigned char)(final_color.z * 255);
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

std::vector<vec3> calculate_vertex_normals(const Model& model) {
    std::vector<vec3> vertex_normals(model.nverts(), {0, 0, 0});
    std::vector<int> vertex_face_count(model.nverts(), 0);
    
    // Calculate face normals and accumulate to vertex normals
    for (int i = 0; i < model.nfaces(); i++) {
        vec3 v0 = model.vert(i, 0);
        vec3 v1 = model.vert(i, 1);
        vec3 v2 = model.vert(i, 2);
        
        vec3 edge1 = v1 - v0;
        vec3 edge2 = v2 - v0;
        vec3 face_normal = normalized(cross(edge1, edge2));
        
        // Add face normal to each vertex
        for (int j = 0; j < 3; j++) {
            int vertex_idx = model.get_vertex_index(i, j);
            vertex_normals[vertex_idx] = vertex_normals[vertex_idx] + face_normal;
            vertex_face_count[vertex_idx]++;
        }
    }
    
    // Average the normals for each vertex
    for (int i = 0; i < model.nverts(); i++) {
        if (vertex_face_count[i] > 0) {
            vertex_normals[i] = normalized(vertex_normals[i]);
        }
    }
    
    return vertex_normals;
}

void calculate_tangent_space(const Model& model, int face_idx, vec3& tangent, vec3& bitangent) {
    vec3 v0 = model.vert(face_idx, 0);
    vec3 v1 = model.vert(face_idx, 1);
    vec3 v2 = model.vert(face_idx, 2);
    
    vec2 uv0 = model.tex_coord(face_idx, 0);
    vec2 uv1 = model.tex_coord(face_idx, 1);
    vec2 uv2 = model.tex_coord(face_idx, 2);
    
    vec3 edge1 = v1 - v0;
    vec3 edge2 = v2 - v0;
    vec2 deltaUV1 = uv1 - uv0;
    vec2 deltaUV2 = uv2 - uv0;
    
    double f = 1.0 / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
    
    tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
    tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
    tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
    tangent = normalized(tangent);
    
    bitangent.x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
    bitangent.y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
    bitangent.z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);
    bitangent = normalized(bitangent);
}

void cpu_rasterize_models(const std::vector<Model>& models, TGAImage& framebuffer, 
                         std::vector<double>& zbuffer, const mat<4,4>& Model, 
                         bool smooth_shading, bool use_normal_mapping, bool use_color_texture) {
    // -- CPU rasterization of all loaded models
    for (const auto &model : models) {
        // Calculate vertex normals for smooth shading
        std::vector<vec3> vertex_normals;
        if (smooth_shading) {
            vertex_normals = calculate_vertex_normals(model);
        }
        
        for (int i=0; i<model.nfaces(); i++) {
            vec4 clip[3];
            vec3 worldPos[3];
            vec3 normals[3];
            vec2 texCoords[3];
            
            for (int d : {0,1,2}) {
                vec3 v = model.vert(i, d);
                worldPos[d] = v;  // Store world position before transformation
                clip[d] = Perspective * ModelView * Model * vec4{v.x, v.y, v.z, 1.};
                texCoords[d] = model.tex_coord(i, d);
            }
            
            if (smooth_shading) {
                // Use vertex normals for smooth shading
                for (int d : {0,1,2}) {
                    int vertex_idx = model.get_vertex_index(i, d);
                    normals[d] = vertex_normals[vertex_idx];
                }
            } else {
                // Calculate face normal for flat shading
                vec3 edge1 = worldPos[1] - worldPos[0];
                vec3 edge2 = worldPos[2] - worldPos[0];
                vec3 faceNormal = normalized(cross(edge1, edge2));
                
                // Use the same normal for all vertices (flat shading)
                for (int d : {0,1,2}) {
                    normals[d] = faceNormal;
                }
            }
            
            // Get tangent and bitangent vectors for this face
            vec3 tangents[3], bitangents[3];
            for (int d : {0,1,2}) {
                tangents[d] = model.tangent(i, d);
                bitangents[d] = model.bitangent(i, d);
            }
            
            rasterize(clip, worldPos, normals, texCoords, tangents, bitangents, model, zbuffer, framebuffer, use_normal_mapping, use_color_texture);
        }
    }
}

void render_shadow_map(const std::vector<Model>& models, ShadowMap& shadow_map, const mat<4,4>& Model) {
    shadow_map.clear();
    
    for (const auto& model : models) {
        for (int i = 0; i < model.nfaces(); i++) {
            vec4 clip[3];
            vec3 worldPos[3];
            
            for (int d : {0,1,2}) {
                vec3 v = model.vert(i, d);
                worldPos[d] = v;
                clip[d] = shadow_map.light_projection * shadow_map.light_view * Model * vec4{v.x, v.y, v.z, 1.};
            }
            
            // Simple depth-only rasterization for shadow map
            rasterize_shadow_depth(clip, worldPos, shadow_map);
        }
    }
}

void rasterize_shadow_depth(const vec4 clip[3], const vec3 worldPos[3], ShadowMap& shadow_map) {
    vec4 ndc[3] = { clip[0]/clip[0].w, clip[1]/clip[1].w, clip[2]/clip[2].w };
    vec2 screen[3] = { 
        vec2{(ndc[0].x + 1.0) * 0.5 * shadow_map.width, (ndc[0].y + 1.0) * 0.5 * shadow_map.height},
        vec2{(ndc[1].x + 1.0) * 0.5 * shadow_map.width, (ndc[1].y + 1.0) * 0.5 * shadow_map.height},
        vec2{(ndc[2].x + 1.0) * 0.5 * shadow_map.width, (ndc[2].y + 1.0) * 0.5 * shadow_map.height}
    };

    mat<3,3> ABC = {{ {screen[0].x, screen[0].y, 1.}, {screen[1].x, screen[1].y, 1.}, {screen[2].x, screen[2].y, 1.} }};
    if (ABC.det()<1) return;

    vec2 bboxmin, bboxmax;
    bboxmin[0] = std::numeric_limits<double>::max();
    bboxmin[1] = std::numeric_limits<double>::max();
    bboxmax[0] = -std::numeric_limits<double>::max();
    bboxmax[1] = -std::numeric_limits<double>::max();
    for (int i=0; i<3; i++) {
        for (int j=0; j<2; j++) {
            bboxmin[j] = std::min(bboxmin[j], screen[i][j]);
            bboxmax[j] = std::max(bboxmax[j], screen[i][j]);
        }
    }
    for (int x=(int)bboxmin.x; x<=(int)bboxmax.x; x++) {
        for (int y=(int)bboxmin.y; y<=(int)bboxmax.y; y++) {
            vec3 bc_screen = ABC.invert_transpose() * vec3{static_cast<double>(x), static_cast<double>(y), 1.};
            if (bc_screen.x<0 || bc_screen.y<0 || bc_screen.z<0) continue;
            double z = bc_screen * vec3{ ndc[0].z, ndc[1].z, ndc[2].z };
            if (z > shadow_map.get_depth(x, y)) {
                shadow_map.set_depth(x, y, z);
            }
        }
    }
}

void cpu_rasterize_models_with_shadows(const std::vector<Model>& models, TGAImage& framebuffer, 
                                      std::vector<double>& zbuffer, const mat<4,4>& Model, 
                                      const ShadowMap& shadow_map, bool smooth_shading, 
                                      bool use_normal_mapping, bool use_color_texture) {
    for (const auto& model : models) {
        for (int i = 0; i < model.nfaces(); i++) {
            vec4 clip[3];
            vec3 worldPos[3];
            vec3 normals[3];
            vec2 texCoords[3];
            
            for (int d : {0,1,2}) {
                vec3 v = model.vert(i, d);
                worldPos[d] = v;
                clip[d] = Perspective * ModelView * Model * vec4{v.x, v.y, v.z, 1.};
                texCoords[d] = model.tex_coord(i, d);
            }
            
            if (smooth_shading) {
                // Use face normals for better performance (simplified smooth shading)
                vec3 edge1 = worldPos[1] - worldPos[0];
                vec3 edge2 = worldPos[2] - worldPos[0];
                vec3 faceNormal = normalized(cross(edge1, edge2));
                
                for (int d : {0,1,2}) {
                    normals[d] = faceNormal;
                }
            } else {
                vec3 edge1 = worldPos[1] - worldPos[0];
                vec3 edge2 = worldPos[2] - worldPos[0];
                vec3 faceNormal = normalized(cross(edge1, edge2));
                
                for (int d : {0,1,2}) {
                    normals[d] = faceNormal;
                }
            }
            
            vec3 tangents[3], bitangents[3];
            for (int d : {0,1,2}) {
                tangents[d] = model.tangent(i, d);
                bitangents[d] = model.bitangent(i, d);
            }
            
            rasterize_with_shadows(clip, worldPos, normals, texCoords, tangents, bitangents, 
                                 model, zbuffer, framebuffer, shadow_map, use_normal_mapping, use_color_texture);
        }
    }
}


void rasterize_with_shadows(const vec4 clip[3], const vec3 worldPos[3], const vec3 normals[3], 
                           const vec2 texCoords[3], const vec3 tangents[3], const vec3 bitangents[3],
                           const Model& model, std::vector<double> &zbuffer, TGAImage &framebuffer, 
                           const ShadowMap& shadow_map, bool use_normal_mapping, bool use_color_texture) {
    vec4 ndc[3]    = { clip[0]/clip[0].w, clip[1]/clip[1].w, clip[2]/clip[2].w };                // normalized device coordinates
    vec2 screen[3] = { (Viewport*ndc[0]).xy(), (Viewport*ndc[1]).xy(), (Viewport*ndc[2]).xy() }; // screen coordinates

    mat<3,3> ABC = {{ {screen[0].x, screen[0].y, 1.}, {screen[1].x, screen[1].y, 1.}, {screen[2].x, screen[2].y, 1.} }};
    if (ABC.det()<1) return; // backface culling + discarding triangles that cover less than a pixel

    vec2 bboxmin, bboxmax;
    bboxmin[0] = std::numeric_limits<double>::max();
    bboxmin[1] = std::numeric_limits<double>::max();
    bboxmax[0] = -std::numeric_limits<double>::max();
    bboxmax[1] = -std::numeric_limits<double>::max();
    for (int i=0; i<3; i++) {
        for (int j=0; j<2; j++) {
            bboxmin[j] = std::min(bboxmin[j], screen[i][j]);
            bboxmax[j] = std::max(bboxmax[j], screen[i][j]);
        }
    }
    for (int x=(int)bboxmin.x; x<=(int)bboxmax.x; x++) {
        for (int y=(int)bboxmin.y; y<=(int)bboxmax.y; y++) {
            vec3 bc_screen = ABC.invert_transpose() * vec3{static_cast<double>(x), static_cast<double>(y), 1.};
            if (bc_screen.x<0 || bc_screen.y<0 || bc_screen.z<0) continue;                                                    // negative barycentric coordinate => the pixel is outside the triangle
            double z = bc_screen * vec3{ ndc[0].z, ndc[1].z, ndc[2].z };
            if (z <= zbuffer[x+y*framebuffer.width()]) continue;
            zbuffer[x+y*framebuffer.width()] = z;
            
            // Interpolate world position, normal, and UV coordinates using barycentric coordinates
            vec3 worldPos_interp = bc_screen.x * worldPos[0] + bc_screen.y * worldPos[1] + bc_screen.z * worldPos[2];
            vec3 normal_interp = bc_screen.x * normals[0] + bc_screen.y * normals[1] + bc_screen.z * normals[2];
            vec2 uv_interp = bc_screen.x * texCoords[0] + bc_screen.y * texCoords[1] + bc_screen.z * texCoords[2];
            
            // Sample normal map if available and enabled
            vec3 final_normal = normal_interp;
            if (use_normal_mapping && model.has_normal()) {
                vec3 normal_map_sample = model.normal(uv_interp);
                
                // Interpolate tangent and bitangent vectors
                vec3 tangent_interp = bc_screen.x * tangents[0] + bc_screen.y * tangents[1] + bc_screen.z * tangents[2];
                vec3 bitangent_interp = bc_screen.x * bitangents[0] + bc_screen.y * bitangents[1] + bc_screen.z * bitangents[2];
                
                // Normalize interpolated vectors
                tangent_interp = normalized(tangent_interp);
                bitangent_interp = normalized(bitangent_interp);
                
                // Transform normal from tangent space to world space using TBN matrix
                mat<3,3> TBN = {{{tangent_interp.x, bitangent_interp.x, normal_interp.x},
                                {tangent_interp.y, bitangent_interp.y, normal_interp.y},
                                {tangent_interp.z, bitangent_interp.z, normal_interp.z}}};
                final_normal = normalized(TBN * normal_map_sample);
            }
            
            // Calculate Phong lighting with fast shadows
            vec3 lighting = calculate_phong_lighting_fast_shadows(worldPos_interp, final_normal, material, light, viewPos);

            vec3 final_color = lighting;
            if (use_color_texture && model.has_color()) {
                vec3 texture_color = model.color(uv_interp);
                final_color.x = texture_color.x * lighting.x;
                final_color.y = texture_color.y * lighting.y;
                final_color.z = texture_color.z * lighting.z;
            }
            
            TGAColor color;
            color[0] = (unsigned char)(final_color.x * 255);
            color[1] = (unsigned char)(final_color.y * 255);
            color[2] = (unsigned char)(final_color.z * 255);
            color[3] = 255;
            framebuffer.set(x, y, color);
        }
    }
}
