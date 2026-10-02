#include "../DirectX/engine/commom/StartupTrace.h"
#include <Windows.h>
#include <json.hpp>
#include <cassert>
#include <cmath>
#include <iostream>
#include <thread>

namespace fs = std::filesystem;
using cg2::StartupTrace;

static nlohmann::json Read(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    assert(input);
    return nlohmann::json::parse(input);
}

int main(int argc, char** argv) {
    const std::string mode = argc > 1 ? argv[1] : "enabled";
    const fs::path directory = fs::current_path() / (L"trace_日本語_" +
        std::to_wstring(GetCurrentProcessId()) + L"_" + std::to_wstring(GetTickCount64()));
    fs::create_directories(directory);
    const fs::path output = directory / L"計測.json";
    _wputenv_s(L"CG2_STARTUP_TRACE_PATH", output.c_str());
    _wputenv_s(L"CG2_STARTUP_TRACE", mode == "disabled" ? L"0" : L"1");
    _wputenv_s(L"CG2_STARTUP_CACHE", L"0");

    if (mode == "disabled") {
        assert(!StartupTrace::Enabled());
        { StartupTrace::Scope scope("disabled scope"); StartupTrace::Count("disabled counter"); }
        StartupTrace::Mark("disabled marker");
        StartupTrace::Flush();
        assert(!fs::exists(output));
        std::cout << "PASS: trace disabled produces no output.\n";
        return 0;
    }

    assert(StartupTrace::Enabled());
    const std::string escapedName = "名前\"\\\n\t";
    StartupTrace::Mark(escapedName);
    {
        StartupTrace::Scope outer("outer");
        { StartupTrace::Scope child("child A"); StartupTrace::Count("count", 2.5); }
        {
            StartupTrace::Scope child("child B");
            { StartupTrace::Scope grandchild("grandchild"); StartupTrace::Count("count", 0.5); }
        }
        std::thread worker([] {
            StartupTrace::Scope root("worker root");
            StartupTrace::Scope child("worker child");
            StartupTrace::Count("worker.count");
        });
        worker.join();
    }

    if (mode == "unavailable") {
        const fs::path regularFile = directory / L"regular-file";
        { std::ofstream file(regularFile); file << "not a directory"; }
        const fs::path invalidPath = regularFile / L"trace.json";
        _wputenv_s(L"CG2_STARTUP_TRACE_PATH", invalidPath.c_str());
        StartupTrace::Flush();
        assert(!fs::exists(invalidPath));
        _wputenv_s(L"CG2_STARTUP_TRACE_PATH", output.c_str());
    }

    StartupTrace::Flush();
    const auto document = Read(output);
    assert(document.at("schema") == 1 && document.at("cacheEnabled") == false);
    assert(document.at("counters").at("count").get<double>() == 3.0);
    assert(document.at("counters").at("worker.count").get<double>() == 1.0);
    std::map<std::string, nlohmann::json> byName;
    for (const auto& event : document.at("events")) {
        byName[event.at("name").get<std::string>()] = event;
        const double duration = event.at("durationMs").get<double>();
        const double self = event.at("selfMs").get<double>();
        assert(duration >= 0 && self >= 0 && self <= duration);
    }
    assert(byName.count(escapedName) == 1);
    assert(byName.at("outer").at("depth") == 0);
    assert(byName.at("child A").at("depth") == 1);
    assert(byName.at("child B").at("depth") == 1);
    assert(byName.at("grandchild").at("depth") == 2);
    assert(byName.at("worker root").at("depth") == 0);
    assert(byName.at("worker child").at("depth") == 1);
    const auto duration = [&byName](const char* name) { return byName.at(name).at("durationMs").get<double>(); };
    const auto self = [&byName](const char* name) { return byName.at(name).at("selfMs").get<double>(); };
    // Trace serialization rounds to 0.001 ms. Test accounting, never an absolute time budget.
    assert(std::abs(self("outer") - (duration("outer") - duration("child A") - duration("child B"))) < 0.004);
    assert(std::abs(self("child B") - (duration("child B") - duration("grandchild"))) < 0.003);
    assert(std::abs(self("grandchild") - duration("grandchild")) < 0.002);
    assert(std::abs(self("worker root") - (duration("worker root") - duration("worker child"))) < 0.003);
    std::cout << "PASS: " << mode << " trace nested self time, per-thread depth, counters, JSON escaping, Unicode output";
    if (mode == "unavailable") std::cout << ", unavailable-path recovery";
    std::cout << ".\n";
}
