cbuffer Constants : register(b0)
{
    uint2 g_outputSize;
}


Texture2D<float> weights : register(t0);
Texture2D inputTex : register(t1);
RWTexture2D<float4> outputTex : register(u0);

[numthreads(8, 8, 1)]
void ComputeWeight(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= g_outputSize.x || dispatchId.y >= g_outputSize.y)
        return;

    float weights[KERNEL_HEIGHT * KERNEL_WIDTH];
    for (uint row = 0; row < KERNEL_HEIGHT; ++row)
    {
        for (uint col = 0; col < KERNEL_WIDTH; ++col)
        {
            weights[row * KERNEL_WIDTH + col] = weights.Load(int3(col, row, 0));
        }
    }

    float blurredOutput = 0.0f;
    for (uint row = 0; row < KERNEL_HEIGHT; ++row)
    {
        for (uint col = 0; col < KERNEL_WIDTH; ++col)
        {
            blurredOutput += (inputTex.Load(int3(col, row, 0).r) * weights[row * KERNEL_WIDTH + col]);
        }
    }

    outputTex[dispatchId.xy] = float4(blurredOutput, blurredOutput, blurredOutput, 1.0f);
}
