// SPDSample
//
// Copyright (c) 2020 Advanced Micro Devices, Inc. All rights reserved.
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
#pragma once

#include "Device.h"
#include "DynamicBufferRing.h"
#include "ResourceViewHeaps.h"
#include "Texture.h"
#include "UploadHeap.h"

#include <string>

namespace CS570
{
    class ComputeHistogram
    {
    public:
        void OnCreate(
            CAULDRON_DX12::Texture& input,
            CAULDRON_DX12::Device *pDevice,
            CAULDRON_DX12::UploadHeap *pUploadHeap,
            CAULDRON_DX12::ResourceViewHeaps *pResourceViewHeaps,
            CAULDRON_DX12::DynamicBufferRing *pConstantBufferRing);

        void SetInput(CAULDRON_DX12::Texture& input);

        void OnDestroy();

        void Draw(ID3D12GraphicsCommandList *pCommandList);

        CAULDRON_DX12::CBV_SRV_UAV& GetOutputSrv() { return *m_pCurrentOutputSrv; }

    private:
        void CreateOutputResource(CAULDRON_DX12::Texture& input);
        CAULDRON_DX12::Texture m_histogramOutput1;
        CAULDRON_DX12::Texture m_histogramOutput2;

        struct Constants
        {
            uint32_t outputWidth = 0u;
            uint32_t outputHeight = 0u;
        };

        Constants m_constants;

        CAULDRON_DX12::Device *m_pDevice = nullptr;

        CAULDRON_DX12::CBV_SRV_UAV m_constBuffer; // dimension
        CAULDRON_DX12::CBV_SRV_UAV m_inputTextureSrv; //src

        CAULDRON_DX12::CBV_SRV_UAV m_outputUav1;
        CAULDRON_DX12::CBV_SRV_UAV m_outputSrv1;
        CAULDRON_DX12::CBV_SRV_UAV m_outputUav2;
        CAULDRON_DX12::CBV_SRV_UAV m_outputSrv2;
        CAULDRON_DX12::CBV_SRV_UAV* m_pCurrentOutputSrv = nullptr;

        CAULDRON_DX12::ResourceViewHeaps *m_pResourceViewHeaps = nullptr;
        CAULDRON_DX12::DynamicBufferRing *m_pConstantBufferRing = nullptr;
        ID3D12RootSignature* m_pComputeRootSignature = nullptr;
        ID3D12PipelineState* m_pQuadCountPipeline = nullptr;
        ID3D12PipelineState* m_pSumCountsPipeline = nullptr;
    };
}