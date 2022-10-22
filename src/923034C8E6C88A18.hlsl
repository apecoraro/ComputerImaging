// dxc -EmainPS -T ps_6_0 .\923034C8E6C88A18.hlsl

#line 1 "hlsl.hlsl"

cbuffer Constants : register(b0)
{
    bool g_useLinearSampler;
};

struct INPUT
{
    float2 vTexcoord : TEXCOORD;
};

Texture2D InputImage : register(t0);

SamplerState InputSampler : register(s0);





float4 mainPS(INPUT input) : SV_Target
{
    return InputImage.SampleLevel(InputSampler, input.vTexcoord, 0.0f);
}

