cbuffer MatCB : register(b0)
{
    float4 Albedo;
    float4 Params;
};
Texture2D AlbedoTX : register(t0);
SamplerState LinearSampler : register(s0);
struct PSIn
{
    float4 pos : SV_Position;
    float3 wnrm : NORMAL0;
    float3 wpos : TEXCOORD0;
    float2 uv : TEXCOORD1;
};
struct PSOut
{
    float4 albedo : SV_Target0;
    float4 nrmRough : SV_Target1;
    float4 metallic : SV_Target2;
    float4 wpos : SV_Target3;
};
PSOut main(PSIn pin)
{
    PSOut pout;
    float3 n = normalize(pin.wnrm);
    float3 alb = Albedo.rgb;
    if (Params.z > 0.5) {
        alb = AlbedoTX.Sample(LinearSampler, pin.uv).rgb;
    }
    pout.albedo = float4(alb, 1.0);
    pout.nrmRough = float4(n, Params.x);
    pout.metallic = float4(Params.y, 0.0, 0.0, 1.0);
    pout.wpos = float4(pin.wpos, 1.0);
    return pout;
}
