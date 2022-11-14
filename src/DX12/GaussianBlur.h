#pragma once

#include "ImageProcessor.h"

#include "Device.h"
#include "DynamicBufferRing.h"
#include "ResourceViewHeaps.h"
#include "Texture.h"
#include "UploadHeap.h"

#include <string>

namespace CS570
{
    class ComputeGaussianWeights
    {
    public:
        void OnCreate(
            uint32_t blurKernelSize,
            float blurKernelVariance,
            CAULDRON_DX12::Device* pDevice,
            CAULDRON_DX12::UploadHeap* pUploadHeap,
            CAULDRON_DX12::ResourceViewHeaps* pResourceViewHeaps,
            CAULDRON_DX12::DynamicBufferRing* pConstantBufferRing);
        void OnDestroy();
        void Draw(ID3D12GraphicsCommandList* pCommandList);
        CAULDRON_DX12::Texture& GetOutputResource() { return m_blurWeights; }

        void SetVariance(float variance)
        {
            m_constants.sigmaSquared = variance * variance;
            m_constants.oneOverSigmaSquared = 1.0f / m_constants.sigmaSquared;
        }
    private:
        void CreateOutputResource(uint32_t blurKernelSize);

        ID3D12RootSignature* m_pRootSignature = nullptr;
        ID3D12PipelineState* m_pPipeline = nullptr;

        CAULDRON_DX12::Texture m_blurWeights;

        struct Constants
        {
            uint32_t outputWidth = 0u;
            uint32_t outputHeight = 0u;
            float sigmaSquared = 0.0f;
            float oneOverSigmaSquared = 0.0f;
        };

        Constants m_constants;

        CAULDRON_DX12::Device* m_pDevice = nullptr;

        CAULDRON_DX12::CBV_SRV_UAV m_constBuffer;

        CAULDRON_DX12::CBV_SRV_UAV m_outputUav;

        CAULDRON_DX12::ResourceViewHeaps* m_pResourceViewHeaps = nullptr;
        CAULDRON_DX12::DynamicBufferRing* m_pConstantBufferRing = nullptr;
    };

    class GaussianBlur : public BaseImageProcessor
    {
    public:
        void OnCreate(
            CAULDRON_DX12::Texture& input,
            uint32_t blurKernelSize,
            float blurKernelVariance,
            CAULDRON_DX12::Device* pDevice,
            CAULDRON_DX12::UploadHeap* pUploadHeap,
            CAULDRON_DX12::ResourceViewHeaps* pResourceViewHeaps,
            CAULDRON_DX12::DynamicBufferRing* pConstantBufferRing);
        void OnDestroy();

        void Draw(ID3D12GraphicsCommandList *pCommandList) override;

        CAULDRON_DX12::Texture& GetOutputResource() override { return m_blurredOutput; }
        CAULDRON_DX12::CBV_SRV_UAV& GetOutputSrv() override { return m_outputUav; }

        void SetVariance(float variance)
        {
            m_computeWeights.SetVariance(variance);
        }
    private:
        void CreateOutputResource(CAULDRON_DX12::Texture& input);

        ComputeGaussianWeights m_computeWeights;

        ID3D12RootSignature* m_pRootSignature = nullptr;
        ID3D12PipelineState* m_pPipeline = nullptr;

        CAULDRON_DX12::Texture m_blurredOutput;

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