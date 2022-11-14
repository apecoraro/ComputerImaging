cbuffer Constants : register(b0)
{
    uint2 g_outputSize;
    float g_logConstant;
    float g_powerConstant;
    float g_powerRaise;
    float g_weightInput1;
    float g_weightInput2;
}

RWTexture2D<float4> outputTex : register(u0);
Texture2D inputTex1 : register(t0);
Texture2D inputTex2 : register(t1);
SamplerState inputSampler : register(s0);

[numthreads(8, 8, 1)]
void Add(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= g_outputSize.x || dispatchId.y >= g_outputSize.y)
        return;

    float2 inputUV = (float2(dispatchId.xy) / float2(g_outputSize.xy));
    
    float4 color1 = inputTex1.SampleLevel(inputSampler, inputUV, 0) * g_weightInput1;
    float4 color2 = inputTex2.SampleLevel(inputSampler, inputUV, 0) * g_weightInput2;

    outputTex[dispatchId.xy] = color1 + color2;
}

[numthreads(8, 8, 1)]
void Subtract(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= g_outputSize.x || dispatchId.y >= g_outputSize.y)
        return;

    float2 inputUV = (float2(dispatchId.xy) / float2(g_outputSize.xy));
    
    float4 color1 = inputTex1.SampleLevel(inputSampler, inputUV, 0) * g_weightInput1;
    float4 color2 = inputTex2.SampleLevel(inputSampler, inputUV, 0) * g_weightInput2;
    
    outputTex[dispatchId.xy] = color1 - color2;
}

[numthreads(8, 8, 1)]
void Product(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= g_outputSize.x || dispatchId.y >= g_outputSize.y)
        return;

    float2 inputUV = (float2(dispatchId.xy) / float2(g_outputSize.xy));
    
    float4 color1 = inputTex1.SampleLevel(inputSampler, inputUV, 0) * g_weightInput1;
    float4 color2 = inputTex2.SampleLevel(inputSampler, inputUV, 0) * g_weightInput2;
    
    outputTex[dispatchId.xy] = color1 * color2;
}

[numthreads(8, 8, 1)]
void Negative(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= g_outputSize.x || dispatchId.y >= g_outputSize.y)
        return;

    float2 inputUV = (float2(dispatchId.xy) / float2(g_outputSize.xy));

    float4 color = inputTex1.SampleLevel(inputSampler, inputUV, 0);

    outputTex[dispatchId.xy] = 1.0f - color;
}

[numthreads(8, 8, 1)]
void Log(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= g_outputSize.x || dispatchId.y >= g_outputSize.y)
        return;

    float2 inputUV = (float2(dispatchId.xy) / float2(g_outputSize.xy));

    float4 color = inputTex1.SampleLevel(inputSampler, inputUV, 0);

    outputTex[dispatchId.xy] = g_logConstant * log(1.0f + color);
}

[numthreads(8, 8, 1)]
void Power(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= g_outputSize.x || dispatchId.y >= g_outputSize.y)
        return;

    float2 inputUV = (float2(dispatchId.xy) / float2(g_outputSize.xy));

    float4 color = inputTex1.SampleLevel(inputSampler, inputUV, 0);

    outputTex[dispatchId.xy] = g_powerConstant * pow(color + 0.001f, g_powerRaise);
}
