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
#pragma once

namespace CAULDRON_DX12
{
    class ResourceViewHeaps;

    class PostProcCS
    {
    public:
        PostProcCS();
        ~PostProcCS();

        struct CreateParams
        {
            Device*                    pDevice = nullptr;
            ResourceViewHeaps*         pResourceViewHeaps = nullptr;
            std::string                shaderFilename;
            std::string                shaderEntryPoint;
            std::string                strShaderCompilerParams;
            uint32_t                   constantBufferCount = 1u;
            uint32_t                   UAVTableSize = 0;
            uint32_t                   SRVTableSize = 0;
            uint32_t                   dwWidth = 0, dwHeight = 0, dwDepth = 0;
            DefineList*                userDefines = 0;
            uint32_t                   numStaticSamplers = 0;
            D3D12_STATIC_SAMPLER_DESC* pStaticSamplers = 0;
        };

        void OnCreate(const PostProcCS::CreateParams& params);

        void OnCreate( // LEGACY OnCreate()
            Device *pDevice,
            ResourceViewHeaps *pResourceViewHeaps,
            const std::string &shaderFilename,
            const std::string &shaderEntryPoint,
            uint32_t UAVTableSize,
            uint32_t SRVTableSize,
            uint32_t dwWidth, uint32_t dwHeight, uint32_t dwDepth,
            DefineList* userDefines = 0,
            uint32_t numStaticSamplers = 0,
            D3D12_STATIC_SAMPLER_DESC* pStaticSamplers = 0
        );
        void OnDestroy();
        void Draw(ID3D12GraphicsCommandList* pCommandList, D3D12_GPU_VIRTUAL_ADDRESS constantBuffer, CBV_SRV_UAV *pUAVTable, CBV_SRV_UAV *pSRVTable, uint32_t ThreadX, uint32_t ThreadY, uint32_t ThreadZ);
        void Draw(ID3D12GraphicsCommandList* pCommandList,
            uint32_t dwConstantBufferCount,
            D3D12_GPU_VIRTUAL_ADDRESS* pConstantBuffers,
            CBV_SRV_UAV *pUAVTable, CBV_SRV_UAV *pSRVTable,
            uint32_t ThreadX, uint32_t ThreadY, uint32_t ThreadZ);
    private:
        Device                      *m_pDevice;
        ResourceViewHeaps           *m_pResourceViewHeaps;

        ID3D12RootSignature	        *m_pRootSignature;
        ID3D12PipelineState	        *m_pPipeline = NULL;
    };
}
