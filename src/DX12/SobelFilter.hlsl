cbuffer Constants : register(b0)
{
    uint2 g_outputSize;
}

Texture2D inputTex : register(t0);
RWTexture2D<float4> outputTex : register(u0);

[numthreads(8, 8, 1)]
void HorizontalFilter(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= g_outputSize.x || dispatchId.y >= g_outputSize.y)
        return;

    float weights[9] =
    {
        -1.0f, 0, 1.0f,
        -2.0f, 0, 2.0f,
        -1.0f, 0, 1.0f
    };

    const int halfWidth = 3 >> 1;
    const int halfHeight = 3 >> 1;
    int2 offsetXY = int2(-halfWidth, -halfHeight);
    float weightedOutput = 0.0f;
    [loop]
    for (uint row = 0; row < 3; ++row)
    {
        [loop]
        for (uint col = 0; col < 3; col += 2)
        {
            weightedOutput +=
                inputTex.Load(int3(dispatchId.xy + offsetXY, 0)).r * weights[row * 3 + col];
            offsetXY.x += 2;
        }
        offsetXY.x = -halfWidth;
        ++offsetXY.y;
    }

    outputTex[dispatchId.xy] = float4(weightedOutput, weightedOutput, weightedOutput, 1.0f);
}

[numthreads(8, 8, 1)]
void VerticalFilter(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= g_outputSize.x || dispatchId.y >= g_outputSize.y)
        return;

    float weights[9] =
    {
        1.0f,  2.0f,  1.0f,
        0.0f,  0.0f,  0.0f,
       -1.0f, -2.00, -1.0f
    };

    const int halfWidth = 3 >> 1;
    const int halfHeight = 3 >> 1;
    int2 offsetXY = int2(-halfWidth, -halfHeight);
    float weightedOutput = 0.0f;
    [loop]
    for (uint row = 0; row < 3; row += 2)
    {
        [loop]
        for (uint col = 0; col < 3; ++col)
        {
            weightedOutput +=
                inputTex.Load(int3(dispatchId.xy + offsetXY, 0)).r * weights[row * 3 + col];
            ++offsetXY.x;
        }
        offsetXY.x = -halfWidth;
        offsetXY.y += 2;
    }

    outputTex[dispatchId.xy] = float4(weightedOutput, weightedOutput, weightedOutput, 1.0f);
}
