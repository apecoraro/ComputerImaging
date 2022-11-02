cbuffer Constants : register(b0)
{
    uint2 g_outputSize;
}

uint ComputeBinNumber(float red)
{
    float numberOfBins = 8.0;

    return min(uint(numberOfBins * red), 7);
}

Texture2D inputTex : register(t0);
Texture1D<uint> histogramLUT : register(t1);
RWTexture2D<float4> outputTex : register(u0);

[numthreads(8, 8, 1)]
void Equalize(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= g_outputSize.x || dispatchId.y >= g_outputSize.y)
        return;
 
    int3 baseXY = int3(dispatchId.xy, 0);
    float red = inputTex.Load(baseXY).r;

    uint binNumber = ComputeBinNumber(red);

    uint remappedBinNumber = histogramLUT.Load(int2(binNumber, 0));

    const float binSize = 255.0 / 8.0;
    const float remapped[8] = { 0, binSize * 1.25, 2.5 * binSize, 3.5 * binSize, 4.5 * binSize, 5.5 * binSize, 6.75 * binSize, 8.0 * binSize };
    float newRed = remapped[remappedBinNumber] / 255.0;
    
    outputTex[baseXY.xy] = float4(newRed, newRed, newRed, 1);
}
