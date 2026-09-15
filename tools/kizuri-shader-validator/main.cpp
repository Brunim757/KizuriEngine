#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#pragma comment(lib, "d3dcompiler.lib")

static int CompileShader(const wchar_t* path, const char* target, const char* entry)
{
    FILE* f = nullptr;
    if (_wfopen_s(&f, path, L"rb") != 0 || f == nullptr)
    {
        fprintf(stderr, "FAILED open: %S\n", path);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = static_cast<char*>(malloc(size));
    if (buf == nullptr)
    {
        fclose(f);
        return 1;
    }
    fread(buf, 1, size, f);
    fclose(f);

    Microsoft::WRL::ComPtr<ID3DBlob> blob;
    Microsoft::WRL::ComPtr<ID3DBlob> errors;
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

    HRESULT hr = D3DCompile(
        buf, size, nullptr, nullptr, nullptr, entry, target,
        flags, 0, &blob, &errors);
    free(buf);

    if (FAILED(hr))
    {
        if (errors != nullptr && errors->GetBufferSize() > 0)
        {
            fprintf(stderr, "  VS compile error: %s\n",
                static_cast<const char*>(errors->GetBufferPointer()));
        }
        return 1;
    }
    return 0;
}

int wmain(int argc, wchar_t* argv[])
{
    const wchar_t* shadersDir = L"engine/KizuriRenderer/shaders";
    if (argc > 1)
    {
        shadersDir = argv[1];
    }

    if (!std::filesystem::exists(shadersDir))
    {
        fprintf(stderr, "Shaders directory not found: %S\n", shadersDir);
        return 1;
    }

    int failures = 0;
    for (const auto& entry : std::filesystem::directory_iterator(shadersDir))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }
        const std::filesystem::path& path = entry.path();
        if (path.extension() != ".hlsl")
        {
            continue;
        }

        const std::wstring filename = path.filename().wstring();
        int vsResult = CompileShader(path.c_str(), "vs_5_0", "main");
        int psResult = CompileShader(path.c_str(), "ps_5_0", "main");
        if (vsResult != 0 && psResult != 0)
        {
            fprintf(stderr, "FAILED: %S (neither VS nor PS compiled)\n", filename.c_str());
            ++failures;
        }
        else
        {
            printf("OK: %S\n", filename.c_str());
        }
    }

    if (failures > 0)
    {
        fprintf(stderr, "FAILED: %d shader(s) failed compilation\n", failures);
        return 1;
    }
    printf("All shaders passed.\n");
    return 0;
}