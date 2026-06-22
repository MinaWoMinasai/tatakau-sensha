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
	Layout_.NumElements = 3;
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

void InputDesc::InitializeForSkinning()
{
	Initialize();
	for (uint32_t index = 0; index < 3; ++index) {
		ElementDescs_[index].InputSlot = 0;
		ElementDescs_[index].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	}
	ElementDescs_[3].SemanticName = "WEIGHT";
	ElementDescs_[3].SemanticIndex = 0;
	ElementDescs_[3].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	ElementDescs_[3].InputSlot = 1;
	ElementDescs_[3].AlignedByteOffset = 0;
	ElementDescs_[3].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	ElementDescs_[4].SemanticName = "INDEX";
	ElementDescs_[4].SemanticIndex = 0;
	ElementDescs_[4].Format = DXGI_FORMAT_R32G32B32A32_SINT;
	ElementDescs_[4].InputSlot = 1;
	ElementDescs_[4].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	ElementDescs_[4].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	Layout_.pInputElementDescs = ElementDescs_;
	Layout_.NumElements = 5;
}
