#include "stdafx.h"
#include "SaveTexture.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"
#include "d3d12x/d3dx12.h"
#include "Helper.h"
#include "Error.h"
#include "DxgiFormatHelper.h"


namespace CAULDRON_DX12
{
    void SaveTexture::CopyRenderTargetIntoStagingTexture(ID3D12Device *pDevice, ID3D12GraphicsCommandList* pCmdLst2, ID3D12Resource* pResourceFrom, D3D12_RESOURCE_STATES resourceCurrentState)
    {
        m_bufferFromDesc = pResourceFrom->GetDesc();

        CD3DX12_HEAP_PROPERTIES readBackHeapProperties(D3D12_HEAP_TYPE_READBACK);

        D3D12_RESOURCE_DESC bufferDesc = {};
        bufferDesc.Alignment = 0;
        bufferDesc.DepthOrArraySize = 1;
        bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
        bufferDesc.Height = 1;
        bufferDesc.Width = m_bufferFromDesc.Width * m_bufferFromDesc.Height * GetPixelByteSize(m_bufferFromDesc.Format);
        bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        bufferDesc.MipLevels = 1;
        bufferDesc.SampleDesc.Count = 1;
        bufferDesc.SampleDesc.Quality = 0;

        pDevice->CreateCommittedResource(
            &readBackHeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&m_pResourceReadBack));

        SetName(m_pResourceReadBack, "CopyRenderTargetIntoStagingTexture::pResourceReadBack");

        pCmdLst2->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pResourceFrom, resourceCurrentState, D3D12_RESOURCE_STATE_COPY_SOURCE));

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout[1] = { 0 };
        uint32_t num_rows[1] = { 0 };
        UINT64 row_sizes_in_bytes[1] = { 0 };
        pDevice->GetCopyableFootprints(&m_bufferFromDesc, 0, 1, 0, layout, num_rows, row_sizes_in_bytes, &m_uploadHeapSize);

        CD3DX12_TEXTURE_COPY_LOCATION copyDest(m_pResourceReadBack, layout[0]);
        CD3DX12_TEXTURE_COPY_LOCATION copySrc(pResourceFrom, 0);
        pCmdLst2->CopyTextureRegion(&copyDest, 0, 0, 0, &copySrc, nullptr);

        pCmdLst2->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pResourceFrom, D3D12_RESOURCE_STATE_COPY_SOURCE, resourceCurrentState));
    }

    void SaveTexture::SaveStagingTextureAsJpeg(ID3D12Device *pDevice, ID3D12CommandQueue *pDirectQueue, const char *pFilename)
    {
        ProcessStagingBuffer(pDevice, pDirectQueue, [pFilename](int width, int height, uint8_t* pImageBuffer) {
            stbi_write_jpg(pFilename, width, height, 4, pImageBuffer, 100);
        });
    }

    void SaveTexture::ProcessStagingBuffer(ID3D12Device *pDevice, ID3D12CommandQueue *pDirectQueue, std::function<void(int, int, uint8_t*)> writeFile)
    {
        ID3D12Fence *pFence;
        ThrowIfFailed(pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pFence)));
        SetName(pFence, "CopyRenderTargetIntoStagingTexture::pFence");
        ThrowIfFailed(pDirectQueue->Signal(pFence, 1));

        //wait for fence
        HANDLE mHandleFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        pFence->SetEventOnCompletion(1, mHandleFenceEvent);
        WaitForSingleObject(mHandleFenceEvent, INFINITE);
        CloseHandle(mHandleFenceEvent);

        pFence->Release();

        uint8_t* pImageBuffer = NULL;
        D3D12_RANGE range;
        range.Begin = 0;
        range.End = m_uploadHeapSize;
        m_pResourceReadBack->Map(0, &range, reinterpret_cast<void**>(&pImageBuffer));

        writeFile((int)m_bufferFromDesc.Width, (int)m_bufferFromDesc.Height, pImageBuffer);

        m_pResourceReadBack->Unmap(0, NULL);

        m_pResourceReadBack->Release();
        m_pResourceReadBack = nullptr;
    }
}
