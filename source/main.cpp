#include <limits>
#include <algorithm>
#include "geometry.h"
#include "model.h"
#include "tgaimage.h"
#include "viewer.h"

mat<4,4> ModelView, Viewport, Perspective;

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

void rasterize(const vec4 clip[3], std::vector<double> &zbuffer, TGAImage &framebuffer, const TGAColor color) {
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

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " obj/model.obj" << std::endl;
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
    std::vector<Model> models;
    models.reserve(argc-1);
    for (int m=1; m<argc; m++) models.emplace_back(argv[m]);

    // Initialize viewer
    if (!viewer_init(width, height, "sw_renderer - interactive")) {
        std::cerr << "Viewer not available. Rebuild with USE_RAYLIB enabled." << std::endl;
        return 1;
    }

    // Persistent CPU framebuffer and staging buffer
    TGAImage framebuffer(width, height, TGAImage::RGB);
    std::vector<double> zbuffer(width*height, -std::numeric_limits<double>::max());
    std::vector<unsigned char> rgba(width*height*4, 255);

    double angleY = 0.0;
    double angleX = 0.0;
    // ==== Main render loop ====
    while (!viewer_should_close()) {
        const double dt = 1.0/60.0; // viewer is vsynced to 60 FPS; keys sampled each loop
        const double speed = 1.5; // radians/sec
        if (viewer_key_down(ViewerKey_Right)) angleY += speed*dt;
        if (viewer_key_down(ViewerKey_Left))  angleY -= speed*dt;
        if (viewer_key_down(ViewerKey_Up))    angleX += speed*dt;
        if (viewer_key_down(ViewerKey_Down))  angleX -= speed*dt;

        // -- Update rotation from input, then build model rotation matrices (Y then X)
        const double cy = std::cos(angleY), sy = std::sin(angleY);
        const double cx = std::cos(angleX), sx = std::sin(angleX);
        mat<4,4> RotY = {{{ cy, 0, sy, 0}, {0, 1, 0, 0}, {-sy, 0, cy, 0}, {0, 0, 0, 1}}};
        mat<4,4> RotX = {{{ 1, 0, 0, 0}, {0, cx, -sx, 0}, {0, sx, cx, 0}, {0, 0, 0, 1}}};
        mat<4,4> Model = RotY * RotX;

        // -- Clear CPU framebuffer and z-buffer
        for (int i=0; i<width*height; ++i) zbuffer[i] = -std::numeric_limits<double>::max();
        TGAColor clear; clear[0]=30; clear[1]=30; clear[2]=30; clear.bytespp=4;
        for (int y=0; y<height; ++y) for (int x=0; x<width; ++x) framebuffer.set(x,y,clear);

        // -- CPU rasterization of all loaded models
        for (const auto &model : models) {
            for (int i=0; i<model.nfaces(); i++) {
                vec4 clip[3];
                for (int d : {0,1,2}) {
                    vec3 v = model.vert(i, d);
                    clip[d] = Perspective * ModelView * Model * vec4{v.x, v.y, v.z, 1.};
                }
                TGAColor rnd; for (int c=0; c<3; c++) rnd[c] = std::rand()%255;
                rasterize(clip, zbuffer, framebuffer, rnd);
            }
        }

        viewer_present_from_tga(framebuffer, rgba);
    }
    viewer_shutdown();
    return 0;
    
}
