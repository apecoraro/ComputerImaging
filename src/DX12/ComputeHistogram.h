#pragma once

#include "Device.h"
#include "DynamicBufferRing.h"
#include "ResourceViewHeaps.h"
#include "Texture.h"
#include "UploadHeap.h"

#include <string>

namespace CS570
{
    class QuadCount
    {
    public:
        void OnCreate(ID3D12RootSignature* pRootSignature, CAULDRON_DX12::Device* pDevice);

        void OnDestroy();

        void Setup(ID3D12GraphicsCommandList* pCommandList, ID3D12RootSignature* pRootSignature);
        void Draw(ID3D12GraphicsCommandList* pCommandList, uint32_t outputWidth, uint32_t outputHeight);

    private:
        ID3D12PipelineState* m_pPipeline = nullptr;
    };

    class SumQuads
    {
    public:
        void OnCreate(ID3D12RootSignature* pRootSignature, CAULDRON_DX12::Device* pDevice);

        void OnDestroy();

        void Setup(ID3D12GraphicsCommandList* pCommandList, ID3D12RootSignature* pRootSignature);
        void Draw(ID3D12GraphicsCommandList* pCommandList, uint32_t outputWidth, uint32_t outputHeight);

    private:
        ID3D12PipelineState* m_pPipeline = nullptr;
    };

    class CreateLUT
    {
    public:
        void OnCreate(
            ID3D12RootSignature* pRootSignature,
            const CAULDRON_DX12::Texture& input,
            CAULDRON_DX12::Device* pDevice,
            CAULDRON_DX12::ResourceViewHeaps* pResourceViewHeaps);

        void OnDestroy();

        void Draw(
            ID3D12GraphicsCommandList* pCommandList,
            CAULDRON_DX12::DynamicBufferRing* pConstantBufferRing,
            ID3D12RootSignature* pRootSignature,
            CAULDRON_DX12::CBV_SRV_UAV* pInputSrv,
            bool generateInverseLUT=false);

        CAULDRON_DX12::Texture& GetOutputResource() { return m_outputLUT; }

    private:
        ID3D12PipelineState* m_pCreateLUT = nullptr;
        ID3D12PipelineState* m_pCreateInverseLUT = nullptr;

        struct LutConstants
        {
            uint32_t outputSize;
            uint32_t pixelCount;
        };
        LutConstants m_lutConstants;

        CAULDRON_DX12::Texture m_outputLUT;
        CAULDRON_DX12::CBV_SRV_UAV m_outputUav;
    };

    class ComputeHistogram
    {
    public:
        void OnCreate(
            CAULDRON_DX12::Texture& input,
            CAULDRON_DX12::Device *pDevice,
            CAULDRON_DX12::UploadHeap *pUploadHeap,
            CAULDRON_DX12::ResourceViewHeaps *pResourceViewHeaps,
            CAULDRON_DX12::DynamicBufferRing *pConstantBufferRing);
        void OnDestroy();

        void Draw(ID3D12GraphicsCommandList *pCommandList);

        CAULDRON_DX12::Texture& GetOutputResource() { return m_createLUT.GetOutputResource(); }

    private:
        void CreateOutputResource(CAULDRON_DX12::Texture& input);

        ID3D12RootSignature* m_pRootSignature = nullptr;

        CAULDRON_DX12::Texture m_histogramOutput1;
        D3D12_RESOURCE_STATES m_histogramOutput1State;

        CAULDRON_DX12::Texture m_histogramOutput2;
        D3D12_RESOURCE_STATES m_histogramOutput2State;

        struct Constants
        {
            uint32_t inputWidth = 0u;
            uint32_t inputHeight = 0u;
            uint32_t outputWidth = 0u;
            uint32_t outputHeight = 0u;
        };

        Constants m_constants;

        CAULDRON_DX12::Device* m_pDevice = nullptr;

        CAULDRON_DX12::CBV_SRV_UAV m_constBuffer; // dimension
        CAULDRON_DX12::CBV_SRV_UAV m_inputTextureSrv; //src

        CAULDRON_DX12::CBV_SRV_UAV m_outputUav1;
        CAULDRON_DX12::CBV_SRV_UAV m_outputSrv1;
        CAULDRON_DX12::CBV_SRV_UAV m_outputUav2;
        CAULDRON_DX12::CBV_SRV_UAV m_outputSrv2;
        CAULDRON_DX12::CBV_SRV_UAV* m_pCurrentOutputSrv = nullptr;

        CAULDRON_DX12::ResourceViewHeaps* m_pResourceViewHeaps = nullptr;
        CAULDRON_DX12::DynamicBufferRing* m_pConstantBufferRing = nullptr;

        QuadCount m_quadCount;
        SumQuads m_sumQuads;
        CreateLUT m_createLUT;
    };
}