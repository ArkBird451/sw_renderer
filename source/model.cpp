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
