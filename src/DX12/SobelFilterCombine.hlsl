cbuffer Constants : register(b0)
{
    uint2 g_outputSize;
}

Texture2D horizFilterTex : register(t0);
Texture2D vertFilterTex : register(t1);
RWTexture2D<float4> outputTex : register(u0);

[numthreads(8, 8, 1)]
void Combine(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= g_outputSize.x || dispatchId.y >= g_outputSize.y)
        return;
 
    int3 baseXY = int3(dispatchId.xy, 0);
    float horiz = horizFilterTex.Load(baseXY).r;
    float vert = vertFilterTex.Load(baseXY).r;

    float output = sqrt((horiz * horiz) + (vert * vert));
    outputTex[baseXY.xy] = float4(output, output, output, 1);
}
