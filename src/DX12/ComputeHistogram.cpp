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
                0, pOutBlob->GetBufferPointer(), pOutBlob->GetBufferSize(), IID_PPV_ARGS(&m_pRootSignature))
        );
        CAULDRON_DX12::SetName(m_pRootSignature, std::string("ComputeHistogram::QuadCount"));

        pOutBlob->Release();
        if (pErrorBlob)
            pErrorBlob->Release();
    }

    m_quadCount.OnCreate(m_pRootSignature, pDevice);

    m_sumQuads.OnCreate(m_pRootSignature, pDevice);

    m_createLUT.OnCreate(m_pRootSignature, input, 8, pDevice, pResourceViewHeaps);

    m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(1, &m_constBuffer);
    m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(1, &m_inputTextureSrv);

    input.CreateSRV(0, &m_inputTextureSrv);
    m_constants.inputWidth = input.GetWidth();
    m_constants.inputHeight = input.GetHeight();

    CreateOutputResource(input);
}

void QuadCount::OnCreate(ID3D12RootSignature* pRootSignature, Device* pDevice)
{
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
    descPso.pRootSignature = pRootSignature;
    descPso.NodeMask = 0;

    ThrowIfFailed(pDevice->GetDevice()->CreateComputePipelineState(&descPso, IID_PPV_ARGS(&m_pPipeline)));
}

void SumQuads::OnCreate(ID3D12RootSignature* pRootSignature, Device* pDevice)
{
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
    descPso.pRootSignature = pRootSignature;
    descPso.NodeMask = 0;

    ThrowIfFailed(pDevice->GetDevice()->CreateComputePipelineState(&descPso, IID_PPV_ARGS(&m_pPipeline)));
}

