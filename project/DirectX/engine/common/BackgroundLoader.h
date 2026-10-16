#pragma once

#include <algorithm>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>
#include <objbase.h>

namespace myEngine {
// CPU-only file/decode work. GPU resources, descriptors, scene state and command
// lists must stay on the main thread. Decoding (PNG/mips, Assimp) is CPU bound,
// so use spare cores while leaving one for the main thread; 2..4 workers bound
// disk/memory contention.
class BackgroundLoader {
public:
    static BackgroundLoader& Get()
    {
        static BackgroundLoader loader;
        return loader;
    }

    template <class Function> auto Submit(Function action) -> std::shared_future<std::invoke_result_t<Function>>
    {
        using Result = std::invoke_result_t<Function>;
        auto task = std::make_shared<std::packaged_task<Result()>>(std::move(action));
        auto result = task->get_future().share();
        {
            std::lock_guard lock(mutex_);
            if (stopping_) {
                throw std::runtime_error("Background loader is shutting down");
            }
            jobs_.push([task] {
                (*task)();
            });
        }
        wake_.notify_one();
        return result;
    }

    // Drop unstarted work and join active decodes before managers/COM are freed.
    void Shutdown()
    {
        {
            std::lock_guard lock(mutex_);
            stopping_ = true;
            std::queue<std::function<void()>>{}.swap(jobs_);
        }
        wake_.notify_all();
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    ~BackgroundLoader()
    {
        Shutdown();
    }

private:
    BackgroundLoader()
    {
        try {
            const unsigned cores = std::thread::hardware_concurrency();
            const unsigned workerCount = (std::clamp)(cores > 1 ? cores - 1 : 2u, 2u, 4u);
            for (unsigned index = 0; index < workerCount; ++index) {
                workers_.emplace_back([this] {
                    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
                    for (;;) {
                        std::function<void()> job;
                        {
                            std::unique_lock lock(mutex_);
                            wake_.wait(lock, [this] {
                                return stopping_ || !jobs_.empty();
                            });
                            if (stopping_) {
                                break;
                            }
                            job = std::move(jobs_.front());
                            jobs_.pop();
                        }
                        job(); // packaged_task transports exceptions to its future.
                    }
                    if (SUCCEEDED(com)) {
                        CoUninitialize();
                    }
                });
            }
        }
        catch (...) {
            Shutdown();
            throw;
        }
    }

    BackgroundLoader(const BackgroundLoader&) = delete;
    BackgroundLoader& operator=(const BackgroundLoader&) = delete;
    std::mutex mutex_;
    std::condition_variable wake_;
    std::queue<std::function<void()>> jobs_;
    std::vector<std::thread> workers_;
    bool stopping_ = false;
};
} // namespace myEngine
