#include "rasterizer.h"
#include <algorithm>
#include <cmath>
#include <random>
#include <chrono>

// External matrix variables from main.cpp
extern mat<4,4> ModelView, Viewport, Perspective;

// Global lighting setup
const RenderMaterial material = {
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

vec3 calculate_phong_lighting(const vec3& worldPos, const vec3& normal, const RenderMaterial& mat, const Light& light, const vec3& viewPos) {
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

vec3 calculate_phong_lighting_with_shadows(const vec3& worldPos, const vec3& normal, const RenderMaterial& mat, const Light& light, const vec3& viewPos, const ShadowMap& shadow_map) {
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

vec3 calculate_phong_lighting_fast_shadows(const vec3& worldPos, const vec3& normal, const RenderMaterial& mat, const Light& light, const vec3& viewPos) {
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
               const ObjModel& model, std::vector<double> &zbuffer, std::vector<Color> &framebuffer, bool use_normal_mapping, bool use_color_texture) {
    vec4 ndc[3]    = { clip[0]/clip[0].w, clip[1]/clip[1].w, clip[2]/clip[2].w };                // normalized device coordinates
    vec2 screen[3] = { (Viewport*ndc[0]).xy(), (Viewport*ndc[1]).xy(), (Viewport*ndc[2]).xy() }; // screen coordinates

    mat<3,3> ABC = {{ {screen[0].x, screen[0].y, 1.}, {screen[1].x, screen[1].y, 1.}, {screen[2].x, screen[2].y, 1.} }};
    if (ABC.det()<1) return; // backface culling + discarding triangles that cover less than a pixel

    auto [bbminx,bbmaxx] = std::minmax({screen[0].x, screen[1].x, screen[2].x}); // bounding box for the triangle
    auto [bbminy,bbmaxy] = std::minmax({screen[0].y, screen[1].y, screen[2].y}); // defined by its top left and bottom right corners

    #pragma omp parallel for
    for (int y=std::max<int>(bbminy, 0); y<=std::min<int>(bbmaxy, 799); y++) { // clip the bounding box by the screen
        for (int x=std::max<int>(bbminx, 0); x<=std::min<int>(bbmaxx, 799); x++) {
            double px = x + 0.5;
            double py = y + 0.5;
            vec3 bc = ABC.invert_transpose() * vec3{px, py, 1.}; // barycentric coordinates of {x,y} w.r.t the triangle
            if (bc.x<0 || bc.y<0 || bc.z<0) continue;            // negative barycentric coordinate as the pixel is outside the triangle
            double z = bc * vec3{ ndc[0].z, ndc[1].z, ndc[2].z };
            if (z <= zbuffer[x+y*800]) continue;
            zbuffer[x+y*800] = z;
            
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
            
            // Convert to Raylib Color
            Color color;
            color.r = (unsigned char)(final_color.x * 255);
            color.g = (unsigned char)(final_color.y * 255);
            color.b = (unsigned char)(final_color.z * 255);
            color.a = 255;
            
            framebuffer[y * 800 + x] = color;
        }
    }
}

void rasterize_simple(const vec4 clip[3], std::vector<double> &zbuffer, std::vector<Color> &framebuffer, const Color color) {
    vec4 ndc[3]    = { clip[0]/clip[0].w, clip[1]/clip[1].w, clip[2]/clip[2].w };                // normalized device coordinates
    vec2 screen[3] = { (Viewport*ndc[0]).xy(), (Viewport*ndc[1]).xy(), (Viewport*ndc[2]).xy() }; // screen coordinates

    mat<3,3> ABC = {{ {screen[0].x, screen[0].y, 1.}, {screen[1].x, screen[1].y, 1.}, {screen[2].x, screen[2].y, 1.} }};
    if (ABC.det()<1) return; // backface culling + discarding triangles that cover less than a pixel

    auto [bbminx,bbmaxx] = std::minmax({screen[0].x, screen[1].x, screen[2].x}); // bounding box for the triangle
    auto [bbminy,bbmaxy] = std::minmax({screen[0].y, screen[1].y, screen[2].y}); // defined by its top left and bottom right corners
#pragma omp parallel for
    for (int x=std::max<int>(bbminx, 0); x<=std::min<int>(bbmaxx, 799); x++) { // clip the bounding box by the screen
        for (int y=std::max<int>(bbminy, 0); y<=std::min<int>(bbmaxy, 799); y++) {
            // Use sub-pixel precision to reduce shimmering
            double px = x + 0.5;
            double py = y + 0.5;
            vec3 bc = ABC.invert_transpose() * vec3{px, py, 1.}; // barycentric coordinates of {x,y} w.r.t the triangle
            if (bc.x<0 || bc.y<0 || bc.z<0) continue;                                                    // negative barycentric coordinate => the pixel is outside the triangle
            double z = bc * vec3{ ndc[0].z, ndc[1].z, ndc[2].z };
            if (z <= zbuffer[x+y*800]) continue;
            zbuffer[x+y*800] = z;
            
            framebuffer[y * 800 + x] = color;
        }
    }
}

std::vector<vec3> calculate_vertex_normals(const ObjModel& model) {
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

void calculate_tangent_space(const ObjModel& model, int face_idx, vec3& tangent, vec3& bitangent) {
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

void cpu_rasterize_models(const std::vector<ObjModel>& models, std::vector<Color>& framebuffer, 
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

void render_shadow_map(const std::vector<ObjModel>& models, ShadowMap& shadow_map, const mat<4,4>& Model) {
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
            // Use sub-pixel precision for shadow mapping
            double px = x + 0.5;
            double py = y + 0.5;
            vec3 bc_screen = ABC.invert_transpose() * vec3{px, py, 1.};
            if (bc_screen.x<0 || bc_screen.y<0 || bc_screen.z<0) continue;
            double z = bc_screen * vec3{ ndc[0].z, ndc[1].z, ndc[2].z };
            if (z > shadow_map.get_depth(x, y)) {
                shadow_map.set_depth(x, y, z);
            }
        }
    }
}

void cpu_rasterize_models_with_shadows(const std::vector<ObjModel>& models, std::vector<Color>& framebuffer, 
                                      std::vector<double>& zbuffer, const mat<4,4>& Model, 
                                      const ShadowMap& shadow_map, bool smooth_shading, 
                                      bool use_normal_mapping, bool use_color_texture) {
    for (const auto& model : models) {
        // Calculate vertex normals for smooth shading
        std::vector<vec3> vertex_normals;
        if (smooth_shading) {
            vertex_normals = calculate_vertex_normals(model);
        }
        
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
                // Use vertex normals for smooth shading
                for (int d : {0,1,2}) {
                    int vertex_idx = model.get_vertex_index(i, d);
                    normals[d] = vertex_normals[vertex_idx];
                }
            } else {
                // Use face normals for flat shading
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
                           const ObjModel& model, std::vector<double> &zbuffer, std::vector<Color> &framebuffer, 
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
            // Use sub-pixel precision to reduce shimmering
            double px = x + 0.5;
            double py = y + 0.5;
            vec3 bc_screen = ABC.invert_transpose() * vec3{px, py, 1.};
            if (bc_screen.x<0 || bc_screen.y<0 || bc_screen.z<0) continue;                                                    // negative barycentric coordinate => the pixel is outside the triangle
            double z = bc_screen * vec3{ ndc[0].z, ndc[1].z, ndc[2].z };
            if (z <= zbuffer[x+y*800]) continue;
            zbuffer[x+y*800] = z;
            
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
            
            Color color;
            color.r = (unsigned char)(final_color.x * 255);
            color.g = (unsigned char)(final_color.y * 255);
            color.b = (unsigned char)(final_color.z * 255);
            color.a = 255;
            framebuffer[y * 800 + x] = color;
        }
    }
}

// SSAO Implementation
void SSAOData::generate_kernel() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);
    
    kernel.clear();
    kernel.reserve(64); // Reserve space for maximum kernel size
    
    for (int i = 0; i < 64; i++) {
        vec3 sample;
        sample.x = dis(gen) * 2.0f - 1.0f; // Random between -1 and 1
        sample.y = dis(gen) * 2.0f - 1.0f;
        sample.z = dis(gen); // Random between 0 and 1 (hemisphere)
        
        // Normalize and scale by random distance
        sample = normalized(sample);
        sample = sample * dis(gen); // Scale by random distance for better distribution
        
        kernel.push_back(sample);
    }
}

void SSAOData::generate_noise() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);
    
    noise.clear();
    noise.reserve(16 * 16); // 16x16 noise texture
    
    for (int i = 0; i < 16 * 16; i++) {
        vec3 noise_vec;
        noise_vec.x = dis(gen) * 2.0f - 1.0f; // Random between -1 and 1
        noise_vec.y = dis(gen) * 2.0f - 1.0f;
        noise_vec.z = 0.0f; // Z component not used for rotation
        
        noise.push_back(normalized(noise_vec));
    }
}

