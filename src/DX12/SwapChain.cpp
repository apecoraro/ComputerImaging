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
#include "Helper.h"
#include "Error.h"
#include "Misc.h"
#include "Device.h"
#include "FreeSyncHDR.h"

#include "SwapChain.h"
#include "DxgiFormatHelper.h"

#pragma comment(lib, "dxgi.lib")

namespace CAULDRON_DX12
{
    //--------------------------------------------------------------------------------------
    //
    // OnCreate
    //
    //--------------------------------------------------------------------------------------
    void SwapChain::OnCreate(Device *pDevice, uint32_t numberBackBuffers, HWND hWnd)
    {
        m_hWnd = hWnd;
        m_pDevice = pDevice->GetDevice();
        m_pDirectQueue = pDevice->GetGraphicsQueue();
        m_BackBufferCount = numberBackBuffers;

        // Init FS HDR
        fsHdrInit(pDevice->GetAGSContext(), pDevice->GetAGSGPUInfo(), hWnd);

        // set some safe format to start with
        m_swapChainFormat = fsHdrGetFormat(DISPLAYMODE_SDR);

        CreateDXGIFactory1(IID_PPV_ARGS(&m_pFactory));

        // Describe the swap chain.
        m_descSwapChain = {};
        m_descSwapChain.BufferCount = m_BackBufferCount;
        m_descSwapChain.Width = 0;
        m_descSwapChain.Height = 0;
        m_descSwapChain.Format = m_swapChainFormat; 
        m_descSwapChain.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        m_descSwapChain.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        m_descSwapChain.SampleDesc.Count = 1;
        m_descSwapChain.Flags = 0;

        m_swapChainFence.OnCreate(pDevice, format("swapchain fence").c_str());

        IDXGISwapChain1 *pSwapChain;
        ThrowIfFailed(m_pFactory->CreateSwapChainForHwnd(
            m_pDirectQueue,        // Swap chain needs the queue so that it can force a flush on it.
            m_hWnd,
            &m_descSwapChain,
            nullptr,
            nullptr,
            &pSwapChain
        ));

        ThrowIfFailed(m_pFactory->MakeWindowAssociation(m_hWnd, DXGI_MWA_NO_ALT_ENTER));

        ThrowIfFailed(pSwapChain->QueryInterface(__uuidof(IDXGISwapChain3), (void**)&m_pSwapChain));
        pSwapChain->Release();

        // if SDR, convert add gamma for the swapchain format so blending is correct
        if (m_displayMode == DISPLAYMODE_SDR)
        {
            m_swapChainFormat = ConvertIntoGammaFormat(m_swapChainFormat);
        }

        //
        // create RTV heaps
        //
        D3D12_DESCRIPTOR_HEAP_DESC descHeapRtv;
        descHeapRtv.NumDescriptors = m_descSwapChain.BufferCount;
        descHeapRtv.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        descHeapRtv.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        descHeapRtv.NodeMask = 0;
        ThrowIfFailed(m_pDevice->CreateDescriptorHeap(&descHeapRtv, IID_PPV_ARGS(&m_RTVHeaps)));

        CreateRTV();
    }

    //--------------------------------------------------------------------------------------
    //
    // OnDestroy
    //
    //--------------------------------------------------------------------------------------
    void SwapChain::OnDestroy()
    {
        m_swapChainFence.OnDestroy();

        m_RTVHeaps->Release();

        m_pSwapChain->Release();
        m_pFactory->Release();
    }

    //--------------------------------------------------------------------------------------
    //
    // EnumerateDisplayModes
    //
    //--------------------------------------------------------------------------------------
    void SwapChain::EnumerateDisplayModes(std::vector<DisplayModes> *pModes, std::vector<const char *> *pNames)
    {
        fsHdrEnumerateDisplayModes(pModes);

        if (pNames != NULL)
        {
            pNames->clear();
            for (DisplayModes mode : *pModes)
                pNames->push_back(fsHdrGetDisplayModeString(mode));
        }
    }

    //--------------------------------------------------------------------------------------
    //
    // EnumerateDisplayModes
    //
    //--------------------------------------------------------------------------------------
    bool SwapChain::IsModeSupported(DisplayModes displayMode)
    {
        std::vector<DisplayModes> displayModesAvailable;
        EnumerateDisplayModes(&displayModesAvailable);
        return  std::find(displayModesAvailable.begin(), displayModesAvailable.end(), displayMode) != displayModesAvailable.end();
    }

