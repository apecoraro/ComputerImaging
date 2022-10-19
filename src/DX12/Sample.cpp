#include "stdafx.h"

#include "Sample.h"

#include "DXCHelper.h"
#include "Imgui.h"
#include "ImGuiHelper.h"
#include "Misc.h"
#include "ShaderCompilerHelper.h"

#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#include <experimental/filesystem>

#include <commdlg.h>

using namespace CS570;

static inline void SetWindowClientSize(HWND hWnd, LONG width, LONG height)
{
    RECT rect;
    rect.left = 0;
    rect.top = 0;
    rect.right = width;
    rect.bottom = height;
    DWORD style = GetWindowLong(hWnd, GWL_STYLE);
    AdjustWindowRect(&rect, style, FALSE);
    width = rect.right - rect.left;
    height = rect.bottom - rect.top;
    SetWindowPos(hWnd, NULL, -1, -1, width, height, SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER);
}

const bool VALIDATION_ENABLED = false;

Sample::Sample(LPCSTR name) : FrameworkWindows(name)
{
    m_lastFrameTime = MillisecondsNow();
    m_time = 0;
    m_bPlay = true;
}

//--------------------------------------------------------------------------------------
//
// OnParseCommandLine
//
//--------------------------------------------------------------------------------------
void Sample::OnParseCommandLine(LPSTR lpCmdLine, uint32_t* pWidth, uint32_t* pHeight, bool *pbFullScreen)
{
    // set some default values
    *pWidth = 1920;
    *pHeight = 1080;
    *pbFullScreen = false;

    //read globals
    auto process = [&](json jData)
    {
        *pWidth = jData.value("width", *pWidth);
        *pHeight = jData.value("height", *pHeight);
        *pbFullScreen = jData.value("fullScreen", *pbFullScreen);
        m_isCpuValidationLayerEnabled = jData.value("CpuValidationLayerEnabled", m_isCpuValidationLayerEnabled);
        m_isGpuValidationLayerEnabled = jData.value("GpuValidationLayerEnabled", m_isGpuValidationLayerEnabled);
#ifdef FFX_CACAO_ENABLE_PROFILING
        m_isBenchmarking = jData.value("benchmark", m_isBenchmarking);
#endif
    };

    {
        std::ifstream f("SampleSettings.json");
        if (!f)
        {
            MessageBox(NULL, "Config file not found!\n", "Cauldron Panic!", MB_ICONERROR);
            exit(0);
        }

        try
        {
            f >> m_jsonConfigFile;
        }
        catch (json::parse_error)
        {
            MessageBox(NULL, "Error parsing SampleSettings.json!\n", "Cauldron Panic!", MB_ICONERROR);
            exit(0);
        }
    }


    json globals = m_jsonConfigFile["globals"];
    process(globals);
}

void Sample::OnCreate(HWND hWnd)
{
    m_hWnd = hWnd;

    m_displayGUI = true;

    DWORD dwAttrib = GetFileAttributes(".\\media\\");
    if ((dwAttrib != INVALID_FILE_ATTRIBUTES) && ((dwAttrib & FILE_ATTRIBUTE_DIRECTORY)) != 0)
    {
        std::string path = "./media";
        for (const auto& entry: std::experimental::filesystem::directory_iterator(path))
        {
            std::string extension = entry.path().extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(), ::toupper);
            if (extension == ".PPM")
                m_mediaFiles.push_back(entry.path().string());
        }
    }

    if (m_mediaFiles.empty())
    {
        MessageBox(NULL, "Media files not found!\n\nThe media files should be at ./media.", "Media fail!", MB_ICONERROR);
        exit(0);
    }

    m_fullscreen = false;

    m_device.OnCreate("CS570", "Cauldron", m_isCpuValidationLayerEnabled, m_isGpuValidationLayerEnabled, hWnd);
    m_device.CreatePipelineCache();

    InitDirectXCompiler();

    CAULDRON_DX12::fsHdrInit(m_device.GetAGSContext(), m_device.GetAGSGPUInfo(), hWnd);

    uint32_t dwNumberOfBackBuffers = 2;
    m_swapChain.OnCreate(&m_device, dwNumberOfBackBuffers, hWnd);

    m_node = new SampleRenderer();

    ImGUI_Init((void *)hWnd);

    std::string inputImage1 = m_mediaFiles[0];
    std::string inputImage2 = m_mediaFiles.size() == 1 ? inputImage1 : m_mediaFiles[1];
    m_node->OnCreate(&m_device, inputImage1, inputImage2, "Log", &m_swapChain);
}

