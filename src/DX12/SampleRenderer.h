#pragma once

#include "CommandListRing.h"
#include "ComputeHistogram.h"
#include "Device.h"
#include "DynamicBufferRing.h"
#include "GPUTimestamps.h"
#include "ImageProcessor.h"
#include "ImageRenderer.h"
#include "Imgui.h"
#include "ResourceViewHeaps.h"
#include "Texture.h"
#include "StaticBufferPool.h"
#include "SwapChain.h"

#include <string>

static const int k_backBufferCount = 2;

namespace CS570
{
    class SampleRenderer
    {
    public:
        struct State
        {
            float time;
        };

        SampleRenderer()
        {
        }

        void OnCreate(
            CAULDRON_DX12::Device* pDevice,
            const std::string& inputImage1,
            const std::string& inputImage2,
            const std::string& initialOperation,
            CAULDRON_DX12::SwapChain *pSwapChain);
        void LoadScene();
        void OnDestroy();

        void OnCreateWindowSizeDependentResources(CAULDRON_DX12::SwapChain* pSwapChain, uint32_t Width, uint32_t Height);
        void OnDestroyWindowSizeDependentResources();

        const std::vector<TimeStamp> &GetTimingValues() { return m_timeStamps; }

        void OnRender(State *pState, CAULDRON_DX12::SwapChain* pSwapChain);
        void OnPostRender();

        void SetOperation(const std::string& operation);
        void SetInput1(const std::string& inputImage1);
        void SetInput2(const std::string& inputImage2);

        void SetLogConstant(float constant) { m_logOperation.SetLogConstant(constant); }
        void SetPowerConstant(float constant) { m_powerOperation.SetPowerConstant(constant); }
        void SetPowerRaise(float raise) { m_powerOperation.SetPowerRaise(raise); }

        void SetDisplayFilter(D3D12_FILTER filter) { m_displayFilter = filter; }

        void SaveOutput() { m_saveOutput = true; }
        void SaveCCLOutput() { m_saveCCLOutput = true; }

    private:
        void LoadInputTexture(const std::string& inputImage, CAULDRON_DX12::Texture& inputTexture);
        void LoadInputTextures(
            const std::string& inputImage1,
            const std::string& inputImage2);

        CAULDRON_DX12::Device* m_pDevice = nullptr;

        uint32_t m_width = 0u;
        uint32_t m_height = 0u;
        D3D12_VIEWPORT m_viewPort = {};
        D3D12_RECT m_rectScissor = {};

        // Initialize helper classes
        CAULDRON_DX12::ResourceViewHeaps m_resourceViewHeaps;
        CAULDRON_DX12::UploadHeap m_uploadHeap;
        CAULDRON_DX12::DynamicBufferRing m_constantBufferRing;
        CAULDRON_DX12::StaticBufferPool m_vidMemBufferPool;
        CAULDRON_DX12::CommandListRing m_commandListRing;
        CAULDRON_DX12::GPUTimestamps m_gpuTimer;

        CAULDRON_DX12::Texture m_inputTexture1;
        CAULDRON_DX12::Texture m_inputTexture2;

        bool m_rebuildImage1 = false;
        bool m_rebuildImage2 = false;
        std::string m_inputImage1;
        std::string m_inputImage2;

        ImageProcessor* m_pCurrentOperation = nullptr;
        ImageProcessor m_addOperation;
        ImageProcessor m_subtractOperation;
        ImageProcessor m_productOperation;
        ImageProcessor m_negativeOperation;
        ImageProcessor m_logOperation;
        ImageProcessor m_powerOperation;

        ComputeHistogram m_computeHistogram;

        D3D12_FILTER m_displayFilter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
        ImageRenderer m_imageRenderer;

        CAULDRON_DX12::ImGUI m_imGUI;

        std::vector<TimeStamp> m_timeStamps;
        bool m_saveOutput = false;
        bool m_saveCCLOutput = false;
    };
} // namespace CS570