cbuffer Constants : register(b0)
{
    uint2 g_outputSizeAndPixelCount;
}

Texture2D<uint2> inputTex : register(t0);
RWTexture1D<uint> outputTex : register(u0);

[numthreads(8, 1, 1)]
void CreateLUT(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= g_outputSizeAndPixelCount.x)
        return;
 
    float oneOverPixelCount = 1.0/float(g_outputSizeAndPixelCount.y);
    int3 inputIndex = int3(0, 0, 0);
    int3 indexOffsets[3] = { int3(1, 0, 0), int3(-1, 1, 0), int3(1, 0, 0) };
    float cumulativeOutput = 0.0f;
    for (uint currentBin = 0; currentBin < dispatchId.x; ++currentBin)
    {
        uint2 binCounts = inputTex.Load(inputIndex);

        inputIndex.x += indexOffsets[currentBin];

        float2 currentBinOutput = float2(binCounts * oneOverPixelCount);

        cumulativeOutput += currentBinOutput.x;

        outputTex[currentBin] = uint(cumulativeOutput);

        cumulativeOutput += currentBinOutput.y;
    }
    outputTex[dispatchId.x] = uint(cumulativeOutput);
}
