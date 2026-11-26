#include "game/debug/MenuFlow.h"
#if MYENGINE_DEVELOPER_TOOLS
#include "Input.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <vector>

namespace menuflow {
namespace {
struct State {
    bool loaded = false, active = false;
    std::vector<std::string> lines;
    size_t next = 0;
    std::string screen, detail, shot, command;
    std::string burstName;
    int burstLeft = 0, burstIndex = 0;
    uint64_t frame = 0;
    int wait = 0, waitLimit = 0, hold = 0;
    bool waiting = false, failed = false, done = false;
    float selfX = 640.0f, selfY = 360.0f, targetX = 640.0f, targetY = 360.0f;
    std::string follow, followScreen;
    std::string waitFor;
    myEngine::Input::TestFrame input;
    std::ofstream log;
};

State& Get()
{
    static State state;
    return state;
}

void Load(State& s)
{
    if (s.loaded) {
        return;
    }
    s.loaded = true;
    char path[MAX_PATH]{};
    const DWORD length = GetEnvironmentVariableA("MYENGINE_MENU_FLOW", path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return;
    }
    std::ifstream file(path);
    for (std::string line; std::getline(file, line);) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
            line.pop_back();
        }
        if (!line.empty() && line[0] != '#') {
            s.lines.push_back(line);
        }
    }
    s.active = !s.lines.empty();
    if (s.active) {
        std::filesystem::create_directories("generated/menu_flow");
        s.log.open("generated/menu_flow/" + std::filesystem::path(path).stem().string() + ".log");
        s.input.enabled = true;
    }
}

void Log(State& s, const std::string& text)
{
    s.log << s.frame << " [" << s.screen << (s.detail.empty() ? "" : ":" + s.detail) << "] " << text << std::endl;
}

uint8_t Key(const std::string& name)
{
    static const std::map<std::string, uint8_t> kKeys{
        {"W", DIK_W},         {"A", DIK_A},       {"S", DIK_S},         {"D", DIK_D},          {"Q", DIK_Q},          {"R", DIK_R},
        {"F", DIK_F},         {"UP", DIK_UP},     {"DOWN", DIK_DOWN},   {"LEFT", DIK_LEFT},    {"RIGHT", DIK_RIGHT},  {"ENTER", DIK_RETURN},
        {"ESC", DIK_ESCAPE},  {"SPACE", DIK_SPACE}, {"TAB", DIK_TAB},   {"1", DIK_1},          {"2", DIK_2},          {"3", DIK_3}};
    const auto found = kKeys.find(name);
    return found == kKeys.end() ? uint8_t{0} : found->second;
}

WORD Button(const std::string& name)
{
    static const std::map<std::string, WORD> kButtons{{"A", XINPUT_GAMEPAD_A},          {"B", XINPUT_GAMEPAD_B},
                                                      {"X", XINPUT_GAMEPAD_X},          {"Y", XINPUT_GAMEPAD_Y},
                                                      {"START", XINPUT_GAMEPAD_START},  {"BACK", XINPUT_GAMEPAD_BACK},
                                                      {"UP", XINPUT_GAMEPAD_DPAD_UP},   {"DOWN", XINPUT_GAMEPAD_DPAD_DOWN},
                                                      {"LEFT", XINPUT_GAMEPAD_DPAD_LEFT}, {"RIGHT", XINPUT_GAMEPAD_DPAD_RIGHT}};
    const auto found = kButtons.find(name);
    return found == kButtons.end() ? WORD{0} : found->second;
}

SHORT Axis(float value)
{
    return static_cast<SHORT>(std::clamp(value, -1.0f, 1.0f) * 32767.0f);
}

/// @brief 押している物を全部離す。
void Release(State& s)
{
    s.input.keys.fill(0);
    s.input.presses.fill(false);
    s.input.pad = {};
    s.input.leftButton = false;
}

/// @brief 目標を狙う・目標へ向かう入力にする。
void Follow(State& s)
{
    const float dx = s.targetX - s.selfX, dy = s.targetY - s.selfY;
    const float length = (std::max)(1.0f, std::sqrt(dx * dx + dy * dy));
    const float ux = dx / length, uy = dy / length;
    if (s.follow == "aim") {
        s.input.mouse = {s.targetX, s.targetY};
        s.input.mouseMoved = true;
    } else if (s.follow == "aimpad") {
        s.input.pad.sThumbRX = Axis(ux);
        s.input.pad.sThumbRY = Axis(-uy);
    } else if (s.follow == "go") {
        s.input.keys[DIK_A] = ux < -0.38f ? 0x80 : 0;
        s.input.keys[DIK_D] = ux > 0.38f ? 0x80 : 0;
        s.input.keys[DIK_W] = uy < -0.38f ? 0x80 : 0;
        s.input.keys[DIK_S] = uy > 0.38f ? 0x80 : 0;
    } else if (s.follow == "gopad") {
        s.input.pad.sThumbLX = Axis(ux);
        s.input.pad.sThumbLY = Axis(-uy);
    }
}

bool Matches(const State& s, const std::string& name)
{
    return s.screen == name || (name.size() > 1 && name.back() == '*' && s.screen.rfind(name.substr(0, name.size() - 1), 0) == 0);
}