void CreateLUT::OnCreate(
    ID3D12RootSignature* pRootSignature,
    const Texture& input,
    uint32_t binCount,
    Device* pDevice,
    ResourceViewHeaps* pResourceViewHeaps)
{
    D3D12_SHADER_BYTECODE shaderByteCode = {};
    DefineList defines;
    CAULDRON_DX12::CompileShaderFromFile(
        "DX12/HistogramCreateLUT.hlsl",
        &defines,
        "CreateLUT",
        "-T cs_6_0 /Zi /Zss -Od -Qembed_debug",
        &shaderByteCode);

    D3D12_COMPUTE_PIPELINE_STATE_DESC descPso = {};
    descPso.CS = shaderByteCode;
    descPso.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
    descPso.pRootSignature = pRootSignature;
    descPso.NodeMask = 0;

    ThrowIfFailed(pDevice->GetDevice()->CreateComputePipelineState(&descPso, IID_PPV_ARGS(&m_pPipeline)));

    m_lutConstants.outputSize = binCount;
    m_lutConstants.pixelCount = input.GetWidth() * input.GetHeight();

    CD3DX12_RESOURCE_DESC outputDesc =
        CD3DX12_RESOURCE_DESC::Tex2D(
            DXGI_FORMAT_R32_UINT,
            binCount, 1,
            1, // array size
            1, // mip size
            1, // sample count
            0, // sample quality
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    m_outputLUT.InitRenderTarget(pDevice, "HistogramLUT", &outputDesc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(1, &m_outputUav);
    m_outputLUT.CreateUAV(0, &m_outputUav);

    pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(1, &m_outputSrv);
    m_outputLUT.CreateSRV(0, &m_outputSrv);
}

void ComputeHistogram::SetInput(Texture& input)
{
    m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(1, &m_inputTextureSrv);
    input.CreateSRV(0, &m_inputTextureSrv);
}

void ComputeHistogram::CreateOutputResource(Texture& input)
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

    histWidth >>= 1;
    histHeight >>= 1;
    extraWidth = histWidth % 2;
    extraHeight = histHeight % 2;
    CD3DX12_RESOURCE_DESC outputDesc2 =
        CD3DX12_RESOURCE_DESC::Tex2D(
            DXGI_FORMAT_R16G16_UINT,
            histWidth + extraWidth, histHeight + extraHeight, // 1/4 the size of output1, but needs to be multiple of 2.
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

    if (m_pRootSignature != nullptr)
    {
        m_pRootSignature->Release();
        m_pRootSignature = nullptr;
    }
}

void QuadCount::OnDestroy()
{
    if (m_pPipeline != nullptr)
    {
        m_pPipeline->Release();
        m_pPipeline = nullptr;
    }
}

void SumQuads::OnDestroy()
{
    if (m_pPipeline != nullptr)
    {
        m_pPipeline->Release();
        m_pPipeline = nullptr;
    }
}

void CreateLUT::OnDestroy()
{
    m_outputLUT.OnDestroy();

    if (m_pPipeline != nullptr)
    {
        m_pPipeline->Release();
        m_pPipeline = nullptr;
    }
}

void ComputeHistogram::Draw(ID3D12GraphicsCommandList* pCommandList)
{
    UserMarker marker(pCommandList, "ComputeHistogram");

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

    m_quadCount.Setup(pCommandList, m_pRootSignature);

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


    CBV_SRV_UAV* pSumQuadsOutput = nullptr;
    // If the first output is already 2x2 then we can skip the reduction.
    if (m_constants.outputWidth > 2 || m_constants.outputHeight > 2)
    {
        m_sumQuads.Setup(pCommandList, m_pRootSignature);

        auto outputUav = m_outputUav2.GetGPU();
        auto outputSrv = m_outputSrv2.GetGPU();
        auto pOutputResource = &m_histogramOutput2;

        auto inputUav = m_outputUav1.GetGPU();
        auto inputSrv = m_outputSrv1.GetGPU();
        auto pInputResource = &m_histogramOutput1;

        m_constants.inputWidth = pInputResource->GetWidth();
        m_constants.inputHeight = pInputResource->GetHeight();

        m_constants.outputWidth = pOutputResource->GetWidth();
        m_constants.outputHeight = pOutputResource->GetHeight();

        while (true)
        {
            m_pConstantBufferRing->AllocConstantBuffer(constantsSize, (void**)&pConstMem, &cbHandle);
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

            if (m_constants.outputWidth == 2 && m_constants.outputHeight == 2)
                break;

            m_constants.inputWidth = m_constants.outputWidth;
            m_constants.inputHeight = m_constants.outputHeight;

            m_constants.outputWidth = max(m_constants.outputWidth >> 1, 2);
            m_constants.outputHeight = max(m_constants.outputHeight >> 1, 2);

            auto pTmp = pOutputResource;
            pOutputResource = pInputResource;
            pInputResource = pTmp;

            auto tmpUav = outputUav;
            outputUav = inputUav;
            inputUav = tmpUav;

            auto tmpSrv = outputSrv;
            outputSrv = inputSrv;
            inputSrv = tmpSrv;
        }

        if (memcmp(&outputSrv, &m_outputSrv1.GetGPU(), sizeof(outputSrv)) == 0)
            pSumQuadsOutput = &m_outputSrv1;
        else
            pSumQuadsOutput = &m_outputSrv2;
    }
    else
        pSumQuadsOutput = &m_outputSrv1;

    m_createLUT.Draw(pCommandList, m_pConstantBufferRing, m_pRootSignature, pSumQuadsOutput);
}

void QuadCount::Setup(ID3D12GraphicsCommandList* pCommandList, ID3D12RootSignature* pRootSignature)
{
    pCommandList->SetPipelineState(m_pPipeline);
    pCommandList->SetComputeRootSignature(pRootSignature);
}

void QuadCount::Draw(ID3D12GraphicsCommandList* pCommandList, uint32_t outputWidth, uint32_t outputHeight)
{
    uint32_t dispatchX = (outputWidth + 7) / 8;
    uint32_t dispatchY = (outputHeight + 7) / 8;
    uint32_t dispatchZ = 1;
    pCommandList->Dispatch(dispatchX, dispatchY, dispatchZ);
}

void CreateLUT::Draw(
    ID3D12GraphicsCommandList* pCommandList,
    DynamicBufferRing* pConstantBufferRing,
    ID3D12RootSignature* pRootSignature,
    CBV_SRV_UAV* pInputSrv)
{
    CD3DX12_RESOURCE_BARRIER barrier =
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_outputLUT.GetResource(),
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    pCommandList->ResourceBarrier(1, &barrier);

    D3D12_GPU_VIRTUAL_ADDRESS cbHandle;
    uint32_t* pConstMem;
    pConstantBufferRing->AllocConstantBuffer(sizeof(m_lutConstants), (void**)&pConstMem, &cbHandle);

    memcpy(pConstMem, &m_lutConstants, sizeof(m_lutConstants));

    pCommandList->SetComputeRootConstantBufferView(0, cbHandle);

    pCommandList->SetComputeRootDescriptorTable(1, m_outputUav.GetGPU());
    pCommandList->SetComputeRootDescriptorTable(2, pInputSrv->GetGPU());

    uint32_t dispatchX = (m_lutConstants.outputSize + 7) / 8;
    pCommandList->Dispatch(dispatchX, 1, 1);
    barrier =
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_outputLUT.GetResource(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    pCommandList->ResourceBarrier(1, &barrier);
}

void SumQuads::Setup(ID3D12GraphicsCommandList* pCommandList, ID3D12RootSignature* pRootSignature)
{
    pCommandList->SetPipelineState(m_pPipeline);
    pCommandList->SetComputeRootSignature(pRootSignature);
}

void SumQuads::Draw(ID3D12GraphicsCommandList* pCommandList, uint32_t outputWidth, uint32_t outputHeight)
{
    uint32_t dispatchX = (outputWidth + 7) / 8;
    uint32_t dispatchY = (outputHeight + 7) / 8;
    uint32_t dispatchZ = 1;
    pCommandList->Dispatch(dispatchX, dispatchY, dispatchZ);
}
