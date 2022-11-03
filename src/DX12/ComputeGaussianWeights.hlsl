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
    const float PI = 3.141593f;
    const float kFactor = 1.0f / (2.0f * PI);

    float eFactor = kFactor * g_oneOverSigmaSquared;

    float2 xy = float2(dispatchId.xy - g_outputSize);
    float2 xySquared = xy * xy;
    float ePower = (-1.0f * (xySquared.x + xySquared.y)) * 0.5f * g_oneOverSigmaSquared;

    float eRaisedToPower = pow(e, ePower);
    
    float weight = eFactor * eRaisedToPower;

    outputTex[dispatchId.xy] = float4(weight, weight, weight, weight);
}
