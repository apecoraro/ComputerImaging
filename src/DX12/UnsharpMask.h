#pragma once

#include "ImageProcessor.h"

#include "Device.h"
#include "DynamicBufferRing.h"
#include "GaussianBlur.h"
#include "ResourceViewHeaps.h"
#include "Texture.h"
#include "SobelFilterCombine.h"
#include "UploadHeap.h"

#include <string>

namespace CS570
{
    class UnsharpMask : public BaseImageProcessor
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

        void SetWeight(float weight)
        {
            m_addOperation.SetWeightInput2(weight);
        }

        void Draw(ID3D12GraphicsCommandList *pCommandList) override;

        CAULDRON_DX12::CBV_SRV_UAV& GetOutputSrv() override { return m_addOperation.GetOutputSrv(); }
        CAULDRON_DX12::Texture& GetOutputResource() override { return m_addOperation.GetOutputResource(); }

        void SetBlurVariance(float variance)
        {
            m_gaussianBlur.SetVariance(variance);
        }

    private:
        GaussianBlur m_gaussianBlur;
        ImageProcessor m_subtractOperation;
        ImageProcessor m_addOperation;
    };
}