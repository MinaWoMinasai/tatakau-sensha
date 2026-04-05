#include "InputDesc.h"

void InputDesc::Initialize()
{
	ElementDescs_[0].SemanticName = "POSITION";
	ElementDescs_[0].SemanticIndex = 0;
	ElementDescs_[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	ElementDescs_[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	ElementDescs_[1].SemanticName = "TEXCOORD";
	ElementDescs_[1].SemanticIndex = 0;
	ElementDescs_[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	ElementDescs_[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	ElementDescs_[2].SemanticName = "NORMAL";
	ElementDescs_[2].SemanticIndex = 0;
	ElementDescs_[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	ElementDescs_[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	Layout_.pInputElementDescs = ElementDescs_;
	Layout_.NumElements = _countof(ElementDescs_);
}

void InputDesc::InitializeForTrail()
{
    // 0: 座標 (POSITION)
    ElementDescs_[0].SemanticName = "POSITION";
    ElementDescs_[0].SemanticIndex = 0;
    ElementDescs_[0].Format = DXGI_FORMAT_R32G32B32_FLOAT; // Vector3
    ElementDescs_[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    // 1: 頂点カラー (COLOR) ★これが重要！
    ElementDescs_[1].SemanticName = "COLOR";
    ElementDescs_[1].SemanticIndex = 0;
    ElementDescs_[1].Format = DXGI_FORMAT_R32G32B32A32_FLOAT; // Vector4 (RGBA)
    ElementDescs_[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    // 2: UV座標 (TEXCOORD)
    ElementDescs_[2].SemanticName = "TEXCOORD";
    ElementDescs_[2].SemanticIndex = 0;
    ElementDescs_[2].Format = DXGI_FORMAT_R32G32_FLOAT; // Vector2
    ElementDescs_[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    Layout_.pInputElementDescs = ElementDescs_;
    Layout_.NumElements = 3; // 座標、色、UV の 3つ
}