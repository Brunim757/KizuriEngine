struct VSOut
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};
VSOut main(uint vid : SV_VertexID)
{
    VSOut vout;
    float2 uv = float2((vid << 1) & 2, vid & 2);
    vout.pos = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, 0.0, 1.0);
    vout.uv = uv;
    return vout;
}