void Run(State& s, myEngine::Input& input)
{
    std::istringstream line(s.lines[s.next++]);
    std::string op, a;
    line >> op >> a;
    Log(s, s.lines[s.next - 1]);
    if (op == "wait") {
        int limit = 0;
        s.waitFor = a;
        s.waitLimit = line >> limit ? limit : 1200;
        s.waiting = true;
    } else if (op == "expect") {
        std::string detail;
        line >> detail;
        if (!Matches(s, a) || (!detail.empty() && s.detail.find(detail) == std::string::npos)) {
            Log(s, "FAIL expect " + a + " " + detail);
            s.failed = true;
        }
    } else if (op == "key" || op == "hold") {
        int frames = 3;
        if (op == "hold") {
            line >> frames;
        }
        if (const uint8_t key = Key(a)) {
            s.input.keys[key] = 0x80;
            s.input.presses[key] = true;
        } else {
            Log(s, "FAIL key " + a);
            s.failed = true;
        }
        s.hold = frames;
    } else if (op == "pad" || op == "padhold") {
        int frames = 3;
        if (op == "padhold") {
            line >> frames;
        }
        s.input.padConnected = true;
        s.input.pad.wButtons = Button(a);
        s.hold = frames;
    } else if (op == "stick") {
        float x = 0.0f, y = 0.0f;
        int frames = 3;
        line >> x >> y >> frames;
        s.input.padConnected = true;
        (a == "R" ? s.input.pad.sThumbRX : s.input.pad.sThumbLX) = Axis(x);
        (a == "R" ? s.input.pad.sThumbRY : s.input.pad.sThumbLY) = Axis(y);
        s.hold = frames;
    } else if (op == "mouse") {
        float y = 0.0f;
        line >> y;
        s.input.mouse = {static_cast<float>(std::atof(a.c_str())), y};
        s.input.mouseMoved = true;
        s.wait = 3;
    } else if (op == "aim" || op == "aimpad" || op == "go" || op == "gopad") {
        s.follow = op;
        s.followScreen = s.screen;
        s.hold = (std::max)(2, std::atoi(a.c_str()));
        s.input.padConnected = op == "aimpad" || op == "gopad" || s.input.padConnected;
    } else if (op == "click") {
        s.input.leftButton = true;
        s.hold = 3;
    } else if (op == "sleep") {
        s.wait = std::atoi(a.c_str());
    } else if (op == "shot") {
        s.shot = a;
        s.wait = 12;
    } else if (op == "frames") {
        s.burstName = a;
        line >> s.burstLeft;
        s.burstIndex = 0;
        std::error_code error;
        std::filesystem::create_directories("generated/menu_flow/" + a, error);
        Log(s, "frames " + a);
    } else if (op == "blur") {
        input.OnFocusChanged(false);
        s.wait = 3;
    } else if (op == "do") {
        s.command = a;
        s.wait = 3;
    } else if (op == "quit") {
        s.done = true;
    }
}
} // namespace

bool IsActive()
{
    Load(Get());
    return Get().active;
}

void SetScreen(const std::string& name, const std::string& detail)
{
    State& s = Get();
    if (s.active && (s.screen != name || s.detail != detail)) {
        s.screen = name;
        s.detail = detail;
        Log(s, "screen");
    }
}

bool TakeFrameShot(std::string& path)
{
    State& s = Get();
    if (!s.active || s.burstLeft <= 0) {
        return false;
    }
    char name[16]{};
    snprintf(name, sizeof(name), "%03d", s.burstIndex);
    path = "generated/menu_flow/" + s.burstName + "/" + name + ".png";
    Log(s, "frame " + s.burstName + " " + name);
    ++s.burstIndex;
    --s.burstLeft;
    return true;
}

void Note(const std::string& text)
{
    State& s = Get();
    if (s.active && s.burstLeft > 0) {
        Log(s, text);
    }
}

bool TakeShot(std::string& name)
{
    State& s = Get();
    if (!s.active || s.shot.empty()) {
        return false;
    }
    name = s.shot;
    s.shot.clear();
    return true;
}

void SetTarget(float selfX, float selfY, float targetX, float targetY)
{
    State& s = Get();
    s.selfX = selfX;
    s.selfY = selfY;
    s.targetX = targetX;
    s.targetY = targetY;
}

bool TakeCommand(const char* name)
{
    State& s = Get();
    if (!s.active || s.command != name) {
        return false;
    }
    s.command.clear();
    return true;
}

void Step(myEngine::Input& input)
{
    State& s = Get();
    Load(s);
    if (!s.active) {
        return;
    }
    ++s.frame;
    s.input.presses.fill(false);
    if (s.hold > 0) {
        if (!s.follow.empty()) {
            Follow(s);
            if (s.screen != s.followScreen) {
                s.hold = 1;
            }
        }
        if (--s.hold == 0) {
            s.follow.clear();
            Release(s);
            s.wait = 3;
        }
    } else if (s.wait > 0) {
        --s.wait;
    } else if (s.waiting) {
        if (Matches(s, s.waitFor)) {
            s.waiting = false;
            s.wait = 4;
        } else if (--s.waitLimit <= 0) {
            Log(s, "FAIL timeout " + s.waitFor);
            s.failed = true;
            s.waiting = false;
        }
    } else if (s.next < s.lines.size() && !s.done) {
        Run(s, input);
    } else {
        s.done = true;
    }
    input.SetTestFrame(s.input);
    s.input.mouseMoved = false;
    if (s.done && s.hold == 0) {
        Log(s, s.failed ? "RESULT FAIL" : "RESULT OK");
        s.log.close();
        s.active = false;
        PostQuitMessage(s.failed ? 3 : 0);
    }
}
} // namespace menuflow
#endif