void SSAOData::update_depth_buffer(const std::vector<double>& scene_depth) {
    depth_buffer = scene_depth;
}

float SSAOData::calculate_occlusion(int x, int y, const SSAOParams& params) const {
    if (x < 0 || x >= width || y < 0 || y >= height) return 1.0f;
    
    float current_depth = (float)depth_buffer[y * width + x];
    if (current_depth <= 0.0f) return 1.0f; // No occlusion for background
    
    float occlusion = 0.0f;
    int sample_count = std::min(params.kernel_size, (int)kernel.size());
    int valid_samples = 0;
    
    // Get noise rotation for this pixel
    int noise_x = x % 16;
    int noise_y = y % 16;
    vec3 noise_vec = noise[noise_y * 16 + noise_x];
    
    for (int i = 0; i < sample_count; i++) {
        vec3 sample = kernel[i];
        
        // Rotate sample by noise vector
        vec3 rotated_sample;
        rotated_sample.x = sample.x * noise_vec.x - sample.y * noise_vec.y;
        rotated_sample.y = sample.x * noise_vec.y + sample.y * noise_vec.x;
        rotated_sample.z = sample.z;
        
        // Scale by radius (use larger scale for more visible effect)
        vec3 scaled_sample = rotated_sample * params.radius;
        
        // Sample position
        int sample_x = x + (int)scaled_sample.x;
        int sample_y = y + (int)scaled_sample.y;
        
        if (sample_x < 0 || sample_x >= width || sample_y < 0 || sample_y >= height) {
            continue;
        }
        
        float sample_depth = (float)depth_buffer[sample_y * width + sample_x];
        if (sample_depth <= 0.0f) continue;
        
        valid_samples++;
        
        // Calculate depth difference (positive means sample is closer to camera)
        float depth_diff = current_depth - sample_depth;
        
        // Check if sample is occluded (sample is closer and within bias)
        if (depth_diff > params.bias) {
            // Calculate range attenuation based on distance
            float distance = std::sqrt(scaled_sample.x * scaled_sample.x + scaled_sample.y * scaled_sample.y);
            float range = std::max(0.0f, 1.0f - distance / params.radius);
            occlusion += range * (depth_diff / params.radius);
        }
    }
    
    if (valid_samples == 0) return 1.0f;
    
    // Normalize and apply power
    occlusion = occlusion / valid_samples;
    occlusion = std::pow(occlusion, params.power);
    return std::max(0.0f, 1.0f - occlusion * params.intensity);
}