//--------------------------------------------------------------------------------------
//
// OnDestroy
//
//--------------------------------------------------------------------------------------
void Sample::OnDestroy()
{
#ifdef FFX_CACAO_ENABLE_PROFILING
    m_isBenchmarking = false;
#endif

    ImGUI_Shutdown();

    m_device.GPUFlush();

    // Fullscreen state should always be false before exiting the app.
    m_swapChain.SetFullScreen(false);

    m_node->OnDestroyWindowSizeDependentResources();
    m_node->OnDestroy();

    delete m_node;

    m_swapChain.OnDestroyWindowSizeDependentResources();
    m_swapChain.OnDestroy();

    //shut down the shader compiler
    DestroyShaderCache(&m_device);

    m_device.OnDestroy();
}

//--------------------------------------------------------------------------------------
//
// OnEvent
//
//--------------------------------------------------------------------------------------
bool Sample::OnEvent(MSG msg)
{
    if (ImGUI_WndProcHandler(msg.hwnd, msg.message, msg.wParam, msg.lParam))
        return true;

    return true;
}

//--------------------------------------------------------------------------------------
//
// SetFullScreen
//
//--------------------------------------------------------------------------------------
void Sample::SetFullScreen(bool fullscreen)
{
    m_device.GPUFlush();

    m_swapChain.SetFullScreen(fullscreen);
}

//--------------------------------------------------------------------------------------
//
// OnResize
//
//--------------------------------------------------------------------------------------
void Sample::OnResize(uint32_t width, uint32_t height, bool force)
{
#ifdef FFX_CACAO_ENABLE_PROFILING
    if (m_isBenchmarking && !m_benchmarkWarmUpFramesToRun)
    {
        if (width != m_benchmarkScreenWidth || height != m_benchmarkScreenHeight)
        {
            MessageBox(NULL, "Attempt to change screen resolution when benchmarking!\n", "Benchmark Failed!", MB_ICONERROR);
            exit(0);
        }
    }
#endif

    if (m_Width != width || m_Height != height || force)
    {
        // Flush GPU
        //
        m_device.GPUFlush();

        // If resizing but no minimizing
        //
        if (m_Width > 0 && m_Height > 0)
        {
            if (m_node!=NULL)
            {
                m_node->OnDestroyWindowSizeDependentResources();
            }
            m_swapChain.OnDestroyWindowSizeDependentResources();
        }

        m_Width = width;
        m_Height = height;

        // if resizing but not minimizing the recreate it with the new size
        //
        if (m_Width > 0 && m_Height > 0)
        {
            m_swapChain.OnCreateWindowSizeDependentResources(m_Width, m_Height, m_vsyncEnabled, CAULDRON_DX12::DISPLAYMODE_SDR);
            if (m_node != NULL)
            {
                m_node->OnCreateWindowSizeDependentResources(&m_swapChain, m_Width, m_Height);
            }
        }
    }
}

