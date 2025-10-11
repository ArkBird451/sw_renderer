#pragma once

#include <vector>
#include <string>
#include "geometry.h"
#include "tgaimage.h"

class Model {
    std::vector<vec3> verts = {};    // array of vertices
    std::vector<int> facet_vrt = {}; // per-triangle index in the above array
    std::vector<vec2> tex_coords = {}; // texture coordinates
    std::vector<int> facet_tex = {};  // per-triangle texture index
    TGAImage normal_map;              // normal map texture
    TGAImage color_texture;            // color/diffuse texture
    bool has_normal_map = false;
    bool has_color_texture = false;
public:
    Model(const std::string& filename);
    Model(const std::string& filename, const std::string& normal_map_filename);
    Model(const std::string& filename, const std::string& normal_map_filename, const std::string& color_texture_filename);
    int nverts() const; // number of vertices
    int nfaces() const; // number of triangles
    vec3 vert(const int i) const;                          // 0 <= i < nverts()
    vec3 vert(const int iface, const int nthvert) const;   // 0 <= iface <= nfaces(), 0 <= nthvert < 3
    vec2 tex_coord(const int iface, const int nthvert) const; // texture coordinate for face vertex
    int get_vertex_index(const int iface, const int nthvert) const; // get vertex index for face
    vec3 normal(const vec2& uv) const; // sample normal map at UV coordinates
    vec3 color(const vec2& uv) const; // sample color texture at UV coordinates
    bool has_normal() const { return has_normal_map; }
    bool has_color() const { return has_color_texture; }
};
