cbuffer Constants : register(b0)
{
    uint g_outputSize;
    uint g_pixelCount;
}

Texture2D<uint2> inputTex : register(t0);
RWTexture1D<uint> outputTex : register(u0);

[numthreads(8, 1, 1)]
void InitInverseLUT(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= g_outputSize.x)
        return;
 
    outputTex[dispatchId.x] = 8;
}
