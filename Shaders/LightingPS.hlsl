Texture2D AlbedoTX : register(t0);
Texture2D NormalRoughTX : register(t1);
Texture2D MetallicTX : register(t2);
Texture2D PositionTX : register(t3);
SamplerState LinearSampler : register(s0);
cbuffer LightCB : register(b0)
{
    float4 CamPos;
    float4 LightDir;
    float4 LightColor;
};
struct PSIn
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};
float NdfGGX(float cosLh, float roughness)
{
    float alpha = roughness * roughness;
    float alphaSq = alpha * alpha;
    float denom = (cosLh * cosLh) * (alphaSq - 1.0) + 1.0;
    return alphaSq / (3.14159265 * denom * denom + 1e-6);
}
float GaSchlickG1(float cosTheta, float k)
{
    return cosTheta / (cosTheta * (1.0 - k) + k + 1e-6);
}
float GaSchlickGGX(float cosLi, float cosLo, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return GaSchlickG1(cosLi, k) * GaSchlickG1(cosLo, k);
}
float4 main(PSIn pin) : SV_Target
{
    float3 albedo = AlbedoTX.Sample(LinearSampler, pin.uv).rgb;
    float4 nr = NormalRoughTX.Sample(LinearSampler, pin.uv);
    float3 N = normalize(nr.xyz);
    float roughness = clamp(nr.w, 0.04, 1.0);
    float metallic = clamp(MetallicTX.Sample(LinearSampler, pin.uv).r, 0.0, 1.0);
    float3 wpos = PositionTX.Sample(LinearSampler, pin.uv).xyz;
    float3 V = normalize(CamPos.xyz - wpos);
    float3 L = normalize(-LightDir.xyz);
    float3 H = normalize(L + V);
    float cosLi = saturate(dot(N, L));
    float cosLo = saturate(dot(N, V));
    float cosLh = saturate(dot(N, H));
    float vh = saturate(dot(V, H));
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    float3 F = F0 + (1.0 - F0) * pow(1.0 - vh, 5.0);
    float D = NdfGGX(cosLh, roughness);
    float G = GaSchlickGGX(cosLi, cosLo, roughness);
    float3 spec = (F * D * G) / max(4.0 * cosLi * cosLo, 1e-5);
    float3 kd = (1.0 - F) * (1.0 - metallic);
    float3 diff = kd * albedo;
    float3 col = (diff + spec) * LightColor.rgb * cosLi;
    float3 amb = albedo * 0.03;
    return float4(col + amb, 1.0);
}
