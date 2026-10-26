#pragma once
#include "Struct.h"
#include <string>

// Model の実装ファイル（Model.cpp・ModelLoader.cpp・ModelPrimitives.cpp・ModelTangents.cpp）の間で共有する補助関数。
// Model を使う側からは読み込まない。
namespace myEngine::modelDetail {

/// @brief 入力の3成分をエンジンのVector3へ変換する。
Vector3 ToVector3(const Vector4& value);
/// @brief 2つの3Dベクトルの差を返す。
Vector3 SubtractVector3(const Vector3& a, const Vector3& b);
/// @brief 2つの3Dベクトルの外積を返す。
Vector3 CrossVector3(const Vector3& a, const Vector3& b);
/// @brief 2つの3Dベクトルの内積を返す。
float DotVector3(const Vector3& a, const Vector3& b);
/// @brief 3Dベクトルの長さを返す。
float LengthVector3(const Vector3& value);
/// @brief ベクトル3を正規化する。
Vector3 NormalizeVector3(const Vector3& value);
/// @brief 3Dベクトルへ指定倍率を掛けて返す。
Vector3 MultiplyVector3(const Vector3& value, float scalar);
/// @brief 2つの3Dベクトルの和を返す。
Vector3 AddVector3(const Vector3& a, const Vector3& b);

/// @brief 頂点位置とUVからモデルの接線を生成する。
void GenerateModelTangents(ModelData& modelData);
/// @brief 材質の指定がない図形に使う、既定の材質を作って返す。
MaterialData MakeDefaultPrimitiveMaterial();
/// @brief OBJ形式モデルであるか判定する。
bool IsObjModel(const std::string& filename);

} // namespace myEngine::modelDetail
