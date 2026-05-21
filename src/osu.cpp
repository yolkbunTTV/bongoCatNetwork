#include "header.hpp"

namespace osu {
Json::Value left_key_value, right_key_value, smoke_key_value, wave_key_value;
int offset_x, offset_y;
int paw_r, paw_g, paw_b, paw_a;
int paw_edge_r, paw_edge_g, paw_edge_b, paw_edge_a;
int paw_tip_r, paw_tip_g, paw_tip_b, paw_tip_a;
int paw_edge_tip_r, paw_edge_tip_g, paw_edge_tip_b, paw_edge_tip_a;
double paw_tip_length;

static inline sf::Color lerp_color(int r1, int g1, int b1, int a1,
                                   int r2, int g2, int b2, int a2, double t) {
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    return sf::Color(
        (sf::Uint8)(r1 + (r2 - r1) * t),
        (sf::Uint8)(g1 + (g2 - g1) * t),
        (sf::Uint8)(b1 + (b2 - b1) * t),
        (sf::Uint8)(a1 + (a2 - a1) * t)
    );
}
double scale;
bool is_mouse, is_left_handed, is_enable_toggle_smoke;
sf::Sprite bg, up, left, right, device, smoke, wave;

int key_state = 0;

bool left_key_state = false;
bool right_key_state = false;
bool wave_key_state = false;
bool previous_smoke_key_state = false;
bool current_smoke_key_state = false;
bool is_toggle_smoke = false;
double timer_left_key = -1;
double timer_right_key = -1;
double timer_wave_key = -1;

bool init() {
    // getting configs
    Json::Value osu = data::cfg["osu"];

    is_mouse = osu["mouse"].asBool();
    is_enable_toggle_smoke = osu["toggleSmoke"].asBool();

    paw_r = osu["paw"][0].asInt();
    paw_g = osu["paw"][1].asInt();
    paw_b = osu["paw"][2].asInt();
    paw_a = osu["paw"].size() == 3 ? 255 : osu["paw"][3].asInt();

    paw_edge_r = osu["pawEdge"][0].asInt();
    paw_edge_g = osu["pawEdge"][1].asInt();
    paw_edge_b = osu["pawEdge"][2].asInt();
    paw_edge_a = osu["pawEdge"].size() == 3 ? 255 : osu["pawEdge"][3].asInt();

    // pawTip / pawEdgeTip are optional; when omitted the arm is a single color
    if (osu.isMember("pawTip") && osu["pawTip"].isArray() && osu["pawTip"].size() >= 3) {
        paw_tip_r = osu["pawTip"][0].asInt();
        paw_tip_g = osu["pawTip"][1].asInt();
        paw_tip_b = osu["pawTip"][2].asInt();
        paw_tip_a = osu["pawTip"].size() == 3 ? 255 : osu["pawTip"][3].asInt();
    } else {
        paw_tip_r = paw_r; paw_tip_g = paw_g; paw_tip_b = paw_b; paw_tip_a = paw_a;
    }

    if (osu.isMember("pawEdgeTip") && osu["pawEdgeTip"].isArray() && osu["pawEdgeTip"].size() >= 3) {
        paw_edge_tip_r = osu["pawEdgeTip"][0].asInt();
        paw_edge_tip_g = osu["pawEdgeTip"][1].asInt();
        paw_edge_tip_b = osu["pawEdgeTip"][2].asInt();
        paw_edge_tip_a = osu["pawEdgeTip"].size() == 3 ? 255 : osu["pawEdgeTip"][3].asInt();
    } else {
        paw_edge_tip_r = paw_edge_r; paw_edge_tip_g = paw_edge_g;
        paw_edge_tip_b = paw_edge_b; paw_edge_tip_a = paw_edge_a;
    }

    // fraction of the arm (from the cursor end backwards) that fades to the tip color
    paw_tip_length = osu.isMember("pawTipLength") ? osu["pawTipLength"].asDouble() : 0.45;
    if (paw_tip_length < 0.0) paw_tip_length = 0.0;
    if (paw_tip_length > 1.0) paw_tip_length = 1.0;

    bool chk[256];
    std::fill(chk, chk + 256, false);
    left_key_value = osu["key1"];
    for (Json::Value &v : left_key_value) {
        chk[v.asInt()] = true;
    }
    right_key_value = osu["key2"];
    for (Json::Value &v : right_key_value) {
        if (chk[v.asInt()]) {
            data::error_msg("Overlapping osu! keybinds", "Error reading configs");
            return false;
        }
    }
    wave_key_value = osu["wave"];
    for (Json::Value &v : wave_key_value) {
        if (chk[v.asInt()]) {
            data::error_msg("Overlapping osu! keybinds", "Error reading configs");
            return false;
        }
    }
    smoke_key_value = osu["smoke"];

    is_left_handed = data::cfg["decoration"]["leftHanded"].asBool();

    if (is_mouse) {
        offset_x = (data::cfg["decoration"]["offsetX"])[0].asInt();
        offset_y = (data::cfg["decoration"]["offsetY"])[0].asInt();
        scale = (data::cfg["decoration"]["scalar"])[0].asDouble();
    } else {
        offset_x = (data::cfg["decoration"]["offsetX"])[1].asInt();
        offset_y = (data::cfg["decoration"]["offsetY"])[1].asInt();
        scale = (data::cfg["decoration"]["scalar"])[1].asDouble();
    }

    // importing sprites
    up.setTexture(data::load_texture("img/osu/up.png"));
    left.setTexture(data::load_texture("img/osu/left.png"));
    right.setTexture(data::load_texture("img/osu/right.png"));
    wave.setTexture(data::load_texture("img/osu/wave.png"));
    if (is_mouse) {
        bg.setTexture(data::load_texture("img/osu/mousebg.png"));
        device.setTexture(data::load_texture("img/osu/mouse.png"), true);
    } else {
        bg.setTexture(data::load_texture("img/osu/tabletbg.png"));
        device.setTexture(data::load_texture("img/osu/tablet.png"), true);
    }
    smoke.setTexture(data::load_texture("img/osu/smoke.png"));
    device.setScale(scale, scale);

    return true;
}

struct ArmSlice { sf::Vector2f a, b; double p; };

static void draw_arm_strip(const std::vector<ArmSlice> &slices,
                           const sf::Color &base, const sf::Color &tip,
                           double p_threshold) {
    sf::VertexArray strip(sf::TriangleStrip);
    for (size_t k = 0; k < slices.size(); k++) {
        const ArmSlice &s = slices[k];
        if (k > 0) {
            const ArmSlice &prev = slices[k - 1];
            bool prev_tip = prev.p >= p_threshold;
            bool curr_tip = s.p >= p_threshold;
            if (prev_tip != curr_tip && (s.p != prev.p)) {
                double alpha = (p_threshold - prev.p) / (s.p - prev.p);
                sf::Vector2f ia = prev.a + (s.a - prev.a) * (float)alpha;
                sf::Vector2f ib = prev.b + (s.b - prev.b) * (float)alpha;
                const sf::Color &col_prev = prev_tip ? tip : base;
                const sf::Color &col_curr = curr_tip ? tip : base;
                strip.append(sf::Vertex(ia, col_prev));
                strip.append(sf::Vertex(ib, col_prev));
                strip.append(sf::Vertex(ia, col_curr));
                strip.append(sf::Vertex(ib, col_curr));
            }
        }
        const sf::Color &col = (s.p >= p_threshold) ? tip : base;
        strip.append(sf::Vertex(s.a, col));
        strip.append(sf::Vertex(s.b, col));
    }
    window.draw(strip);
}

void draw() {
    window.draw(bg);

    // initializing pss and pss2 (kuvster's magic)
    Json::Value paw_draw_info = data::cfg["mousePaw"];
    int x_paw_start = paw_draw_info["pawStartingPoint"][0].asInt();
    int y_paw_start = paw_draw_info["pawStartingPoint"][1].asInt();
    auto [x, y] = input::get_xy();
    int oof = 6;
    std::vector<double> pss = {(float) x_paw_start, (float) y_paw_start};
    double dist = hypot(x_paw_start - x, y_paw_start - y);
    double centreleft0 = x_paw_start - 0.7237 * dist / 2;
    double centreleft1 = y_paw_start + 0.69 * dist / 2;
    for (int i = 1; i < oof; i++) {
        std::vector<double> bez = {(float) x_paw_start, (float) y_paw_start, centreleft0, centreleft1, x, y};
        auto [p0, p1] = input::bezier(1.0 * i / oof, bez, 6);
        pss.push_back(p0);
        pss.push_back(p1);
    }
    pss.push_back(x);
    pss.push_back(y);
    double a = y - centreleft1;
    double b = centreleft0 - x;
    double le = hypot(a, b);
    a = x + a / le * 60;
    b = y + b / le * 60;
    int x_paw_end = paw_draw_info["pawEndingPoint"][0].asInt();
    int y_paw_end = paw_draw_info["pawEndingPoint"][1].asInt();
    dist = hypot(x_paw_end - a, y_paw_end - b);
    double centreright0 = x_paw_end - 0.6 * dist / 2;
    double centreright1 = y_paw_end + 0.8 * dist / 2;
    int push = 20;
    double s = x - centreleft0;
    double t = y - centreleft1;
    le = hypot(s, t);
    s *= push / le;
    t *= push / le;
    double s2 = a - centreright0;
    double t2 = b - centreright1;
    le = hypot(s2, t2);
    s2 *= push / le;
    t2 *= push / le;
    for (int i = 1; i < oof; i++) {
        std::vector<double> bez = {x, y, x + s, y + t, a + s2, b + t2, a, b};
        auto [p0, p1] = input::bezier(1.0 * i / oof, bez, 8);
        pss.push_back(p0);
        pss.push_back(p1);
    }
    pss.push_back(a);
    pss.push_back(b);
    for (int i = oof - 1; i > 0; i--) {
        std::vector<double> bez = {1.0 * x_paw_end, 1.0 * y_paw_end, centreright0, centreright1, a, b};
        auto [p0, p1] = input::bezier(1.0 * i / oof, bez, 6);
        pss.push_back(p0);
        pss.push_back(p1);
    }
    pss.push_back(x_paw_end);
    pss.push_back(y_paw_end);
    double mpos0 = (a + x) / 2 - 52 - 15;
    double mpos1 = (b + y) / 2 - 34 + 5;
    double dx = -38;
    double dy = -50;

    const int iter = 25;

    std::vector<double> pss2 = {pss[0] + dx, pss[1] + dy};
    for (int i = 1; i < iter; i++) {
        auto [p0, p1] = input::bezier(1.0 * i / iter, pss, 38);
        pss2.push_back(p0 + dx);
        pss2.push_back(p1 + dy);
    }
    pss2.push_back(pss[36] + dx);
    pss2.push_back(pss[37] + dy);

    device.setPosition(mpos0 + dx + offset_x, mpos1 + dy + offset_y);

    // drawing mouse
    if (is_mouse) {
        window.draw(device);
    }

    // drawing arms
    double p_threshold = 1.0 - paw_tip_length;
    sf::Color paw_col(paw_r, paw_g, paw_b, paw_a);
    sf::Color paw_tip_col(paw_tip_r, paw_tip_g, paw_tip_b, paw_tip_a);
    int shad = paw_edge_a / 3;
    int shad_tip = paw_edge_tip_a / 3;

    // fill cross-sections: pair pss2[i] with pss2[52-i-2], from shoulder (i=0) to tip (i=24)
    std::vector<ArmSlice> fill_slices;
    fill_slices.reserve(13);
    for (int i = 0; i < 26; i += 2) {
        ArmSlice s;
        s.a = sf::Vector2f((float)pss2[i], (float)pss2[i + 1]);
        s.b = sf::Vector2f((float)pss2[52 - i - 2], (float)pss2[52 - i - 1]);
        s.p = i / 24.0;
        fill_slices.push_back(s);
    }
    draw_arm_strip(fill_slices, paw_col, paw_tip_col, p_threshold);

    // edge cross-sections: walk the spine pss2[0],[2],...,[50] with shrinking width
    auto build_edge_slices = [&](double w_start, double w_decay) {
        std::vector<ArmSlice> out;
        out.reserve(26);
        double w = w_start;
        for (int i = 0; i <= 50; i += 2) {
            double vec0, vec1;
            if (i < 50) {
                vec0 = pss2[i] - pss2[i + 2];
                vec1 = pss2[i + 1] - pss2[i + 3];
            } else {
                vec0 = pss2[48] - pss2[50];
                vec1 = pss2[49] - pss2[51];
            }
            double d = hypot(vec0, vec1);
            ArmSlice s;
            s.a = sf::Vector2f((float)(pss2[i] + vec1 / d * w / 2), (float)(pss2[i + 1] - vec0 / d * w / 2));
            s.b = sf::Vector2f((float)(pss2[i] - vec1 / d * w / 2), (float)(pss2[i + 1] + vec0 / d * w / 2));
            s.p = 1.0 - fabs(i - 25.0) / 25.0;
            out.push_back(s);
            if (i < 50) w -= w_decay;
        }
        return out;
    };

    // first arm arc (shadow, alpha/3)
    auto edge_slices = build_edge_slices(7.0, 0.08);
    {
        double w0 = 7.0;
        sf::CircleShape circ(w0 / 2);
        circ.setFillColor(sf::Color(paw_edge_r, paw_edge_g, paw_edge_b, shad));
        circ.setPosition(pss2[0] - w0 / 2, pss2[1] - w0 / 2);
        window.draw(circ);
    }
    draw_arm_strip(edge_slices,
                   sf::Color(paw_edge_r, paw_edge_g, paw_edge_b, shad),
                   sf::Color(paw_edge_tip_r, paw_edge_tip_g, paw_edge_tip_b, shad_tip),
                   p_threshold);
    {
        double w1 = 7.0 - 0.08 * 25;
        sf::CircleShape circ(w1 / 2);
        circ.setFillColor(sf::Color(paw_edge_r, paw_edge_g, paw_edge_b, shad));
        circ.setPosition(pss2[50] - w1 / 2, pss2[51] - w1 / 2);
        window.draw(circ);
    }

    // second arm arc (full alpha)
    auto edge2_slices = build_edge_slices(6.0, 0.08);
    {
        double w0 = 6.0;
        sf::CircleShape circ2(w0 / 2);
        circ2.setFillColor(sf::Color(paw_edge_r, paw_edge_g, paw_edge_b, paw_edge_a));
        circ2.setPosition(pss2[0] - w0 / 2, pss2[1] - w0 / 2);
        window.draw(circ2);
    }
    draw_arm_strip(edge2_slices,
                   sf::Color(paw_edge_r, paw_edge_g, paw_edge_b, paw_edge_a),
                   sf::Color(paw_edge_tip_r, paw_edge_tip_g, paw_edge_tip_b, paw_edge_tip_a),
                   p_threshold);
    {
        double w1 = 6.0 - 0.08 * 25;
        sf::CircleShape circ2(w1 / 2);
        circ2.setFillColor(sf::Color(paw_edge_r, paw_edge_g, paw_edge_b, paw_edge_a));
        circ2.setPosition(pss2[50] - w1 / 2, pss2[51] - w1 / 2);
        window.draw(circ2);
    }

    // drawing keypresses
    bool left_key = false;

    for (Json::Value &v : left_key_value) {
        if (input::is_pressed(v.asInt())) {
            left_key = true;
            break;
        }
    }

    if (left_key) {
        if (!left_key_state) {
            key_state = 1;
            left_key_state = true;
        }
    } else {
        left_key_state = false;
    }

    bool right_key = false;

    for (Json::Value &v : right_key_value) {
        if (input::is_pressed(v.asInt())) {
            right_key = true;
            break;
        }
    }

    if (right_key) {
        if (!right_key_state) {
            key_state = 2;
            right_key_state = true;
        }
    } else {
        right_key_state = false;
    }
    
    bool wave_key = false;

    for (Json::Value &v : wave_key_value) {
        if (input::is_pressed(v.asInt())) {
            wave_key = true;
            break;
        }
    }

    if (wave_key) {
        if (!wave_key_state) {
            key_state = 3;
            wave_key_state = true;
        }
    } else {
        wave_key_state = false;
    }

    if (!left_key_state && !right_key_state && !wave_key_state) {
        key_state = 0;
        window.draw(up);
    }

    if (key_state == 1) {
        if ((clock() - std::max(timer_right_key, timer_wave_key)) / CLOCKS_PER_SEC > BONGO_KEYPRESS_THRESHOLD) {
            if (!is_left_handed) {
                window.draw(left);
            } else {
                window.draw(right);
            }
            timer_left_key = clock();
        } else {
            window.draw(up);
        }
    } else if (key_state == 2) {
        if ((clock() - std::max(timer_left_key, timer_wave_key)) / CLOCKS_PER_SEC > BONGO_KEYPRESS_THRESHOLD) {
            if (!is_left_handed) {
                window.draw(right);
            } else {
                window.draw(left);
            }
            timer_right_key = clock();
        } else {
            window.draw(up);
        }
    } else if (key_state == 3) {
        if ((clock() - std::max(timer_left_key, timer_right_key)) / CLOCKS_PER_SEC > BONGO_KEYPRESS_THRESHOLD) {
            window.draw(wave);
            timer_wave_key = clock();
        } else {
            window.draw(up);
        }
    }

    // drawing tablet
    if (!is_mouse) {
        window.draw(device);
    }
    
    // draw smoke
    bool is_smoke_key_pressed = false;

    for (Json::Value &v : smoke_key_value) {
        if (input::is_pressed(v.asInt())) {
            is_smoke_key_pressed = true;
            break;
        }
    }

    if (is_enable_toggle_smoke) {
        previous_smoke_key_state = current_smoke_key_state;
        current_smoke_key_state = is_smoke_key_pressed;

        bool is_smoke_key_down = current_smoke_key_state && (current_smoke_key_state != previous_smoke_key_state);

        if (is_smoke_key_down) {
            is_toggle_smoke = !is_toggle_smoke;
        }
    }
    else {
        is_toggle_smoke = is_smoke_key_pressed;
    }

    if (is_toggle_smoke) {
        window.draw(smoke);
    }
}
}; // namespace osu
