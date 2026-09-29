#include "Kizuri/RHI.h"
#include <unordered_map>
#include <vector>
#include <cstring>
namespace Kizuri {
class NullRHI : public IRHI {
public:
  NullRHI()
    : w(0)
    , h(0)
    , nextId(1)
    , total(0)
    , discarded(0)
    , curVS(0)
    , curPS(0)
    , curLayout(0)
    , curVB(0)
    , curIB(0) {
    curRS.cull = RHICull::Back;
    curRS.fill = RHIFill::Solid;
    curRS.frontCCW = false;
    curDS.depthEnable = true;
    curDS.depthWrite = true;
    curBlend.enable = false;
    curTopo = RHITopology::TriangleList;
    curVP.x = 0.0f;
    curVP.y = 0.0f;
    curVP.w = 0.0f;
    curVP.h = 0.0f;
    curVP.minD = 0.0f;
    curVP.maxD = 1.0f;
  }
  bool Initialize(const RHIDesc& desc) override {
    w = desc.width;
    h = desc.height;
    return w > 0 && h > 0;
  }
  void Shutdown() override {
    buffers.clear();
    cbuffers.clear();
    targets.clear();
    w = 0;
    h = 0;
  }
  bool Resize(int nw, int nh) override {
    if (nw <= 0 || nh <= 0) {
      return false;
    }
    w = nw;
    h = nh;
    return true;
  }
  void Clear(float r, float g, float b, float a) override {
    (void)r;
    (void)g;
    (void)b;
    (void)a;
    Note(false);
  }
  void Present(bool vsync) override {
    (void)vsync;
  }
  const char* BackendName() const override {
    return "Null";
  }
  int Width() const override {
    return w;
  }
  int Height() const override {
    return h;
  }
  void* GetNativeDevice() const override {
    return nullptr;
  }
  void* GetNativeContext() const override {
    return nullptr;
  }
  void GetCacheStats(uint64_t& t, uint64_t& d) const override {
    t = total;
    d = discarded;
  }
  RHIBuffer CreateBuffer(uint64_t size, uint32_t stride, bool isIndex, const void* initialData) override {
    (void)stride;
    (void)isIndex;
    (void)initialData;
    if (size == 0) {
      return 0;
    }
    uint64_t id = nextId++;
    buffers[id] = size;
    return id;
  }
  void DestroyBuffer(RHIBuffer buf) override {
    buffers.erase(buf);
  }
  void SetVertexBuffer(RHIBuffer buf, uint32_t offset) override {
    (void)offset;
    if (curVB == buf) {
      Note(true);
      return;
    }
    Note(false);
    curVB = buf;
  }
  void SetIndexBuffer(RHIBuffer buf) override {
    if (curIB == buf) {
      Note(true);
      return;
    }
    Note(false);
    curIB = buf;
  }
  RHIConstBuffer CreateConstantBuffer(uint64_t size, const void* initialData) override {
    if (size == 0) {
      return 0;
    }
    uint64_t id = nextId++;
    std::vector<unsigned char> mem(static_cast<size_t>(size), 0);
    if (initialData != nullptr) {
      std::memcpy(mem.data(), initialData, static_cast<size_t>(size));
    }
    cbuffers[id] = mem;
    return id;
  }
  void UpdateConstantBuffer(RHIConstBuffer buf, const void* data, uint64_t size) override {
    auto it = cbuffers.find(buf);
    if (it == cbuffers.end() || data == nullptr) {
      return;
    }
    size_t n = static_cast<size_t>(size);
    if (n > it->second.size()) {
      n = it->second.size();
    }
    std::memcpy(it->second.data(), data, n);
  }
  void DestroyConstantBuffer(RHIConstBuffer buf) override {
    cbuffers.erase(buf);
  }
  void SetVertexConstantBuffer(uint32_t slot, RHIConstBuffer buf) override {
    (void)slot;
    (void)buf;
    Note(false);
  }
  void SetPixelConstantBuffer(uint32_t slot, RHIConstBuffer buf) override {
    (void)slot;
    (void)buf;
    Note(false);
  }
  RHIVertexShader CreateVertexShaderFromFile(const char* path, const char* entry) override {
    if (path == nullptr || entry == nullptr || path[0] == '\0') {
      return 0;
    }
    uint64_t id = nextId++;
    shaders[id] = 1;
    return id;
  }
  RHIPixelShader CreatePixelShaderFromFile(const char* path, const char* entry) override {
    if (path == nullptr || entry == nullptr || path[0] == '\0') {
      return 0;
    }
    uint64_t id = nextId++;
    shaders[id] = 1;
    return id;
  }
  void SetVertexShader(RHIVertexShader vs) override {
    if (curVS == vs) {
      Note(true);
      return;
    }
    Note(false);
    curVS = vs;
  }
  void SetPixelShader(RHIPixelShader ps) override {
    if (curPS == ps) {
      Note(true);
      return;
    }
    Note(false);
    curPS = ps;
  }
  RHIInputLayout CreateInputLayoutPNU(RHIVertexShader vs) override {
    if (vs == 0) {
      return 0;
    }
    uint64_t id = nextId++;
    shaders[id] = 1;
    return id;
  }
  void SetInputLayout(RHIInputLayout layout) override {
    if (curLayout == layout) {
      Note(true);
      return;
    }
    Note(false);
    curLayout = layout;
  }
  void SetViewport(const RHIViewport& vp) override {
    if (curVP.x == vp.x && curVP.y == vp.y && curVP.w == vp.w && curVP.h == vp.h && curVP.minD == vp.minD && curVP.maxD == vp.maxD) {
      Note(true);
      return;
    }
    Note(false);
    curVP = vp;
  }
  void SetTopology(RHITopology topo) override {
    if (curTopo == topo) {
      Note(true);
      return;
    }
    Note(false);
    curTopo = topo;
  }
  void SetRasterizerState(const RHIRasterizer& rs) override {
    if (curRS.cull == rs.cull && curRS.fill == rs.fill && curRS.frontCCW == rs.frontCCW) {
      Note(true);
      return;
    }
    Note(false);
    curRS = rs;
  }
  void SetDepthStencilState(const RHIDepthStencil& ds) override {
    if (curDS.depthEnable == ds.depthEnable && curDS.depthWrite == ds.depthWrite) {
      Note(true);
      return;
    }
    Note(false);
    curDS = ds;
  }
  void SetBlendState(const RHIBlend& blend) override {
    if (curBlend.enable == blend.enable) {
      Note(true);
      return;
    }
    Note(false);
    curBlend = blend;
  }
  RHIRenderTarget CreateRenderTarget(int tw, int th, RHIFormat fmt) override {
    if (tw <= 0 || th <= 0) {
      return 0;
    }
    uint64_t id = nextId++;
    targets[id] = static_cast<int>(fmt);
    return id;
  }
  void DestroyRenderTarget(RHIRenderTarget rt) override {
    targets.erase(rt);
  }
  void SetRenderTargets(uint32_t count, const RHIRenderTarget* colorRTs, RHIRenderTarget depthRT) override {
    (void)count;
    (void)colorRTs;
    (void)depthRT;
    Note(false);
  }
  void ClearRenderTarget(RHIRenderTarget rt, float r, float g, float b, float a) override {
    (void)rt;
    (void)r;
    (void)g;
    (void)b;
    (void)a;
  }
  void ClearDepth(RHIRenderTarget depthRT) override {
    (void)depthRT;
  }
  void BindBackbuffer() override {
    Note(false);
  }
  void SetPixelTexture(uint32_t slot, RHIRenderTarget rt) override {
    (void)slot;
    (void)rt;
    Note(false);
  }
  RHISampler CreateSamplerLinear() override {
    uint64_t id = nextId++;
    shaders[id] = 1;
    return id;
  }
  void SetPixelSampler(uint32_t slot, RHISampler sampler) override {
    (void)slot;
    (void)sampler;
    Note(false);
  }
  void DrawIndexed(uint32_t indexCount, uint32_t startIndex, int32_t baseVertex) override {
    (void)indexCount;
    (void)startIndex;
    (void)baseVertex;
  }
  void DrawFullscreenTriangle() override {
  }
private:
  void Note(bool dup) {
    ++total;
    if (dup) {
      ++discarded;
    }
  }
  int w;
  int h;
  uint64_t nextId;
  uint64_t total;
  uint64_t discarded;
  std::unordered_map<uint64_t, uint64_t> buffers;
  std::unordered_map<uint64_t, std::vector<unsigned char>> cbuffers;
  std::unordered_map<uint64_t, int> targets;
  std::unordered_map<uint64_t, int> shaders;
  RHIVertexShader curVS;
  RHIPixelShader curPS;
  RHIInputLayout curLayout;
  RHIBuffer curVB;
  RHIBuffer curIB;
  RHIRasterizer curRS;
  RHIDepthStencil curDS;
  RHIBlend curBlend;
  RHITopology curTopo;
  RHIViewport curVP;
};
IRHI* CreateNullRHI() {
  return new NullRHI();
}
}