static std::string ShowSelectFileDialog(HWND hWnd)
{
    OPENFILENAME ofn;       // common dialog box structure
    TCHAR szFile[512] = { 0 };       // if using TCHAR macros

    // Initialize OPENFILENAME
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = _T("*.PPM\0");
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn) == TRUE)
    {
                // use ofn.lpstrFile
    }
}
//--------------------------------------------------------------------------------------
//
// BuildUI, also loads the scene!
//
//--------------------------------------------------------------------------------------
void Sample::BuildUI()
{
    ImGuiStyle& style = ImGui::GetStyle();
    style.FrameBorderSize = 1.0f;

    static bool opened = true;
    ImGui::Begin("CS570", &opened);
    if (ImGui::CollapsingHeader("Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        static int currentFilter = 0;
        const char* filters = "Bilinear\0Nearest\0";
        if (ImGui::Combo("Display Filter", &currentFilter, filters))
            m_node->SetDisplayFilter(currentFilter == 0 ? D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT : D3D12_FILTER_MIN_MAG_MIP_POINT);

        static int currentOperation = 4; // Log
        const char* operations[6] = { "Add", "Subtract", "Product", "Negative", "Log", "Power" };
        std::string operation = operations[currentOperation];
        if (ImGui::Combo("Operation", &currentOperation, operations, 6))
        {
            operation = operations[currentOperation];
            m_node->SetOperation(operation);
        }

        std::vector<const char*> inputs;
        for (const auto& mediaFile : m_mediaFiles)
            inputs.push_back(mediaFile.c_str());

        bool inputsUpdated = false;
        static int currentInput1 = 0;
        int lastInput1 = currentInput1;
        if (ImGui::Combo("Input1", &currentInput1, inputs.data(), inputs.size()))
            inputsUpdated = lastInput1 != currentInput1;

        static int currentInput2 = m_mediaFiles.size() == 1 ? 0 : 1;
        int lastInput2 = currentInput2;
        if (operation == "Add" || operation == "Subtract" || operation == "Product")
        {
            if (ImGui::Combo("Input2", &currentInput2, inputs.data(), inputs.size()))
                inputsUpdated = lastInput2 != currentInput2;
        }

        if (inputsUpdated)
        {
            m_device.GPUFlush();
            if (lastInput1 != currentInput1)
                m_node->SetInput1(m_mediaFiles[currentInput1]);
            if (lastInput2 != currentInput2)
                m_node->SetInput2(m_mediaFiles[currentInput2]);
        }

        if (operation == "Log")
        {
            static float logConstant = 1.0f;
            if (ImGui::SliderFloat("Log Constant", &logConstant, 0.0f, 3.0f, "%.2f"))
            {
                m_node->SetLogConstant(logConstant);
            }
        }
        else if (operation == "Power")
        {
            static float powerConstant = 1.0f;
            if (ImGui::SliderFloat("Power Constant", &powerConstant, 0.0f, 3.0f, "%.2f"))
            {
                m_node->SetPowerConstant(powerConstant);
            }

            static float powerRaise = 1.0f;
            if (ImGui::SliderFloat("Power Raise", &powerRaise, 0.0f, 2.0f, "%.3f"))
            {
                m_node->SetPowerRaise(powerRaise);
            }
        }

        /*if (ImGui::CollapsingHeader("Profiler", ImGuiTreeNodeFlags_DefaultOpen))
        {
            std::vector<TimeStamp> timeStamps = m_node->GetTimingValues();
            if (timeStamps.size() > 0)
            {
                for (uint32_t i = 1; i < timeStamps.size() - 1; i++)
                {
                    float deltaTime = timeStamps[i].m_microseconds;
                    ImGui::Text("%-17s: %7.1f us", timeStamps[i].m_label.c_str(), deltaTime);
                }

                //scrolling data and average computing
                static float values[128] = { 0.0f };
                float minTotal = FLT_MAX;
                float maxTotal = -1.0f;
                // Copy previous total times one element to the left.
                for (uint32_t i = 0; i < 128 - 1; i++)
                {
                    values[i] = values[i + 1];
                    if (values[i] < minTotal)
                        minTotal = values[i];
                    if (values[i] > maxTotal)
                        maxTotal = values[i];
                }
                // Store current total time at end.
                values[127] = timeStamps.back().m_microseconds;

                // round down to nearest 1000.0f
                float rangeStart = static_cast<uint32_t>(minTotal / 1000.0f) * 1000.0f;
                // round maxTotal up to nearest 10,000.0f
                float rangeStop = (static_cast<uint32_t>(maxTotal / 10000.0f) * 10000.0f) + 10000.0f;

                ImGui::PlotLines("", values, 128, 0, "", rangeStart, rangeStop, ImVec2(0, 80));
            }
        }*/
    }

    static std::string saveText;
    if (ImGui::CollapsingHeader("Save", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::Button("Output"))
        {
            m_node->SaveOutput();
            saveText = "Saved output to Output.ppm";
        }

        ImGui::SameLine();

        if (ImGui::Button("CCL Output"))
        {
            m_node->SaveCCLOutput();
            saveText = "Saved connected component output to CCL_Output.ppm";
        }
    }
    if (!saveText.empty())
        ImGui::Text(saveText.c_str());

    ImGui::End();
}

void Sample::OnRender()
{
    double timeNow = MillisecondsNow();
    m_deltaTime = timeNow - m_lastFrameTime;
    m_lastFrameTime = timeNow;

    if (m_bPlay)
        m_time += (float)m_deltaTime / 1000.0f;

    ImGUI_UpdateIO();
    ImGui::NewFrame();

    BuildUI();
    if (m_bPlay)
        m_time += (float)m_deltaTime / 1000.0f;

    m_state.time = m_time;
    m_node->OnRender(&m_state, &m_swapChain);
    m_swapChain.Present();

    m_node->OnPostRender();
}

int WINAPI WinMain(HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow)
{
    LPCSTR Name = "CS570";
    return RunFramework(hInstance, lpCmdLine, nCmdShow, new Sample(Name));
}
