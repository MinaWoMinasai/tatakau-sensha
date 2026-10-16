#pragma once
#include "game/render/settings/StageSurfaceSettings.h"
#include "Calculation.h"
#include <array>

namespace stagesurface {
/// @brief 床用の四角形メッシュを作る。
inline myEngine::ModelData FloorMesh()
{
    myEngine::ModelData mesh;
    mesh.vertices = {{{-1, -1, 0, 1}, {0, 1}, {0, 0, -1}},
                     {{-1, 1, 0, 1}, {0, 0}, {0, 0, -1}},
                     {{1, -1, 0, 1}, {1, 1}, {0, 0, -1}},
                     {{1, 1, 0, 1}, {1, 0}, {0, 0, -1}}};
    mesh.indices = {0, 1, 2, 2, 1, 3};
    return mesh;
}

// 6つの面、12本の辺の面取り、8つの角の小面。共有のメッシュ1つで
// 硬い法線と元の[-1,+1]のXY範囲を保つ。
inline myEngine::ModelData BlockMesh(float bevel = kBevel)
{
    myEngine::ModelData mesh;
    mesh.vertices.reserve(96);
    mesh.indices.reserve(132);
    const float q = 1.0f - bevel;
    auto face = [&](const std::array<myEngine::Vector3, 4>& points, int count, const myEngine::Vector3& outward) {
        const myEngine::Vector3 normal = myEngine::Normalize(outward);
        const uint32_t first = static_cast<uint32_t>(mesh.vertices.size());
        for (int index = 0; index < count; ++index) {
            const auto& point = points[index];
            mesh.vertices.push_back(
                {{point.x, point.y, point.z, 1.0f}, {index == 1 || index == 2 ? 1.0f : 0.0f, index >= 2 ? 0.0f : 1.0f}, normal});
        }
        for (int index = 1; index + 1 < count; ++index) {
            uint32_t b = first + index, c = first + index + 1;
            if (myEngine::Dot(myEngine::Cross(points[index] - points[0], points[index + 1] - points[0]), normal) < 0) {
                std::swap(b, c);
            }
            mesh.indices.insert(mesh.indices.end(), {first, b, c});
        }
    };
    for (float sign : {-1.0f, 1.0f}) {
        face({{{sign, -q, -q}, {sign, q, -q}, {sign, q, q}, {sign, -q, q}}}, 4, {sign, 0, 0});
        face({{{-q, sign, -q}, {q, sign, -q}, {q, sign, q}, {-q, sign, q}}}, 4, {0, sign, 0});
        face({{{-q, -q, sign}, {q, -q, sign}, {q, q, sign}, {-q, q, sign}}}, 4, {0, 0, sign});
    }
    if (bevel <= 0.0f) {
        return mesh; // 鋭い角の共有ボックス: 24頂点、12三角形。
    }
    for (float a : {-1.0f, 1.0f}) {
        for (float b : {-1.0f, 1.0f}) {
            face({{{a, b * q, -q}, {a * q, b, -q}, {a * q, b, q}, {a, b * q, q}}}, 4, {a, b, 0});
            face({{{a, -q, b * q}, {a * q, -q, b}, {a * q, q, b}, {a, q, b * q}}}, 4, {a, 0, b});
            face({{{-q, a, b * q}, {-q, a * q, b}, {q, a * q, b}, {q, a, b * q}}}, 4, {0, a, b});
            for (float c : {-1.0f, 1.0f}) {
                face({{{a, b * q, c * q}, {a * q, b, c * q}, {a * q, b * q, c}, {}}}, 3, {a, b, c});
            }
        }
    }
    return mesh;
}

/// @brief 同じ種類のセルをまとめた壁の矩形。
struct WallRectangle {
    size_t x, y, width, height;
    int type;
};

/// @brief 指定段数の壁モデルの名前を返す。
inline std::string StackModelName(int levels)
{
    return std::string(kWallModel) + "_" + std::to_string(levels);
}

/// @brief 指定段数だけ積み重ねた壁のメッシュを作る。
inline myEngine::ModelData StackMesh(int levels)
{
    myEngine::ModelData result;
    const auto box = BlockMesh(0.0f);
    const float step = 2.0f / static_cast<float>(levels);
    for (int layer = 0; layer < levels; ++layer) {
        const uint32_t first = static_cast<uint32_t>(result.vertices.size());
        const float center = -1.0f + step * (static_cast<float>(layer) + .5f);
        for (auto vertex : box.vertices) {
            // Z方向にごく小さく凹んだ継ぎ目を入れ、XY方向は完全に連続させる。
            vertex.position.z = center + vertex.position.z * (step * .5f - .003f);
            result.vertices.push_back(vertex);
        }
        for (auto index : box.indices) {
            result.indices.push_back(first + index);
        }
    }
    return result;
}

// 有効で同じ種類のセルをちょうど覆う。埋まっているセルが欠けたり増えたりはしない。
// 占有マップが変わったときにこれを再実行すると、新たに露出した壁の端が現れる。
inline std::vector<WallRectangle> MergeWallCells(const std::vector<int>& cells, size_t width, size_t height)
{
    std::vector<WallRectangle> result;
    if (!width || !height || cells.size() != width * height) {
        return result;
    }
    std::vector<bool> used(cells.size(), false);
    for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width; ++x) {
            const size_t start = y * width + x;
            if (!cells[start] || used[start]) {
                continue;
            }
            size_t w = 1, h = 1;
            while (x + w < width && !used[start + w] && cells[start + w] == cells[start]) {
                ++w;
            }
            while (y + h < height) {
                bool valid = true;
                for (size_t i = 0; i < w; ++i) {
                    if (used[(y + h) * width + x + i] || cells[(y + h) * width + x + i] != cells[start]) {
                        valid = false;
                        break;
                    }
                }
                if (!valid) {
                    break;
                }
                ++h;
            }
            for (size_t dy = 0; dy < h; ++dy) {
                for (size_t dx = 0; dx < w; ++dx) {
                    used[(y + dy) * width + x + dx] = true;
                }
            }
            result.push_back({x, y, w, h, cells[start]});
        }
    }
    return result;
}
} // namespace stagesurface
