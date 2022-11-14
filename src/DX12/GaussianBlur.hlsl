cbuffer Constants : register(b0)
{
    uint2 g_outputSize;
}


Texture2D<float> weightsTex : register(t0);
Texture2D inputTex : register(t1);
RWTexture2D<float4> outputTex : register(u0);

[numthreads(8, 8, 1)]
void Blur(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= g_outputSize.x || dispatchId.y >= g_outputSize.y)
        return;

    float weights[KERNEL_HEIGHT * KERNEL_WIDTH];
    float weightsSum = 0.0f;
    [loop]
    for (uint row = 0; row < KERNEL_HEIGHT; ++row)
    {
        [loop]
        for (uint col = 0; col < KERNEL_WIDTH; ++col)
        {
            float weight = weightsTex.Load(int3(col, row, 0));
            weights[row * KERNEL_WIDTH + col] = weight;
            weightsSum += weight;
        }
    }
    
    float normalizationFactor = 1.0f / weightsSum;

    const int halfWidth = KERNEL_WIDTH >> 1;
    const int halfHeight = KERNEL_HEIGHT >> 1;
    int2 offsetXY = int2(-halfWidth, -halfHeight);
    float blurredOutput = 0.0f;
    [loop]
    for (row = 0; row < KERNEL_HEIGHT; ++row)
    {
        [loop]
        for (uint col = 0; col < KERNEL_WIDTH; ++col)
        {
            blurredOutput += (
                (
                    inputTex.Load(int3(dispatchId.xy + offsetXY, 0)).r * weights[row * KERNEL_WIDTH + col]
                ) * normalizationFactor
            );
            ++offsetXY.x;
        }
        offsetXY.x = -halfWidth;
        ++offsetXY.y;
    }

    outputTex[dispatchId.xy] = float4(blurredOutput, blurredOutput, blurredOutput, 1.0f);
}
