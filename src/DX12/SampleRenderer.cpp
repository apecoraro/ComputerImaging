#include "SampleRenderer.h"

#include "Error.h"
#include "Texture.h"
#include "SaveTexture.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <queue>
#include <vector>

#include "stdafx.h"

using namespace CS570;

void SampleRenderer::OnCreate(
    CAULDRON_DX12::Device* pDevice,
    const std::string& inputImage1,
    const std::string& inputImage2,
    const std::string& initialOperation,
    CAULDRON_DX12::SwapChain *pSwapChain)
{
    m_pDevice = pDevice;

    const uint32_t cbvDescriptorCount = 3000;
    const uint32_t srvDescriptorCount = 3000;
    const uint32_t uavDescriptorCount = 100;
    const uint32_t dsvDescriptorCount = 100;
    const uint32_t rtvDescriptorCount = 1000;
    const uint32_t samplerDescriptorCount = 50;
    m_resourceViewHeaps.OnCreate(pDevice, cbvDescriptorCount, srvDescriptorCount, uavDescriptorCount, dsvDescriptorCount, rtvDescriptorCount, samplerDescriptorCount);

    uint32_t commandListsPerBackBuffer = 8;
    m_commandListRing.OnCreate(pDevice, k_backBufferCount + 1, commandListsPerBackBuffer, pDevice->GetGraphicsQueue()->GetDesc());

    const uint32_t constantBuffersMemSize = 20 * 1024 * 1024;
    m_constantBufferRing.OnCreate(pDevice, k_backBufferCount, constantBuffersMemSize, &m_resourceViewHeaps);

    const uint32_t staticMemSizeBytes = 128 * 1024 * 1024;
    m_vidMemBufferPool.OnCreate(pDevice, staticMemSizeBytes, true, "StaticMemory");

    m_gpuTimer.OnCreate(pDevice, k_backBufferCount);

    const uint32_t uploadHeapMemSize = 1000 * 1024 * 1024;
    m_uploadHeap.OnCreate(pDevice, uploadHeapMemSize);

    m_inputImage1 = inputImage1;
    m_inputImage2 = inputImage2;
    LoadInputTextures(inputImage1, inputImage2);

    m_addOperation.OnCreate("Add",
        m_inputTexture1, m_inputTexture2,
        m_pDevice, &m_uploadHeap, &m_resourceViewHeaps, &m_constantBufferRing);
    m_subtractOperation.OnCreate("Subtract",
        m_inputTexture1, m_inputTexture2,
        m_pDevice, &m_uploadHeap, &m_resourceViewHeaps, &m_constantBufferRing);
    m_productOperation.OnCreate("Product",
        m_inputTexture1, m_inputTexture2,
        m_pDevice, &m_uploadHeap, &m_resourceViewHeaps, &m_constantBufferRing);
    m_negativeOperation.OnCreate("Negative",
        m_inputTexture1, m_inputTexture2,
        m_pDevice, &m_uploadHeap, &m_resourceViewHeaps, &m_constantBufferRing);
    m_logOperation.OnCreate("Log",
        m_inputTexture1, m_inputTexture2,
        m_pDevice, &m_uploadHeap, &m_resourceViewHeaps, &m_constantBufferRing);
    m_powerOperation.OnCreate("Power",
        m_inputTexture1, m_inputTexture2,
        m_pDevice, &m_uploadHeap, &m_resourceViewHeaps, &m_constantBufferRing);

    m_histogramEqualizer.OnCreate(m_inputTexture1,
        m_pDevice, &m_uploadHeap, &m_resourceViewHeaps, &m_constantBufferRing);

    m_histogramMatcher.OnCreate(m_inputTexture1, m_inputTexture2,
        m_pDevice, &m_uploadHeap, &m_resourceViewHeaps, &m_constantBufferRing);

    m_gaussianBlur.OnCreate(m_inputTexture1, m_blurKernelSize, m_blurVariance,
        m_pDevice, &m_uploadHeap, &m_resourceViewHeaps, &m_constantBufferRing);

    SetOperation(initialOperation);

    m_imageRenderer.OnCreate(m_pDevice, &m_resourceViewHeaps, &m_vidMemBufferPool, pSwapChain->GetFormat());

    // Initialize UI rendering resources
    m_imGUI.OnCreate(pDevice, &m_uploadHeap, &m_resourceViewHeaps, &m_constantBufferRing, pSwapChain->GetFormat());

    // Make sure upload heap has finished uploading before continuing
    m_vidMemBufferPool.UploadData(m_uploadHeap.GetCommandList());
    m_uploadHeap.FlushAndFinish();
}

