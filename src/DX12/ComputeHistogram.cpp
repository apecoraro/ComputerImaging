#include "ComputeHistogram.h"

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

void ComputeHistogram::OnCreate(
    Texture& input,
    Device* pDevice,
    UploadHeap* pUploadHeap,
    ResourceViewHeaps* pResourceViewHeaps,
    DynamicBufferRing* pConstantBufferRing)
{
    m_pDevice = pDevice;
    m_pResourceViewHeaps = pResourceViewHeaps;
    m_pConstantBufferRing = pConstantBufferRing;

    m_quadCount.OnCreate(input, pDevice, pUploadHeap, pResourceViewHeaps, pConstantBufferRing);

    m_sumQuads.OnCreate(input, pDevice, pUploadHeap, pResourceViewHeaps, pConstantBufferRing);

    m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(1, &m_constBuffer);
    m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(1, &m_inputTextureSrv);

    input.CreateSRV(0, &m_inputTextureSrv);

    CreateOutputResource(input);
}

void QuadCount::OnCreate(
    Texture& input,
    Device* pDevice,
    UploadHeap* pUploadHeap,
    ResourceViewHeaps* pResourceViewHeaps,
    DynamicBufferRing* pConstantBufferRing)
{
    {
        int parameterCount = 0;
        CD3DX12_ROOT_PARAMETER rtSlot[3];

        rtSlot[parameterCount++].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL);

        CD3DX12_DESCRIPTOR_RANGE uavDescRange = {};
        uavDescRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
        rtSlot[parameterCount++].InitAsDescriptorTable(1, &uavDescRange, D3D12_SHADER_VISIBILITY_ALL);

        CD3DX12_DESCRIPTOR_RANGE srvDescRange = {};
        srvDescRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
        rtSlot[parameterCount++].InitAsDescriptorTable(1, &srvDescRange, D3D12_SHADER_VISIBILITY_ALL);

        CD3DX12_ROOT_SIGNATURE_DESC descRootSignature = CD3DX12_ROOT_SIGNATURE_DESC();
        descRootSignature.NumParameters = parameterCount;
        descRootSignature.pParameters = rtSlot;
        descRootSignature.NumStaticSamplers = 0;
        descRootSignature.pStaticSamplers = nullptr;

        descRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

        ID3DBlob* pOutBlob = nullptr;
        ID3DBlob* pErrorBlob = nullptr;

        ThrowIfFailed(D3D12SerializeRootSignature(
            &descRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &pOutBlob, &pErrorBlob));
        ThrowIfFailed(
            pDevice->GetDevice()->CreateRootSignature(
                0, pOutBlob->GetBufferPointer(), pOutBlob->GetBufferSize(), IID_PPV_ARGS(&m_pComputeRootSignature))
        );
        CAULDRON_DX12::SetName(m_pComputeRootSignature, std::string("ComputeHistogram::QuadCount"));

        pOutBlob->Release();
        if (pErrorBlob)
            pErrorBlob->Release();
    }

    D3D12_SHADER_BYTECODE shaderByteCode = {};
    DefineList defines;
    CAULDRON_DX12::CompileShaderFromFile(
        "DX12/HistogramQuadCount.hlsl",
        &defines,
        "QuadCount",
        "-T cs_6_0 /Zi /Zss -Od -Qembed_debug",
        &shaderByteCode);

    D3D12_COMPUTE_PIPELINE_STATE_DESC descPso = {};
    descPso.CS = shaderByteCode;
    descPso.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
    descPso.pRootSignature = m_pComputeRootSignature;
    descPso.NodeMask = 0;

    ThrowIfFailed(pDevice->GetDevice()->CreateComputePipelineState(&descPso, IID_PPV_ARGS(&m_pQuadCountPipeline)));
}

