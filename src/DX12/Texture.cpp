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
#include "dxgi.h"
#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include "Misc.h"
#include "Helper.h"
#include "Texture.h"
#include "DxgiFormatHelper.h"

namespace CAULDRON_DX12
{
    //--------------------------------------------------------------------------------------
    // Constructor of the Texture class
    // initializes all members
    //--------------------------------------------------------------------------------------
    Texture::Texture() {}

    //--------------------------------------------------------------------------------------
    // Destructor of the Texture class
    //--------------------------------------------------------------------------------------
    Texture::~Texture() {}


    void Texture::OnDestroy()
    {
        if (m_pResource)
        {
            m_pResource->Release();
            m_pResource = nullptr;
        }
    }

    bool Texture::isDXT(DXGI_FORMAT format) const
    {
        return (format >= DXGI_FORMAT_BC1_TYPELESS) && (format <= DXGI_FORMAT_BC5_SNORM);
    }

    bool Texture::isCubemap() const
    {
        return m_header.arraySize == 6;
    }

    INT32 Texture::Init(Device* pDevice, const char* pDebugName, const CD3DX12_RESOURCE_DESC* pDesc, D3D12_RESOURCE_STATES initialState, const D3D12_CLEAR_VALUE* pClearValue)
    {
        HRESULT hr = pDevice->GetDevice()->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            pDesc,
            initialState,
            pClearValue,
            IID_PPV_ARGS(&m_pResource));
        assert(hr == S_OK);

        m_header.format = pDesc->Format;
        m_header.width = (UINT32)pDesc->Width;
        m_header.height = (UINT32)pDesc->Height;
        m_header.mipMapCount = pDesc->MipLevels;
        m_header.depth = (UINT32)pDesc->Depth();
        m_header.arraySize = (UINT32)pDesc->ArraySize();

        SetName(m_pResource, pDebugName);