static uint16_t ReadColor(std::ifstream& inputFile, std::string& lineOfPixels, std::stringstream& streamOfPixels)
{
    uint16_t color = 0xFF;
    bool parsedColor = false;
    while (!parsedColor)
    {
        if (streamOfPixels.eof())
        {
            if (inputFile.eof())
                break;

            std::getline(inputFile, lineOfPixels);
            streamOfPixels.clear();
            streamOfPixels << lineOfPixels;
        }

        streamOfPixels >> color;

        parsedColor = !streamOfPixels.fail();
    }

    assert(parsedColor);

    return color;
}

static void LoadPPMTextData(std::ifstream& inputFile, uint32_t width, uint32_t height, uint32_t maxValue, float* pImageBuffer)
{
    float invMaxValue = 1.0f / static_cast<float>(maxValue);

    std::string lineOfPixels;
    std::stringstream streamOfPixels;

    std::getline(inputFile, lineOfPixels);
    streamOfPixels << lineOfPixels;

    float* pWritePtr = pImageBuffer;
    for (uint32_t hIndex = 0; hIndex < height; ++hIndex)
    {
        for (uint32_t wIndex = 0; wIndex < width; ++wIndex)
        {
            uint16_t red = ReadColor(inputFile, lineOfPixels, streamOfPixels);
            uint16_t green = ReadColor(inputFile, lineOfPixels, streamOfPixels);
            uint16_t blue = ReadColor(inputFile, lineOfPixels, streamOfPixels);

            *pWritePtr = static_cast<float>(red) * invMaxValue;
            ++pWritePtr;
            *pWritePtr = static_cast<float>(green) * invMaxValue;
            ++pWritePtr;
            *pWritePtr = static_cast<float>(blue) * invMaxValue;
            ++pWritePtr;
            *pWritePtr = 1.0f; // alpha
            ++pWritePtr;
        }
    }
}


template <typename T>
void LoadPPMBinaryData(std::ifstream& inputFile, uint32_t width, uint32_t height, uint32_t maxValue, float* pImageBuffer)
{
    float invMaxValue = 1.0f / static_cast<float>(maxValue);

    float* pReadPtr = pImageBuffer;
    for (uint32_t hIndex = 0; hIndex < height; ++hIndex)
    {
        std::vector<T> pixelRowBytes(width * 3);
        inputFile.read((char*)pixelRowBytes.data(), sizeof(T) * width * 3);
        for (uint32_t wIndex = 0; wIndex < width; ++wIndex)
        {
            *pReadPtr = static_cast<float>(pixelRowBytes[(wIndex * 3) + 0]) * invMaxValue;
            ++pReadPtr;
            *pReadPtr = static_cast<float>(pixelRowBytes[(wIndex * 3) + 1]) * invMaxValue;
            ++pReadPtr;
            *pReadPtr = static_cast<float>(pixelRowBytes[(wIndex * 3) + 2]) * invMaxValue;
            ++pReadPtr;
            *pReadPtr = 1.0f; // alpha
            ++pReadPtr;
        }
    }
}

