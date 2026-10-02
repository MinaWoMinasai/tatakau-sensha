#pragma once
#include "Struct.h"
#include "Sprite.h"

/// @brief 画面のフェードイン・フェードアウトの進行と描画を管理する。
class Fade {
public:
    enum class Status {
        None,    // フェードなし
        FadeIn,  // フェードイン中
        FadeOut, // フェードアウト中
    };

    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize();
    /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
    void Update();
    /// @brief 現在の状態を描画する。描画先と対応するパイプラインの準備後に呼ぶ。
    void Draw();

    // フェード開始
    void Start(Status status, float duration);

    // フェード停止
    void Stop();

    // フェード終了判定
    bool IsFinished() const;

private:
    std::unique_ptr<cg2::Sprite> sprite;

    // 02_13 16枚目 現在のフェードの状態
    Status status_ = Status::None;

    // 02_13 17枚目 フェードの持続時間
    float duration_ = 0.0f;
    // 02_13 17枚目 経過時間カウンター
    float counter_ = 0.0f;
};
