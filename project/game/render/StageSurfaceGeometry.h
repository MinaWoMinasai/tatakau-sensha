#pragma once
#include "game/render/settings/StageSurfaceSettings.h"
#include "Calculation.h"
#include <array>

namespace stagesurface {
inline cg2::ModelData FloorMesh() {
    cg2::ModelData mesh;
    mesh.vertices = {
        {{-1, -1, 0, 1}, {0, 1}, {0, 0, -1}},
        {{-1,  1, 0, 1}, {0, 0}, {0, 0, -1}},
        {{ 1, -1, 0, 1}, {1, 1}, {0, 0, -1}},
        {{ 1,  1, 0, 1}, {1, 0}, {0, 0, -1}}
    };
    mesh.indices = {0, 1, 2, 2, 1, 3};
    return mesh;
}

// Six faces, twelve edge chamfers and eight corner facets. A single shared mesh
// retains hard normals and the original [-1,+1] XY envelope.
inline cg2::ModelData BlockMesh() {
    cg2::ModelData mesh;
    mesh.vertices.reserve(96);
    mesh.indices.reserve(132);
    const float q = 1.0f - kBevel;
    auto face = [&](const std::array<cg2::Vector3, 4>& points, int count, const cg2::Vector3& outward) {
        const cg2::Vector3 normal = cg2::Normalize(outward);
        const uint32_t first = static_cast<uint32_t>(mesh.vertices.size());
        for (int index = 0; index < count; ++index) {
            const auto& point = points[index];
            mesh.vertices.push_back({{point.x, point.y, point.z, 1.0f},
                                     {index == 1 || index == 2 ? 1.0f : 0.0f, index >= 2 ? 0.0f : 1.0f}, normal});
        }
        for (int index = 1; index + 1 < count; ++index) {
            uint32_t b = first + index, c = first + index + 1;
            if (cg2::Dot(cg2::Cross(points[index] - points[0], points[index + 1] - points[0]), normal) < 0)
                std::swap(b, c);
            mesh.indices.insert(mesh.indices.end(), {first, b, c});
        }
    };
    for (float sign : {-1.0f, 1.0f}) {
        face({{{sign,-q,-q},{sign,q,-q},{sign,q,q},{sign,-q,q}}}, 4, {sign,0,0});
        face({{{-q,sign,-q},{q,sign,-q},{q,sign,q},{-q,sign,q}}}, 4, {0,sign,0});
        face({{{-q,-q,sign},{q,-q,sign},{q,q,sign},{-q,q,sign}}}, 4, {0,0,sign});
    }
    for (float a : {-1.0f, 1.0f}) for (float b : {-1.0f, 1.0f}) {
        face({{{a,b*q,-q},{a*q,b,-q},{a*q,b,q},{a,b*q,q}}}, 4, {a,b,0});
        face({{{a,-q,b*q},{a*q,-q,b},{a*q,q,b},{a,q,b*q}}}, 4, {a,0,b});
        face({{{-q,a,b*q},{-q,a*q,b},{q,a*q,b},{q,a,b*q}}}, 4, {0,a,b});
        for (float c : {-1.0f, 1.0f})
            face({{{a,b*q,c*q},{a*q,b,c*q},{a*q,b*q,c},{}}}, 3, {a,b,c});
    }
    return mesh;
}
} // namespace stagesurface
