// dxc -EQuadCount -T cs_6_0 /Zi /Zss -Od -Qembed_debug .\1879E31BD3C8A389.hlsl

#line 1 "hlsl.hlsl"
cbuffer Constants : register(b0)
{
    uint2 g_outputSize;
}

uint ComputeBinNumber(float red)
{
    float numberOfBins = 7.999;

    return uint(numberOfBins * red);
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

    redBit = ComputeBinNumber(inputTex.Load(basePixelIndex + int3(1, 0, 0)).r);
    if (binNumber1 == redBit)
        binCounts.r += 1;
    if (binNumber2 == redBit)
        binCounts.g += 1;

    redBit = ComputeBinNumber(inputTex.Load(basePixelIndex + int3(0, 1, 0)).r);
    if (binNumber1 == redBit)
        binCounts.r += 1;
    if (binNumber2 == redBit)
        binCounts.g += 1;

    redBit = ComputeBinNumber(inputTex.Load(basePixelIndex + int3(1, 1, 0)).r);
    if (binNumber1 == redBit)
        binCounts.r += 1;
    if (binNumber2 == redBit)
        binCounts.g += 1;

    outputTex[dispatchId.xy] = binCounts;
}

[numthreads(8, 8, 1)]
void SumCounts(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= g_outputSize.x || dispatchId.y >= g_outputSize.y)
        return;

    int3 basePixelIndex = int3(dispatchId.xy, 0);
    uint2 binCounts = inputTex.Load(basePixelIndex).rg;
    binCounts += inputTex.Load(basePixelIndex + int3(2, 0, 0)).rg;
    binCounts += inputTex.Load(basePixelIndex + int3(0, 2, 0)).rg;
    binCounts += inputTex.Load(basePixelIndex + int3(2, 2, 0)).rg;

    outputTex[dispatchId.xy] = binCounts;
}

