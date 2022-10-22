// dxc -EQuadCount -T cs_6_0 /Zi /Zss .\1F469A5958514919.hlsl

#line 1 "hlsl.hlsl"
cbuffer Constants : register(b0)
{
    uint2 g_outputSize;
}

RWTexture2D<uint2> outputTex : register(u0);
Texture2D inputTex : register(t0);

[numthreads(8, 8, 1)]
void QuadCount(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= g_outputSize.x || dispatchId.y >= g_outputSize.y)
        return;

    int2 evenOddPixel = dispatchId.xy % 1;

    int3 basePixelIndex = int3(dispatchId.xy - evenOddPixel, 0);

    uint binNumber = ((evenOddPixel.y * 4) + evenOddPixel.x);
    uint bin2Intensity = 1 << binNumber;
    uint bin1Intensity = bin2Intensity >> 1;

    uint2 binCounts = uint2(0, 0);

    uint red = uint(inputTex.Load(int3(dispatchId.xy, 0)).r * 255.0);
    if (bin1Intensity & red)
        binCounts.r += 1;
    if (bin2Intensity & red)
        binCounts.g += 1;

    red = uint(inputTex.Load(basePixelIndex + int3((evenOddPixel + int2(1, 0)) % 1, 0)).r * 255.0);
    if (bin1Intensity & red)
        binCounts.r += 1;
    if (bin2Intensity & red)
        binCounts.g += 1;

    red = uint(inputTex.Load(basePixelIndex + int3((evenOddPixel + int2(0, 1)) % 1, 0)).r * 255.0);
    if (bin1Intensity & red)
        binCounts.r += 1;
    if (bin2Intensity & red)
        binCounts.g += 1;

    red = uint(inputTex.Load(basePixelIndex + int3((evenOddPixel + int2(1, 1)) % 1, 0)).r * 255.0);
    if (bin1Intensity & red)
        binCounts.r += 1;
    if (bin2Intensity & red)
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

