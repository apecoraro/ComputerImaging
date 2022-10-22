cbuffer Constants : register(b0)
{
    uint2 g_inputSize;
    uint2 g_outputSize;
}

RWTexture2D<uint2> outputTex : register(u0);
Texture2D<uint2> inputTex : register(t0);

[numthreads(8, 8, 1)]
void SumQuads(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= g_outputSize.x || dispatchId.y >= g_outputSize.y)
        return;
 
    int2 evenOddPixel = dispatchId.xy % 2;
    int3 basePixelIndex = int3(((dispatchId.xy - evenOddPixel) * 2) + evenOddPixel, 0);
    uint2 binCounts = inputTex.Load(basePixelIndex);
    binCounts += inputTex.Load(basePixelIndex + int3(2, 0, 0));
    binCounts += inputTex.Load(basePixelIndex + int3(0, 2, 0));
    binCounts += inputTex.Load(basePixelIndex + int3(2, 2, 0));
    
    outputTex[dispatchId.xy] = uint2(binCounts.rg);
}