static void LoadPPM(const std::string& imageFile, IMG_INFO* pImageHeader, std::vector<uint8_t>* pImageData)
{
    std::ifstream inputFile(imageFile);

    assert(inputFile.is_open());

    std::string imageTypeCode;
    inputFile >> imageTypeCode;

    bool widthHeightParsed = false;
    std::string headerLine;
    while (!widthHeightParsed)
    {
        if (inputFile.eof())
            throw "Invalid ppm file, failed to parse width and height from header.";

        std::getline(inputFile, headerLine);
        if (!headerLine.empty() && headerLine.front() != '#')
        {
            std::stringstream str;
            str << headerLine;
            str >> pImageHeader->width;
            str >> pImageHeader->height;
            widthHeightParsed = true;
        }
    }

    bool maxValueParsed = false;
    uint32_t maxPixelValue = 0u;
    while (!maxValueParsed)
    {
        if (inputFile.eof())
            throw "Invalid ppm file, failed to parse width and height from header.";

        std::getline(inputFile, headerLine);
        if (!headerLine.empty() && headerLine.front() != '#')
        {
            std::stringstream str;
            str << headerLine;
            str >> maxPixelValue;
            maxValueParsed = true;
        }
    }

    pImageHeader->bitCount = 32 * 4;
    pImageHeader->format = DXGI_FORMAT_R32G32B32A32_FLOAT;

    pImageData->resize(pImageHeader->width * pImageHeader->height * sizeof(float) * 4);
    if (imageTypeCode == "P3")
        LoadPPMTextData(inputFile, pImageHeader->width, pImageHeader->height, maxPixelValue, reinterpret_cast<float*>(pImageData->data()));
    else if (maxPixelValue > 255)
        LoadPPMBinaryData<uint16_t>(inputFile, pImageHeader->width, pImageHeader->height, maxPixelValue, reinterpret_cast<float*>(pImageData->data()));
    else
        LoadPPMBinaryData<uint8_t>(inputFile, pImageHeader->width, pImageHeader->height, maxPixelValue, reinterpret_cast<float*>(pImageData->data()));

    pImageHeader->depth = 1u;
    pImageHeader->arraySize = 1u;
    pImageHeader->mipMapCount = 1u;
}

void SampleRenderer::LoadInputTexture(
    const std::string& inputImage,
    CAULDRON_DX12::Texture& inputTexture)
{
    std::string extension = inputImage.substr(inputImage.length() - 4);
    std::transform(extension.begin(), extension.end(), extension.begin(), ::toupper);
    if (extension == ".PPM")
    {
        IMG_INFO imageHeader = {};
        std::vector<uint8_t> imageData;
        LoadPPM(inputImage, &imageHeader, &imageData);
        inputTexture.InitFromData(m_pDevice, "InputImage1", m_uploadHeap, imageHeader, imageData.data());
    }
    else
        inputTexture.InitFromFile(m_pDevice, &m_uploadHeap, inputImage.c_str());
}

void SampleRenderer::LoadInputTextures(
    const std::string& inputImage1,
    const std::string& inputImage2)
{
    //const std::string inputImage1 = "CCL_Output.ppm";
    LoadInputTexture(inputImage1, m_inputTexture1);
    LoadInputTexture(inputImage2, m_inputTexture2);
}

void SampleRenderer::SetOperation(const std::string& operation)
{
    m_currentOperation = operation;
    if (operation == "Add") m_pCurrentOperation = &m_addOperation;
    else if (operation == "Subtract") m_pCurrentOperation = &m_subtractOperation;
    else if (operation == "Product") m_pCurrentOperation = &m_productOperation;
    else if (operation == "Negative") m_pCurrentOperation = &m_negativeOperation;
    else if (operation == "Log") m_pCurrentOperation = &m_logOperation;
    else if (operation == "Power") m_pCurrentOperation = &m_powerOperation;
    else if (operation == "Histogram Equalization") m_pCurrentOperation = &m_histogramEqualizer;
    else if (operation == "Histogram Match") m_pCurrentOperation = &m_histogramMatcher;
    else if (operation == "Gaussian Blur") m_pCurrentOperation = &m_gaussianBlur;
}

void SampleRenderer::SetInput1(const std::string& inputImage1)
{
    m_rebuildImage1 = true;
    m_inputImage1 = inputImage1;
}