void apply_ssao(std::vector<Color>& framebuffer, const std::vector<double>& depth_buffer, 
                const SSAOData& ssao_data, const SSAOParams& params, int width, int height) {
    // Update SSAO depth buffer
    const_cast<SSAOData&>(ssao_data).update_depth_buffer(depth_buffer);
    
    // Apply SSAO to each pixel
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float occlusion_factor = ssao_data.calculate_occlusion(x, y, params);
            
            // Apply occlusion to the pixel with enhanced contrast
            int pixel_idx = y * width + x;
            Color& pixel = framebuffer[pixel_idx];
            
            // Apply occlusion with enhanced contrast for visibility
            float enhanced_factor = std::max(0.2f, occlusion_factor); // Minimum brightness of 20%
            pixel.r = (unsigned char)(pixel.r * enhanced_factor);
            pixel.g = (unsigned char)(pixel.g * enhanced_factor);
            pixel.b = (unsigned char)(pixel.b * enhanced_factor);
        }
    }
}

void render_with_ssao(const std::vector<ObjModel>& models, std::vector<Color>& framebuffer, 
                      std::vector<double>& zbuffer, const mat<4,4>& Model, 
                      const ShadowMap& shadow_map, const SSAOData& ssao_data, const SSAOParams& ssao_params,
                      bool smooth_shading, bool use_normal_mapping, bool use_color_texture) {
    // First render the scene normally with shadows
    cpu_rasterize_models_with_shadows(models, framebuffer, zbuffer, Model, shadow_map, 
                                     smooth_shading, use_normal_mapping, use_color_texture);
    
    // Then apply SSAO
    apply_ssao(framebuffer, zbuffer, ssao_data, ssao_params, 800, 800);
}

