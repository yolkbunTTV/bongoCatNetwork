#pragma once
#define BONGO_KEYPRESS_THRESHOLD 0
#define WINDOW_WIDTH 612
#define WINDOW_HEIGHT 352
#define MAX_FRAMERATE 60

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <map>

#include <time.h>

#include <math.h>
#include <string.h>

#include <SFML/Graphics.hpp>
#include "json/json.h"

#include <cstdint>

extern sf::RenderWindow window;

// Remote state injected by bongo-client when running in 2-PC mode.
// When `active` is true, input:: reads from these fields instead of polling
// the local OS. The server fills these via UDP packets — see bongo-server.cpp
// and bongo-client.cpp.
namespace network {
extern bool active;
extern uint8_t key_bitmap[32];   // 256 VK codes, MSB-first within each byte
extern int32_t mouse_x, mouse_y;
extern int32_t screen_w, screen_h;
extern bool osu_foreground;
extern int32_t osu_left, osu_top, osu_right, osu_bottom;

inline bool bitmap_has(int key_code) {
    if (key_code < 0 || key_code > 255) return false;
    return (key_bitmap[key_code >> 3] & (0x80 >> (key_code & 7))) != 0;
}
}; // namespace network

namespace data {
extern Json::Value cfg;

void error_msg(std::string error, std::string title);

bool init();

sf::Texture &load_texture(std::string path);
}; // namespace data

namespace input {
bool init();

bool is_pressed(int key_code);

bool is_joystick_connected();
bool is_joystick_pressed(int key_code);

std::pair<double, double> bezier(double ratio, std::vector<double> &points, int length);

std::pair<double, double> get_xy();

void drawDebugPanel();

void cleanup();
}; // namespace input

namespace osu {
bool init();

void draw();
}; // namespace osu

namespace taiko {
bool init();

void draw();
}; // namespace taiko

namespace ctb {
bool init();

void draw();
}; // namespace ctb

namespace mania {
bool init();

void draw();
}; // namespace mania

namespace custom {
bool init();

void draw();
}; // namespace custom
