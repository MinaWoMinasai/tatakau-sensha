#include "BuiltInGameModule.h"

#include "../runtime/SceneRegistry.h"
#include "../scene/Action3DScene.h"
#include "../scene/GameScene.h"
#include "../scene/GraphicsLabScene.h"
#include "../scene/PlayerLabScene.h"
#include "../scene/TestScene.h"
#include "../scene/TitleScene.h"
#include "../scene/UnderwaterLabScene.h"
#include "../scene/InkShooterScene.h"
#include "../naval/scene/NavalBattleScene.h"
#if defined(USE_IMGUI) && !defined(NDEBUG)
#include "../scene/VfxLabScene.h"
#endif

std::string_view BuiltInGameModule::GetId() const
{
	return kId;
}

std::string_view BuiltInGameModule::GetDisplayName() const
{
	return kDisplayName;
}

bool BuiltInGameModule::RegisterScenes(SceneRegistry& registry) const
{
	bool success = true;
	success = registry.Register<TitleScene>("TITLE") && success;
	success = registry.Register<TestScene>("TEST") && success;
	success = registry.Register<GameScene>("GAME") && success;
	success = registry.Register<PlayerLabScene>("PLAYER_LAB") && success;
	success = registry.Register<Action3DScene>("ACTION3D") && success;
	success = registry.Register<GraphicsLabScene>("GRAPHICS_LAB") && success;
	success = registry.Register<NavalBattleScene>("NAVAL_BATTLE") && success;
	success = registry.Register<InkShooterScene>("INK_SHOOTER_LAB") && success;
#if defined(USE_IMGUI) && !defined(NDEBUG)
	success = registry.Register<VfxLabScene>("VFX_LAB") && success;
	success = registry.Register<UnderwaterLabScene>("UNDERWATER_LAB") && success;
#endif
	return success;
}