void SampleRenderer::SetInput2(const std::string& inputImage2)
{
    m_rebuildImage2 = true;
    m_inputImage2 = inputImage2;
}

void SampleRenderer::OnPostRender()
{
    if (!m_rebuildImage1 && !m_rebuildImage2 && !m_recreateBlurWeights)
        return;

    m_pDevice->GPUFlush();

    if (m_rebuildImage1)
    {
        m_inputTexture1.OnDestroy();
        LoadInputTexture(m_inputImage1, m_inputTexture1);

        m_rebuildImage1 = false;
    }

    if (m_rebuildImage2)
    {
        m_inputTexture2.OnDestroy();
        LoadInputTexture(m_inputImage2, m_inputTexture2);

        m_rebuildImage2 = false;
    }

    m_addOperation.OnDestroy();
    m_subtractOperation.OnDestroy();
    m_productOperation.OnDestroy();
    m_negativeOperation.OnDestroy();
    m_logOperation.OnDestroy();
    m_powerOperation.OnDestroy();

    m_histogramEqualizer.OnDestroy();
    m_histogramMatcher.OnDestroy();

    m_gaussianBlur.OnDestroy();

    m_addOperation.OnCreate("Add",
        m_inputTexture1, m_inputTexture2,
        m_pDevice, &m_uploadHeap, &m_resourceViewHeaps, &m_constantBufferRing);
    m_subtractOperation.OnCreate("Subtract",
        m_inputTexture1, m_inputTexture2,
        m_pDevice, &m_uploadHeap, &m_resourceViewHeaps, &m_constantBufferRing);
    m_productOperation.OnCreate("Product",
        m_inputTexture1, m_inputTexture2,
        m_pDevice, &m_uploadHeap, &m_resourceViewHeaps, &m_constantBufferRing);
    m_negativeOperation.OnCreate("Negative",
        m_inputTexture1, m_inputTexture2,
        m_pDevice, &m_uploadHeap, &m_resourceViewHeaps, &m_constantBufferRing);
    m_logOperation.OnCreate("Log",
        m_inputTexture1, m_inputTexture2,
        m_pDevice, &m_uploadHeap, &m_resourceViewHeaps, &m_constantBufferRing);
    m_powerOperation.OnCreate("Power",
        m_inputTexture1, m_inputTexture2,
        m_pDevice, &m_uploadHeap, &m_resourceViewHeaps, &m_constantBufferRing);

    m_histogramEqualizer.OnCreate(m_inputTexture1,
        m_pDevice, &m_uploadHeap, &m_resourceViewHeaps, &m_constantBufferRing);

    m_histogramMatcher.OnCreate(m_inputTexture1, m_inputTexture2,
        m_pDevice, &m_uploadHeap, &m_resourceViewHeaps, &m_constantBufferRing);

    m_gaussianBlur.OnCreate(m_inputTexture1, m_blurKernelSize, m_blurVariance,
        m_pDevice, &m_uploadHeap, &m_resourceViewHeaps, &m_constantBufferRing);

    m_vidMemBufferPool.UploadData(m_uploadHeap.GetCommandList());
    m_uploadHeap.FlushAndFinish();

    m_pDevice->GPUFlush();
}

void SampleRenderer::OnDestroy()
{
    m_imGUI.OnDestroy();

    m_addOperation.OnDestroy();
    m_subtractOperation.OnDestroy();
    m_productOperation.OnDestroy();
    m_negativeOperation.OnDestroy();
    m_logOperation.OnDestroy();
    m_powerOperation.OnDestroy();

    m_histogramEqualizer.OnDestroy();
    m_histogramMatcher.OnDestroy();

    m_imageRenderer.OnDestroy();

    m_inputTexture1.OnDestroy();

    m_uploadHeap.OnDestroy();
    m_gpuTimer.OnDestroy();
    m_vidMemBufferPool.OnDestroy();
    m_constantBufferRing.OnDestroy();
    m_commandListRing.OnDestroy();
    m_resourceViewHeaps.OnDestroy();
}

