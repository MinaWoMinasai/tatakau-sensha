#include "TriangleMeshGround.h"
#include "Calculation.h"
#include <algorithm>
#include <cmath>

namespace {
Vector3 TransformPoint(const Vector3& point, const Matrix4x4& matrix) {
	Vector3 result = TransformNormal(point, matrix);
	result.x += matrix.m[3][0];
	result.y += matrix.m[3][1];
	result.z += matrix.m[3][2];
	return result;
}

bool IntersectDownRay(
	const Vector3& origin,
	const TriangleMeshGround::GroundTriangle& triangle,
	float maxDistance,
	float& outDistance) {
	const Vector3 direction = { 0.0f, -1.0f, 0.0f };
	const Vector3 edge1 = triangle.vertices[1] - triangle.vertices[0];
	const Vector3 edge2 = triangle.vertices[2] - triangle.vertices[0];
	const Vector3 p = Cross(direction, edge2);
	const float determinant = Dot(edge1, p);
	if (std::abs(determinant) < 1.0e-6f) {
		return false;
	}
	const float inverseDeterminant = 1.0f / determinant;
	const Vector3 t = origin - triangle.vertices[0];
	const float u = Dot(t, p) * inverseDeterminant;
	if (u < -1.0e-5f || u > 1.0f + 1.0e-5f) {
		return false;
	}
	const Vector3 q = Cross(t, edge1);
	const float v = Dot(direction, q) * inverseDeterminant;
	if (v < -1.0e-5f || u + v > 1.0f + 1.0e-5f) {
		return false;
	}
	const float distance = Dot(edge2, q) * inverseDeterminant;
	if (distance < 0.0f || distance > maxDistance) {
		return false;
	}
	outDistance = distance;
	return true;
}
}

void TriangleMeshGround::Clear() {
	triangles_.clear();
}

void TriangleMeshGround::AddMesh(const ModelData& modelData, const Matrix4x4& worldMatrix) {
	const auto addTriangle = [&](uint32_t i0, uint32_t i1, uint32_t i2) {
		if (i0 >= modelData.vertices.size() || i1 >= modelData.vertices.size() ||
			i2 >= modelData.vertices.size()) {
			return;
		}
		GroundTriangle triangle{};
		const VertexData* source[3] = {
			&modelData.vertices[i0], &modelData.vertices[i1], &modelData.vertices[i2]
		};
		for (int i = 0; i < 3; ++i) {
			triangle.vertices[i] = TransformPoint(
				{ source[i]->position.x, source[i]->position.y, source[i]->position.z }, worldMatrix);
		}
		const Vector3 cross = Cross(
			triangle.vertices[1] - triangle.vertices[0],
			triangle.vertices[2] - triangle.vertices[0]);
		if (Length(cross) < 1.0e-5f) {
			return;
		}
		triangle.normal = Normalize(cross);
		if (triangle.normal.y < 0.0f) {
			triangle.normal = -triangle.normal;
		}
		triangles_.push_back(triangle);
	};

	if (!modelData.indices.empty()) {
		for (size_t i = 0; i + 2 < modelData.indices.size(); i += 3) {
			addTriangle(modelData.indices[i], modelData.indices[i + 1], modelData.indices[i + 2]);
		}
	} else {
		for (uint32_t i = 0; i + 2 < modelData.vertices.size(); i += 3) {
			addTriangle(i, i + 1, i + 2);
		}
	}
}

bool TriangleMeshGround::RaycastDown(
	const Vector3& origin,
	float maxDistance,
	float minimumGroundNormalY,
	GroundRayHit& outHit) const {
	bool found = false;
	float nearestDistance = maxDistance;
	for (const GroundTriangle& triangle : triangles_) {
		if (triangle.normal.y < minimumGroundNormalY) {
			continue;
		}
		float distance = 0.0f;
		if (!IntersectDownRay(origin, triangle, maxDistance, distance) || distance >= nearestDistance) {
			continue;
		}
		nearestDistance = distance;
		outHit.distance = distance;
		outHit.position = origin + Vector3{ 0.0f, -distance, 0.0f };
		outHit.normal = triangle.normal;
		found = true;
	}
	return found;
}
