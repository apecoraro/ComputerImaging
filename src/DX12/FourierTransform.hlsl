cbuffer Constants : register(b0)
{
    uint2 g_outputSize;
}

Texture2D inputTex : register(t0);
RWTexture2D<float4> outputTex : register(u0);

#define PI 3.14159265359f

float2 computeEulers(float uv, float xy, float MN)
{
    float power = (2 * PI * (uv * xy)) / MN;
    float2 complex = float2(sin(power), -sin(power));

    return complex;
}

[numthreads(8, 8, 1)]
void HorizontalPass(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= g_outputSize.x || dispatchId.y >= g_outputSize.y)
        return;

    float2 sum = float2(0.0f, 0.0f);
    const uint v = dispatchId.y;
    [loop]
    for (uint x = 0; x < g_outputSize.x; ++x)
    {
        float2 verticalPassOutput = inputTex.Load(x, v);
        float2 complex = computeEulers(dispatchId.x, x, g_outputSize.x);
        sum += (complex * verticalPassOutput);
    }

    outputTex[dispatchId.xy] = float4(sum, 0.0f, 1.0f);
}

[numthreads(8, 8, 1)]
void VerticalPass(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= g_outputSize.x || dispatchId.y >= g_outputSize.y)
        return;

    float2 sum = float2(0.0f, 0.0f);
    const uint x = dispatchId.x;
    [loop]
    for (uint y = 0; y < g_outputSize.y; ++y)
    {
        float image = inputTex.Load(x, y);
        float xyPower = pow(-1.0f, x + y);
        float2 complex = computeEulers(dispatchId.y, y, g_outputSize.y);
        sum += (image * xyPower * complex);
    }
    
    outputTex[dispatchId.xy] = float4(sum, 0.0f, 1.0f);
}
