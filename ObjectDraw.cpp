#include "ObjectDraw.h"

void ObjectDraw::Initialize(Microsoft::WRL::ComPtr<ID3D12Device>& device, Descriptor descriptor, Command command)
{
	worldTransformSuzanne_.scale = { 1.0f, 1.0f, 1.0f };
	worldTransformSuzanne_.rotate = { 0.0f, 0.0f, 0.0f };
	worldTransformSuzanne_.translate = {0.0f, -2.0f, 0.0f};
	suzanneModel_.Initialize(device, descriptor);
	suzanneModel_.Load(command, "suzanne.obj", "white512x512.png", 3);
	
	worldTransformBunny_.scale = { 1.0f, 1.0f, 1.0f };
	worldTransformBunny_.rotate = { 0.0f, 0.0f, 0.0f };
	worldTransformBunny_.translate = { -2.0f, 0.0f, 0.0f };
	bunnyModel_.Initialize(device, descriptor);
	bunnyModel_.Load(command, "bunny.obj", "uvChecker.png", 4);
	
	worldTransformTeapot_.scale = { 1.0f, 1.0f, 1.0f };
	worldTransformTeapot_.rotate = { 0.0f, 0.0f, 0.0f };
	worldTransformTeapot_.translate = { -3.0f, -2.0f, 0.0f };
	teapotModel_.Initialize(device, descriptor);
	teapotModel_.Load(command, "teapot.obj", "checkerBoard.png", 5);

	worldTransformMultiMesh_.scale = { 1.0f, 1.0f, 1.0f };
	worldTransformMultiMesh_.rotate = { 0.0f, 0.0f, 0.0f };
	worldTransformMultiMesh_.translate = { 0.0f, 1.0f, 0.0f };
	multiMeshModel_.Initialize(device, descriptor);
	multiMeshModel_.Load(command, "multiMesh.obj", "uvChecker.png", 6);
}

void ObjectDraw::Update()
{

	ImGui::Begin("Suzanne");
	ImGui::DragFloat3("scale", &worldTransformSuzanne_.scale.x, 0.1f);
	ImGui::DragFloat3("rotate", &worldTransformSuzanne_.rotate.x, 0.1f);
	ImGui::DragFloat3("transform", &worldTransformSuzanne_.translate.x, 0.1f);
	ImGui::End();

	ImGui::Begin("Bunny");
	ImGui::DragFloat3("scale", &worldTransformBunny_.scale.x, 0.1f);
	ImGui::DragFloat3("rotate", &worldTransformBunny_.rotate.x, 0.1f);
	ImGui::DragFloat3("transform", &worldTransformBunny_.translate.x, 0.1f);
	ImGui::End();

	ImGui::Begin("Teapot");
	ImGui::DragFloat3("scale", &worldTransformTeapot_.scale.x, 0.1f);
	ImGui::DragFloat3("rotate", &worldTransformTeapot_.rotate.x, 0.1f);
	ImGui::DragFloat3("transform", &worldTransformTeapot_.translate.x, 0.1f);
	ImGui::End();

	ImGui::Begin("MultiMesh");
	ImGui::DragFloat3("scale", &worldTransformMultiMesh_.scale.x, 0.1f);
	ImGui::DragFloat3("rotate", &worldTransformMultiMesh_.rotate.x, 0.1f);
	ImGui::DragFloat3("transform", &worldTransformMultiMesh_.translate.x, 0.1f);
	ImGui::End();
}

void ObjectDraw::Draw(Renderer renderer, DebugCamera debugCamera, Microsoft::WRL::ComPtr<ID3D12Resource> materialResource, Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource)
{
	suzanneModel_.DrawPro(renderer, worldTransformSuzanne_, debugCamera.GetViewMatrix(), materialResource, directionalLightResource);
	bunnyModel_.DrawPro(renderer, worldTransformBunny_, debugCamera.GetViewMatrix(), materialResource, directionalLightResource);
	teapotModel_.DrawPro(renderer, worldTransformTeapot_, debugCamera.GetViewMatrix(), materialResource, directionalLightResource);
	multiMeshModel_.DrawPro(renderer, worldTransformMultiMesh_, debugCamera.GetViewMatrix(), materialResource, directionalLightResource);
}
