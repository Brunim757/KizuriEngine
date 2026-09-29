#pragma once
#include <string>
namespace Kizuri {
bool ShowOpenSceneDialog(void* hwnd, std::string& outPath);
bool ShowSaveSceneDialog(void* hwnd, std::string& outPath);
}
