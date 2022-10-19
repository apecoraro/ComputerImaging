#include "ImageRenderer.h"

#include "UserMarkers.h"

#include "stdafx.h"

namespace CS570
{
    void ImageRenderer::OnCreate(
        CAULDRON_DX12::Device* pDevice,
        CAULDRON_DX12::ResourceViewHeaps* pResourceViewHeaps,
        CAULDRON_DX12::StaticBufferPool* pStaticBufferPool,
        DXGI_FORMAT outFormat)
    {
        D3D12_STATIC_SAMPLER_DESC linearSampler = {};
        linearSampler.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
        linearSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        linearSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        linearSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        linearSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        linearSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        linearSampler.MinLOD = 0.0f;
        linearSampler.MaxLOD = D3D12_FLOAT32_MAX;
        linearSampler.MipLODBias = 0;
        linearSampler.MaxAnisotropy = 1;
        linearSampler.ShaderRegister = 0;
        linearSampler.RegisterSpace = 0;
        linearSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        m_linearSampler.OnCreate(pDevice, "DX12/RenderImage.hlsl", pResourceViewHeaps, pStaticBufferPool, 1, 1, &linearSampler, outFormat);

        D3D12_STATIC_SAMPLER_DESC pointSampler = {};
        pointSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        pointSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        pointSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        pointSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        pointSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        pointSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        pointSampler.MinLOD = 0.0f;
        pointSampler.MaxLOD = D3D12_FLOAT32_MAX;
        pointSampler.MipLODBias = 0;
        pointSampler.MaxAnisotropy = 1;
        pointSampler.ShaderRegister = 0;
        pointSampler.RegisterSpace = 0;
        pointSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        m_pointSampler.OnCreate(pDevice, "DX12/RenderImage.hlsl", pResourceViewHeaps, pStaticBufferPool, 1, 1, &pointSampler, outFormat);
    }

    void ImageRenderer::OnDestroy()
    {
        m_linearSampler.OnDestroy();
        m_pointSampler.OnDestroy();
    }

    void ImageRenderer::UpdatePipelines(DXGI_FORMAT outFormat)
    {
        m_linearSampler.UpdatePipeline(outFormat);
        m_pointSampler.UpdatePipeline(outFormat);
    }

    void ImageRenderer::Draw(
        ID3D12GraphicsCommandList* pCommandList,
        D3D12_FILTER inputFilter,
        CAULDRON_DX12::CBV_SRV_UAV* pInputImage)
    {
        CAULDRON_DX12::UserMarker marker(pCommandList, "ImageRenderer");

        if (inputFilter == D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT)
            m_linearSampler.Draw(pCommandList, 1, pInputImage, 0);
        else
            m_pointSampler.Draw(pCommandList, 1, pInputImage, 0);
    }
}
