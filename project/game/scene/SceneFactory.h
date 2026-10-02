#pragma once

#include "AbstractSceneFactory.h"
#include "../runtime/SceneRegistry.h"

/// @brief 登録済みのシーン名からシーンを生成する窓口を提供する。
class SceneFactory : public AbstractSceneFactory {
public:
    /// @brief インスタンスの初期値と利用先を設定する。
    explicit SceneFactory(SceneRegistry registry);

    /// @brief シーンを生成する。
    std::unique_ptr<IScene> CreateScene(const std::string& sceneName) override;
    /// @brief 指定したシーン名が登録されているか判定する。
    bool ContainsScene(std::string_view sceneName) const override;
    /// @brief 登録済みシーン名前を返す。
    std::vector<std::string> GetRegisteredSceneNames() const override;

private:
    SceneRegistry registry_;
};
