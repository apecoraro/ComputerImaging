// AMD AMDUtils code
// 
// Copyright(c) 2018 Advanced Micro Devices, Inc.All rights reserved.
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "stdafx.h"
#include "Device.h"
#include "Error.h"
#include "Helper.h"
#include "DynamicBufferRing.h"
#include "ShaderCompilerHelper.h"
#include "PostProcCS.h"

#if _DEBUG
#define DEFAULT_SHADER_COMPILE_FLAGS  "-T cs_6_0 /Zi /Zss"
#else
#define DEFAULT_SHADER_COMPILE_FLAGS  "-T cs_6_0"
#endif

namespace CAULDRON_DX12
{
    PostProcCS::PostProcCS()
    {
    }

    PostProcCS::~PostProcCS()
    {
    }

    void PostProcCS::OnCreate(
        Device *pDevice,
        ResourceViewHeaps *pResourceViewHeaps,
        const std::string &shaderFilename,
        const std::string &shaderEntryPoint,
        uint32_t UAVTableSize,
        uint32_t SRVTableSize,
        uint32_t dwWidth, uint32_t dwHeight, uint32_t dwDepth,
        DefineList* userDefines,
        uint32_t numStaticSamplers,
        D3D12_STATIC_SAMPLER_DESC* pStaticSamplers
    )
    {
        CreateParams params = {};
        params.pDevice = pDevice;
        params.pResourceViewHeaps = pResourceViewHeaps;
        params.shaderFilename = shaderFilename;
        params.shaderEntryPoint = shaderEntryPoint;
        params.UAVTableSize = UAVTableSize;
        params.SRVTableSize = SRVTableSize;
        params.dwWidth  = dwWidth;
        params.dwHeight = dwHeight;
        params.dwDepth  = dwDepth;
        params.userDefines = 0;
        params.numStaticSamplers = 0;
        params.pStaticSamplers = 0;
        params.strShaderCompilerParams = DEFAULT_SHADER_COMPILE_FLAGS;
        this->OnCreate(params);
    }

    void PostProcCS::OnCreate(const PostProcCS::CreateParams& params)
    {
        m_pDevice = params.pDevice;

        m_pResourceViewHeaps = params.pResourceViewHeaps;

        // Compile shaders
        //
        D3D12_SHADER_BYTECODE shaderByteCode = {};
        DefineList defines;
        if (params.userDefines)
            defines = *params.userDefines;
        defines["WIDTH" ] = std::to_string(params.dwWidth);
        defines["HEIGHT"] = std::to_string(params.dwHeight);
        defines["DEPTH" ] = std::to_string(params.dwDepth);
        bool bCompile = CompileShaderFromFile(params.shaderFilename.c_str(), &defines, params.shaderEntryPoint.c_str(), params.strShaderCompilerParams.c_str(), &shaderByteCode);

        // Create root signature
        //
        {
            // we'll always have a constant buffer
            uint32_t parameterCount = 0;
            std::vector<CD3DX12_ROOT_PARAMETER> RTSlot(params.constantBufferCount);
            for (; parameterCount < params.constantBufferCount; parameterCount++)
            {
                RTSlot[parameterCount].InitAsConstantBufferView(parameterCount, 0, D3D12_SHADER_VISIBILITY_ALL); // b0...b(constBufCount-1)
            }

            // if we have a UAV table
            CD3DX12_DESCRIPTOR_RANGE uavDescRange = {};
            if (params.UAVTableSize > 0)
            {
                RTSlot.resize(RTSlot.size() + 1);
                uavDescRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, params.UAVTableSize, 0);
                RTSlot[parameterCount++].InitAsDescriptorTable(1, &uavDescRange, D3D12_SHADER_VISIBILITY_ALL);
            }

            // if we have a SRV table
            CD3DX12_DESCRIPTOR_RANGE srvDescRange = {};
            if (params.SRVTableSize > 0)
            {
                RTSlot.resize(RTSlot.size() + 1);
                srvDescRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, params.SRVTableSize, 0);
                RTSlot[parameterCount++].InitAsDescriptorTable(1, &srvDescRange, D3D12_SHADER_VISIBILITY_ALL);
            }

