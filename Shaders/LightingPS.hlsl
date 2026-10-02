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
    float4 Counts;
    float4 LightA[16];
    float4 LightB[16];
    float4 LightC[16];
    float4 LightD[16];
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
float3 ShadeOne(float3 N, float3 V, float3 L, float3 lightColor, float3 albedo, float roughness, float metallic)
{
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
    return (diff + spec) * lightColor * cosLi;
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
    float3 Lsun = normalize(-LightDir.xyz);
    float3 col = ShadeOne(N, V, Lsun, LightColor.rgb, albedo, roughness, metallic);
    int count = (int)Counts.x;
    if (count > 16) {
        count = 16;
    }
    for (int i = 0; i < count; ++i) {
        float kind = LightA[i].w;
        if (kind > 1.5) {
            float3 Ld = normalize(-LightA[i].xyz);
            col += ShadeOne(N, V, Ld, LightC[i].rgb, albedo, roughness, metallic);
        } else {
            float3 toLight = LightA[i].xyz - wpos;
            float dist = length(toLight);
            float3 L = toLight / max(dist, 1e-4);
            float range = max(LightB[i].w, 1e-3);
            float att = pow(saturate(1.0 - (dist * dist) / (range * range)), 2.0);
            float spot = 1.0;
            if (kind > 0.5) {
                float cosT = dot(L, -LightB[i].xyz);
                spot = smoothstep(LightC[i].a, LightC[i].a + LightD[i].x, cosT);
            }
            col += ShadeOne(N, V, L, LightC[i].rgb, albedo, roughness, metallic) * att * spot;
        }
    }
    float3 amb = albedo * 0.03;
    return float4(col + amb, 1.0);
}
