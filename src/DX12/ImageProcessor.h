#pragma once

#include "Device.h"
#include "DynamicBufferRing.h"
#include "ResourceViewHeaps.h"
#include "Texture.h"
#include "UploadHeap.h"

#include <string>

namespace CS570
{
    class BaseImageProcessor
    {
    public:
        virtual void Draw(ID3D12GraphicsCommandList* pCommandList) = 0;
        virtual CAULDRON_DX12::CBV_SRV_UAV& GetOutputSrv() = 0;
    };

    class ImageProcessor : public BaseImageProcessor
    {
    public:
        void OnCreate(
            const std::string& shaderEntryFunc,
            CAULDRON_DX12::Texture& input1,
            CAULDRON_DX12::Texture& input2,
            CAULDRON_DX12::Device *pDevice,
            CAULDRON_DX12::UploadHeap *pUploadHeap,
            CAULDRON_DX12::ResourceViewHeaps *pResourceViewHeaps,
            CAULDRON_DX12::DynamicBufferRing *pConstantBufferRing);

        void OnDestroy();

        void Draw(ID3D12GraphicsCommandList *pCommandList) override;

        void SetLogConstant(float logConstant) { m_constants.logConstant = logConstant; }
        void SetPowerConstant(float powerConstant) { m_constants.powerConstant = powerConstant; }
        void SetPowerRaise(float powerRaise) { m_constants.powerRaise = powerRaise; }

        CAULDRON_DX12::CBV_SRV_UAV& GetOutputSrv() { return m_outputSrv; }

    private:
        void CreateOutputResource(CAULDRON_DX12::Texture& input1, CAULDRON_DX12::Texture& input2);
        CAULDRON_DX12::Texture m_outputTexture;

        struct Constants
        {
            uint32_t outputWidth = 0u;
            uint32_t outputHeight = 0u;
            float logConstant = 1.0f;
            float powerConstant = 1.0f;
            float powerRaise = 1.0f;
        };

        Constants m_constants;

        CAULDRON_DX12::Device *m_pDevice = nullptr;

        CAULDRON_DX12::CBV_SRV_UAV m_constBuffer; // dimension
        CAULDRON_DX12::CBV_SRV_UAV m_inputTextureSrvTable; //src
        CAULDRON_DX12::CBV_SRV_UAV m_outputUav; //dest
        CAULDRON_DX12::CBV_SRV_UAV m_outputSrv; //dest

        CAULDRON_DX12::ResourceViewHeaps *m_pResourceViewHeaps = nullptr;
        CAULDRON_DX12::DynamicBufferRing *m_pConstantBufferRing = nullptr;
        ID3D12RootSignature	*m_pRootSignature = nullptr;
        ID3D12PipelineState	*m_pPipeline = nullptr;
    };
}