// Toon Shader Implementation
vec3 calculate_toon_lighting(const vec3& worldPos, const vec3& normal, const RenderMaterial& mat, const Light& light, const vec3& viewPos) {
    // Calculate standard Phong lighting first
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
    
    // Quantize the lighting into 4 distinct levels for toon effect
    float intensity = (result.x + result.y + result.z) / 3.0f; // Average intensity
    
    // Quantize intensity into 4 levels
    float quantized_intensity;
    if (intensity < 0.25f) {
        quantized_intensity = 0.1f;  // Dark shadow
    } else if (intensity < 0.5f) {
        quantized_intensity = 0.3f;  // Medium shadow
    } else if (intensity < 0.75f) {
        quantized_intensity = 0.6f;  // Light
    } else {
        quantized_intensity = 0.9f;  // Bright highlight
    }
    
    // Apply quantized intensity to the color
    result.x *= quantized_intensity / intensity;
    result.y *= quantized_intensity / intensity;
    result.z *= quantized_intensity / intensity;
    
    // Clamp again after quantization
    result.x = std::min(1.0f, std::max(0.0f, (float)result.x));
    result.y = std::min(1.0f, std::max(0.0f, (float)result.y));
    result.z = std::min(1.0f, std::max(0.0f, (float)result.z));
    
    return result;
}

void rasterize_toon(const vec4 clip[3], const vec3 worldPos[3], const vec3 normals[3], 
                   const vec2 texCoords[3], const vec3 tangents[3], const vec3 bitangents[3],
                   const ObjModel& model, std::vector<double> &zbuffer, std::vector<Color> &framebuffer, 
                   bool use_normal_mapping, bool use_color_texture) {
    vec4 ndc[3]    = { clip[0]/clip[0].w, clip[1]/clip[1].w, clip[2]/clip[2].w };                // normalized device coordinates
    vec2 screen[3] = { (Viewport*ndc[0]).xy(), (Viewport*ndc[1]).xy(), (Viewport*ndc[2]).xy() }; // screen coordinates

    mat<3,3> ABC = {{ {screen[0].x, screen[0].y, 1.}, {screen[1].x, screen[1].y, 1.}, {screen[2].x, screen[2].y, 1.} }};
    if (ABC.det()<1) return; // backface culling + discarding triangles that cover less than a pixel

    auto [bbminx,bbmaxx] = std::minmax({screen[0].x, screen[1].x, screen[2].x}); // bounding box for the triangle
    auto [bbminy,bbmaxy] = std::minmax({screen[0].y, screen[1].y, screen[2].y}); // defined by its top left and bottom right corners

    #pragma omp parallel for
    for (int y=std::max<int>(bbminy, 0); y<=std::min<int>(bbmaxy, 799); y++) { // clip the bounding box by the screen
        for (int x=std::max<int>(bbminx, 0); x<=std::min<int>(bbmaxx, 799); x++) {
            double px = x + 0.5;
            double py = y + 0.5;
            vec3 bc = ABC.invert_transpose() * vec3{px, py, 1.}; // barycentric coordinates of {x,y} w.r.t the triangle
            if (bc.x<0 || bc.y<0 || bc.z<0) continue;            // negative barycentric coordinate as the pixel is outside the triangle
            double z = bc * vec3{ ndc[0].z, ndc[1].z, ndc[2].z };
            if (z <= zbuffer[x+y*800]) continue;
            zbuffer[x+y*800] = z;
            
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
            
            // Calculate toon lighting
            vec3 lighting = calculate_toon_lighting(worldPos_interp, final_normal, material, light, viewPos);
            
            // Apply color texture if enabled
            vec3 final_color = lighting;
            if (use_color_texture && model.has_color()) {
                vec3 texture_color = model.color(uv_interp);
                // Use texture color as base material color, then apply lighting
                final_color.x = texture_color.x * lighting.x;
                final_color.y = texture_color.y * lighting.y;
                final_color.z = texture_color.z * lighting.z;
            }
            
            // Convert to Raylib Color
            Color color;
            color.r = (unsigned char)(final_color.x * 255);
            color.g = (unsigned char)(final_color.y * 255);
            color.b = (unsigned char)(final_color.z * 255);
            color.a = 255;
            
            framebuffer[y * 800 + x] = color;
        }
    }
}

