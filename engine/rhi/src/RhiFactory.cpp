#include "D3D11Internal.h"

namespace kizuri::rhi {

IDevice* RhiFactory::CreateDevice()
{
    D3D11Device* device = new D3D11Device();
    if (!device->Initialize())
    {
        delete device;
        return nullptr;
    }
    return device;
}

} // namespace kizuri::rhi