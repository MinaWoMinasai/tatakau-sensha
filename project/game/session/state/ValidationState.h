#pragma once
#include "game/session/GameplayTypes.h"

namespace gameplay {
/// @brief ValidationStateに属する資源と実行状態を保持する。
struct ValidationState {
    float neonTriangleDemoLineWidth_ = 0.16f;

    float neonTriangleDemoRotateSpeed_ = 0.75f;

    float neonTriangleDemoRotation_ = 0.0f;

    cg2::Vector4 neonTriangleDemoColor_ = {0.15f, 0.95f, 1.0f, 1.0f};

    int experienceValidationVariant_ = 0;

    int experienceValidationStyle_ = 2, experiencePreviewRarity_ = 0;

    bool experienceBuildPreserved_ = false;

    bool experienceEvolutionVerified_ = false;

    int experienceDroneSamples_ = 0;

    float experienceValidationElapsed_ = 0, experienceValidationStateAge_ = 0, experienceMeleeAge_ = 0;

    std::string experienceValidationState_;

    std::vector<std::string> experienceValidationCaptures_, experienceValidationErrors_;

    std::vector<std::string> experienceValidationIntroOffers_;

    bool experienceMeleeStarted_ = false, experienceMeleeDone_ = false;

    bool experienceCreditArrivalEligible_ = false, experienceCreditDelivered_ = false;

    int experienceEarlyFlightSamples_ = 0, experiencePrematureCredits_ = 0, experienceGroundOrbSamples_ = 0;

    int experienceIntroWallet_ = -1, experienceAfterIntroWallet_ = -1, experienceInitialKills_ = 0;

    int experienceForcedLaterClears_ = 0, experiencePlayerBulletSamples_ = 0;

    int experienceMeleeMinHp_ = 500, experienceMeleeSlashSamples_ = 0, experienceMeleeBulletSamples_ = 0;

    float experienceMeleeDisplacement_ = 0;

    cg2::Vector3 experienceMeleeTargetStart_{};

    unsigned experienceGuideStageMask_ = 0, experienceSuccessfulDashes_ = 0;

    nlohmann::json experienceIntroOfferDetails_ = nlohmann::json::array();
};
} // namespace gameplay
