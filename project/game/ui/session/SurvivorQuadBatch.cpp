#include "game/ui/session/SurvivorQuadBatch.h"
#include "game/session/GameplaySystems.h"
#include "Calculation.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include "WinApp.h"
#include <algorithm>
#include <cmath>

namespace gameplay {

namespace {
constexpr size_t kMaterialStride = (sizeof(myEngine::Material) + 255u) & ~size_t{255u};
constexpr uint32_t kQuadVertices = 6;

bool SameColor(const myEngine::Vector4& a, const myEngine::Vector4& b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}
} // namespace

void SurvivorQuadBatch::Initialize(size_t capacity)
{
    if (capacity_ > 0) {
        return;
    }
    capacity_ = capacity;
    auto* dxCommon = myEngine::Object3dCommon::GetInstance()->GetDxCommon();
    vertices_ = dxCommon->CreateBufferResource(sizeof(myEngine::VertexData) * kQuadVertices * capacity_);
    view_.BufferLocation = vertices_->GetGPUVirtualAddress();
    view_.SizeInBytes = static_cast<UINT>(sizeof(myEngine::VertexData) * kQuadVertices * capacity_);
    view_.StrideInBytes = sizeof(myEngine::VertexData);
    vertices_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));
    materials_ = dxCommon->CreateBufferResource(kMaterialStride * capacity_);
    materials_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
    for (size_t i = 0; i < capacity_; ++i) {
        *reinterpret_cast<myEngine::Material*>(materialData_ + kMaterialStride * i) = myEngine::MakeDefaultMaterial();
    }
    transform_ = dxCommon->CreateBufferResource(sizeof(myEngine::TransformationMatrixWithShadow));
    myEngine::TransformationMatrixWithShadow* transform = nullptr;
    transform_->Map(0, nullptr, reinterpret_cast<void**>(&transform));
    transform->World = myEngine::MakeIdentity4x4();
    transform->WorldInverseTranspose = myEngine::MakeIdentity4x4();
    transform->LightWVP = myEngine::MakeIdentity4x4();
    transform->WVP = myEngine::MakeOrthographicMatrix(0.0f, 0.0f, static_cast<float>(myEngine::WinApp::kClientWidth),
                                                      static_cast<float>(myEngine::WinApp::kClientHeight), 0.0f, 100.0f);
    entries_.reserve(capacity_);
    runs_.reserve(capacity_);
}

const SurvivorQuadBatch::Texture& SurvivorQuadBatch::Find(const std::string& path)
{
    const auto found = textures_.find(path);
    if (found != textures_.end()) {
        return found->second;
    }
    auto* manager = myEngine::TextureManager::GetInstance();
    manager->LoadTexture(path);
    const auto& meta = manager->GetMetaData(path);
    Texture texture;
    texture.handle = manager->GetSrvHandleGPU(path);
    texture.size = {static_cast<float>(meta.width), static_cast<float>(meta.height)};
    return textures_.emplace(path, texture).first->second;
}

void SurvivorQuadBatch::Preload(const std::string& texture)
{
    Find(texture);
}

void SurvivorQuadBatch::Begin()
{
    entries_.clear();
}

void SurvivorQuadBatch::Quad(Layer layer, const std::string& texture, const myEngine::Vector2& position, const myEngine::Vector2& size, float rotation,
                             const myEngine::Vector4& color)
{
    if (entries_.size() >= capacity_ || size.x <= 0.0f || size.y <= 0.0f) {
        return;
    }
    entries_.push_back({layer, Find(texture).handle, position, size, rotation, color});
}

myEngine::Vector2 SurvivorQuadBatch::Measure(const std::string& text, const myEngine::TextStyle& style)
{
    const std::string key = style.fontFamily + "|" + std::to_string(style.fontSize) + "|" + std::to_string(style.fontWeight) + "|" +
                            std::to_string(style.outlineThickness) + "|" + text;
    auto found = labels_.find(key);
    if (found == labels_.end()) {
        found = labels_.emplace(key, myEngine::TextRenderer::GetInstance()->GetOrCreateTexture(text, style)).first;
    }
    const Texture& texture = Find(found->second);
    const float scale = myEngine::TextRenderer::GetRasterScale();
    return {texture.size.x / scale, texture.size.y / scale};
}

float SurvivorQuadBatch::Label(Layer layer, const std::string& text, const myEngine::TextStyle& style, float x, float y, float anchorX,
                               const myEngine::Vector4& color, float scale)
{
    if (text.empty()) {
        return 0.0f;
    }
    const myEngine::Vector2 size = Measure(text, style);
    const std::string key = style.fontFamily + "|" + std::to_string(style.fontSize) + "|" + std::to_string(style.fontWeight) + "|" +
                            std::to_string(style.outlineThickness) + "|" + text;
    Quad(layer, labels_[key], {x + size.x * scale * (0.5f - anchorX), y}, {size.x * scale, size.y * scale}, 0.0f, color);
    return size.x * scale;
}

