cbuffer Constants : register(b0)
{
    uint3 g_outputSizeAndPixelCount;
}

uint ComputeBinNumber(float red)
{
    float numberOfBins = 8.0;

    return min(uint(numberOfBins * red), 7);
}

Texture2D inputTex : register(t0);
Texture1D<uint> histogramLUT : register(t1);
RWTexture2D outputTex : register(u0);

[numthreads(8, 1, 1)]
void Equalize(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= g_outputSizeAndPixelCount.x || dispatchId.y >= g_outputSizeAndPixelCount.y)
        return;
 
    int3 baseXY = int3(dispatchId.xy, 0);
    float red = inputTex.Load(baseXY);

    uint binNumber = ComputeBinNumber(red);

    uint remappedBinNumber = histogramLUT.Load(int2(binNumber, 0));

    float binSize = 255.0 / 8.0;
    float newRed = (binNumber * binSize) + (binSize * 0.5);
    
    outputTex[baseXY] = float4(newRed, 0, 0, 1);

}
