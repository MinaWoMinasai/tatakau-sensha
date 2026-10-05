// Executes the production owner destructor, release helper and failed-load catch
// with explicit-release resource adapters. No D3D device is needed; actual engine
// palette/fallback SRV release is covered by the existing rendering tests.
#include <cassert>
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
unsigned nextDescriptor = 1;
unsigned releaseCount = 0;
unsigned dissolveResetCount = 0;
std::set<unsigned> allocated;
std::vector<std::string> events;

unsigned Allocate() {
    const unsigned id = nextDescriptor++;
    assert(allocated.insert(id).second);
    return id;
}
void Free(unsigned& id) {
    if (!id) return;
    assert(allocated.erase(id) == 1); // A double free is a regression.
    id = 0;
    ++releaseCount;
}
struct Model {
    unsigned palette = 0;
    // Implicit destruction deliberately does not free descriptors, as in the
    // actual engine types whose caller owns the GPU-complete lifetime boundary.
    void Acquire() { assert(!palette); palette = Allocate(); }
    void ReleaseGpuResources() { events.push_back("model.release"); Free(palette); }
};
struct Renderer {
    unsigned fallbackMask = 0;
    void Acquire() { assert(!fallbackMask); fallbackMask = Allocate(); }
    void ReleaseGpuResources() { events.push_back("renderer.release"); Free(fallbackMask); }
};
struct Object {};
struct Dissolve {
    void Reset(Model& model) {
        assert(model.palette != 0); // Playback restoration must precede release.
        ++dissolveResetCount;
        events.push_back("dissolve.reset");
    }
};
}

class NeonSkinnedPreview {
public:
    ~NeonSkinnedPreview();
    void ReleaseResources();
    void FailLoadForTest(const std::exception& error);
    void Acquire(bool rendererReady = true, bool fullyLoaded = true) {
        assert(!model_);
        model_ = std::make_unique<Model>();
        model_->Acquire();
        object_ = std::make_unique<Object>();
        if (rendererReady) renderer_.Acquire();
        ready_ = fullyLoaded;
        enabled_ = true;
        loadError_.clear();
    }
    std::unique_ptr<Model> model_;
    std::unique_ptr<Object> object_;
    Renderer renderer_;
    Dissolve dissolve_;
    bool ready_ = false, enabled_ = false;
    std::string loadError_;
};

#include "preview_owner_methods.inc"

int main() {
    const unsigned sharedTexture = Allocate();
    const size_t cacheBaseline = allocated.size();
    const unsigned initialFrees = releaseCount;
    {
        NeonSkinnedPreview untouched;
    }
    assert(allocated.size() == cacheBaseline && releaseCount == initialFrees);

    const unsigned initialResets = dissolveResetCount;
    events.clear();
    {
        NeonSkinnedPreview loaded;
        loaded.Acquire();
        assert(allocated.size() == cacheBaseline + 2);
    }
    assert(allocated.size() == cacheBaseline && releaseCount == initialFrees + 2);
    assert(dissolveResetCount == initialResets + 1);
    assert(events.front() == "dissolve.reset");

    for (bool rendererReady : {false, true}) {
        NeonSkinnedPreview partial;
        partial.Acquire(rendererReady, false);
        assert(allocated.size() == cacheBaseline + (rendererReady ? 2u : 1u));
        const std::runtime_error loadFailure("injected model/pipeline failure");
        partial.FailLoadForTest(loadFailure);
        assert(allocated.size() == cacheBaseline);
        assert(!partial.ready_ && !partial.enabled_ && !partial.model_ && !partial.object_);
        assert(partial.loadError_ == loadFailure.what());
        // The same owner can retry after a partial failure without losing old
        // descriptors or carrying a ready flag for freed resources.
        partial.Acquire();
        assert(partial.ready_ && allocated.size() == cacheBaseline + 2);
        const unsigned retryFrees = releaseCount;
        partial.ReleaseResources();
        partial.ReleaseResources();
        assert(allocated.size() == cacheBaseline && releaseCount == retryFrees + 2);
        assert(!partial.ready_ && !partial.model_ && !partial.object_);
    }

    for (unsigned scene = 0; scene < 100; ++scene) {
        {
            NeonSkinnedPreview recreated;
            recreated.Acquire();
        }
        assert(allocated.size() == cacheBaseline);
    }
    assert(allocated.contains(sharedTexture)); // Shared TextureManager cache persists.
    unsigned releaseSharedTexture = sharedTexture;
    Free(releaseSharedTexture);
    assert(allocated.empty());
    std::cout << "Production Neon Preview ownership: uninitialized, loaded teardown, partial load failure/retry, "
                 "idempotent release and 100 recreated owners passed.\n";
}
