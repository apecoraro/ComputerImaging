cbuffer Constants : register(b0)
{
    uint2 g_outputSize;
    float g_sigmaSquared;
    float g_oneOverSigmaSquared;
}

RWTexture2D<float4> outputTex : register(u0);

[numthreads(8, 8, 1)]
void ComputeWeight(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= g_outputSize.x || dispatchId.y >= g_outputSize.y)
        return;

    const float e = 2.7182818f;
    float powerDenominator = 0.5f * g_oneOverSigmaSquared;
    int2 centerXY = int2(g_outputSize) >> 1;
    float2 xy = float2(int2(dispatchId.xy) - centerXY);
    float2 xySquared = xy * xy;
    float ePower = (-1.0f * (xySquared.x + xySquared.y)) * powerDenominator;

    float eRaisedToPower = pow(e, ePower);
    
    float weight = eRaisedToPower;

    outputTex[dispatchId.xy] = float4(weight, weight, weight, weight);
}
