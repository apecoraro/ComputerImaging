// dxc -ESumQuads -T cs_6_0 /Zi /Zss -Od -Qembed_debug .\98FC70E3F5455AB6.hlsl

#line 1 "hlsl.hlsl"
cbuffer Constants : register(b0)
{
    uint2 g_outputSize;
}

RWTexture2D<uint2> outputTex : register(u0);
Texture2D<uint2> inputTex : register(t0);

[numthreads(8, 8, 1)]
void SumQuads(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= g_outputSize.x || dispatchId.y >= g_outputSize.y)
        return;

    int3 basePixelIndex = int3(dispatchId.xy, 0) * 2;
    uint2 binCounts = inputTex.Load(basePixelIndex);
    binCounts += inputTex.Load(basePixelIndex + int3(2, 0, 0));
    binCounts += inputTex.Load(basePixelIndex + int3(0, 2, 0));
    binCounts += inputTex.Load(basePixelIndex + int3(2, 2, 0));

    outputTex[dispatchId.xy] = uint2(binCounts.rg);
}

