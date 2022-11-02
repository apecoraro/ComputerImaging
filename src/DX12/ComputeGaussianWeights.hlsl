cbuffer Constants : register(b0)
{
    uint2 g_outputSize;
    float g_sigmaSquared;
    float g_oneOverSigmaSquared;
}

RWTexture2D<float4> outputTex : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= g_outputSize.x || dispatchId.y >= g_outputSize.y)
        return;
    
    const float e = 2.7182818284590452353602874713527f;
    const float PI = 3.1415926535897932384626433832795f;
    const float kFactor = 1.0f / (2.0f * PI);

    float eFactor = e * kFactor * g_oneOverSigmaSquared;
    float2 xy = float2(dispatchId.xy - g_outputSize);
    float ePower;
}
