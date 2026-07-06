#pragma once

#include "Struct.h"
#include <vector>

struct GroundRayHit {
	Vector3 position{};
	Vector3 normal{ 0.0f, 1.0f, 0.0f };
	float distance = 0.0f;
};

// CPU側の静的メッシュに対する、3Dアクション用の足元レイ判定。
// 任意のOBJモデルを登録でき、壁面を除外して歩行可能な床だけを返す。
class TriangleMeshGround {
public:
	struct GroundTriangle {
		Vector3 vertices[3]{};
		Vector3 normal{ 0.0f, 1.0f, 0.0f };
	};

	void Clear();
	void AddMesh(const ModelData& modelData, const Matrix4x4& worldMatrix);
	bool RaycastDown(
		const Vector3& origin,
		float maxDistance,
		float minimumGroundNormalY,
		GroundRayHit& outHit) const;

	size_t GetTriangleCount() const { return triangles_.size(); }

private:
	std::vector<GroundTriangle> triangles_;
};