            // the root signature contains 3 slots to be used
            CD3DX12_ROOT_SIGNATURE_DESC descRootSignature = CD3DX12_ROOT_SIGNATURE_DESC();
            descRootSignature.NumParameters = static_cast<uint32_t>(RTSlot.size());
            descRootSignature.pParameters = RTSlot.data();
            descRootSignature.NumStaticSamplers = params.numStaticSamplers;
            descRootSignature.pStaticSamplers = params.pStaticSamplers;

            // deny uneccessary access to certain pipeline stages   
            descRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

            ID3DBlob *pOutBlob, *pErrorBlob = NULL;
            ThrowIfFailed(D3D12SerializeRootSignature(&descRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &pOutBlob, &pErrorBlob));
            ThrowIfFailed(
                m_pDevice->GetDevice()->CreateRootSignature(0, pOutBlob->GetBufferPointer(), pOutBlob->GetBufferSize(), IID_PPV_ARGS(&m_pRootSignature))
            );
            SetName(m_pRootSignature, std::string("PostProcCS::m_pRootSignature::") + params.shaderFilename);

            pOutBlob->Release();
            if (pErrorBlob)
                pErrorBlob->Release();
        }

        {
            D3D12_COMPUTE_PIPELINE_STATE_DESC descPso = {};
            descPso.CS = shaderByteCode;
            descPso.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
            descPso.pRootSignature = m_pRootSignature;
            descPso.NodeMask = 0;

            ThrowIfFailed(m_pDevice->GetDevice()->CreateComputePipelineState(&descPso, IID_PPV_ARGS(&m_pPipeline)));
            SetName(m_pRootSignature, std::string("PostProcCS::m_pPipeline::") + params.shaderFilename);
        }
    }


    void PostProcCS::OnDestroy()
    {
        m_pPipeline->Release();
        m_pRootSignature->Release();
    }

    void PostProcCS::Draw(ID3D12GraphicsCommandList* pCommandList, D3D12_GPU_VIRTUAL_ADDRESS constantBuffer, CBV_SRV_UAV* pUAVTable, CBV_SRV_UAV* pSRVTable, uint32_t ThreadX, uint32_t ThreadY, uint32_t ThreadZ)
    {
        if (m_pPipeline == NULL)
            return;

        Draw(pCommandList, 1u, &constantBuffer, pUAVTable, pSRVTable, ThreadX, ThreadY, ThreadZ);
    }

    void PostProcCS::Draw(ID3D12GraphicsCommandList* pCommandList,
        uint32_t dwConstantBufferCount,
        D3D12_GPU_VIRTUAL_ADDRESS* pConstantBuffers,
        CBV_SRV_UAV* pUAVTable, CBV_SRV_UAV* pSRVTable,
        uint32_t ThreadX, uint32_t ThreadY, uint32_t ThreadZ)
    {
        // Bind Descriptor heaps and the root signature
        //                
        ID3D12DescriptorHeap *pDescriptorHeaps[] = { m_pResourceViewHeaps->GetCBV_SRV_UAVHeap(), m_pResourceViewHeaps->GetSamplerHeap() };
        pCommandList->SetDescriptorHeaps(2, pDescriptorHeaps);
        pCommandList->SetComputeRootSignature(m_pRootSignature);

        // Bind Descriptor the descriptor sets
        //                
        uint32_t params = 0u;
        for (; params < dwConstantBufferCount; ++params)
        {
            pCommandList->SetComputeRootConstantBufferView(params, pConstantBuffers[params]);
        }
        if (pUAVTable)
            pCommandList->SetComputeRootDescriptorTable(params++, pUAVTable->GetGPU());
        if (pSRVTable)
            pCommandList->SetComputeRootDescriptorTable(params++, pSRVTable->GetGPU());

        // Bind Pipeline
        //
        pCommandList->SetPipelineState(m_pPipeline);

        // Dispatch
        //
        pCommandList->Dispatch(ThreadX, ThreadY, ThreadZ);
    }
}