// dxc -EQuadCount -T cs_6_0 /Zi /Zss -Od -Qembed_debug .\1A23BE5F554071A0.hlsl

#line 1 "hlsl.hlsl"
cbuffer Constants : register(b0)
{
    uint2 g_inputSize;
    uint2 g_outputSize;
}

uint ComputeBinNumber(float red)
{
    float numberOfBins = 8.0;

    return min(uint(numberOfBins * red), 7);
}

RWTexture2D<uint2> outputTex : register(u0);
Texture2D inputTex : register(t0);

[numthreads(8, 8, 1)]
void QuadCount(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= g_outputSize.x || dispatchId.y >= g_outputSize.y)
        return;

    int2 evenOddPixel = dispatchId.xy % 2;

    int3 basePixelIndex = int3(dispatchId.xy - evenOddPixel, 0);

    uint binNumber1 = (evenOddPixel.y * 4) + (evenOddPixel.x * 2);
    uint binNumber2 = binNumber1 + 1;

    uint2 binCounts = uint2(0, 0);

    uint redBit = ComputeBinNumber(inputTex.Load(basePixelIndex).r);
    if (binNumber1 == redBit)
        binCounts.r += 1;
    if (binNumber2 == redBit)
        binCounts.g += 1;

    int3 offsetPixelIndex = basePixelIndex + int3(1, 0, 0);
    redBit = ComputeBinNumber(inputTex.Load(offsetPixelIndex).r);
    if (binNumber1 == redBit && offsetPixelIndex.x < g_inputSize.x)
        binCounts.r += 1;
    if (binNumber2 == redBit && offsetPixelIndex.x < g_inputSize.x)
        binCounts.g += 1;

    offsetPixelIndex = basePixelIndex + int3(0, 1, 0);
    redBit = ComputeBinNumber(inputTex.Load(offsetPixelIndex).r);
    if (binNumber1 == redBit && offsetPixelIndex.y < g_inputSize.y)
        binCounts.r += 1;
    if (binNumber2 == redBit && offsetPixelIndex.y < g_inputSize.y)
        binCounts.g += 1;

    offsetPixelIndex = basePixelIndex + int3(1, 1, 0);
    redBit = ComputeBinNumber(inputTex.Load(offsetPixelIndex).r);
    if (binNumber1 == redBit && offsetPixelIndex.x < g_inputSize.x && offsetPixelIndex.y < g_inputSize.y)
        binCounts.r += 1;
    if (binNumber2 == redBit && offsetPixelIndex.x < g_inputSize.x && offsetPixelIndex.y < g_inputSize.y)
        binCounts.g += 1;

    outputTex[dispatchId.xy] = binCounts;
}

