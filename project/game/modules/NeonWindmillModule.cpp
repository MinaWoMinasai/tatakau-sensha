#include "NeonWindmillModule.h"
#include "../runtime/SceneRegistry.h"
#include "../scene/NeonWindmillScene.h"

bool NeonWindmillModule::RegisterScenes(SceneRegistry& registry) const
{
    return registry.Register<NeonWindmillScene>("NEON_WINDMILL");
}
