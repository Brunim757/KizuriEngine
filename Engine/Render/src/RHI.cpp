#include "Kizuri/RHI.h"
namespace Kizuri {
IRHI* CreateNullRHI();
IRHI* CreateD3D11RHI();
IRHI* CreateRHI(RHI_API api) {
  if (api == RHI_API::D3D11) {
    return CreateD3D11RHI();
  }
  return CreateNullRHI();
}
void DestroyRHI(IRHI* rhi) {
  delete rhi;
}
}