void SampleRenderer::OnCreateWindowSizeDependentResources(CAULDRON_DX12::SwapChain* pSwapChain, uint32_t width, uint32_t height)
{
    m_width = width;
    m_height = height;

    m_viewPort = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };

    m_rectScissor = { 0, 0, (LONG)width, (LONG)height };

    m_imageRenderer.UpdatePipelines(pSwapChain->GetFormat());

    m_imGUI.UpdatePipeline(pSwapChain->GetFormat());
}

void SampleRenderer::OnDestroyWindowSizeDependentResources()
{
}

void SampleRenderer::LoadScene()
{
    m_uploadHeap.FlushAndFinish();

    //once everything is uploaded we dont need he upload heaps anymore
    m_vidMemBufferPool.FreeUploadHeap();
}

static void WritePPMHeader(std::ofstream& fstream, int width, int height)
{
    assert(fstream.is_open());

    int maxValue = 255; // This assumes our output format is rgba8
    std::stringstream header;
    header << "P6" << " " << width << " " << height << " " << maxValue << " ";
    //header << "P3" << std::endl << width << " " << height << std::endl << maxValue << std::endl;

    fstream << header.str();
}

struct PixelColor
{
    uint8_t rgb[3];
};

static constexpr const uint32_t k_numObjectColors = 10;
static const PixelColor k_objectColors[k_numObjectColors] = {
    {0, 0, 0},
    {255, 0, 0},
    {0, 255, 0},
    {0, 0, 255},
    {255, 0, 255},
    {255, 255, 0},
    {0, 255, 255},
    {255, 128, 128},
    {128, 255, 128},
    {128, 128, 255}
};

uint32_t FindLargestSetBit(uint8_t red)
{
    uint32_t bitCount = 0;
    while (red != 0)
    {
        red >>= 1;
        ++bitCount;
    }

    return bitCount;
}

