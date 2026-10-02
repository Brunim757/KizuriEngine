cbuffer GeoCB : register(b0)
{
    row_major float4x4 World;
    row_major float4x4 View;
    row_major float4x4 Proj;
};
struct VSIn
{
    float3 pos : POSITION;
    float3 nrm : NORMAL;
    float2 uv : TEXCOORD0;
};
float4 main(VSIn vin) : SV_Position
{
    float4 wp = mul(float4(vin.pos, 1.0), World);
    float4 vp = mul(wp, View);
    return mul(vp, Proj);
}