void SurvivorQuadBatch::End()
{
    std::stable_sort(entries_.begin(), entries_.end(), [](const Entry& a, const Entry& b) {
        return a.layer < b.layer;
    });
    runs_.clear();
    for (size_t i = 0; i < entries_.size(); ++i) {
        const Entry& entry = entries_[i];
        const float c = std::cos(entry.rotation), s = std::sin(entry.rotation);
        const float w = entry.size.x * 0.5f, h = entry.size.y * 0.5f;
        auto corner = [&](float x, float y) {
            return myEngine::Vector4{entry.position.x + x * c - y * s, entry.position.y + x * s + y * c, 0.0f, 1.0f};
        };
        const myEngine::Vector4 leftTop = corner(-w, -h), rightTop = corner(w, -h), leftBottom = corner(-w, h), rightBottom = corner(w, h);
        const myEngine::Vector3 normal{0.0f, 0.0f, -1.0f};
        myEngine::VertexData* out = vertexData_ + i * kQuadVertices;
        out[0] = {leftBottom, {0.0f, 1.0f}, normal};
        out[1] = {leftTop, {0.0f, 0.0f}, normal};
        out[2] = {rightBottom, {1.0f, 1.0f}, normal};
        out[3] = {leftTop, {0.0f, 0.0f}, normal};
        out[4] = {rightTop, {1.0f, 0.0f}, normal};
        out[5] = {rightBottom, {1.0f, 1.0f}, normal};
        if (!runs_.empty()) {
            Run& last = runs_.back();
            if (last.layer == entry.layer && last.handle.ptr == entry.handle.ptr && SameColor(entries_[i - 1].color, entry.color)) {
                last.count += kQuadVertices;
                continue;
            }
        }
        const uint32_t material = static_cast<uint32_t>(runs_.size());
        reinterpret_cast<myEngine::Material*>(materialData_ + kMaterialStride * material)->color = entry.color;
        runs_.push_back({entry.layer, entry.handle, static_cast<uint32_t>(i) * kQuadVertices, kQuadVertices, material});
    }
}

void SurvivorQuadBatch::Draw()
{
    auto* common = myEngine::SpriteCommon::GetInstance();
    auto* list = common->GetDxCommon()->GetList().Get();
    auto* textures = myEngine::TextureManager::GetInstance();
    auto prepare = [&](bool add) {
        common->PreDraw(add ? myEngine::kAdd : myEngine::kNormal);
        list->IASetVertexBuffers(0, 1, &view_);
        list->SetGraphicsRootConstantBufferView(1, transform_->GetGPUVirtualAddress());
        // 共通のシェーダーが読む、使わない画像（Sprite::Draw と同じ）。
        const D3D12_GPU_DESCRIPTOR_HANDLE flat =
            textures->GetSrvHandleGPU(myEngine::TextureManager::GetFlatNormalTexturePath(), myEngine::TextureManager::TextureColorSpace::LinearData);
        list->SetGraphicsRootDescriptorTable(11, flat);
        list->SetGraphicsRootDescriptorTable(12, flat);
        list->SetGraphicsRootDescriptorTable(13, flat);
        list->SetGraphicsRootDescriptorTable(
            14, textures->GetSrvHandleGPU(myEngine::TextureManager::GetBrdfLutTexturePath(), myEngine::TextureManager::TextureColorSpace::LinearData));
        list->SetGraphicsRootDescriptorTable(
            15, textures->GetSrvHandleGPU(myEngine::TextureManager::GetPbrIrradianceTexturePath(), myEngine::TextureManager::TextureColorSpace::LinearData));
        list->SetGraphicsRootDescriptorTable(16, textures->GetSrvHandleGPU(myEngine::TextureManager::GetPbrPrefilteredEnvironmentTexturePath(),
                                                                          myEngine::TextureManager::TextureColorSpace::LinearData));
    };
    int blend = -1;
    for (const Run& run : runs_) {
        const int add = run.layer == PreviewGlow || run.layer == IconGlow ? 1 : 0;
        if (add != blend) {
            prepare(add == 1);
            blend = add;
        }
        const D3D12_GPU_VIRTUAL_ADDRESS material = materials_->GetGPUVirtualAddress() + kMaterialStride * run.material;
        list->SetGraphicsRootConstantBufferView(0, material);
        list->SetGraphicsRootConstantBufferView(10, material);
        list->SetGraphicsRootDescriptorTable(2, run.handle);
        list->DrawInstanced(run.count, 1, run.start, 0);
    }
    common->PreDraw(myEngine::kNormal);
}

} // namespace gameplay