static void WriteConnectedComponentImage(
    CAULDRON_DX12::SaveTexture& saver,
    ID3D12Device* pDevice,
    ID3D12CommandQueue* pDirectQueue,
    const char* pFilename)
{
    saver.ProcessStagingBuffer(pDevice, pDirectQueue, [pFilename](int width, int height, uint8_t* pImageBuffer) {
        struct PixelIndex
        {
            int x;
            int y;
            PixelIndex(int _x, int _y) : x(_x), y(_y) {}
        };

        std::queue<PixelIndex> processingQueue;
        struct PixelLabel
        {
            uint32_t label = 0;
        };

        /*uint8_t pImageBuffer[] = {
            //       0            1                  2               3
                0, 0, 0, 0,     0, 0, 0, 0,     0, 0, 0, 0,    255, 0, 0, 0, // 0
                0, 0, 0, 0,     0, 0, 0, 0,   255, 0, 0, 0,    255, 0, 0, 0, // 1
                0, 0, 0, 0,   255, 0, 0, 0,   255, 0, 0, 0,    255, 0, 0, 0, // 2
              255, 0, 0, 0,     0, 0, 0, 0,     0, 0, 0, 0,    255, 0, 0, 0  // 3
        };
        width = 4;
        height = 4;*/

        PixelLabel currentLabel = { 1 };
        uint32_t currentLargestSetBit = 0;
        std::vector<std::vector<PixelLabel>> pixelLabels(height, std::vector<PixelLabel>(width));
        uint8_t* pReadPtr = pImageBuffer;
        for (int row = 0; row < height; ++row)
        {
            for (int col = 0; col < width; ++col)
            {
                uint8_t red = *pReadPtr;
                if (red != 0 && pixelLabels[row][col].label == 0)
                {
                    currentLargestSetBit = FindLargestSetBit(red);
                    // label this pixel with the current label
                    pixelLabels[row][col].label = currentLabel.label;
                    // push the pixel on the queue so we can examine its neighbors
                    processingQueue.push(PixelIndex(col, row));
                    while (!processingQueue.empty())
                    {
                        auto pixelIndex = processingQueue.front();
                        processingQueue.pop();

                        // examine the neighbors of the current pixel
                        const PixelIndex neighborOffsets[4] = { {-1, 0}, {1, 0}, {0, -1}, {0, 1} };
                        for (int offsetIndex = 0; offsetIndex < 4; ++offsetIndex)
                        {
                            int nextX = pixelIndex.x + neighborOffsets[offsetIndex].x;
                            int nextY = pixelIndex.y + neighborOffsets[offsetIndex].y;
                            // no need to examine pixels that we've already examined.
                            if (nextY == row && nextX < col || nextY < row)
                                continue;
                            // don't go past bounds
                            if (nextX < 0 || nextX >= width || nextY < 0 || nextY >= height)
                                continue;
                            // don't examine pixels that are already labelled
                            if (pixelLabels[nextY][nextX].label == 0)
                            {
                                // if they aren't the correct value then skip
                                int nextIndex = (nextY * width * 4) + (nextX * 4);
                                uint8_t nextRed = pImageBuffer[nextIndex];
                                // If the largest set bit is the same then consider it the same object
                                if (FindLargestSetBit(nextRed) == currentLargestSetBit)
                                {
                                    // label this pixel with the current label
                                    pixelLabels[nextY][nextX].label = currentLabel.label;
                                    // push the next pixel on the queue so we can examine its neighbors
                                    processingQueue.push(PixelIndex(nextX, nextY));
                                }
                            }
                        }
                    }

                    ++currentLabel.label;
                }

                ++pReadPtr; // red
                ++pReadPtr; // green
                ++pReadPtr; // blue
                ++pReadPtr; // alpha
            }
        }

        std::ofstream fstream(pFilename, std::ios::binary);

        WritePPMHeader(fstream, width, height);

        for (int row = 0; row < height; ++row)
        {
            for (int col = 0; col < width; ++col)
            {
                PixelLabel pixelLabel = pixelLabels[row][col];
                const PixelColor& pixelColor = k_objectColors[pixelLabel.label % k_numObjectColors];

                //fstream << (int)pixelColor.rgb[0] << " ";
                //fstream << (int)pixelColor.rgb[1] << " ";
                //fstream << (int)pixelColor.rgb[2] << "    ";
                fstream << pixelColor.rgb[0];
                fstream << pixelColor.rgb[1];
                fstream << pixelColor.rgb[2];
            }
            //fstream << std::endl;
        }
    });
}

static void WriteOutputToPPM(
    CAULDRON_DX12::SaveTexture& saver,
    ID3D12Device* pDevice,
    ID3D12CommandQueue* pDirectQueue,
    const char* pFilename)
{
    saver.ProcessStagingBuffer(pDevice, pDirectQueue, [pFilename](int width, int height, uint8_t* pImageBuffer) {
        std::ofstream fstream(pFilename, std::ios::binary);

        WritePPMHeader(fstream, width, height);

        uint8_t* pReadPtr = pImageBuffer;
        for (int row = 0; row < height; ++row)
        {
            for (int col = 0; col < width; ++col)
            {
                uint8_t red = *pReadPtr;
                fstream << red;
                //fstream << (int)red << " ";
                ++pReadPtr;

                uint8_t green = *pReadPtr;
                fstream << green;
                //fstream << (int)green << " ";
                ++pReadPtr;

                uint8_t blue = *pReadPtr;
                fstream << blue;
                //fstream << (int)blue << " ";
                ++pReadPtr;

                //fstream << "  ";
                // jump past the alpha
                ++pReadPtr;
            }
            //fstream << std::endl;
        }
    });
}

