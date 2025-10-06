#include <limits>
#include <algorithm>
#include "geometry.h"
#include "model.h"
#include "tgaimage.h"
#include "viewer.h"
#include "rasterizer.h"

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

TGAColor hsv_to_rgb(double hue, double saturation = 1.0, double value = 1.0) {
    // Normalize hue to [0, 360)
    hue = fmod(hue, 360.0);
    if (hue < 0) hue += 360.0;
    
    // HSV to RGB conversion
    double h = hue / 60.0;
    int sector = (int)h;
    double f = h - sector;
    double p = 0.0, q = 1.0 - f, t = f;
    
    double r, g, b;
    switch (sector % 6) {
        case 0: r = 1.0; g = t; b = 0.0; break;
        case 1: r = q; g = 1.0; b = 0.0; break;
        case 2: r = 0.0; g = 1.0; b = t; break;
        case 3: r = 0.0; g = q; b = 1.0; break;
        case 4: r = t; g = 0.0; b = 1.0; break;
        case 5: r = 1.0; g = 0.0; b = q; break;
        default: r = 1.0; g = 0.0; b = 0.0; break;
    }
    
    // Apply saturation and value
    r = r * saturation * value;
    g = g * saturation * value;
    b = b * saturation * value;
    
    TGAColor color;
    color[0] = (unsigned char)(r * 255);  // Red
    color[1] = (unsigned char)(g * 255);  // Green  
    color[2] = (unsigned char)(b * 255);  // Blue
    color.bytespp = 3;
    
    return color;
}


void render_frame(const std::vector<Model>& models, TGAImage& framebuffer, std::vector<double>& zbuffer, 
                 std::vector<unsigned char>& rgba, double angleX, double angleY) {
    const int width = framebuffer.width();
    const int height = framebuffer.height();
    
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
    cpu_rasterize_models(models, framebuffer, zbuffer, Model);

    viewer_present_from_tga(framebuffer, rgba);
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

        render_frame(models, framebuffer, zbuffer, rgba, angleX, angleY);
    }
    viewer_shutdown();
    return 0;
    
}