void cpu_rasterize_models_toon(const std::vector<ObjModel>& models, std::vector<Color>& framebuffer, 
                               std::vector<double>& zbuffer, const mat<4,4>& Model, 
                               bool smooth_shading, bool use_normal_mapping, bool use_color_texture) {
    // -- CPU rasterization of all loaded models with toon shading
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
            
            rasterize_toon(clip, worldPos, normals, texCoords, tangents, bitangents, model, zbuffer, framebuffer, use_normal_mapping, use_color_texture);
        }
    }
}

void render_toon_outlines(const std::vector<ObjModel>& models, std::vector<Color>& framebuffer, 
                          std::vector<double>& zbuffer, const mat<4,4>& Model, 
                          bool smooth_shading, bool use_normal_mapping, bool use_color_texture) {
    // Create a temporary framebuffer for outline detection
    std::vector<Color> temp_framebuffer = framebuffer;
    std::vector<double> temp_zbuffer = zbuffer;
    
    // First render the scene with toon shading using the specified shading options
    cpu_rasterize_models_toon(models, temp_framebuffer, temp_zbuffer, Model, smooth_shading, use_normal_mapping, use_color_texture);
    
    // Detect edges by comparing depth differences
    const double edge_threshold = 0.01; // Threshold for edge detection
    const int outline_width = 2; // Width of outline in pixels
    
    for (int y = outline_width; y < 800 - outline_width; y++) {
        for (int x = outline_width; x < 800 - outline_width; x++) {
            double current_depth = temp_zbuffer[y * 800 + x];
            if (current_depth <= -std::numeric_limits<double>::max()) continue; // Skip background
            
            bool is_edge = false;
            
            // Check surrounding pixels for depth differences
            for (int dy = -outline_width; dy <= outline_width; dy++) {
                for (int dx = -outline_width; dx <= outline_width; dx++) {
                    if (dx == 0 && dy == 0) continue;
                    
                    int nx = x + dx;
                    int ny = y + dy;
                    
                    if (nx < 0 || nx >= 800 || ny < 0 || ny >= 800) continue;
                    
                    double neighbor_depth = temp_zbuffer[ny * 800 + nx];
                    if (neighbor_depth <= -std::numeric_limits<double>::max()) {
                        is_edge = true; // Edge against background
                        break;
                    }
                    
                    double depth_diff = std::abs(current_depth - neighbor_depth);
                    if (depth_diff > edge_threshold) {
                        is_edge = true;
                        break;
                    }
                }
                if (is_edge) break;
            }
            
            // Draw black outline
            if (is_edge) {
                framebuffer[y * 800 + x] = {0, 0, 0, 255}; // Black outline
            } else {
                framebuffer[y * 800 + x] = temp_framebuffer[y * 800 + x]; // Original toon color
            }
        }
    }
}