void SampleRenderer::OnRender(State *pState, CAULDRON_DX12::SwapChain *pSwapChain)
{
    // Timing values
    UINT64 gpuTicksPerSecond;
    m_pDevice->GetGraphicsQueue()->GetTimestampFrequency(&gpuTicksPerSecond);

    // Let our resource managers do some house keeping
    m_constantBufferRing.OnBeginFrame();
    m_gpuTimer.OnBeginFrame(gpuTicksPerSecond, &m_timeStamps);

    // command buffer calls
    ID3D12GraphicsCommandList* pCmdLst1 = m_commandListRing.GetNewCommandList();

    m_gpuTimer.GetTimeStamp(pCmdLst1, "Begin Frame");

    m_pCurrentOperation->Draw(pCmdLst1);

    CD3DX12_RESOURCE_BARRIER barriers[] = {
        CD3DX12_RESOURCE_BARRIER::Transition(
            pSwapChain->GetCurrentBackBufferResource(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET),
    };
    pCmdLst1->ResourceBarrier(sizeof(barriers) / sizeof(CD3DX12_RESOURCE_BARRIER), barriers);

    // submit command buffer
    ThrowIfFailed(pCmdLst1->Close());
    ID3D12CommandList* CmdListList1[] = { pCmdLst1 };
    m_pDevice->GetGraphicsQueue()->ExecuteCommandLists(1, CmdListList1);

    pSwapChain->WaitForSwapChain();

    m_commandListRing.OnBeginFrame();

    ID3D12GraphicsCommandList* pCmdLst2 = m_commandListRing.GetNewCommandList();

    pCmdLst2->RSSetViewports(1, &m_viewPort);
    pCmdLst2->RSSetScissorRects(1, &m_rectScissor);
    pCmdLst2->OMSetRenderTargets(1, pSwapChain->GetCurrentBackBufferRTV(), true, NULL);

    m_imageRenderer.Draw(pCmdLst2, m_displayFilter, &m_pCurrentOperation->GetOutputSrv());

    CAULDRON_DX12::SaveTexture saver;
    if (m_saveOutput || m_saveCCLOutput)
        saver.CopyRenderTargetIntoStagingTexture(m_pDevice->GetDevice(), pCmdLst2, pSwapChain->GetCurrentBackBufferResource(), D3D12_RESOURCE_STATE_RENDER_TARGET);

    // Render HUD  ------------------------------------------------------------------------
    {
        pCmdLst2->RSSetViewports(1, &m_viewPort);
        pCmdLst2->RSSetScissorRects(1, &m_rectScissor);
        pCmdLst2->OMSetRenderTargets(1, pSwapChain->GetCurrentBackBufferRTV(), true, NULL);

        m_imGUI.Draw(pCmdLst2);

        m_gpuTimer.GetTimeStamp(pCmdLst2, "ImGUI rendering");
    }

    // Transition swapchain into present mode
    CD3DX12_RESOURCE_BARRIER swapChainBarrier[] = {
        CD3DX12_RESOURCE_BARRIER::Transition(
           pSwapChain->GetCurrentBackBufferResource(),
           D3D12_RESOURCE_STATE_RENDER_TARGET,
           D3D12_RESOURCE_STATE_PRESENT),
    };
    pCmdLst2->ResourceBarrier(sizeof(swapChainBarrier) / sizeof(CD3DX12_RESOURCE_BARRIER), swapChainBarrier);

    m_gpuTimer.OnEndFrame();

    m_gpuTimer.CollectTimings(pCmdLst2);

    // Close & Submit the command list ----------------------------------------------------
    ThrowIfFailed(pCmdLst2->Close());

    ID3D12CommandList* CmdListList2[] = { pCmdLst2 };
    m_pDevice->GetGraphicsQueue()->ExecuteCommandLists(1, CmdListList2);

    if (m_saveOutput)
    {
        m_pDevice->GPUFlush();
        WriteOutputToPPM(
            saver,
            m_pDevice->GetDevice(),
            m_pDevice->GetGraphicsQueue(),
            "Output.ppm");
        m_saveOutput = false;
    }
    else if (m_saveCCLOutput)
    {
        m_pDevice->GPUFlush();
        WriteConnectedComponentImage(
            saver,
            m_pDevice->GetDevice(),
            m_pDevice->GetGraphicsQueue(),
            "CCL_Output.ppm");
        m_saveCCLOutput = false;
    }
}