    ID3D12Resource *SwapChain::GetCurrentBackBufferResource()
    {
        uint32_t backBuffferIndex = m_pSwapChain->GetCurrentBackBufferIndex();

        ID3D12Resource *pBackBuffer;
        ThrowIfFailed(m_pSwapChain->GetBuffer(backBuffferIndex, IID_PPV_ARGS(&pBackBuffer)));
        pBackBuffer->Release();
        return pBackBuffer;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE *SwapChain::GetCurrentBackBufferRTV()
    {
        uint32_t backBuffferIndex = m_pSwapChain->GetCurrentBackBufferIndex();
        return &m_CPUView[backBuffferIndex];
    }

    void SwapChain::WaitForSwapChain()
    {
        uint32_t backBuffferIndex = m_pSwapChain->GetCurrentBackBufferIndex();
        m_swapChainFence.CpuWaitForFence(m_BackBufferCount - 1);
    };

    void SwapChain::Present()
    {
        if (m_bVSyncOn)
        {
            ThrowIfFailed(m_pSwapChain->Present(1, 0));
        }
        else
        {
            ThrowIfFailed(m_pSwapChain->Present(0, 0));
        }

        // issue a fence so we can tell when this frame ended
        m_swapChainFence.IssueFence(m_pDirectQueue);
    }

    void SwapChain::SetFullScreen(bool fullscreen)
    {
        // This sets app to Fullscreen Exclusive mode which is different from fullscreen borderless mode.
        ThrowIfFailed(m_pSwapChain->SetFullscreenState(fullscreen, nullptr));
    }

    void SwapChain::OnCreateWindowSizeDependentResources(uint32_t dwWidth, uint32_t dwHeight, bool bVSyncOn, DisplayModes displayMode, bool disableLocalDimming)
    {
        // check whether the requested mode is supported and fall back to SDR if not supported
        bool bIsModeSupported = IsModeSupported(displayMode);
        if (bIsModeSupported == false)
        {
            assert(!"FS HDR display mode not supported");
            displayMode = DISPLAYMODE_SDR;
        }

        m_displayMode = displayMode;
        m_swapChainFormat = fsHdrGetFormat(displayMode);
        m_bVSyncOn = bVSyncOn;

        // note that FS HDR modes require to be in fullscreen mode!
        BOOL isFullScreen;
        ThrowIfFailed(m_pSwapChain->GetFullscreenState(&isFullScreen, nullptr));
        if (m_displayMode != DISPLAYMODE_SDR)
        { 
            assert(isFullScreen==TRUE);
        }

        // Set up the swap chain to allow back buffers to live on multiple GPU nodes.
        ThrowIfFailed(
            m_pSwapChain->ResizeBuffers(
                m_descSwapChain.BufferCount,
                dwWidth,
                dwHeight,
                m_swapChainFormat,
                m_descSwapChain.Flags)
        );

        fsHdrSetDisplayMode(displayMode, disableLocalDimming);

        // if SDR, convert add gamma for the swapchain format so blending is correct
        //if (m_displayMode == DISPLAYMODE_SDR)
        //{
        //    m_swapChainFormat = ConvertIntoGammaFormat(m_swapChainFormat);
        //}

        CreateRTV();
    }

    void SwapChain::OnDestroyWindowSizeDependentResources()
    {
    }

    void SwapChain::CreateRTV()
    {
        //
        // create RTV's
        //
        uint32_t colorDescriptorSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        m_CPUView.resize(m_descSwapChain.BufferCount);
        for (uint32_t i = 0; i < m_descSwapChain.BufferCount; i++)
        {
            m_CPUView[i] = m_RTVHeaps->GetCPUDescriptorHandleForHeapStart();
            m_CPUView[i].ptr += colorDescriptorSize * i;

            ID3D12Resource *pBackBuffer;
            ThrowIfFailed(m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer)));
            SetName(pBackBuffer, "SwapChain");

            D3D12_RESOURCE_DESC desc = pBackBuffer->GetDesc();

            D3D12_RENDER_TARGET_VIEW_DESC colorDesc = {};
            colorDesc.Format = m_swapChainFormat;
            colorDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            colorDesc.Texture2D.MipSlice = 0;
            colorDesc.Texture2D.PlaneSlice = 0;

            m_pDevice->CreateRenderTargetView(pBackBuffer, &colorDesc, m_CPUView[i]);
            SetName(pBackBuffer, format("BackBuffer %i", i));
            pBackBuffer->Release();
        }
    }

    DXGI_FORMAT SwapChain::GetFormat()
    {
        return m_swapChainFormat;
    }

    DisplayModes SwapChain::GetDisplayMode()
    {
        return m_displayMode;
    }

    bool SwapChain::IsFullScreen()
    {
        BOOL isFullScreen;
        ThrowIfFailed(m_pSwapChain->GetFullscreenState(&isFullScreen, nullptr));
        return (bool)isFullScreen;
    }
}
