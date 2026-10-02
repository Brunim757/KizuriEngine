Texture2D SrcTX : register(t0);
Texture2D BloomTX : register(t1);
SamplerState LinearSampler : register(s0);
cbuffer BloomCB : register(b0)
{
    float4 BloomParams;
};
struct PSIn
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};
float4 main(PSIn pin) : SV_Target
{
    float3 c = SrcTX.Sample(LinearSampler, pin.uv).rgb;
    float3 b = BloomTX.Sample(LinearSampler, pin.uv).rgb;
    return float4(c + b * BloomParams.x, 1.0);
}
