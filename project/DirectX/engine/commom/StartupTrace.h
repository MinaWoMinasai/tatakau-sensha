#pragma once

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

// CPU wall-clock loading diagnostics, available in Release without the frame
// profiler. Nested inclusive times must not be added together; selfMs excludes
// nested scopes. No disk I/O occurs per frame. Diagnostics never prevent loading.
namespace cg2 {

/// @brief 起動処理の区間名と経過時間を記録し、起動遅延の調査に使う。
class StartupTrace {
    using Clock = std::chrono::steady_clock;
    /// @brief 指定した環境変数を読み取る。
    static std::string Environment(const char* name)
    {
#ifdef _MSC_VER
        char* value = nullptr;
        size_t size = 0;
        if (_dupenv_s(&value, &size, name) != 0 || !value)
            return {};
        const std::string result(value);
        std::free(value);
        return result;
#else
        const char* value = std::getenv(name);
        return value ? value : "";
#endif
    }
    /// @brief 診断情報の出力先パスを返す。
    static std::filesystem::path OutputPath()
    {
#ifdef _MSC_VER
        wchar_t* value = nullptr;
        size_t size = 0;
        if (_wdupenv_s(&value, &size, L"CG2_STARTUP_TRACE_PATH") == 0 && value) {
            const std::filesystem::path result(value);
            std::free(value);
            if (!result.empty())
                return result;
        }
#else
        const auto value = Environment("CG2_STARTUP_TRACE_PATH");
        if (!value.empty())
            return std::filesystem::path(value);
#endif
        return "generated/startup_trace.json";
    }
    /// @brief cg2::StartupTraceが扱う演出の発生時刻・内容を表す。
    struct Event {
        std::string name;
        double startMs, durationMs, selfMs;
        unsigned depth;
    };
    /// @brief 起動計測のイベント列と出力先などの共有状態を保持する。
    struct State {
        Clock::time_point start = Clock::now();
        bool enabled = Environment("CG2_STARTUP_TRACE") != "0";
        std::mutex mutex;
        std::vector<Event> events;
        std::map<std::string, double> counters;
    };
    /// @brief 保持している値またはリソースを返す。
    static State& Get()
    {
        static State state;
        return state;
    }
    /// @brief 計測した時間をミリ秒へ変換する。
    static double Milliseconds(Clock::duration value)
    {
        return std::chrono::duration<double, std::milli>(value).count();
    }
    /// @brief 文字列の値を取り出す。
    static void String(std::ostream& out, const std::string& value)
    {
        out << '"';
        for (unsigned char c : value) {
            if (c == '"' || c == '\\')
                out << '\\' << c;
            else if (c < 0x20)
                out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << unsigned(c) << std::dec << std::setfill(' ');
            else
                out << c;
        }
        out << '"';
    }

public:
    /// @brief この機能が有効か判定する。
    static bool Enabled()
    {
        return Get().enabled;
    }
    /// @brief スコープの開始と終了を起動計測へ記録するRAIIオブジェクトを表す。
    class Scope {
    public:
        /// @brief インスタンスの初期値と利用先を設定する。
        explicit Scope(std::string name) : name_(std::move(name)), enabled_(Enabled())
        {
            if (!enabled_)
                return;
            start_ = Clock::now();
            parent_ = current_;
            depth_ = parent_ ? parent_->depth_ + 1 : 0;
            current_ = this;
        }
        /// @brief この型の終了処理を行う。所有している資源の寿命を終了させる。
        ~Scope()
        {
            if (!enabled_)
                return;
            const auto end = Clock::now();
            const double elapsed = Milliseconds(end - start_);
            if (parent_)
                parent_->childrenMs_ += elapsed;
            current_ = parent_;
            try {
                auto& state = Get();
                std::lock_guard<std::mutex> lock(state.mutex);
                if (state.events.size() < 50000)
                    state.events.push_back(
                        {name_, Milliseconds(start_ - state.start), elapsed, (std::max)(0.0, elapsed - childrenMs_), depth_});
            }
            catch (...) {
            }
        }
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;

    private:
        inline static thread_local Scope* current_ = nullptr;
        std::string name_;
        bool enabled_ = false;
        Clock::time_point start_{};
        Scope* parent_ = nullptr;
        unsigned depth_ = 0;
        double childrenMs_ = 0;
    };
    /// @brief 保持している要素または件数を返す。
    static void Count(const std::string& name, double amount = 1)
    {
        if (!Enabled())
            return;
        try {
            auto& state = Get();
            std::lock_guard<std::mutex> lock(state.mutex);
            state.counters[name] += amount;
        }
        catch (...) {
        }
    }
    /// @brief 指定区間の名前と時刻を計測イベントへ記録する。
    static void Mark(const std::string& name)
    {
        if (!Enabled())
            return;
        try {
            auto& state = Get();
            const double elapsed = Milliseconds(Clock::now() - state.start);
            std::lock_guard<std::mutex> lock(state.mutex);
            if (state.events.size() < 50000)
                state.events.push_back({name, elapsed, 0, 0, 0});
        }
        catch (...) {
        }
    }
    /// @brief 未出力の記録を出力先へ書き込む。
    static void Flush()
    {
        if (!Enabled())
            return;
        try {
            auto& state = Get();
            std::lock_guard<std::mutex> lock(state.mutex);
            const std::filesystem::path path = OutputPath();
            std::error_code ec;
            if (path.has_parent_path())
                std::filesystem::create_directories(path.parent_path(), ec);
            std::ofstream out(path, std::ios::binary | std::ios::trunc);
            if (!out)
                return;
            out << std::fixed << std::setprecision(3)
                << "{\n  \"schema\": 1,\n  \"cacheEnabled\": " << (Environment("CG2_STARTUP_CACHE") == "0" ? "false" : "true")
                << ",\n  \"clock\": \"steady CPU wall time, milliseconds from WinMain\",\n  \"events\": [\n";
            bool comma = false;
            for (const auto& event : state.events) {
                if (comma)
                    out << ",\n";
                comma = true;
                out << "    {\"name\":";
                String(out, event.name);
                out << ",\"startMs\":" << event.startMs << ",\"durationMs\":" << event.durationMs << ",\"selfMs\":" << event.selfMs
                    << ",\"depth\":" << event.depth << '}';
            }
            out << "\n  ],\n  \"counters\": {";
            comma = false;
            for (const auto& entry : state.counters) {
                if (comma)
                    out << ',';
                comma = true;
                out << '\n' << "    ";
                String(out, entry.first);
                out << ':' << entry.second;
            }
            out << "\n  }\n}\n";
        }
        catch (...) {
        }
    }
};

} // namespace cg2
