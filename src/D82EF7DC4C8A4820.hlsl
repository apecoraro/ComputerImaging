// dxc -EmainVS -T vs_6_0 .\D82EF7DC4C8A4820.hlsl

#line 1 "hlsl.hlsl"
struct VERTEX_IN { float3 vPosition : POSITION; float2 vTexture : TEXCOORD; }; struct VERTEX_OUT { float2 vTexture : TEXCOORD; float4 vPosition : SV_POSITION; }; VERTEX_OUT mainVS(VERTEX_IN Input) { VERTEX_OUT Output; Output.vPosition = float4(Input.vPosition, 1.0f); Output.vTexture = Input.vTexture; return Output; }

