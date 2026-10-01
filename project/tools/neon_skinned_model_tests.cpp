#include <assimp/Importer.hpp>
#include <assimp/GltfMaterial.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include "externals/nlohmann/json.hpp"
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <vector>

namespace {
void Require(bool condition, const char* message) {
	if (!condition) throw std::runtime_error(message);
}
void CollectNodes(const aiNode& node, std::set<std::string>& names, size_t& count) {
	names.insert(node.mName.C_Str());
	++count;
	for (unsigned i = 0; i < node.mNumChildren; ++i) CollectNodes(*node.mChildren[i], names, count);
}
}

// 同じAssimp処理フラグで実際のGLBを読み、重みと骨格の整合性を検証する。
int main(int argc, char** argv) {
	try {
		Require(argc == 3, "Usage: neon_skinned_model_tests <model.glb> <report.json>");
		std::ifstream source(argv[1], std::ios::binary);
		uint32_t header[5]{};
		Require(static_cast<bool>(source.read(reinterpret_cast<char*>(header), sizeof(header))), "Missing GLB header");
		Require(header[0] == 0x46546c67 && header[1] == 2 && header[4] == 0x4e4f534a, "Not a GLB 2.0 JSON container");
		Require(header[3] < 8 * 1024 * 1024, "Oversized JSON chunk");
		std::string text(header[3], '\0');
		Require(static_cast<bool>(source.read(text.data(), static_cast<std::streamsize>(text.size()))), "Truncated GLB JSON");
		const auto gltf = nlohmann::json::parse(text);
		Require(gltf.at("extensions").at("VRM").at("meta").at("title") == "AvatarSample_B", "Unexpected VRM identity");
		Assimp::Importer importer;
		const unsigned flags = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_GenSmoothNormals |
			aiProcess_CalcTangentSpace | aiProcess_LimitBoneWeights | aiProcess_ImproveCacheLocality;
		const aiScene* scene = importer.ReadFile(argv[1], flags);
		if (!scene) throw std::runtime_error(importer.GetErrorString());
		Require(scene->mRootNode && scene->mNumMeshes > 0, "Missing geometry/root node");
		std::set<std::string> nodes, bones, weightedBones;
		size_t nodeCount = 0, vertices = 0, weightedVertices = 0, weightEntries = 0;
		unsigned maxInfluences = 0;
		CollectNodes(*scene->mRootNode, nodes, nodeCount);
		nlohmann::json meshes = nlohmann::json::array();
		for (unsigned m = 0; m < scene->mNumMeshes; ++m) {
			const auto& mesh = *scene->mMeshes[m];
			std::vector<float> sums(mesh.mNumVertices, 0.0f);
			std::vector<unsigned> influences(mesh.mNumVertices, 0);
			for (unsigned b = 0; b < mesh.mNumBones; ++b) {
				const auto& bone = *mesh.mBones[b];
				Require(nodes.contains(bone.mName.C_Str()), "Bone is missing from the node hierarchy");
				bones.insert(bone.mName.C_Str());
				for (unsigned w = 0; w < bone.mNumWeights; ++w) {
					const auto& weight = bone.mWeights[w];
					Require(weight.mVertexId < mesh.mNumVertices && std::isfinite(weight.mWeight) && weight.mWeight >= 0, "Invalid skin weight");
					if (weight.mWeight == 0) continue;
					sums[weight.mVertexId] += weight.mWeight;
					++influences[weight.mVertexId];
					++weightEntries;
					weightedBones.insert(bone.mName.C_Str());
				}
			}
			for (unsigned v = 0; v < mesh.mNumVertices; ++v) {
				Require(influences[v] > 0 && influences[v] <= 4, "Missing weight or more than four influences");
				Require(std::abs(sums[v] - 1.0f) < 0.001f, "Unnormalized skin weights");
				++weightedVertices;
				if (influences[v] > maxInfluences) maxInfluences = influences[v];
			}
			for (unsigned f = 0; f < mesh.mNumFaces; ++f) Require(mesh.mFaces[f].mNumIndices == 3, "Non-triangle face");
			vertices += mesh.mNumVertices;
			const auto& material = *scene->mMaterials[mesh.mMaterialIndex];
			aiString materialName, alphaMode;
			int doubleSided = 0;
			material.Get(AI_MATKEY_NAME, materialName);
			material.Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode);
			material.Get(AI_MATKEY_TWOSIDED, doubleSided);
			meshes.push_back({ {"submesh", m}, {"name", mesh.mName.C_Str()}, {"vertices", mesh.mNumVertices},
				{"triangles", mesh.mNumFaces}, {"bones", mesh.mNumBones}, {"material", materialName.C_Str()},
				{"alphaMode", alphaMode.C_Str()}, {"doubleSided", doubleSided != 0} });
		}
		Require(!bones.empty() && weightedVertices == vertices, "Model is not fully skinned");
		const nlohmann::json report = { {"gltfMeshes", gltf.at("meshes").size()}, {"assimpMeshes", scene->mNumMeshes},
			{"submeshes", meshes}, {"nodes", nodeCount}, {"uniqueBones", bones.size()}, {"weightedBones", weightedBones.size()},
			{"vertices", vertices}, {"weightedVertices", weightedVertices}, {"weightEntries", weightEntries},
			{"maxInfluences", maxInfluences}, {"materials", scene->mNumMaterials}, {"animations", scene->mNumAnimations},
			{"embeddedTextures", scene->mNumTextures}, {"passed", true} };
		std::ofstream output(argv[2]);
		output << report.dump(2) << '\n';
		Require(output.good(), "Cannot write inspection report");
		std::cout << report.dump(2) << '\n';
		return 0;
	} catch (const std::exception& error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
