#include "ImageProcessor.h"

#include "Device.h"
#include "DynamicBufferRing.h"
#include "Error.h"
#include "Imgui.h"
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

void ImageProcessor::OnCreate(
    const std::string& shaderEntryFunc,
    Texture& input1,
    Texture& input2,
    Device* pDevice,
    UploadHeap* pUploadHeap,
    ResourceViewHeaps* pResourceViewHeaps,
    DynamicBufferRing* pConstantBufferRing)
{
    m_pDevice = pDevice;
    m_pResourceViewHeaps = pResourceViewHeaps;
    m_pConstantBufferRing = pConstantBufferRing;

    D3D12_SHADER_BYTECODE shaderByteCode = {};
    DefineList defines;
    CAULDRON_DX12::CompileShaderFromFile(
        "DX12/ImageProcessor.hlsl",
        &defines,
        shaderEntryFunc.c_str(),
        "-T cs_6_0 /Zi /Zss",
        &shaderByteCode);

    {
        int parameterCount = 0;
        CD3DX12_ROOT_PARAMETER rtSlot[3];

        rtSlot[parameterCount++].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL);

        CD3DX12_DESCRIPTOR_RANGE uavDescRange = {};
        uavDescRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
        rtSlot[parameterCount++].InitAsDescriptorTable(1, &uavDescRange, D3D12_SHADER_VISIBILITY_ALL);

        CD3DX12_DESCRIPTOR_RANGE srvDescRange = {};
        srvDescRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);
        rtSlot[parameterCount++].InitAsDescriptorTable(1, &srvDescRange, D3D12_SHADER_VISIBILITY_ALL);

        CD3DX12_ROOT_SIGNATURE_DESC descRootSignature = CD3DX12_ROOT_SIGNATURE_DESC();
        descRootSignature.NumParameters = parameterCount;
        descRootSignature.pParameters = rtSlot;

        D3D12_STATIC_SAMPLER_DESC nearestSampler = {};
        nearestSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        nearestSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        nearestSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        nearestSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        nearestSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        nearestSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        nearestSampler.MinLOD = 0.0f;
        nearestSampler.MaxLOD = D3D12_FLOAT32_MAX;
        nearestSampler.MipLODBias = 0;
        nearestSampler.MaxAnisotropy = 1;
        nearestSampler.ShaderRegister = 0;
        nearestSampler.RegisterSpace = 0;
        nearestSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        descRootSignature.NumStaticSamplers = 1;
        descRootSignature.pStaticSamplers = &nearestSampler;

        descRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

        ID3DBlob* pOutBlob = nullptr;
        ID3DBlob* pErrorBlob = nullptr;

        ThrowIfFailed(D3D12SerializeRootSignature(
            &descRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &pOutBlob, &pErrorBlob));
        ThrowIfFailed(
            pDevice->GetDevice()->CreateRootSignature(
                0, pOutBlob->GetBufferPointer(), pOutBlob->GetBufferSize(), IID_PPV_ARGS(&m_pRootSignature))
        );
        CAULDRON_DX12::SetName(m_pRootSignature, std::string("ImageProcessor::") + shaderEntryFunc);

        pOutBlob->Release();
        if (pErrorBlob)
            pErrorBlob->Release();
    }

    D3D12_COMPUTE_PIPELINE_STATE_DESC descPso = {};
    descPso.CS = shaderByteCode;
    descPso.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
    descPso.pRootSignature = m_pRootSignature;
    descPso.NodeMask = 0;

    ThrowIfFailed(pDevice->GetDevice()->CreateComputePipelineState(&descPso, IID_PPV_ARGS(&m_pPipeline)));

    m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(1, &m_constBuffer);
    m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(2, &m_inputTextureSrvTable);

    input1.CreateSRV(0, &m_inputTextureSrvTable);
    input2.CreateSRV(1, &m_inputTextureSrvTable);

    CreateOutputResource(input1, input2);
}

void ImageProcessor::SetInput1(CAULDRON_DX12::Texture& input1)
{
    m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(2, &m_inputTextureSrvTable);
    input1.CreateSRV(0, &m_inputTextureSrvTable);
}

void ImageProcessor::SetInput2(CAULDRON_DX12::Texture& input2)
{
    input2.CreateSRV(1, &m_inputTextureSrvTable);
}

void ImageProcessor::CreateOutputResource(
    CAULDRON_DX12::Texture& input1, CAULDRON_DX12::Texture& input2)
{
    uint32_t newWidth = max(input1.GetWidth(), input2.GetWidth());
    uint32_t newHeight = max(input1.GetHeight(), input2.GetHeight());
    CD3DX12_RESOURCE_DESC outputDesc =
        CD3DX12_RESOURCE_DESC::Tex2D(
            DXGI_FORMAT_R16G16B16A16_FLOAT,
            newWidth, newHeight,
            1, // array size
            1, // mip size
            1, // sample count
            0, // sample quality
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    m_outputTexture.InitRenderTarget(m_pDevice, "ImageProcessorOutput", &outputDesc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(1, &m_outputUav);
    m_outputTexture.CreateUAV(0, &m_outputUav);

    m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(1, &m_outputSrv);
    m_outputTexture.CreateSRV(0, &m_outputSrv);

    m_constants.outputWidth = newWidth;
    m_constants.outputHeight = newHeight;
}

void ImageProcessor::OnDestroy()
{
    m_outputTexture.OnDestroy();

    if (m_pPipeline != NULL)
    {
        m_pPipeline->Release();
        m_pPipeline = NULL;
    }

    if (m_pRootSignature != NULL)
    {
        m_pRootSignature->Release();
        m_pRootSignature = NULL;
    }
}

void ImageProcessor::Draw(ID3D12GraphicsCommandList *pCommandList)
{
    CAULDRON_DX12::UserMarker marker(pCommandList, "ImageProcessor");

    CD3DX12_RESOURCE_BARRIER barrier =
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_outputTexture.GetResource(),
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    pCommandList->ResourceBarrier(1, &barrier);

    D3D12_GPU_VIRTUAL_ADDRESS cbHandle;
    uint32_t* pConstMem;
    uint32_t constantsSize = sizeof(Constants);
    m_pConstantBufferRing->AllocConstantBuffer(constantsSize, (void**)&pConstMem, &cbHandle);

    memcpy(pConstMem, &m_constants, constantsSize);

    ID3D12DescriptorHeap* pDescriptorHeaps[] = { m_pResourceViewHeaps->GetCBV_SRV_UAVHeap(), m_pResourceViewHeaps->GetSamplerHeap() };
    pCommandList->SetDescriptorHeaps(2, pDescriptorHeaps);
    pCommandList->SetComputeRootSignature(m_pRootSignature);

    int params = 0;
    pCommandList->SetComputeRootConstantBufferView(params++, cbHandle);
    pCommandList->SetComputeRootDescriptorTable(params++, m_outputUav.GetGPU());
    pCommandList->SetComputeRootDescriptorTable(params++, m_inputTextureSrvTable.GetGPU());

    pCommandList->SetPipelineState(m_pPipeline);

    uint32_t dispatchX = (m_outputTexture.GetWidth() + 7) / 8;
    uint32_t dispatchY = (m_outputTexture.GetHeight() + 7) / 8;
    uint32_t dispatchZ = 1;
    pCommandList->Dispatch(dispatchX, dispatchY, dispatchZ);

    barrier = 
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_outputTexture.GetResource(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    pCommandList->ResourceBarrier(1, &barrier);
}
