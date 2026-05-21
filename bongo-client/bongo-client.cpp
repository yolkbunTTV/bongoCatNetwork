// bongo-client: runs on the streaming PC. Receives input snapshots from
// bongo-server over UDP and renders the bongo cat overlay so OBS can capture
// it. Reuses the existing bongocat-osu render code (osu/taiko/ctb/mania/
// custom) — only the input acquisition step is redirected to the network
// namespace, see input.cpp.

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include "header.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <regex>
#include <string>

#pragma comment(lib, "ws2_32.lib")

sf::RenderWindow window;

#pragma pack(push, 1)
struct BongoPacket {
    uint32_t magic;
    uint8_t  version;
    uint8_t  flags;
    uint16_t sequence;
    int32_t  mouse_x;
    int32_t  mouse_y;
    int32_t  screen_w;
    int32_t  screen_h;
    int32_t  osu_left;
    int32_t  osu_top;
    int32_t  osu_right;
    int32_t  osu_bottom;
    uint8_t  key_bitmap[32];
};
#pragma pack(pop)
static_assert(sizeof(BongoPacket) == 72, "wire format must be 72 bytes");

static const uint32_t BONGO_MAGIC = 0x4F474E42u; // 'B''N''G''O' little-endian

struct ClientSettings {
    int listen_port = 47812;
    std::string allowed_server_ip; // empty = accept any source
};

static ClientSettings LoadClientSettings(const char* path) {
    ClientSettings s;
    std::ifstream infile(path);
    if (!infile.is_open()) return s;
    std::string line;
    std::string values[2];
    int cc = 0;
    while (std::getline(infile, line)) {
        std::size_t index = line.find(":");
        if (index < 100 && cc < 2) {
            std::string lin = line.substr(index + 1, std::string::npos);
            values[cc] = std::regex_replace(lin, std::regex("^ +| +$|( ) +"), "$1");
            cc++;
        }
    }
    if (cc >= 1 && !values[0].empty()) s.listen_port = std::stoi(values[0]);
    if (cc >= 2 && values[1] != "any") s.allowed_server_ip = values[1];
    return s;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    ClientSettings net_cfg = LoadClientSettings("client settings.txt");

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        MessageBoxA(nullptr, "WSAStartup failed", "bongo-client", MB_OK | MB_ICONERROR);
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        MessageBoxA(nullptr, "socket() failed", "bongo-client", MB_OK | MB_ICONERROR);
        WSACleanup();
        return 1;
    }
    u_long nb = 1;
    ioctlsocket(sock, FIONBIO, &nb);
    int rcvbuf = 1 << 16;
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&rcvbuf), sizeof(rcvbuf));

    in_addr allowed_addr{};
    bool have_allowed = false;
    if (!net_cfg.allowed_server_ip.empty()) {
        if (InetPtonA(AF_INET, net_cfg.allowed_server_ip.c_str(), &allowed_addr) == 1) {
            have_allowed = true;
        }
    }

    sockaddr_in bind_addr{};
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind_addr.sin_port = htons(static_cast<u_short>(net_cfg.listen_port));
    if (bind(sock, reinterpret_cast<sockaddr*>(&bind_addr), sizeof(bind_addr)) == SOCKET_ERROR) {
        char msg[128];
        std::snprintf(msg, sizeof(msg), "bind() to port %d failed (err %d)",
                      net_cfg.listen_port, WSAGetLastError());
        MessageBoxA(nullptr, msg, "bongo-client", MB_OK | MB_ICONERROR);
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    // Activate remote-input mode before input::init / render code runs.
    network::active = true;
    // Seed sane defaults so the first frame doesn't divide by zero.
    network::screen_w = GetSystemMetrics(SM_CXSCREEN);
    network::screen_h = GetSystemMetrics(SM_CYSCREEN);

    window.create(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "BONGO CLIENT",
                  sf::Style::Titlebar | sf::Style::Close);
    window.setFramerateLimit(MAX_FRAMERATE);

    while (!data::init()) {
        continue;
    }
    if (!input::init()) {
        closesocket(sock);
        WSACleanup();
        return EXIT_FAILURE;
    }

    bool is_reload = false;
    bool is_show_input_debug = false;

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            switch (event.type) {
            case sf::Event::Closed:
                window.close();
                break;

            case sf::Event::KeyPressed:
                if (event.key.code == sf::Keyboard::R && event.key.control) {
                    if (!is_reload) {
                        while (!data::init()) {
                            continue;
                        }
                    }
                    is_reload = true;
                    break;
                }
                if (event.key.code == sf::Keyboard::D && event.key.control) {
                    is_show_input_debug = !is_show_input_debug;
                    break;
                }

            default:
                is_reload = false;
            }
        }

        // Drain the UDP socket: newest valid packet wins. Each packet is a
        // full state snapshot so dropped packets are harmless. Cap iterations
        // so a flood from a hostile sender can't stall the render loop.
        for (int drain = 0; drain < 256; ++drain) {
            BongoPacket buf;
            sockaddr_in src{};
            int srclen = sizeof(src);
            int n = recvfrom(sock, reinterpret_cast<char*>(&buf), sizeof(buf), 0,
                             reinterpret_cast<sockaddr*>(&src), &srclen);
            if (n == SOCKET_ERROR || n <= 0) break;
            if (n != sizeof(BongoPacket)) continue;
            if (buf.magic != BONGO_MAGIC || buf.version != 1) continue;
            if (have_allowed && src.sin_addr.s_addr != allowed_addr.s_addr) continue;

            std::memcpy(network::key_bitmap, buf.key_bitmap, sizeof(network::key_bitmap));
            network::mouse_x = buf.mouse_x;
            network::mouse_y = buf.mouse_y;
            int32_t sw = buf.screen_w, sh = buf.screen_h;
            if (sw < 16 || sw > 32767) sw = network::screen_w;
            if (sh < 16 || sh > 32767) sh = network::screen_h;
            network::screen_w = sw;
            network::screen_h = sh;
            network::osu_foreground = (buf.flags & 0x01) != 0;
            network::osu_left = buf.osu_left;
            network::osu_top = buf.osu_top;
            network::osu_right = buf.osu_right;
            network::osu_bottom = buf.osu_bottom;
        }

        int mode = data::cfg["mode"].asInt();

        Json::Value rgb = data::cfg["decoration"]["rgb"];
        int red_value = rgb[0].asInt();
        int green_value = rgb[1].asInt();
        int blue_value = rgb[2].asInt();
        int alpha_value = rgb.size() == 3 ? 255 : rgb[3].asInt();

        window.clear(sf::Color(red_value, green_value, blue_value, alpha_value));
        switch (mode) {
        case 1: osu::draw();    break;
        case 2: taiko::draw();  break;
        case 3: ctb::draw();    break;
        case 4: mania::draw();  break;
        case 5: custom::draw(); break;
        }

        if (is_show_input_debug) {
            input::drawDebugPanel();
        }

        window.display();
    }

    input::cleanup();
    closesocket(sock);
    WSACleanup();
    return 0;
}
