#pragma once

#include <coroutine>
#include <exception>
#include <utility>

namespace myEngine {
// メインスレッドで1フレームに1回ずつ再開する初期化シーケンス。中断してもローカル変数は保持され、
// 破棄すると途中まで作ったシーンも安全に解放される。
class InitializationTask {
public:
    /// @brief コルーチンの状態と、中で発生した例外を保持する。
    struct promise_type {
        std::exception_ptr error;

        InitializationTask get_return_object()
        {
            return InitializationTask(std::coroutine_handle<promise_type>::from_promise(*this));
        }

        std::suspend_always initial_suspend() noexcept
        {
            return {};
        }

        std::suspend_always final_suspend() noexcept
        {
            return {};
        }

        void return_void() noexcept {}

        void unhandled_exception() noexcept
        {
            error = std::current_exception();
        }
    };

    InitializationTask() = default;

    InitializationTask(InitializationTask&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}

    InitializationTask& operator=(InitializationTask&& other) noexcept
    {
        if (this != &other) {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = std::exchange(other.handle_, {});
        }
        return *this;
    }

    ~InitializationTask()
    {
        if (handle_) {
            handle_.destroy();
        }
    }

    /// @brief 実行中または未完了のコルーチンを持っているかを返す。
    explicit operator bool() const
    {
        return static_cast<bool>(handle_);
    }

    /// @brief コルーチンを次の中断点まで進める。完了したらtrueを返し、中で例外が起きていれば再送出する。
    bool Advance()
    {
        if (!handle_) {
            return true;
        }
        if (!handle_.done()) {
            handle_.resume();
        }
        if (handle_.promise().error) {
            std::rethrow_exception(handle_.promise().error);
        }
        return handle_.done();
    }

private:
    /// @brief コルーチンのハンドルを受け取って所有する。
    explicit InitializationTask(std::coroutine_handle<promise_type> handle) : handle_(handle) {}

    std::coroutine_handle<promise_type> handle_{};
};
} // namespace myEngine
