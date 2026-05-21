// bongo-server: runs on the game PC. Captures keyboard + mouse + the
// foreground osu! window rect and broadcasts a state snapshot to bongo-client
// over UDP on the LAN. Mirrors the 2-PC streaming setup from pengu-overlay.

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <regex>
#include <string>
#include <thread>

#pragma comment(lib, "ws2_32.lib")

#pragma pack(push, 1)
struct BongoPacket {
    uint32_t magic;       // 'B''N''G''O'
    uint8_t  version;     // 1
    uint8_t  flags;       // bit0 = osu! foreground
    uint16_t sequence;
    int32_t  mouse_x;
    int32_t  mouse_y;
    int32_t  screen_w;
    int32_t  screen_h;
    int32_t  osu_left;
    int32_t  osu_top;
    int32_t  osu_right;
    int32_t  osu_bottom;
    uint8_t  key_bitmap[32]; // 256 VK codes, MSB-first within each byte
};
#pragma pack(pop)
static_assert(sizeof(BongoPacket) == 72, "wire format must be 72 bytes");

static const uint32_t BONGO_MAGIC = 0x4F474E42u; // 'B''N''G''O' little-endian

static void GetDesktopResolution(int& horizontal, int& vertical) {
    RECT desktop;
    const HWND hDesktop = GetDesktopWindow();
    GetWindowRect(hDesktop, &desktop);
    horizontal = desktop.right;
    vertical = desktop.bottom;
}

struct Settings {
    std::string client_ip = "127.0.0.1";
    int client_port = 47812;
    int send_hz = 120;
};

static Settings LoadSettings(const char* path) {
    Settings s;
    std::ifstream infile(path);
    if (!infile.is_open()) {
        std::printf("warning: could not open %s, using defaults\n", path);
        return s;
    }
    std::string line;
    std::string values[3];
    int cc = 0;
    while (std::getline(infile, line)) {
        std::size_t index = line.find(":");
        if (index < 100 && cc < 3) {
            std::string lin = line.substr(index + 1, std::string::npos);
            values[cc] = std::regex_replace(lin, std::regex("^ +| +$|( ) +"), "$1");
            cc++;
        }
    }
    if (cc >= 1 && !values[0].empty()) s.client_ip = values[0];
    if (cc >= 2 && !values[1].empty()) s.client_port = std::stoi(values[1]);
    if (cc >= 3 && !values[2].empty()) s.send_hz = std::stoi(values[2]);
    return s;
}

int main() {
    Settings cfg = LoadSettings("server settings.txt");
    if (cfg.send_hz < 1) cfg.send_hz = 1;
    if (cfg.send_hz > 1000) cfg.send_hz = 1000;

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::printf("WSAStartup failed\n");
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        std::printf("socket() failed: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(static_cast<u_short>(cfg.client_port));
    if (InetPtonA(AF_INET, cfg.client_ip.c_str(), &dest.sin_addr) != 1) {
        std::printf("invalid client ip: %s\n", cfg.client_ip.c_str());
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    std::printf("bongo-server: sending to %s:%d at %d Hz\n",
                cfg.client_ip.c_str(), cfg.client_port, cfg.send_hz);
    std::printf("(close this window to stop)\n");

    int screen_w = 0, screen_h = 0;
    GetDesktopResolution(screen_w, screen_h);

    const auto period = std::chrono::microseconds(1000000 / cfg.send_hz);
    auto next_tick = std::chrono::steady_clock::now();
    uint16_t seq = 0;

    for (;;) {
        BongoPacket pkt{};
        pkt.magic = BONGO_MAGIC;
        pkt.version = 1;
        pkt.sequence = seq++;

        POINT p{0, 0};
        GetCursorPos(&p);
        pkt.mouse_x = p.x;
        pkt.mouse_y = p.y;
        pkt.screen_w = screen_w;
        pkt.screen_h = screen_h;

        for (int i = 0; i < 256; ++i) {
            if (GetAsyncKeyState(i) & 0x8000) {
                pkt.key_bitmap[i >> 3] |= static_cast<uint8_t>(0x80 >> (i & 7));
            }
        }

        // Foreground osu! window detection — mirrors get_xy() in input.cpp.
        HWND handle = GetForegroundWindow();
        if (handle) {
            char title[256] = {0};
            GetWindowTextA(handle, title, sizeof(title) - 1);
            if (std::string(title).find("osu!") == 0) {
                RECT r;
                if (GetWindowRect(handle, &r)) {
                    pkt.flags |= 0x01;
                    pkt.osu_left = r.left;
                    pkt.osu_top = r.top;
                    pkt.osu_right = r.right;
                    pkt.osu_bottom = r.bottom;
                }
            }
        }

        sendto(sock, reinterpret_cast<const char*>(&pkt), sizeof(pkt), 0,
               reinterpret_cast<sockaddr*>(&dest), sizeof(dest));

        next_tick += period;
        std::this_thread::sleep_until(next_tick);
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}
