#pragma once

#include "ImageProcessor.h"
#include "ComputeHistogram.h"

#include "Device.h"
#include "DynamicBufferRing.h"
#include "ResourceViewHeaps.h"
#include "Texture.h"
#include "UploadHeap.h"

#include <string>

namespace CS570
{
    class SobelFilterCombine : public BaseImageProcessor
    {
    public:
        void OnCreate(
            CAULDRON_DX12::Texture& horizFilterTex,
            CAULDRON_DX12::Texture& vertFilterTex,
            CAULDRON_DX12::Device* pDevice,
            CAULDRON_DX12::UploadHeap* pUploadHeap,
            CAULDRON_DX12::ResourceViewHeaps* pResourceViewHeaps,
            CAULDRON_DX12::DynamicBufferRing* pConstantBufferRing);
        void OnDestroy();

        void Draw(ID3D12GraphicsCommandList *pCommandList) override;

        CAULDRON_DX12::CBV_SRV_UAV& GetOutputSrv() override { return m_outputUav; }
        CAULDRON_DX12::Texture& GetOutputResource() override { return m_output; }

    private:
        void CreateOutputResource(CAULDRON_DX12::Texture& input);

        ID3D12RootSignature* m_pRootSignature = nullptr;
        ID3D12PipelineState* m_pPipeline = nullptr;

        CAULDRON_DX12::Texture m_output;

        struct Constants
        {
            uint32_t outputWidth = 0u;
            uint32_t outputHeight = 0u;
        };

        Constants m_constants;

        CAULDRON_DX12::Device* m_pDevice = nullptr;

        CAULDRON_DX12::CBV_SRV_UAV m_constBuffer; // dimension

        CAULDRON_DX12::CBV_SRV_UAV m_inputSrvTable;
        CAULDRON_DX12::CBV_SRV_UAV m_outputUav;
        CAULDRON_DX12::CBV_SRV_UAV m_outputSrv;

        CAULDRON_DX12::ResourceViewHeaps* m_pResourceViewHeaps = nullptr;
        CAULDRON_DX12::DynamicBufferRing* m_pConstantBufferRing = nullptr;
    };
}