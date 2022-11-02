cbuffer Constants : register(b0)
{
    uint g_outputSize;
    uint g_pixelCount;
}

Texture2D<uint2> inputTex : register(t0);
RWTexture1D<uint> outputTex : register(u0);

float2 TwoDecimals(float2 value)
{
    // reduce to two decimals
    return round(value * 100.0) * 0.01;
}

uint ComputeRemapValue(uint binNumber)
{
    float oneOverPixelCount = 1.0 / float(g_pixelCount);
    int3 inputIndex = int3(0, 0, 0);
    int3 indexOffsets[3] = { int3(1, 0, 0), int3(-1, 1, 0), int3(1, 0, 0) };
    int inputOffsetIndex = 0;
    float cumulativeOutput = 0.0f;
    for (uint currentBin = 0; currentBin < (binNumber + 1); currentBin += 2)
    {
        uint2 binCounts = inputTex.Load(inputIndex);

        inputIndex += indexOffsets[inputOffsetIndex++];

        // Two counts per texel.
        float2 currentBinOutput = TwoDecimals(TwoDecimals(float2(binCounts) * oneOverPixelCount) * 7.0);

        cumulativeOutput += currentBinOutput.x;

        cumulativeOutput += (currentBin + 1) < (binNumber + 1) ? currentBinOutput.y : 0;
    }
    
    return min(round(uint(cumulativeOutput)), 7);
}

[numthreads(8, 1, 1)]
void CreateLUT(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= g_outputSize.x)
        return;
 
    outputTex[dispatchId.x] = ComputeRemapValue(dispatchId.x);
}

[numthreads(8, 1, 1)]
void CreateInverseLUT(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= g_outputSize.x)
        return;
 
    uint outputIndex = ComputeRemapValue(dispatchId.x);

    InterlockedMin(outputTex[outputIndex], dispatchId.x);
}