void SumQuads::OnCreate(
    Texture& input,
    Device* pDevice,
    UploadHeap* pUploadHeap,
    ResourceViewHeaps* pResourceViewHeaps,
    DynamicBufferRing* pConstantBufferRing)
{
    {
        int parameterCount = 0;
        CD3DX12_ROOT_PARAMETER rtSlot[3];

        rtSlot[parameterCount++].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL);

        CD3DX12_DESCRIPTOR_RANGE uavDescRange = {};
        uavDescRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
        rtSlot[parameterCount++].InitAsDescriptorTable(1, &uavDescRange, D3D12_SHADER_VISIBILITY_ALL);

        CD3DX12_DESCRIPTOR_RANGE srvDescRange = {};
        srvDescRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
        rtSlot[parameterCount++].InitAsDescriptorTable(1, &srvDescRange, D3D12_SHADER_VISIBILITY_ALL);

        CD3DX12_ROOT_SIGNATURE_DESC descRootSignature = CD3DX12_ROOT_SIGNATURE_DESC();
        descRootSignature.NumParameters = parameterCount;
        descRootSignature.pParameters = rtSlot;
        descRootSignature.NumStaticSamplers = 0;
        descRootSignature.pStaticSamplers = nullptr;

        descRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

        ID3DBlob* pOutBlob = nullptr;
        ID3DBlob* pErrorBlob = nullptr;

        ThrowIfFailed(D3D12SerializeRootSignature(
            &descRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &pOutBlob, &pErrorBlob));
        ThrowIfFailed(
            pDevice->GetDevice()->CreateRootSignature(
                0, pOutBlob->GetBufferPointer(), pOutBlob->GetBufferSize(), IID_PPV_ARGS(&m_pComputeRootSignature))
        );
        CAULDRON_DX12::SetName(m_pComputeRootSignature, std::string("ComputeHistogram::QuadCount"));

        pOutBlob->Release();
        if (pErrorBlob)
            pErrorBlob->Release();
    }

    D3D12_SHADER_BYTECODE shaderByteCode = {};
    DefineList defines;
    shaderByteCode = {};
    CAULDRON_DX12::CompileShaderFromFile(
        "DX12/HistogramSumQuads.hlsl",
        &defines,
        "SumQuads",
        "-T cs_6_0 /Zi /Zss -Od -Qembed_debug",
        &shaderByteCode);

    D3D12_COMPUTE_PIPELINE_STATE_DESC descPso = {};
    descPso.CS = shaderByteCode;
    descPso.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
    descPso.pRootSignature = m_pComputeRootSignature;
    descPso.NodeMask = 0;

    ThrowIfFailed(pDevice->GetDevice()->CreateComputePipelineState(&descPso, IID_PPV_ARGS(&m_pSumCountsPipeline)));
}

void ComputeHistogram::SetInput(CAULDRON_DX12::Texture& input)
{
    m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(1, &m_inputTextureSrv);
    input.CreateSRV(0, &m_inputTextureSrv);
}

