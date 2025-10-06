#pragma once
#include <vector>
#include "tgaimage.h"

enum ViewerKey { ViewerKey_Left, ViewerKey_Right, ViewerKey_Up, ViewerKey_Down };

bool viewer_init(int width, int height, const char* title);
bool viewer_should_close();
bool viewer_key_down(ViewerKey key);
void viewer_present_from_tga(const TGAImage &img, std::vector<unsigned char> &rgbaScratch);
void viewer_present_with_timing(const TGAImage &img, std::vector<unsigned char> &rgbaScratch, 
                                double render_time_ms, double angleX, double angleY);
void viewer_shutdown();



