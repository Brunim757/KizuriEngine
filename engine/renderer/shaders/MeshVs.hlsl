struct VsIn
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 Uv : TEXCOORD0;
};

struct VsOut
{
    float4 SvPosition : SV_Position;
    float2 Uv : TEXCOORD0;
    float3 WorldNormal : NORMAL0;
    float3 WorldPosition : POSITION1;
};

cbuffer FrameParams : register(b0)
{
    float4x4 ViewProj;
    float4 CameraPosition;
    float4 ScreenSize;
};

cbuffer ObjectParams : register(b3)
{
    float4x4 World;
    float4x4 WorldInverseTranspose;
    float4 Tint;
};

VsOut main(VsIn input)
{
    VsOut output;
    float4 world = mul(World, float4(input.Position, 1.0));
    output.WorldPosition = world.xyz;
    output.WorldNormal = normalize(mul((float3x3)WorldInverseTranspose, input.Normal));
    output.Uv = input.Uv;
    output.SvPosition = mul(ViewProj, world);
    return output;
}