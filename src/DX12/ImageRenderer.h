#pragma once

#include "Device.h"
#include "DynamicBufferRing.h"
#include "PostProcPS.h"
#include "ResourceViewHeaps.h"
#include "StaticBufferPool.h"

namespace CS570
{
    class ImageRenderer
    {
    public:
        void OnCreate(
            CAULDRON_DX12::Device *pDevice,
            CAULDRON_DX12::ResourceViewHeaps* pResourceViewHeaps,
            CAULDRON_DX12::StaticBufferPool* pStaticBufferPool,
            DXGI_FORMAT outFormat);

        void OnDestroy();

        void UpdatePipelines(DXGI_FORMAT outFormat);
        void Draw(ID3D12GraphicsCommandList *pCommandList, D3D12_FILTER filter, CAULDRON_DX12::CBV_SRV_UAV *pDebugBufferSRV);

    private:
        CAULDRON_DX12::PostProcPS m_linearSampler;
        CAULDRON_DX12::PostProcPS m_pointSampler;
    };
}