        return hr;
    }

    INT32 Texture::InitRenderTarget(Device* pDevice, const char* pDebugName, const CD3DX12_RESOURCE_DESC* pDesc, D3D12_RESOURCE_STATES initialState, const FLOAT* clearColor)
    {
        // Performance tip: Tell the runtime at resource creation the desired clear value.
        D3D12_CLEAR_VALUE clearValue;
        clearValue.Format = pDesc->Format;
        clearValue.Color[0] = 0.0f;
        clearValue.Color[1] = 0.0f;
        clearValue.Color[2] = 0.0f;
        clearValue.Color[3] = 0.0f;

        bool isRenderTarget = pDesc->Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET ? 1 : 0;

        if (clearColor)
        {
            clearValue.Color[0] = clearColor[0];
            clearValue.Color[1] = clearColor[1];
            clearValue.Color[2] = clearColor[2];
            clearValue.Color[3] = clearColor[3];
        }

        Init(pDevice, pDebugName, pDesc, initialState, isRenderTarget ? &clearValue : nullptr);

        return 0;
    }

    bool Texture::InitBuffer(Device* pDevice, const char* pDebugName, const CD3DX12_RESOURCE_DESC* pDesc, uint32_t structureSize, D3D12_RESOURCE_STATES state)
    {
        assert(pDevice && pDesc);
        assert(pDesc->Dimension == D3D12_RESOURCE_DIMENSION_BUFFER && pDesc->Height == 1 && pDesc->MipLevels == 1);

        D3D12_RESOURCE_DESC desc = *pDesc;

        if (pDesc->Format != DXGI_FORMAT_UNKNOWN)
        {
            // Formatted buffer
            assert(structureSize == 0);
            m_structuredBufferStride = 0;
            m_header.format = pDesc->Format;
            m_header.width = (UINT32)pDesc->Width;

            // Fix up the D3D12_RESOURCE_DESC to be a typeless buffer.  The type/format will be associated with the UAV/SRV
            desc.Format = DXGI_FORMAT_UNKNOWN;
            desc.Width = GetPixelByteSize(m_header.format) * pDesc->Width;
        }
        else
        {
            // Structured buffer
            m_structuredBufferStride = structureSize;
            m_header.format = DXGI_FORMAT_UNKNOWN;
            m_header.width = (UINT32)pDesc->Width / m_structuredBufferStride;
        }

        m_header.height = 1;
        m_header.mipMapCount = 1;
        m_header.mipMapCount = pDesc->MipLevels;
        m_header.depth = (UINT32)pDesc->Depth();
        m_header.arraySize = (UINT32)pDesc->ArraySize();

        HRESULT hr = pDevice->GetDevice()->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &desc,
            state,
            nullptr,
            IID_PPV_ARGS(&m_pResource));

        SetName(m_pResource, pDebugName);

        return hr == S_OK;
    }

    bool Texture::InitCounter(Device* pDevice, const char* pDebugName, const CD3DX12_RESOURCE_DESC* pCounterDesc, uint32_t counterSize, D3D12_RESOURCE_STATES state)
    {
        return InitBuffer(pDevice, pDebugName, pCounterDesc, counterSize, state);
    }

    void Texture::CreateRTV(uint32_t index, RTV* pRV, D3D12_RENDER_TARGET_VIEW_DESC* pRtvDesc)
    {
        ID3D12Device* pDevice;
        m_pResource->GetDevice(__uuidof(*pDevice), reinterpret_cast<void**>(&pDevice));

        pDevice->CreateRenderTargetView(m_pResource, pRtvDesc, pRV->GetCPU(index));

        pDevice->Release();
    }

    void Texture::CreateSRV(uint32_t index, CBV_SRV_UAV* pRV, D3D12_SHADER_RESOURCE_VIEW_DESC* pSrvDesc)
    {
        ID3D12Device* pDevice;
        m_pResource->GetDevice(__uuidof(*pDevice), reinterpret_cast<void**>(&pDevice));

        pDevice->CreateShaderResourceView(m_pResource, pSrvDesc, pRV->GetCPU(index));

        pDevice->Release();
    }

    void Texture::CreateUAV(uint32_t index, Texture* pCounterTex, CBV_SRV_UAV* pRV, D3D12_UNORDERED_ACCESS_VIEW_DESC* pUavDesc)
    {
        ID3D12Device* pDevice;
        m_pResource->GetDevice(__uuidof(*pDevice), reinterpret_cast<void**>(&pDevice));

        pDevice->CreateUnorderedAccessView(m_pResource, pCounterTex ? pCounterTex->GetResource() : NULL, pUavDesc, pRV->GetCPU(index));

        pDevice->Release();
    }

    void Texture::CreateRTV(uint32_t index, RTV* pRV, int mipLevel, int arraySize, int firstArraySlice)
    {
        D3D12_RESOURCE_DESC texDesc = m_pResource->GetDesc();
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = texDesc.Format;
        if (texDesc.DepthOrArraySize == 1)
        {
            assert(arraySize == -1);
            assert(firstArraySlice == -1);
            if (texDesc.SampleDesc.Count == 1)
            {
                rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
                rtvDesc.Texture2D.MipSlice = (mipLevel == -1) ? 0 : mipLevel;
            }
            else
            {
                rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
                assert(mipLevel == -1);
            }
        }
        else
        {
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
            rtvDesc.Texture2DArray.ArraySize = arraySize;
            rtvDesc.Texture2DArray.FirstArraySlice = firstArraySlice;
            rtvDesc.Texture2DArray.MipSlice = (mipLevel == -1) ? 0 : mipLevel;
        }

        CreateRTV(index, pRV, &rtvDesc);
    }

    bool Texture::InitFromData(Device* pDevice, const char* pDebugName, UploadHeap& uploadHeap, const IMG_INFO& header, const void* data)
    {
        assert(!m_pResource);
        assert(header.arraySize == 1 && header.mipMapCount == 1);

        m_header = header;

        CD3DX12_RESOURCE_DESC RDescs;
        if (header.depth <= 1)
        {
            RDescs = CreateTextureCommitted(pDevice, pDebugName, false);
        }
        else
        {
            RDescs = CreateTexture3DCommitted(pDevice, pDebugName, false);
        }

        // Get mip footprints (if it is an array we reuse the mip footprints for all the elements of the array)
        //
        UINT64 m_uploadHeapSize;
        uint32_t num_rows = {};
        UINT64 row_sizes_in_bytes = {};
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT placedTexFootprint = {};
        pDevice->GetDevice()->GetCopyableFootprints(&RDescs, 0, 1, 0, &placedTexFootprint, &num_rows, &row_sizes_in_bytes, &m_uploadHeapSize);

        //compute pixel size
        //
        UINT32 bytePP = m_header.bitCount / 8;

        for (uint32_t a = 0; a < m_header.arraySize; a++)
        {
            // allocate memory for mip chain from upload heap
            //
            UINT8* pixels = uploadHeap.BeginSuballocate(SIZE_T(m_uploadHeapSize), D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

            placedTexFootprint.Offset += UINT64(pixels - uploadHeap.BasePtr());

            // copy all the mip slices into the offsets specified by the footprint structure
            //
            for (uint32_t y = 0; y < num_rows * placedTexFootprint.Footprint.Depth; y++)
            {
                memcpy(
                    pixels + (y * placedTexFootprint.Footprint.RowPitch),
                    (UINT8*)data + (y * header.width * bytePP),
                    row_sizes_in_bytes);
            }

            CD3DX12_TEXTURE_COPY_LOCATION Dst(m_pResource, 0);
            CD3DX12_TEXTURE_COPY_LOCATION Src(uploadHeap.GetResource(), placedTexFootprint);
            uploadHeap.AddCopy(Src, Dst);

            uploadHeap.EndSuballocate();
        }
        uploadHeap.AddBarrier(m_pResource);

        return true;
    }

    void Texture::CreateUAV(uint32_t index, CBV_SRV_UAV* pRV, int mipLevel)
    {
        D3D12_RESOURCE_DESC texDesc = m_pResource->GetDesc();

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        
        // TODO: add view overrides and use defaults below if there's none
        switch (texDesc.Format)
        {
        // Override TYPELESS resources to prevent device removed. 
        case DXGI_FORMAT_R32_TYPELESS: uavDesc.Format = DXGI_FORMAT_R32_UINT; break;
        case DXGI_FORMAT_R16_TYPELESS: uavDesc.Format = DXGI_FORMAT_R16_UINT; break;
        default:
            uavDesc.Format = texDesc.Format;
            break;
        }
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        uavDesc.Texture2D.MipSlice = (mipLevel == -1) ? 0 : mipLevel;

        CreateUAV(index, NULL, pRV, &uavDesc);
    }

    void Texture::CreateBufferUAV(uint32_t index, Texture* pCounterTex, CBV_SRV_UAV* pRV)
    {
        D3D12_RESOURCE_DESC resourceDesc = m_pResource->GetDesc();

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = m_header.format;
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        uavDesc.Buffer.FirstElement = 0;
        uavDesc.Buffer.NumElements = m_header.width;
        uavDesc.Buffer.StructureByteStride = m_structuredBufferStride;
        uavDesc.Buffer.CounterOffsetInBytes = 0;
        uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

        CreateUAV(index, pCounterTex, pRV, &uavDesc);
    }

    void Texture::CreateSRV(uint32_t index, CBV_SRV_UAV* pRV, int mipLevel, int arraySize, int firstArraySlice)
    {
        D3D12_RESOURCE_DESC resourceDesc = m_pResource->GetDesc();

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

        if (resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
        {
            srvDesc.Format = m_header.format;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srvDesc.Buffer.FirstElement = 0;
            srvDesc.Buffer.NumElements = m_header.width;
            srvDesc.Buffer.StructureByteStride = m_structuredBufferStride;
            srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        }
        else
        {
            // TODO: add view overrides and use defaults below if there's none
            switch (resourceDesc.Format)
            {
            // Override TYPELESS resources to prevent device removed.
            case DXGI_FORMAT_R32_TYPELESS: srvDesc.Format = DXGI_FORMAT_R32_FLOAT; break;
            case DXGI_FORMAT_R16_TYPELESS: srvDesc.Format = DXGI_FORMAT_R16_FLOAT; break;
            default:
                srvDesc.Format = m_pResource->GetDesc().Format;
                break;
            }

            if (resourceDesc.SampleDesc.Count == 1)
            {
                if (resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D || resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
                {
                    srvDesc.ViewDimension = resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D ? D3D12_SRV_DIMENSION_TEXTURE2D : D3D12_SRV_DIMENSION_TEXTURE3D;

                    srvDesc.Texture2D.MostDetailedMip = (mipLevel == -1) ? 0 : mipLevel;
                    srvDesc.Texture2D.MipLevels = (mipLevel == -1) ? m_header.mipMapCount : 1;

                    assert(arraySize == -1);
                    assert(firstArraySlice == -1);
                }
                else
                {
                    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;

                    srvDesc.Texture2DArray.MostDetailedMip = (mipLevel == -1) ? 0 : mipLevel;
                    srvDesc.Texture2DArray.MipLevels = (mipLevel == -1) ? m_header.mipMapCount : 1;

                    srvDesc.Texture2DArray.FirstArraySlice = (firstArraySlice == -1) ? 0 : firstArraySlice;
                    srvDesc.Texture2DArray.ArraySize = (arraySize == -1) ? resourceDesc.DepthOrArraySize : arraySize;
                }
            }
            else
            {
                if (resourceDesc.DepthOrArraySize == 1)
                {
                    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
                    assert(mipLevel == -1);
                    assert(arraySize == -1);
                    assert(firstArraySlice == -1);
                }
                else
                {
                    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
                    srvDesc.Texture2DMSArray.FirstArraySlice = (firstArraySlice == -1) ? 0 : firstArraySlice;
                    srvDesc.Texture2DMSArray.ArraySize = (arraySize == -1) ? resourceDesc.DepthOrArraySize : arraySize;
                    assert(mipLevel == -1);
                }
            }
        }

        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        CreateSRV(index, pRV, &srvDesc);
    }

    void Texture::CreateCubeSRV(uint32_t index, CBV_SRV_UAV* pRV)
    {
        D3D12_RESOURCE_DESC texDesc = m_pResource->GetDesc();
        D3D12_RESOURCE_DESC desc = m_pResource->GetDesc();
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = desc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCube.MostDetailedMip = 0;
        srvDesc.TextureCube.ResourceMinLODClamp = 0;
        srvDesc.TextureCube.MipLevels = m_header.mipMapCount;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        CreateSRV(index, pRV, &srvDesc);
    }

    void Texture::CreateDSV(uint32_t index, DSV* pRV, int arraySlice)
    {
        ID3D12Device* pDevice;
        m_pResource->GetDevice(__uuidof(*pDevice), reinterpret_cast<void**>(&pDevice));
        D3D12_RESOURCE_DESC texDesc = m_pResource->GetDesc();

        D3D12_DEPTH_STENCIL_VIEW_DESC DSViewDesc = {};
        DSViewDesc.Format = m_header.format == DXGI_FORMAT_D16_UNORM ? m_header.format : DXGI_FORMAT_D32_FLOAT;
        if (texDesc.SampleDesc.Count == 1)
        {
            if (texDesc.DepthOrArraySize == 1)
            {
                DSViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
                DSViewDesc.Texture2D.MipSlice = 0;
            }
            else
            {
                DSViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
                DSViewDesc.Texture2DArray.MipSlice = 0;
                DSViewDesc.Texture2DArray.FirstArraySlice = arraySlice;
                DSViewDesc.Texture2DArray.ArraySize = 1;// texDesc.DepthOrArraySize;
            }
        }
        else
        {
            DSViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
        }


        pDevice->CreateDepthStencilView(m_pResource, &DSViewDesc, pRV->GetCPU(index));

        pDevice->Release();
    }

    INT32 Texture::InitDepthStencil(Device* pDevice, const char* pDebugName, const CD3DX12_RESOURCE_DESC* pDesc)
    {
        // depth buffers need to be created as typeless! That way we can create different views out of them
        assert(pDesc->Format == DXGI_FORMAT_R32_TYPELESS);

        // Performance tip: Tell the runtime at resource creation the desired clear value.
        D3D12_CLEAR_VALUE clearValue;
        clearValue.Format = (pDesc->Format == DXGI_FORMAT_R32_TYPELESS) ? DXGI_FORMAT_D32_FLOAT : pDesc->Format;
        clearValue.DepthStencil.Depth = 1.0f;
        clearValue.DepthStencil.Stencil = 0;

        D3D12_RESOURCE_STATES states = D3D12_RESOURCE_STATE_COMMON;
        //if (pDesc->Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)
        //    states |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        states |= D3D12_RESOURCE_STATE_DEPTH_WRITE;

        pDevice->GetDevice()->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            pDesc,
            states,
            &clearValue,
            IID_PPV_ARGS(&m_pResource));

        m_header.format = pDesc->Format;
        m_header.width = (UINT32)pDesc->Width;
        m_header.height = (UINT32)pDesc->Height;
        m_header.mipMapCount = pDesc->MipLevels;
        m_header.depth = (UINT32)pDesc->Depth();
        m_header.arraySize = (UINT32)pDesc->ArraySize();

        SetName(m_pResource, pDebugName);

        return 0;
    }

    //--------------------------------------------------------------------------------------
    // create a comitted resource using m_header
    //--------------------------------------------------------------------------------------
    CD3DX12_RESOURCE_DESC  Texture::CreateTextureCommitted(Device* pDevice, const char* pDebugName, bool useSRGB, D3D12_RESOURCE_FLAGS resourceFlags)
    {
        m_header.format = SetFormatGamma((DXGI_FORMAT)m_header.format, useSRGB);

        CD3DX12_RESOURCE_DESC RDescs;
        RDescs = CD3DX12_RESOURCE_DESC::Tex2D((DXGI_FORMAT)m_header.format, m_header.width, m_header.height, m_header.arraySize, m_header.mipMapCount, 1, 0, resourceFlags);

        pDevice->GetDevice()->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &RDescs,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&m_pResource)
        );

        SetName(m_pResource, pDebugName);

        return RDescs;
    }

    CD3DX12_RESOURCE_DESC Texture::CreateTexture3DCommitted(Device* pDevice, const char* pDebugName, bool useSRGB, D3D12_RESOURCE_FLAGS resourceFlags)
    {
        m_header.format = SetFormatGamma((DXGI_FORMAT)m_header.format, useSRGB);

        CD3DX12_RESOURCE_DESC RDescs;
        RDescs = CD3DX12_RESOURCE_DESC::Tex3D((DXGI_FORMAT)m_header.format, m_header.width, m_header.height, m_header.depth, m_header.mipMapCount, resourceFlags);

        pDevice->GetDevice()->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &RDescs,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&m_pResource)
        );

        SetName(m_pResource, pDebugName);

        return RDescs;
    }

    void Texture::LoadAndUpload(Device* pDevice, UploadHeap* pUploadHeap, ImgLoader* pDds, ID3D12Resource* pRes, CD3DX12_RESOURCE_DESC* pDesc)
    {
        // Get mip footprints (if it is an array we reuse the mip footprints for all the elements of the array)
        //
        UINT64 m_uploadHeapSize;
        uint32_t num_rows[D3D12_REQ_MIP_LEVELS] = { 0 };
        UINT64 row_sizes_in_bytes[D3D12_REQ_MIP_LEVELS] = { 0 };
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT placedTexFootprint[D3D12_REQ_MIP_LEVELS];
        CD3DX12_RESOURCE_DESC RDescs = pDesc != nullptr ? *pDesc : CD3DX12_RESOURCE_DESC::Tex2D((DXGI_FORMAT)m_header.format, m_header.width, m_header.height, 1, m_header.mipMapCount);
        pDevice->GetDevice()->GetCopyableFootprints(&RDescs, 0, m_header.mipMapCount, 0, placedTexFootprint, num_rows, row_sizes_in_bytes, &m_uploadHeapSize);

        //compute pixel size
        //
        UINT32 bytePP = (UINT32)GetPixelByteSize((DXGI_FORMAT)m_header.format); // note that bytesPerPixel in BC formats is treated as bytesPerBlock 
        UINT32 pixelsPerBlock = 1;
        if ((m_header.format >= DXGI_FORMAT_BC1_TYPELESS) && (m_header.format <= DXGI_FORMAT_BC5_SNORM))
        {
            pixelsPerBlock = (4 * 4); // BC formats have 4*4 pixels per block
            pixelsPerBlock /= 4; // we need to divide by 4 because GetCopyableFootprints introduces a *2 stride divides the rows /4 
        }

        for (uint32_t a = 0; a < m_header.arraySize; a++)
        {
            // allocate memory for mip chain from upload heap
            //
            UINT8 *pixels = pUploadHeap->BeginSuballocate(SIZE_T(m_uploadHeapSize), D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

            // copy all the mip slices into the offsets specified by the footprint structure
            //
            for (uint32_t mip = 0; mip < m_header.mipMapCount; mip++)
            {
                // Copy all rows (from all slices (if 3d texture)) to upload heap pointer.
                pDds->CopyPixels(
                    pixels + placedTexFootprint[mip].Offset, // write location
                    placedTexFootprint[mip].Footprint.RowPitch, // bytes in a row (stride)
                    (placedTexFootprint[mip].Footprint.Width * bytePP) / pixelsPerBlock, // size of actual row of data in source image
                    num_rows[mip] * placedTexFootprint[mip].Footprint.Depth); // number of rows in source image.

                D3D12_PLACED_SUBRESOURCE_FOOTPRINT slice = placedTexFootprint[mip];
                slice.Offset += (pixels - pUploadHeap->BasePtr());

                CD3DX12_TEXTURE_COPY_LOCATION Dst(m_pResource, a * m_header.mipMapCount + mip);
                CD3DX12_TEXTURE_COPY_LOCATION Src(pUploadHeap->GetResource(), slice);
                pUploadHeap->AddCopy(Src, Dst);
            }

            pUploadHeap->EndSuballocate();
        }

        pUploadHeap->AddBarrier(m_pResource);
    }


    //--------------------------------------------------------------------------------------
    // entry function to initialize an image from a .DDS texture
    //--------------------------------------------------------------------------------------
    bool Texture::InitFromFile(Device* pDevice, UploadHeap* pUploadHeap, const char* pFilename, bool useSRGB, float cutOff, D3D12_RESOURCE_FLAGS resourceFlags)
    {
        assert(m_pResource == NULL);

        ImgLoader* img = CreateImageLoader(pFilename);
        bool result = img->Load(pFilename, cutOff, &m_header);
        if (result)
        {
            CD3DX12_RESOURCE_DESC RDescs;
            if (m_header.depth <= 1)
            {
                RDescs = CreateTextureCommitted(pDevice, pFilename, useSRGB, resourceFlags);
            }
            else
            {
                RDescs = CreateTexture3DCommitted(pDevice, pFilename, useSRGB, resourceFlags);
            }
            LoadAndUpload(pDevice, pUploadHeap, img, m_pResource, &RDescs);
        }

        delete(img);

        return result;
    }
}