void ComputeHistogram::CreateOutputResource(CAULDRON_DX12::Texture& input)
{
    // Histogram count requires multiple of 2 dimensions.
    uint32_t extraWidth = input.GetWidth() % 2;
    uint32_t extraHeight = input.GetHeight() % 2;
    uint32_t histWidth = input.GetWidth() + extraWidth;
    uint32_t histHeight = input.GetHeight() + extraHeight;
    CD3DX12_RESOURCE_DESC outputDesc1 =
        CD3DX12_RESOURCE_DESC::Tex2D(
            DXGI_FORMAT_R16G16_UINT,
            histWidth, histHeight,
            1, // array size
            1, // mip size
            1, // sample count
            0, // sample quality
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    m_histogramOutput1State = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    m_histogramOutput1.InitRenderTarget(m_pDevice, "ComputeHistogramOutput1", &outputDesc1, m_histogramOutput1State);

    m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(1, &m_outputUav1);
    m_histogramOutput1.CreateUAV(0, &m_outputUav1);

    m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(1, &m_outputSrv1);
    m_histogramOutput1.CreateSRV(0, &m_outputSrv1);

    CD3DX12_RESOURCE_DESC outputDesc2 =
        CD3DX12_RESOURCE_DESC::Tex2D(
            DXGI_FORMAT_R16G16_UINT,
            histWidth >> 1, histHeight >> 1, // 1/4 the size of output1.
            1, // array size
            1, // mip size
            1, // sample count
            0, // sample quality
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    m_histogramOutput2State = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    m_histogramOutput2.InitRenderTarget(m_pDevice, "ComputeHistogramOutput2", &outputDesc2, m_histogramOutput2State);

    m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(1, &m_outputUav2);
    m_histogramOutput2.CreateUAV(0, &m_outputUav2);

    m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(1, &m_outputSrv2);
    m_histogramOutput2.CreateSRV(0, &m_outputSrv2);
}

void ComputeHistogram::OnDestroy()
{
    m_histogramOutput1.OnDestroy();

    m_quadCount.OnDestroy();
    m_sumQuads.OnDestroy();
}

void QuadCount::OnDestroy()
{
    if (m_pQuadCountPipeline != nullptr)
    {
        m_pQuadCountPipeline->Release();
        m_pQuadCountPipeline = nullptr;
    }

    if (m_pComputeRootSignature != nullptr)
    {
        m_pComputeRootSignature->Release();
        m_pComputeRootSignature = nullptr;
    }
}

void SumQuads::OnDestroy()
{
    if (m_pSumCountsPipeline != nullptr)
    {
        m_pSumCountsPipeline->Release();
        m_pSumCountsPipeline = nullptr;
    }

    if (m_pComputeRootSignature != nullptr)
    {
        m_pComputeRootSignature->Release();
        m_pComputeRootSignature = nullptr;
    }
}

void ComputeHistogram::Draw(ID3D12GraphicsCommandList* pCommandList)
{
    CAULDRON_DX12::UserMarker marker(pCommandList, "ComputeHistogram");

    CD3DX12_RESOURCE_BARRIER barriers[2] = {
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_histogramOutput1.GetResource(),
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
        CD3DX12_RESOURCE_BARRIER()
    };

    if (m_histogramOutput1State == D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
    {
        pCommandList->ResourceBarrier(1, barriers);
        m_histogramOutput1State = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }

    m_quadCount.Setup(pCommandList);

    D3D12_GPU_VIRTUAL_ADDRESS cbHandle;
    uint32_t* pConstMem;
    uint32_t constantsSize = sizeof(Constants);
    m_pConstantBufferRing->AllocConstantBuffer(constantsSize, (void**)&pConstMem, &cbHandle);

    m_constants.outputWidth = m_histogramOutput1.GetWidth();
    m_constants.outputHeight = m_histogramOutput1.GetHeight();
    memcpy(pConstMem, &m_constants, constantsSize);

    ID3D12DescriptorHeap* pDescriptorHeaps[] = { m_pResourceViewHeaps->GetCBV_SRV_UAVHeap(), m_pResourceViewHeaps->GetSamplerHeap() };
    pCommandList->SetDescriptorHeaps(2, pDescriptorHeaps);

    pCommandList->SetComputeRootConstantBufferView(0, cbHandle);
    pCommandList->SetComputeRootDescriptorTable(1, m_outputUav1.GetGPU());
    pCommandList->SetComputeRootDescriptorTable(2, m_inputTextureSrv.GetGPU());

    m_quadCount.Draw(pCommandList, m_histogramOutput1.GetWidth(), m_histogramOutput1.GetHeight());

    barriers[0] =
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_histogramOutput1.GetResource(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    barriers[1] =
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_histogramOutput2.GetResource(),
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    m_histogramOutput1State = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    if (m_histogramOutput2State == D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
    {
        pCommandList->ResourceBarrier(2, barriers);
        m_histogramOutput2State = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }
    else
        pCommandList->ResourceBarrier(1, barriers);


    // If the first output is already 2x2 then we can skip the reduction.
    if (m_constants.outputWidth > 2 || m_constants.outputHeight > 2)
    {
        m_sumQuads.Setup(pCommandList);

        auto outputUav = m_outputUav2.GetGPU();
        auto outputSrv = m_outputSrv2.GetGPU();
        auto pOutputResource = &m_histogramOutput2;

        auto inputUav = m_outputUav1.GetGPU();
        auto inputSrv = m_outputSrv1.GetGPU();
        auto pInputResource = &m_histogramOutput1;

        m_constants.outputWidth = pOutputResource->GetWidth();
        m_constants.outputHeight = pOutputResource->GetHeight();

        while (true)
        {
            m_pConstantBufferRing->AllocConstantBuffer(constantsSize, (void**)&pConstMem, &cbHandle);
            m_constants.outputWidth = max(m_constants.outputWidth, 2);
            m_constants.outputHeight = max(m_constants.outputHeight, 2);
            memcpy(pConstMem, &m_constants, constantsSize);

            pCommandList->SetComputeRootConstantBufferView(0, cbHandle);
            pCommandList->SetComputeRootDescriptorTable(1, outputUav);
            pCommandList->SetComputeRootDescriptorTable(2, inputSrv);

            m_sumQuads.Draw(pCommandList, pOutputResource->GetWidth(), pOutputResource->GetHeight());

            barriers[0] =
                CD3DX12_RESOURCE_BARRIER::Transition(
                    pOutputResource->GetResource(),
                    D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                    D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            barriers[1] =
                CD3DX12_RESOURCE_BARRIER::Transition(
                    pInputResource->GetResource(),
                    D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                    D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            pCommandList->ResourceBarrier(2, barriers);

            m_histogramOutput1State = m_histogramOutput1State == D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS : D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            m_histogramOutput2State = m_histogramOutput2State == D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS : D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

            m_constants.outputWidth >>= 1;
            m_constants.outputHeight >>= 1;
            if (m_constants.outputWidth == 1 && m_constants.outputHeight == 1)
                break;

            auto pTmp = pOutputResource;
            pOutputResource = pInputResource;
            pInputResource = pOutputResource;

            auto tmpUav = outputUav;
            outputUav = inputUav;
            inputUav = tmpUav;

            auto tmpSrv = outputSrv;
            outputSrv = inputSrv;
            inputSrv = outputSrv;
        }

        if (memcmp(&outputSrv, &m_outputSrv1.GetGPU(), sizeof(outputSrv)) == 0)
            m_pCurrentOutputSrv = &m_outputSrv1;
        else
            m_pCurrentOutputSrv = &m_outputSrv2;
    }
    else
        m_pCurrentOutputSrv = &m_outputSrv1;
}

void QuadCount::Setup(ID3D12GraphicsCommandList* pCommandList)
{
    pCommandList->SetPipelineState(m_pQuadCountPipeline);
    pCommandList->SetComputeRootSignature(m_pComputeRootSignature);
}

void QuadCount::Draw(ID3D12GraphicsCommandList* pCommandList, uint32_t outputWidth, uint32_t outputHeight)
{

    uint32_t dispatchX = (outputWidth + 7) / 8;
    uint32_t dispatchY = (outputHeight + 7) / 8;
    uint32_t dispatchZ = 1;
    pCommandList->Dispatch(dispatchX, dispatchY, dispatchZ);
}

void SumQuads::Setup(ID3D12GraphicsCommandList* pCommandList)
{
    pCommandList->SetPipelineState(m_pSumCountsPipeline);
    pCommandList->SetComputeRootSignature(m_pComputeRootSignature);
}

void SumQuads::Draw(ID3D12GraphicsCommandList* pCommandList, uint32_t outputWidth, uint32_t outputHeight)
{

    uint32_t dispatchX = (outputWidth + 7) / 8;
    uint32_t dispatchY = (outputHeight + 7) / 8;
    uint32_t dispatchZ = 1;
    pCommandList->Dispatch(dispatchX, dispatchY, dispatchZ);
}
