#include "UnsharpMask.h"

#include "Device.h"
#include "DynamicBufferRing.h"
#include "Error.h"
#include "Helper.h"
#include "ShaderCompiler.h"
#include "ShaderCompilerHelper.h"
#include "StaticBufferPool.h"
#include "UploadHeap.h"
#include "UserMarkers.h"
#include "Texture.h"

#include "stdafx.h"

using namespace CS570;
using namespace CAULDRON_DX12;

void UnsharpMask::OnCreate(
    CAULDRON_DX12::Texture& input,
    uint32_t blurKernelSize,
    float blurKernelVariance,
    Device* pDevice,
    UploadHeap* pUploadHeap,
    ResourceViewHeaps* pResourceViewHeaps,
    DynamicBufferRing* pConstantBufferRing)
{
    m_gaussianBlur.OnCreate(input, blurKernelSize, blurKernelVariance,
        pDevice, pUploadHeap, pResourceViewHeaps, pConstantBufferRing);

    m_subtractOperation.OnCreate("Subtract", input, m_gaussianBlur.GetOutputResource(),
        pDevice, pUploadHeap, pResourceViewHeaps, pConstantBufferRing);

    m_addOperation.OnCreate("Add", input, m_subtractOperation.GetOutputResource(),
        pDevice, pUploadHeap, pResourceViewHeaps, pConstantBufferRing);
}

void UnsharpMask::OnDestroy()
{
    m_gaussianBlur.OnDestroy();
    m_subtractOperation.OnDestroy();
    m_addOperation.OnDestroy();
}

void UnsharpMask::Draw(ID3D12GraphicsCommandList* pCommandList)
{
    UserMarker marker(pCommandList, "UnsharpMask");

    m_gaussianBlur.Draw(pCommandList);
    m_subtractOperation.Draw(pCommandList);
    m_addOperation.Draw(pCommandList);
}
