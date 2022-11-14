#include "GaussianBlur.h"

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

void GaussianBlur::OnCreate(
    Texture& input,
    uint32_t blurKernelSize,
    float blurKernelVariance,
    Device* pDevice,
    UploadHeap* pUploadHeap,
    ResourceViewHeaps* pResourceViewHeaps,
    DynamicBufferRing* pConstantBufferRing)
{
    m_computeWeights.OnCreate(
        blurKernelSize, blurKernelVariance,
        pDevice, pUploadHeap, pResourceViewHeaps, pConstantBufferRing);

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
        srvDescRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);
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
        CAULDRON_DX12::SetName(m_pRootSignature, std::string("GaussianBlur::Blur"));

        pOutBlob->Release();
        if (pErrorBlob)
            pErrorBlob->Release();
    }

    D3D12_SHADER_BYTECODE shaderByteCode = {};
    DefineList defines;
    defines["KERNEL_WIDTH"] = std::to_string(blurKernelSize);
    defines["KERNEL_HEIGHT"] = std::to_string(blurKernelSize);
    CAULDRON_DX12::CompileShaderFromFile(
        "DX12/GaussianBlur.hlsl",
        &defines,
        "Blur",
        "-T cs_6_0 /Zi /Zss -Od -Qembed_debug",
        &shaderByteCode);

    D3D12_COMPUTE_PIPELINE_STATE_DESC descPso = {};
    descPso.CS = shaderByteCode;
    descPso.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
    descPso.pRootSignature = m_pRootSignature;
    descPso.NodeMask = 0;

    ThrowIfFailed(pDevice->GetDevice()->CreateComputePipelineState(&descPso, IID_PPV_ARGS(&m_pPipeline)));

    m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(1, &m_constBuffer);

    m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(2, &m_inputSrvTable);
    m_computeWeights.GetOutputResource().CreateSRV(0, &m_inputSrvTable);
    input.CreateSRV(1, &m_inputSrvTable);

    CreateOutputResource(input);
}

void ComputeGaussianWeights::OnCreate(
    uint32_t blurKernelSize,
    float blurKernelVariance,
    CAULDRON_DX12::Device* pDevice,
    CAULDRON_DX12::UploadHeap* pUploadHeap,
    CAULDRON_DX12::ResourceViewHeaps* pResourceViewHeaps,
    CAULDRON_DX12::DynamicBufferRing* pConstantBufferRing)
{
    m_pDevice = pDevice;
    m_pResourceViewHeaps = pResourceViewHeaps;
    m_pConstantBufferRing = pConstantBufferRing;

    {
        int parameterCount = 0;
        CD3DX12_ROOT_PARAMETER rtSlot[2];

        rtSlot[parameterCount++].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL);

        CD3DX12_DESCRIPTOR_RANGE uavDescRange = {};
        uavDescRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
        rtSlot[parameterCount++].InitAsDescriptorTable(1, &uavDescRange, D3D12_SHADER_VISIBILITY_ALL);

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
        CAULDRON_DX12::SetName(m_pRootSignature, std::string("GaussianBlur::ComputeWeights"));

        pOutBlob->Release();
        if (pErrorBlob)
            pErrorBlob->Release();
    }

    D3D12_SHADER_BYTECODE shaderByteCode = {};
    DefineList defines;
    CAULDRON_DX12::CompileShaderFromFile(
        "DX12/ComputeGaussianWeights.hlsl",
        &defines,
        "ComputeWeight",
        "-T cs_6_0 /Zi /Zss -Od -Qembed_debug",
        &shaderByteCode);

    D3D12_COMPUTE_PIPELINE_STATE_DESC descPso = {};
    descPso.CS = shaderByteCode;
    descPso.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
    descPso.pRootSignature = m_pRootSignature;
    descPso.NodeMask = 0;

    ThrowIfFailed(pDevice->GetDevice()->CreateComputePipelineState(&descPso, IID_PPV_ARGS(&m_pPipeline)));

    m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(1, &m_constBuffer);

    m_constants.sigmaSquared = blurKernelVariance * blurKernelVariance;
    m_constants.oneOverSigmaSquared = 1.0f / m_constants.sigmaSquared;
    CreateOutputResource(blurKernelSize);
}

