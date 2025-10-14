#include <fstream>
#include <sstream>
#include "model.h"
#include <algorithm>
#include "tgaimage.h"

Model::Model(const std::string& filename) {
    std::ifstream in;
    in.open(filename, std::ifstream::in);
    if (in.fail()) return;

    std::string line;
    while (!in.eof()) {
        std::getline(in, line);
        std::istringstream iss(line.c_str());
        char trash;
        if (!line.compare(0, 2, "v ")) {
            iss >> trash;
            vec3 v;
            for (int i : {0,1,2}) iss >> v[i];
            verts.push_back(v);
        } else if (!line.compare(0, 2, "vt")) {
            iss >> trash >> trash;
            vec2 uv;
            for (int i : {0,1}) iss >> uv[i];
            tex_coords.push_back(uv);
        } else if (!line.compare(0, 2, "f ")) {
            int f,t,n, cnt = 0;
            iss >> trash;

            while (iss >> f >> trash >> t >> trash >> n) {
                facet_vrt.push_back(--f);
                facet_tex.push_back(--t);
                cnt++;
            }
            if (3!=cnt) {
                std::cerr << "Error: the obj file is supposed to be triangulated" << std::endl;
                return;
            }
        }
    }
    
    // Calculate tangent and bitangent vectors for each face
    calculate_tangent_space();
    
    std::cerr << "# v# " << nverts() << " f# "  << nfaces() << " vt# " << tex_coords.size() << std::endl;
}

Model::Model(const std::string& filename, const std::string& normal_map_filename) : Model(filename) {
    if (normal_map.read_tga_file(normal_map_filename.c_str())) {
        normal_map.flip_vertically();
        has_normal_map = true;
        std::cerr << "Normal map loaded: " << normal_map_filename << std::endl;
    } else {
        std::cerr << "Failed to load normal map: " << normal_map_filename << std::endl;
    }
}

Model::Model(const std::string& filename, const std::string& normal_map_filename, const std::string& color_texture_filename) : Model(filename) {
    if (normal_map.read_tga_file(normal_map_filename.c_str())) {
        normal_map.flip_vertically();
        has_normal_map = true;
        std::cerr << "Normal map loaded: " << normal_map_filename << std::endl;
    } else {
        std::cerr << "Failed to load normal map: " << normal_map_filename << std::endl;
    }
    
    if (color_texture.read_tga_file(color_texture_filename.c_str())) {
        color_texture.flip_vertically();
        has_color_texture = true;
        std::cerr << "Color texture loaded: " << color_texture_filename << std::endl;
    } else {
        std::cerr << "Failed to load color texture: " << color_texture_filename << std::endl;
    }
}

int Model::nverts() const { return verts.size(); }
int Model::nfaces() const { return facet_vrt.size()/3; }

vec3 Model::vert(const int i) const {
    return verts[i];
}

vec3 Model::vert(const int iface, const int nthvert) const {
    return verts[facet_vrt[iface*3+nthvert]];
}

int Model::get_vertex_index(const int iface, const int nthvert) const {
    return facet_vrt[iface*3+nthvert];
}

vec2 Model::tex_coord(const int iface, const int nthvert) const {
    int idx = facet_tex[iface*3+nthvert];
    return tex_coords[idx];
}

vec3 Model::normal(const vec2& uv) const {
    if (!has_normal_map) return {0, 0, 1};
    
    int x = (int)(uv.x * normal_map.width());
    int y = (int)(uv.y * normal_map.height());
    x = std::max(0, std::min(x, (int)normal_map.width() - 1));
    y = std::max(0, std::min(y, (int)normal_map.height() - 1));
    
    TGAColor c = normal_map.get(x, y);
    vec3 n;
    n.x = (c[2] / 255.0) * 2.0 - 1.0; // Red -> X
    n.y = (c[1] / 255.0) * 2.0 - 1.0; // Green -> Y  
    n.z = (c[0] / 255.0) * 2.0 - 1.0; // Blue -> Z
    
    
    return normalized(n);
}

vec3 Model::color(const vec2& uv) const {
    if (!has_color_texture) return {1, 1, 1}; // Default white color
    
    int x = (int)(uv.x * color_texture.width());
    int y = (int)(uv.y * color_texture.height());
    x = std::max(0, std::min(x, (int)color_texture.width() - 1));
    y = std::max(0, std::min(y, (int)color_texture.height() - 1));
    
    TGAColor c = color_texture.get(x, y);
    vec3 color;
    // Try swapping red and blue channels
    color.x = c[0] / 255.0; // Red (from blue channel)
    color.y = c[1] / 255.0; // Green  
    color.z = c[2] / 255.0; // Blue (from red channel)
    
    return color;
}

void Model::calculate_tangent_space() {
    tangents.clear();
    bitangents.clear();
    
    for (int i = 0; i < nfaces(); i++) {
        // Get triangle vertices
        vec3 v0 = vert(i, 0);
        vec3 v1 = vert(i, 1);
        vec3 v2 = vert(i, 2);
        
        // Get texture coordinates
        vec2 uv0 = tex_coord(i, 0);
        vec2 uv1 = tex_coord(i, 1);
        vec2 uv2 = tex_coord(i, 2);
        
        // Calculate edges
        vec3 edge1 = v1 - v0;
        vec3 edge2 = v2 - v0;
        vec2 deltaUV1 = uv1 - uv0;
        vec2 deltaUV2 = uv2 - uv0;
        
        // Calculate tangent and bitangent
        double f = 1.0 / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
        
        vec3 tangent;
        tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
        tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
        tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
        tangent = normalized(tangent);
        
        vec3 bitangent;
        bitangent.x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
        bitangent.y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
        bitangent.z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);
        bitangent = normalized(bitangent);
        
        // Store tangent and bitangent for each vertex of the triangle
        tangents.push_back(tangent);
        tangents.push_back(tangent);
        tangents.push_back(tangent);
        
        bitangents.push_back(bitangent);
        bitangents.push_back(bitangent);
        bitangents.push_back(bitangent);
    }
}

vec3 Model::tangent(const int iface, const int nthvert) const {
    int idx = iface * 3 + nthvert;
    if (idx >= 0 && idx < tangents.size()) {
        return tangents[idx];
    }
    return {1, 0, 0}; // Default tangent
}

vec3 Model::bitangent(const int iface, const int nthvert) const {
    int idx = iface * 3 + nthvert;
    if (idx >= 0 && idx < bitangents.size()) {
        return bitangents[idx];
    }
    return {0, 1, 0}; // Default bitangent
}
