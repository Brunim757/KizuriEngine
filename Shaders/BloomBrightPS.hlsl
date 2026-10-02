Texture2D SrcTX : register(t0);
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
    float lum = dot(c, float3(0.299, 0.587, 0.114));
    float k = saturate((lum - BloomParams.x) / max(1.0 - BloomParams.x, 1e-3));
    return float4(c * k, 1.0);
}