void GaussianBlur::CreateOutputResource(Texture& input)
{
    CD3DX12_RESOURCE_DESC outputDesc =
        CD3DX12_RESOURCE_DESC::Tex2D(
            input.GetFormat(),
            input.GetWidth(), input.GetHeight(),
            1, // array size
            1, // mip size
            1, // sample count
            0, // sample quality
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    m_blurredOutput.InitRenderTarget(m_pDevice, "GaussianBlurOutput", &outputDesc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(1, &m_outputUav);
    m_blurredOutput.CreateUAV(0, &m_outputUav);

    m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(1, &m_outputSrv);
    m_blurredOutput.CreateSRV(0, &m_outputSrv);

    m_constants.outputWidth = input.GetWidth();
    m_constants.outputHeight = input.GetHeight();
}

void ComputeGaussianWeights::CreateOutputResource(uint32_t blurKernelSize)
{
    CD3DX12_RESOURCE_DESC outputDesc =
        CD3DX12_RESOURCE_DESC::Tex2D(
            DXGI_FORMAT_R32_FLOAT,
            blurKernelSize, blurKernelSize,
            1, // array size
            1, // mip size
            1, // sample count
            0, // sample quality
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    m_blurWeights.InitRenderTarget(m_pDevice, "GaussianBlurWeights", &outputDesc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(1, &m_outputUav);
    m_blurWeights.CreateUAV(0, &m_outputUav);

    m_constants.outputWidth = blurKernelSize;
    m_constants.outputHeight = blurKernelSize;
}

void GaussianBlur::OnDestroy()
{
    m_computeWeights.OnDestroy();

    m_blurredOutput.OnDestroy();

    if (m_pPipeline != nullptr)
    {
        m_pPipeline->Release();
        m_pPipeline = nullptr;
    }

    if (m_pRootSignature != nullptr)
    {
        m_pRootSignature->Release();
        m_pRootSignature = nullptr;
    }
}

void ComputeGaussianWeights::OnDestroy()
{
    m_blurWeights.OnDestroy();
    if (m_pPipeline != nullptr)
    {
        m_pPipeline->Release();
        m_pPipeline = nullptr;
    }

    if (m_pRootSignature != nullptr)
    {
        m_pRootSignature->Release();
        m_pRootSignature = nullptr;
    }
}

void GaussianBlur::Draw(ID3D12GraphicsCommandList* pCommandList)
{
    UserMarker marker(pCommandList, "GaussianBlur");

    m_computeWeights.Draw(pCommandList);

    CD3DX12_RESOURCE_BARRIER barriers[1] = {
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_blurredOutput.GetResource(),
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
    };

    pCommandList->ResourceBarrier(1, barriers);

    pCommandList->SetPipelineState(m_pPipeline);
    pCommandList->SetComputeRootSignature(m_pRootSignature);

    D3D12_GPU_VIRTUAL_ADDRESS cbHandle;
    uint32_t* pConstMem;
    uint32_t constantsSize = sizeof(Constants);
    m_pConstantBufferRing->AllocConstantBuffer(constantsSize, (void**)&pConstMem, &cbHandle);

    memcpy(pConstMem, &m_constants, constantsSize);

    ID3D12DescriptorHeap* pDescriptorHeaps[] = { m_pResourceViewHeaps->GetCBV_SRV_UAVHeap(), m_pResourceViewHeaps->GetSamplerHeap() };
    pCommandList->SetDescriptorHeaps(2, pDescriptorHeaps);

    pCommandList->SetComputeRootConstantBufferView(0, cbHandle);
    pCommandList->SetComputeRootDescriptorTable(1, m_outputUav.GetGPU());
    pCommandList->SetComputeRootDescriptorTable(2, m_inputSrvTable.GetGPU());

    uint32_t dispatchX = (m_constants.outputWidth + 7) / 8;
    uint32_t dispatchY = (m_constants.outputHeight + 7) / 8;
    uint32_t dispatchZ = 1;
    pCommandList->Dispatch(dispatchX, dispatchY, dispatchZ);

    barriers[0] =
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_blurredOutput.GetResource(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    pCommandList->ResourceBarrier(1, barriers);
}

void ComputeGaussianWeights::Draw(ID3D12GraphicsCommandList* pCommandList)
{
    UserMarker marker(pCommandList, "ComputeGaussianWeights");

    CD3DX12_RESOURCE_BARRIER barriers[1] = {
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_blurWeights.GetResource(),
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
    };

    pCommandList->ResourceBarrier(1, barriers);

    pCommandList->SetPipelineState(m_pPipeline);
    pCommandList->SetComputeRootSignature(m_pRootSignature);

    D3D12_GPU_VIRTUAL_ADDRESS cbHandle;
    uint32_t* pConstMem;
    uint32_t constantsSize = sizeof(Constants);
    m_pConstantBufferRing->AllocConstantBuffer(constantsSize, (void**)&pConstMem, &cbHandle);

    memcpy(pConstMem, &m_constants, constantsSize);

    ID3D12DescriptorHeap* pDescriptorHeaps[] = { m_pResourceViewHeaps->GetCBV_SRV_UAVHeap(), m_pResourceViewHeaps->GetSamplerHeap() };
    pCommandList->SetDescriptorHeaps(2, pDescriptorHeaps);

    pCommandList->SetComputeRootConstantBufferView(0, cbHandle);
    pCommandList->SetComputeRootDescriptorTable(1, m_outputUav.GetGPU());

    uint32_t dispatchX = (m_constants.outputWidth + 7) / 8;
    uint32_t dispatchY = (m_constants.outputHeight + 7) / 8;
    uint32_t dispatchZ = 1;
    pCommandList->Dispatch(dispatchX, dispatchY, dispatchZ);

    barriers[0] =
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_blurWeights.GetResource(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    pCommandList->ResourceBarrier(1, barriers);
}
