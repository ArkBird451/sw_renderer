#include "viewer.h"

#ifdef USE_RAYLIB
#include <raylib.h>
static Texture2D g_tex;
static bool g_initialized = false;
#endif

bool viewer_init(int width, int height, const char* title) {
#ifdef USE_RAYLIB
    InitWindow(width, height, title);
    SetTargetFPS(60);
    Image img = {0};
    img.width = width; img.height = height; img.mipmaps = 1; img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    std::vector<unsigned char> dummy(width*height*4, 255);
    img.data = dummy.data();
    g_tex = LoadTextureFromImage(img);
    g_initialized = true;
    return true;
#else
    (void)width; (void)height; (void)title; return false;
#endif
}

bool viewer_should_close() {
#ifdef USE_RAYLIB
    return WindowShouldClose();
#else
    return true;
#endif
}

bool viewer_key_down(ViewerKey key) {
#ifdef USE_RAYLIB
    switch (key) {
        case ViewerKey_Left:  return IsKeyDown(KEY_LEFT);
        case ViewerKey_Right: return IsKeyDown(KEY_RIGHT);
        case ViewerKey_Up:    return IsKeyDown(KEY_UP);
        case ViewerKey_Down:  return IsKeyDown(KEY_DOWN);
    }
    return false;
#else
    (void)key; return false;
#endif
}

void viewer_present_from_tga(const TGAImage &img, std::vector<unsigned char> &rgbaScratch) {
#ifdef USE_RAYLIB
    if (!g_initialized) return;
    if ((int)rgbaScratch.size() < img.width()*img.height()*4) rgbaScratch.resize(img.width()*img.height()*4);
    for (int y = 0; y < img.height(); ++y) {
        const int srcY = (img.height() - 1 - y);
        for (int x = 0; x < img.width(); ++x) {
            TGAColor c = img.get(x, srcY);
            const int idx = (y*img.width() + x) * 4;
            rgbaScratch[idx+0] = c[2];
            rgbaScratch[idx+1] = c[1];
            rgbaScratch[idx+2] = c[0];
            rgbaScratch[idx+3] = 255;
        }
    }
    // ==== Begin draw to window ====
    UpdateTexture(g_tex, rgbaScratch.data());
    BeginDrawing();
    ClearBackground(BLACK);
    DrawTexture(g_tex, 0, 0, WHITE);
    DrawText("Arrow keys: rotate", 10, 10, 20, RAYWHITE);
    EndDrawing();
#else
    (void)img; (void)rgbaScratch;
#endif
}

void viewer_present_with_timing(const TGAImage &img, std::vector<unsigned char> &rgbaScratch, 
                                double render_time_ms, double angleX, double angleY) {
#ifdef USE_RAYLIB
    if (!g_initialized) return;
    if ((int)rgbaScratch.size() < img.width()*img.height()*4) rgbaScratch.resize(img.width()*img.height()*4);
    for (int y = 0; y < img.height(); ++y) {
        const int srcY = (img.height() - 1 - y);
        for (int x = 0; x < img.width(); ++x) {
            TGAColor c = img.get(x, srcY);
            const int idx = (y*img.width() + x) * 4;
            rgbaScratch[idx+0] = c[2];
            rgbaScratch[idx+1] = c[1];
            rgbaScratch[idx+2] = c[0];
            rgbaScratch[idx+3] = 255;
        }
    }
    // ==== Begin draw to window ====
    UpdateTexture(g_tex, rgbaScratch.data());
    BeginDrawing();
    ClearBackground(BLACK);
    DrawTexture(g_tex, 0, 0, WHITE);
    
    // Display timing information on screen
    char timing_text[256];
    snprintf(timing_text, sizeof(timing_text), "Render Time: %.2f ms", render_time_ms);
    DrawText(timing_text, 10, 10, 20, GREEN);
    
    char angle_text[256];
    snprintf(angle_text, sizeof(angle_text), "Angle X: %.2f, Y: %.2f", angleX, angleY);
    DrawText(angle_text, 10, 35, 18, YELLOW);
    
    DrawText("Arrow keys: rotate", 10, 58, 16, RAYWHITE);
    EndDrawing();
#else
    (void)img; (void)rgbaScratch; (void)render_time_ms; (void)angleX; (void)angleY;
#endif
}

void viewer_shutdown() {
#ifdef USE_RAYLIB
    if (g_initialized) { UnloadTexture(g_tex); CloseWindow(); g_initialized = false; }
#endif
}


