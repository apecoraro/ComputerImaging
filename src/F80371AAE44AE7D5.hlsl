// dxc -Emain -T ps_6_0 .\F80371AAE44AE7D5.hlsl

#line 1 "hlsl.hlsl"
struct PS_INPUT { float4 pos : SV_POSITION; float2 uv : TEXCOORD; float4 col : COLOR; }; sampler sampler0; Texture2D texture0; float4 main(PS_INPUT input) : SV_Target { float4 out_col = input.col * texture0.Sample(sampler0, input.uv); const float gamma = 2.2f; out_col.xyz = pow(out_col.xyz, float3(gamma, gamma, gamma)); return out_col; }

