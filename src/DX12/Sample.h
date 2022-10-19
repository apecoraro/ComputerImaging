#pragma once

#include "FrameworkWindows.h"
#include "SampleRenderer.h"

#include "../libs/json/json.h"

namespace CS570
{
    using json = nlohmann::json;

    class Sample : public FrameworkWindows
    {
    public:
        Sample(LPCSTR name);
        void OnCreate(HWND hWnd);
        void OnDestroy();
        void BuildUI();
        void OnParseCommandLine(LPSTR lpCmdLine, uint32_t* pWidth, uint32_t* pHeight, bool *pbFullScreen);
        void OnRender();
        bool OnEvent(MSG msg);
        void OnResize(uint32_t width, uint32_t height) { OnResize(width, height, false); }
        void OnResize(uint32_t Width, uint32_t Height, bool force);
        void SetFullScreen(bool fullscreen);

    private:
        HWND m_hWnd;

        CAULDRON_DX12::Device m_device;
        CAULDRON_DX12::SwapChain m_swapChain;

        std::vector<std::string> m_mediaFiles;

        SampleRenderer* m_node = nullptr;
        SampleRenderer::State m_state;

        int32_t m_preset;

        float m_time;
        double m_deltaTime;
        double m_lastFrameTime;

        bool m_isCapturing = false;
        bool m_vsyncEnabled = false;
        bool m_bPlay;
        bool m_displayGUI;
        bool m_fullscreen;

        // json config file
        json m_jsonConfigFile;
        bool m_isCpuValidationLayerEnabled;
        bool m_isGpuValidationLayerEnabled;

        int m_presetIndex = 0;
    };
} // namespace CS570