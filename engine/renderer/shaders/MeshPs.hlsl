struct VsOut
{
    float4 SvPosition : SV_Position;
    float2 Uv : TEXCOORD0;
    float3 WorldNormal : NORMAL0;
    float3 WorldPosition : POSITION1;
};

struct GpuLight
{
    float3 Position;
    float Radius;
    float3 Color;
    float Intensity;
};

cbuffer FrameParams : register(b0)
{
    float4x4 ViewProj;
    float4 CameraPosition;
    float4 ScreenSize;
};

cbuffer LightParams : register(b1)
{
    int LightCount;
    float3 LightPad;
    GpuLight Lights[128];
};

cbuffer ClusterParams : register(b2)
{
    float4 TileSize;
    float4 DepthRange;
    float4 AmbientRow;
};

Texture2D<float4> AlbedoTexture : register(t0);
StructuredBuffer<uint2> ClusterGridBuffer : register(t2);
StructuredBuffer<uint> LightIndexList : register(t3);

SamplerState AlbedoSampler : register(s0);

float LinearDepthFromNdc(float ndcZ)
{
    float nearZ = DepthRange.x;
    float farZ = DepthRange.y;
    return (nearZ * farZ) / (farZ - ndcZ * (farZ - nearZ));
}

float4 main(VsOut input) : SV_Target
{
    float linearDepth = LinearDepthFromNdc(input.SvPosition.z);

    float2 tile = floor(input.SvPosition.xy / TileSize.xy);
    uint2 clusterXy = min((uint2)TileSize.zw - 1, (uint2)tile);
    float sliceF = log(linearDepth / DepthRange.x) * DepthRange.w * DepthRange.z;
    uint clusterZ = min((uint)DepthRange.z - 1, (uint)sliceF);

    uint dimX = (uint)TileSize.z;
    uint dimY = (uint)TileSize.w;
    uint cluster = clusterXy.x + clusterXy.y * dimX + clusterZ * dimX * dimY;

    uint2 range = ClusterGridBuffer[cluster];

    float3 lighting = AmbientRow.xxx;
    float3 worldNormal = normalize(input.WorldNormal);

    for (uint i = 0; i < range.y; ++i)
    {
        uint lightIndex = LightIndexList[range.x + i];
        GpuLight light = Lights[lightIndex];
        float3 toLight = light.Position - input.WorldPosition;
        float distanceSquared = dot(toLight, toLight);
        float attenuation = saturate(1.0 - distanceSquared / (light.Radius * light.Radius));
        attenuation *= attenuation;
        if (attenuation < 1e-4)
        {
            continue;
        }
        float ndl = saturate(dot(worldNormal, toLight * rsqrt(distanceSquared)));
        lighting += light.Color * light.Intensity * attenuation * ndl;
    }

    float4 albedo = AlbedoTexture.Sample(AlbedoSampler, input.Uv);
    return float4(albedo.rgb * lighting, 1.0);
}