#pragma once

#include <d3d12.h>

#include <functional>

namespace CAULDRON_DX12
{
    class SaveTexture
    {
    private:
        // State variables
        UINT64 m_uploadHeapSize = 0;
        D3D12_RESOURCE_DESC m_bufferFromDesc = { 0 };
        ID3D12Resource* m_pResourceReadBack = nullptr;

    public:
        void CopyRenderTargetIntoStagingTexture(ID3D12Device *pDevice, ID3D12GraphicsCommandList* pCmdLst2, ID3D12Resource* pResourceFrom, D3D12_RESOURCE_STATES state);

        void SaveStagingTextureAsJpeg(ID3D12Device *pDevice, ID3D12CommandQueue *pDirectQueue, const char *pFilename);

        void ProcessStagingBuffer(ID3D12Device* pDevice, ID3D12CommandQueue* pDirectQueue, std::function<void(int, int, uint8_t*)> writeFile);
    };
}