#pragma once
#include "Kizuri/Assets/MeshCodec.h"
#include <string>
namespace Kizuri {
class LogStore;
bool ImportGltfMesh(const std::string& glbPath, const std::string& keepGuid, MeshAssetData& out, LogStore* log);
}