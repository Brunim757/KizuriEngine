Texture2D PositionTX : register(t0);
Texture2D NormalTX : register(t1);
SamplerState LinearSampler : register(s0);
cbuffer SsaoCB : register(b0)
{
    float4 SsaoParams;
    float4 SsaoCamPos;
    row_major float4x4 SsaoVP;
};
struct PSIn
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};
float SsaoTap(float3 pixelPos, float3 samplePos, float3 kernelPos, float3 camPos, float radius)
{
    float3 d = samplePos - pixelPos;
    float dist = length(d);
    if (dist > radius) {
        return 0.0;
    }
    float dcS = length(samplePos - camPos);
    float dcK = length(kernelPos - camPos);
    if (dcS >= dcK - 0.01) {
        return 0.0;
    }
    return 1.0 - dist / max(radius, 1e-4);
}
float4 main(PSIn pin) : SV_Target
{
    float radius = SsaoParams.x;
    float intensity = SsaoParams.y;
    float3 P = PositionTX.Sample(LinearSampler, pin.uv).xyz;
    if (dot(P, P) < 1e-8) {
        return float4(1.0, 1.0, 1.0, 1.0);
    }
    float3 N = normalize(NormalTX.Sample(LinearSampler, pin.uv).xyz);
    float3 V = normalize(P - SsaoCamPos.xyz);
    float h1 = frac(sin(dot(pin.uv, float2(12.9898, 78.233))) * 43758.5453);
    float h2 = frac(sin(dot(pin.uv, float2(39.346, 11.135))) * 24634.6345);
    float3 rvec = normalize(float3(h1 * 2.0 - 1.0, h2 * 2.0 - 1.0, 0.5));
    float3 T = normalize(rvec - N * dot(rvec, N));
    float3 B = cross(N, T);
    float occ = 0.0;
    for (int k = 0; k < 16; ++k) {
        float fi = (float)k;
        float zk = 0.15 + 0.85 * (fi + 0.5) / 16.0;
        float rr = sqrt(max(1.0 - zk * zk, 0.0));
        float th = fi * 2.399963 + h1 * 6.283185;
        float3 tap = float3(rr * cos(th), rr * sin(th), zk);
        float3 off = T * tap.x + B * tap.y + N * tap.z;
        float3 Sp = P + off * radius;
        float4 clip = mul(float4(Sp, 1.0), SsaoVP);
        if (clip.w <= 0.0) {
            continue;
        }
        float2 suv = clip.xy / clip.w * float2(0.5, -0.5) + float2(0.5, 0.5);
        float3 Sw = PositionTX.SampleLevel(LinearSampler, suv, 0).xyz;
        occ += SsaoTap(P, Sw, Sp, SsaoCamPos.xyz, radius);
    }
    float ao = 1.0 - intensity * occ / 16.0;
    return float4(saturate(ao), saturate(ao), saturate(ao), 1.0);
}
