#include "Core/Graphics/Model_D3D11.hpp"
#include "core/FileSystem.hpp"

#define IDX(x) (size_t)static_cast<uint8_t>(x)

namespace DirectX
{
    inline XMMATRIX XM_CALLCONV XMMatrixInverseTranspose(DirectX::FXMMATRIX M)
    {
        // 世界矩阵的逆的转置仅针对法向量，我们也不需要世界矩阵的平移分量
        // 而且不去掉的话，后续再乘上观察矩阵之类的就会产生错误的变换结果
        XMMATRIX A = M;
        A.r[3] = g_XMIdentityR3;
        return XMMatrixTranspose(XMMatrixInverse(NULL, A));
    }
}

namespace core::Graphics
{
    bool ModelSharedComponent_D3D11::createImage()
    {
        auto* device = m_device->GetD3D11Device();
        assert(device);

        HRESULT hr = S_OK;

        // default: purple & black tile image

        auto RGBA = [](uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_) -> uint32_t
        {
            return uint32_t(r_)
                | (uint32_t(g_) << 8)
                | (uint32_t(b_) << 16)
                | (uint32_t(a_) << 24);
        };
        uint32_t black = RGBA(0, 0, 0, 255);
        uint32_t purple = RGBA(255, 0, 255, 255);
        std::vector<uint32_t> pixels(64 * 64);
        uint32_t* ptr = pixels.data();
        for (int i = 0; i < 32; i += 1)
        {
            for (int j = 0; j < 32; j += 1)
            {
                *ptr = black;
                ptr++;
            }
            for (int j = 0; j < 32; j += 1)
            {
                *ptr = purple;
                ptr++;
            }
        }
        for (int i = 0; i < 32; i += 1)
        {
            for (int j = 0; j < 32; j += 1)
            {
                *ptr = purple;
                ptr++;
            }
            for (int j = 0; j < 32; j += 1)
            {
                *ptr = black;
                ptr++;
            }
        }

        // default: create

        D3D11_TEXTURE2D_DESC def_tex_def = {
            .Width = 64,
            .Height = 64,
            .MipLevels = 1,
            .ArraySize = 1,
            .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
            .SampleDesc = {
                .Count = 1,
                .Quality = 0,
            },
            .Usage = D3D11_USAGE_DEFAULT,
            .BindFlags = D3D11_BIND_SHADER_RESOURCE,
            .CPUAccessFlags = 0,
            .MiscFlags = 0,
        };
        D3D11_SUBRESOURCE_DATA def_dat_def = {
            .pSysMem = pixels.data(),
            .SysMemPitch = 64 * 4,
            .SysMemSlicePitch = 0,
        };
        Microsoft::WRL::ComPtr<ID3D11Texture2D> def_texture2d;
        hr = device->CreateTexture2D(&def_tex_def, &def_dat_def, &def_texture2d);
        if (FAILED(hr))
        {
            assert(false);
            return false;
        }
        D3D11_SHADER_RESOURCE_VIEW_DESC def_srv_def = {
            .Format = def_tex_def.Format,
            .ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
            .Texture2D = {
                .MostDetailedMip = 0,
                .MipLevels = def_tex_def.MipLevels,
            },
        };
        hr = device->CreateShaderResourceView(def_texture2d.Get(), &def_srv_def, &default_image);
        if (FAILED(hr))
        {
            assert(false);
            return false;
        }

        return true;
    }
    bool ModelSharedComponent_D3D11::createSampler()
    {
        auto* device = m_device->GetD3D11Device();
        assert(device);

        HRESULT hr = S_OK;

        // default: create

        D3D11_SAMPLER_DESC def_samp_def = {
            .Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR,
            .AddressU = D3D11_TEXTURE_ADDRESS_WRAP,
            .AddressV = D3D11_TEXTURE_ADDRESS_WRAP,
            .AddressW = D3D11_TEXTURE_ADDRESS_WRAP,
            .MipLODBias = D3D11_DEFAULT_MIP_LOD_BIAS,
            .MaxAnisotropy = D3D11_DEFAULT_MAX_ANISOTROPY,
            .ComparisonFunc = D3D11_COMPARISON_ALWAYS,
            .BorderColor = {},
            .MinLOD = 0.0f,
            .MaxLOD = D3D11_FLOAT32_MAX,
        };
        hr = device->CreateSamplerState(&def_samp_def, &default_sampler);
        if (FAILED(hr))
        {
            assert(false);
            return false;
        }

        return true;
    }
    bool ModelSharedComponent_D3D11::createConstantBuffer()
    {
        auto* device = m_device->GetD3D11Device();
        assert(device);

        HRESULT hr = S_OK;

        // built-in: view-proj matrix

        D3D11_BUFFER_DESC cbo_def = {
            .ByteWidth = sizeof(DirectX::XMFLOAT4X4),
            .Usage = D3D11_USAGE_DEFAULT,
            .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
            .CPUAccessFlags = 0,
            .MiscFlags = 0,
            .StructureByteStride = 0,
        };
        hr = device->CreateBuffer(&cbo_def, NULL, &cbo_mvp);
        if (FAILED(hr))
        {
            assert(false);
            return false;
        }

        // built-in: local-world matrix

        cbo_def.ByteWidth = 2 * sizeof(DirectX::XMFLOAT4X4);
        hr = device->CreateBuffer(&cbo_def, NULL, &cbo_mlw);
        if (FAILED(hr))
        {
            assert(false);
            return false;
        }

        // built-in: camera info

        cbo_def.ByteWidth = 2 * sizeof(DirectX::XMFLOAT4X4);
        hr = device->CreateBuffer(&cbo_def, NULL, &cbo_caminfo);
        if (FAILED(hr))
        {
            assert(false);
            return false;
        }

        // // built-in: alpha mask

        cbo_def.ByteWidth = 2 * sizeof(DirectX::XMFLOAT4);
        hr = device->CreateBuffer(&cbo_def, NULL, &cbo_alpha);
        if (FAILED(hr))
        {
            assert(false);
            return false;
        }

        // // built-in: light
        cbo_def.ByteWidth = 515 * sizeof(DirectX::XMFLOAT4); // 1 ambient + 3 sunshine + 255 pos + 255 color + 1 count
        hr = device->CreateBuffer(&cbo_def, NULL, &cbo_light);
        if (FAILED(hr))
        {
            assert(false);
            return false;
        }

        // // built-in: uv transform
        cbo_def.ByteWidth = 2 * sizeof(DirectX::XMFLOAT4);
        hr = device->CreateBuffer(&cbo_def, NULL, &cbo_uv);
        if (FAILED(hr))
        {
            assert(false);
            return false;
        }

        return true;
    }
    bool ModelSharedComponent_D3D11::createState()
    {
        auto* device = m_device->GetD3D11Device();
        assert(device);

        HRESULT hr = S_OK;

        //// RS \\\\

        // built-in: cull-none

        D3D11_RASTERIZER_DESC rs_def = {
            .FillMode = D3D11_FILL_SOLID,
            .CullMode = D3D11_CULL_NONE,
            .FrontCounterClockwise = FALSE,
            .DepthBias = D3D11_DEFAULT_DEPTH_BIAS,
            .DepthBiasClamp = D3D11_DEFAULT_DEPTH_BIAS_CLAMP,
            .SlopeScaledDepthBias = D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
            .DepthClipEnable = TRUE,
            .ScissorEnable = TRUE,
            .MultisampleEnable = FALSE,
            .AntialiasedLineEnable = FALSE,
        };
        hr = device->CreateRasterizerState(&rs_def, &state_rs_cull_none);
        if (FAILED(hr))
        {
            assert(false);
            return false;
        }

        // built-in: cull-back

        rs_def.CullMode = D3D11_CULL_BACK;
        rs_def.FrontCounterClockwise = TRUE;
        hr = device->CreateRasterizerState(&rs_def, &state_rs_cull_back);
        if (FAILED(hr))
        {
            assert(false);
            return false;
        }

        // built-in: cull-front

        rs_def.CullMode = D3D11_CULL_FRONT;
        rs_def.FrontCounterClockwise = TRUE;
        hr = device->CreateRasterizerState(&rs_def, &state_rs_cull_front);
        if (FAILED(hr))
        {
            assert(false);
            return false;
        }

        //// DS \\\\

        // built-in: depth-test

        D3D11_DEPTH_STENCIL_DESC ds_def = {
            .DepthEnable = TRUE,
            .DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL,
            .DepthFunc = D3D11_COMPARISON_LESS_EQUAL,
            .StencilEnable = FALSE,
            .StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK,
            .StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK,
            .FrontFace = {
                .StencilFailOp = D3D11_STENCIL_OP_KEEP,
                .StencilDepthFailOp = D3D11_STENCIL_OP_KEEP,
                .StencilPassOp = D3D11_STENCIL_OP_KEEP,
                .StencilFunc = D3D11_COMPARISON_ALWAYS,
            },
            .BackFace = {
                .StencilFailOp = D3D11_STENCIL_OP_KEEP,
                .StencilDepthFailOp = D3D11_STENCIL_OP_KEEP,
                .StencilPassOp = D3D11_STENCIL_OP_KEEP,
                .StencilFunc = D3D11_COMPARISON_ALWAYS,
            },
        };
        hr = device->CreateDepthStencilState(&ds_def, &state_ds);
        if (FAILED(hr))
        {
            assert(false);
            return false;
        }

        // built-in: depth-test less only

        ds_def.DepthFunc = D3D11_COMPARISON_LESS;

        hr = device->CreateDepthStencilState(&ds_def, &state_ds_dl);
        if (FAILED(hr))
        {
            assert(false);
            return false;
        }

        // built-in: depth-test less-equal, no write

        ds_def.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        ds_def.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;

        hr = device->CreateDepthStencilState(&ds_def, &state_ds_no_write);
        if (FAILED(hr))
        {
            assert(false);
            return false;
        }

        // built-in: depth-test disable

        ds_def.DepthEnable = FALSE;
        ds_def.DepthFunc = D3D11_COMPARISON_ALWAYS;

        hr = device->CreateDepthStencilState(&ds_def, &state_ds_disable);
        if (FAILED(hr))
        {
            assert(false);
            return false;
        }

        //// BLEND \\\\

        // built-in: disable

        D3D11_RENDER_TARGET_BLEND_DESC rt_blend_def = {
            .BlendEnable = FALSE,
            .SrcBlend = D3D11_BLEND_ONE,
            .DestBlend = D3D11_BLEND_INV_SRC_ALPHA,
            .BlendOp = D3D11_BLEND_OP_ADD,
            .SrcBlendAlpha = D3D11_BLEND_ONE,
            .DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA,
            .BlendOpAlpha = D3D11_BLEND_OP_ADD,
            .RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL,
        };
        D3D11_BLEND_DESC blend_def = {
            .AlphaToCoverageEnable = FALSE,
            .IndependentBlendEnable = FALSE,
            .RenderTarget = {},
        };
        for (auto& rt_blend : blend_def.RenderTarget)
        {
            rt_blend = rt_blend_def;
        }
        hr = device->CreateBlendState(&blend_def, &state_blend);
        if (FAILED(hr))
        {
            assert(false);
            return false;
        }

        // built-in: alpha-blend

        rt_blend_def.BlendEnable = TRUE;
        rt_blend_def.SrcBlend = D3D11_BLEND_SRC_ALPHA;
        rt_blend_def.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        rt_blend_def.BlendOp = D3D11_BLEND_OP_ADD;
        rt_blend_def.SrcBlendAlpha = D3D11_BLEND_ONE;
        rt_blend_def.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        rt_blend_def.BlendOpAlpha = D3D11_BLEND_OP_ADD;
        for (auto& rt_blend : blend_def.RenderTarget)
        {
            rt_blend = rt_blend_def;
        }
        hr = device->CreateBlendState(&blend_def, &state_blend_alpha);
        if (FAILED(hr))
        {
            assert(false);
            return false;
        }

        // built-in: add-blend

        rt_blend_def.BlendEnable = TRUE;
        rt_blend_def.SrcBlend = D3D11_BLEND_SRC_ALPHA;
        rt_blend_def.DestBlend = D3D11_BLEND_ONE;
        rt_blend_def.BlendOp = D3D11_BLEND_OP_ADD;
        rt_blend_def.SrcBlendAlpha = D3D11_BLEND_ONE;
        rt_blend_def.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        rt_blend_def.BlendOpAlpha = D3D11_BLEND_OP_ADD;
        for (auto& rt_blend : blend_def.RenderTarget)
        {
            rt_blend = rt_blend_def;
        }
        hr = device->CreateBlendState(&blend_def, &state_blend_add);
        if (FAILED(hr))
        {
            assert(false);
            return false;
        }

        // built-in: sub-blend

        rt_blend_def.BlendEnable = TRUE;
        rt_blend_def.SrcBlend = D3D11_BLEND_SRC_ALPHA;
        rt_blend_def.DestBlend = D3D11_BLEND_ONE;
        rt_blend_def.BlendOp = D3D11_BLEND_OP_SUBTRACT;
        rt_blend_def.SrcBlendAlpha = D3D11_BLEND_ONE;
        rt_blend_def.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        rt_blend_def.BlendOpAlpha = D3D11_BLEND_OP_ADD;
        for (auto& rt_blend : blend_def.RenderTarget)
        {
            rt_blend = rt_blend_def;
        }
        hr = device->CreateBlendState(&blend_def, &state_blend_sub);
        if (FAILED(hr))
        {
            assert(false);
            return false;
        }

        // built-in: revsub-blend

        rt_blend_def.BlendEnable = TRUE;
        rt_blend_def.SrcBlend = D3D11_BLEND_SRC_ALPHA;
        rt_blend_def.DestBlend = D3D11_BLEND_ONE;
        rt_blend_def.BlendOp = D3D11_BLEND_OP_REV_SUBTRACT;
        rt_blend_def.SrcBlendAlpha = D3D11_BLEND_ONE;
        rt_blend_def.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        rt_blend_def.BlendOpAlpha = D3D11_BLEND_OP_ADD;
        for (auto& rt_blend : blend_def.RenderTarget)
        {
            rt_blend = rt_blend_def;
        }
        hr = device->CreateBlendState(&blend_def, &state_blend_revsub);
        if (FAILED(hr))
        {
            assert(false);
            return false;
        }

        // built-in: mul-blend

        rt_blend_def.BlendEnable = TRUE;
        rt_blend_def.SrcBlend = D3D11_BLEND_DEST_COLOR;
        rt_blend_def.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        rt_blend_def.BlendOp = D3D11_BLEND_OP_ADD;
        rt_blend_def.SrcBlendAlpha = D3D11_BLEND_ONE;
        rt_blend_def.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        rt_blend_def.BlendOpAlpha = D3D11_BLEND_OP_ADD;
        for (auto& rt_blend : blend_def.RenderTarget)
        {
            rt_blend = rt_blend_def;
        }
        hr = device->CreateBlendState(&blend_def, &state_blend_mul);
        if (FAILED(hr))
        {
            assert(false);
            return false;
        }

        // built-in: screen-blend

        rt_blend_def.BlendEnable = TRUE;
        rt_blend_def.SrcBlend = D3D11_BLEND_SRC_ALPHA;
        rt_blend_def.DestBlend = D3D11_BLEND_INV_SRC_COLOR;
        rt_blend_def.BlendOp = D3D11_BLEND_OP_ADD;
        rt_blend_def.SrcBlendAlpha = D3D11_BLEND_ONE;
        rt_blend_def.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        rt_blend_def.BlendOpAlpha = D3D11_BLEND_OP_ADD;
        for (auto& rt_blend : blend_def.RenderTarget)
        {
            rt_blend = rt_blend_def;
        }
        hr = device->CreateBlendState(&blend_def, &state_blend_screen);
        if (FAILED(hr))
        {
            assert(false);
            return false;
        }

        // built-in: min-blend

        rt_blend_def.BlendEnable = TRUE;
        rt_blend_def.SrcBlend = D3D11_BLEND_ONE;
        rt_blend_def.DestBlend = D3D11_BLEND_ONE;
        rt_blend_def.BlendOp = D3D11_BLEND_OP_MIN;
        rt_blend_def.SrcBlendAlpha = D3D11_BLEND_ONE;
        rt_blend_def.DestBlendAlpha = D3D11_BLEND_ONE;
        rt_blend_def.BlendOpAlpha = D3D11_BLEND_OP_MIN;
        for (auto& rt_blend : blend_def.RenderTarget)
        {
            rt_blend = rt_blend_def;
        }
        hr = device->CreateBlendState(&blend_def, &state_blend_min);
        if (FAILED(hr))
        {
            assert(false);
            return false;
        }

        // built-in: max-blend

        rt_blend_def.BlendEnable = TRUE;
        rt_blend_def.SrcBlend = D3D11_BLEND_ONE;
        rt_blend_def.DestBlend = D3D11_BLEND_ONE;
        rt_blend_def.BlendOp = D3D11_BLEND_OP_MAX;
        rt_blend_def.SrcBlendAlpha = D3D11_BLEND_ONE;
        rt_blend_def.DestBlendAlpha = D3D11_BLEND_ONE;
        rt_blend_def.BlendOpAlpha = D3D11_BLEND_OP_MAX;
        for (auto& rt_blend : blend_def.RenderTarget)
        {
            rt_blend = rt_blend_def;
        }
        hr = device->CreateBlendState(&blend_def, &state_blend_max);
        if (FAILED(hr))
        {
            assert(false);
            return false;
        }

        // built-in: inv-blend

        rt_blend_def.BlendEnable = TRUE;
        rt_blend_def.SrcBlend = D3D11_BLEND_INV_DEST_COLOR;
        rt_blend_def.DestBlend = D3D11_BLEND_INV_SRC_COLOR;
        rt_blend_def.BlendOp = D3D11_BLEND_OP_ADD;
        rt_blend_def.SrcBlendAlpha = D3D11_BLEND_ZERO;
        rt_blend_def.DestBlendAlpha = D3D11_BLEND_ONE;
        rt_blend_def.BlendOpAlpha = D3D11_BLEND_OP_ADD;
        for (auto& rt_blend : blend_def.RenderTarget)
        {
            rt_blend = rt_blend_def;
        }
        hr = device->CreateBlendState(&blend_def, &state_blend_inv);
        if (FAILED(hr))
        {
            assert(false);
            return false;
        }

        // built-in: one-blend (overwrite)

        rt_blend_def.BlendEnable = TRUE;
        rt_blend_def.SrcBlend = D3D11_BLEND_ONE;
        rt_blend_def.DestBlend = D3D11_BLEND_ZERO;
        rt_blend_def.BlendOp = D3D11_BLEND_OP_ADD;
        rt_blend_def.SrcBlendAlpha = D3D11_BLEND_ONE;
        rt_blend_def.DestBlendAlpha = D3D11_BLEND_ZERO;
        rt_blend_def.BlendOpAlpha = D3D11_BLEND_OP_ADD;
        for (auto& rt_blend : blend_def.RenderTarget)
        {
            rt_blend = rt_blend_def;
        }
        hr = device->CreateBlendState(&blend_def, &state_blend_one);
        if (FAILED(hr))
        {
            assert(false);
            return false;
        }

        return true;
    }

    bool ModelSharedComponent_D3D11::createResources()
    {
        // load image to shader resource

        if (!createImage()) return false;

        // create sampler state

        if (!createSampler()) return false;

        // create shader and input layout

        if (!createShader()) return false;

        // create constant buffer

        if (!createConstantBuffer()) return false;

        // create state

        if (!createState()) return false;

        return true;
    }
    void ModelSharedComponent_D3D11::onDeviceCreate()
    {
        createResources();
    }
    void ModelSharedComponent_D3D11::onDeviceDestroy()
    {
        default_image.Reset();
        default_sampler.Reset();

        input_layout.Reset();
        input_layout_vc.Reset();
        shader_vertex.Reset();
        shader_vertex_vc.Reset();
        for (auto& v : shader_pixel) v.Reset();
        for (auto& v : shader_pixel_alpha) v.Reset();
        for (auto& v : shader_pixel_nt) v.Reset();
        for (auto& v : shader_pixel_alpha_nt) v.Reset();
        for (auto& v : shader_pixel_vc) v.Reset();
        for (auto& v : shader_pixel_alpha_vc) v.Reset();
        for (auto& v : shader_pixel_nt_vc) v.Reset();
        for (auto& v : shader_pixel_alpha_nt_vc) v.Reset();

        state_rs_cull_none.Reset();
        state_rs_cull_back.Reset();
        state_rs_cull_front.Reset();
        state_ds_disable.Reset();
        state_ds.Reset();
        state_ds_no_write.Reset();
        state_ds_dl.Reset();
        state_blend.Reset();
        state_blend_alpha.Reset();
        state_blend_add.Reset();
        state_blend_sub.Reset();
        state_blend_revsub.Reset();
        state_blend_mul.Reset();
        state_blend_screen.Reset();
        state_blend_min.Reset();
        state_blend_max.Reset();
        state_blend_inv.Reset();
        state_blend_one.Reset();

        cbo_mvp.Reset();
        cbo_mlw.Reset();
        cbo_caminfo.Reset();
        cbo_alpha.Reset();
        cbo_light.Reset();
        cbo_uv.Reset();
    }

    ModelSharedComponent_D3D11::ModelSharedComponent_D3D11(Direct3D11::Device* p_device)
        : m_device(p_device)
    {
        if (!createResources())
            throw std::runtime_error("ModelSharedComponent_D3D11::ModelSharedComponent_D3D11");
        m_device->addEventListener(this);
    }
    ModelSharedComponent_D3D11::~ModelSharedComponent_D3D11()
    {
        m_device->removeEventListener(this);
    }
}

namespace core::Graphics
{
    static void map_sampler_to_d3d11(tinygltf::Sampler& samp, D3D11_SAMPLER_DESC& desc)
    {
    #define MAKE_FILTER(MIN, MAG_MIP) ((MAG_MIP << 16) | (MIN))
        switch (MAKE_FILTER(samp.minFilter, samp.magFilter))
        {
        default:
            desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            break;
        case MAKE_FILTER(TINYGLTF_TEXTURE_FILTER_NEAREST, TINYGLTF_TEXTURE_FILTER_NEAREST):
            desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
            desc.MinLOD = 0.0f;
            desc.MaxLOD = 0.0f;
            break;
        case MAKE_FILTER(TINYGLTF_TEXTURE_FILTER_NEAREST, TINYGLTF_TEXTURE_FILTER_LINEAR):
            desc.Filter = D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
            desc.MinLOD = 0.0f;
            desc.MaxLOD = 0.0f;
            break;
        case MAKE_FILTER(TINYGLTF_TEXTURE_FILTER_NEAREST, TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST):
            desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
            break;
        case MAKE_FILTER(TINYGLTF_TEXTURE_FILTER_NEAREST, TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST):
            desc.Filter = D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
            break;
        case MAKE_FILTER(TINYGLTF_TEXTURE_FILTER_NEAREST, TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR):
            desc.Filter = D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR;
            break;
        case MAKE_FILTER(TINYGLTF_TEXTURE_FILTER_NEAREST, TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR):
            desc.Filter = D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR;
            break;
        case MAKE_FILTER(TINYGLTF_TEXTURE_FILTER_LINEAR, TINYGLTF_TEXTURE_FILTER_NEAREST):
            desc.Filter = D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT;
            desc.MinLOD = 0.0f;
            desc.MaxLOD = 0.0f;
            break;
        case MAKE_FILTER(TINYGLTF_TEXTURE_FILTER_LINEAR, TINYGLTF_TEXTURE_FILTER_LINEAR):
            desc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
            desc.MinLOD = 0.0f;
            desc.MaxLOD = 0.0f;
            break;
        case MAKE_FILTER(TINYGLTF_TEXTURE_FILTER_LINEAR, TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST):
            desc.Filter = D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT;
            break;
        case MAKE_FILTER(TINYGLTF_TEXTURE_FILTER_LINEAR, TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST):
            desc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
            break;
        case MAKE_FILTER(TINYGLTF_TEXTURE_FILTER_LINEAR, TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR):
            desc.Filter = D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
            break;
        case MAKE_FILTER(TINYGLTF_TEXTURE_FILTER_LINEAR, TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR):
            desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            break;
        }
    #undef MAKE_FILTER
        switch (samp.wrapS)
        {
        default:
            desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
            break;
        case TINYGLTF_TEXTURE_WRAP_REPEAT:
            desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
            break;
        case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
            desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
            break;
        case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
            desc.AddressU = D3D11_TEXTURE_ADDRESS_MIRROR;
            break;
        }
        switch (samp.wrapT)
        {
        default:
        case -1:
            desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
            break;
        case TINYGLTF_TEXTURE_WRAP_REPEAT:
            desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
            break;
        case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
            desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
            break;
        case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
            desc.AddressV = D3D11_TEXTURE_ADDRESS_MIRROR;
            break;
        }
    }
    static void map_primitive_topology_to_d3d11(tinygltf::Primitive& prim, D3D11_PRIMITIVE_TOPOLOGY& topo)
    {
        switch (prim.mode)
        {
        default:
            topo = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            break;
        case TINYGLTF_MODE_POINTS:
            topo = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
            break;
        case TINYGLTF_MODE_LINE:
            topo = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
            break;
        case TINYGLTF_MODE_LINE_LOOP:
            assert(false);
            break;
        case TINYGLTF_MODE_LINE_STRIP:
            topo = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
            break;
        case TINYGLTF_MODE_TRIANGLES:
            topo = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            break;
        case TINYGLTF_MODE_TRIANGLE_STRIP:
            topo = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
            break;
        case TINYGLTF_MODE_TRIANGLE_FAN:
            assert(false);
            break;
        }
    }
    static DirectX::XMMATRIX XM_CALLCONV get_local_transfrom_from_node(tinygltf::Node& node)
    {
        if (!node.matrix.empty())
        {
        #pragma warning(disable:4244)
            // [Potential Overflow]
            DirectX::XMFLOAT4X4 mM(
                node.matrix[0], node.matrix[1], node.matrix[2], node.matrix[3],
                node.matrix[4], node.matrix[5], node.matrix[6], node.matrix[7],
                node.matrix[8], node.matrix[9], node.matrix[10], node.matrix[11],
                node.matrix[12], node.matrix[13], node.matrix[14], node.matrix[15]);
        #pragma warning(default:4244)
            return DirectX::XMLoadFloat4x4(&mM);
        }
        else
        {
            DirectX::XMMATRIX mS = DirectX::XMMatrixIdentity();
            DirectX::XMMATRIX mR = DirectX::XMMatrixIdentity();
            DirectX::XMMATRIX mT = DirectX::XMMatrixIdentity();
            if (!node.scale.empty())
            {
            #pragma warning(disable:4244)
                // [Potential Overflow]
                mS = DirectX::XMMatrixScaling(node.scale[0], node.scale[1], node.scale[2]);
            #pragma warning(default:4244)
            }
            if (!node.rotation.empty())
            {
            #pragma warning(disable:4244)
                // [Potential Overflow]
                DirectX::XMFLOAT4 quat(node.rotation[0], node.rotation[1], node.rotation[2], node.rotation[3]);
            #pragma warning(default:4244)
                mR = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&quat));
            }
            if (!node.translation.empty())
            {
            #pragma warning(disable:4244)
                // [Potential Overflow]
                mT = DirectX::XMMatrixTranslation(node.translation[0], node.translation[1], node.translation[2]);
            #pragma warning(default:4244)
            }
            return DirectX::XMMatrixMultiply(DirectX::XMMatrixMultiply(mS, mR), mT);
        }
    }
    static bool getBufferFromAccessor(
        tinygltf::Model const& model,
        tinygltf::Accessor const& accessor,
        uint8_t*& output,
        size_t& total_size_in_bytes,
        std::vector<uint8_t>& intermediate_buffer
    ) {
        // buffer view
        if (accessor.bufferView < 0 || accessor.bufferView >= model.bufferViews.size()) {
            spdlog::error(
                "[core] gltf 2.0 loader -- accessor (index = {}) buffer view index out of bound (value = {})",
                &accessor - model.accessors.data(),
                accessor.bufferView
            );
        }
        const auto& buffer_view = model.bufferViews[accessor.bufferView];
        // buffer
        if (buffer_view.buffer < 0 || buffer_view.buffer >= model.buffers.size()) {
            spdlog::error(
                "[core] gltf 2.0 loader -- buffer view (index = {}) buffer index out of bound (value = {})",
                &buffer_view - model.bufferViews.data(),
                buffer_view.buffer
            );
        }
        const auto& buffer = model.buffers[buffer_view.buffer];
        // total size
        if (tinygltf::GetComponentSizeInBytes(accessor.componentType) < 0) {
            spdlog::error(
                "[core] gltf 2.0 loader -- unknown accessor (index = {}) component type (value = {})",
                &accessor - model.accessors.data(),
                accessor.componentType
            );
            return false;
        }
        if (tinygltf::GetNumComponentsInType(accessor.type) < 0) {
            spdlog::error(
                "[core] gltf 2.0 loader -- unknown accessor (index = {}) type (value = {})",
                &accessor - model.accessors.data(),
                accessor.type
            );
            return false;
        }
        total_size_in_bytes = static_cast<size_t>(tinygltf::GetComponentSizeInBytes(accessor.componentType))
            * static_cast<size_t>(tinygltf::GetNumComponentsInType(accessor.type))
            * accessor.count;
        // no stride
        if (buffer_view.byteStride == 0) {
            output = const_cast<uint8_t*>(buffer.data.data()) + buffer_view.byteOffset + accessor.byteOffset;
            return true;
        }
        // prepare intermediate buffer
        intermediate_buffer.resize(total_size_in_bytes);
        // copy data
        const auto size = static_cast<size_t>(tinygltf::GetComponentSizeInBytes(accessor.componentType))
            * static_cast<size_t>(tinygltf::GetNumComponentsInType(accessor.type));
        auto source = buffer.data.data() + buffer_view.byteOffset + accessor.byteOffset;
        auto target = intermediate_buffer.data();
        for (size_t i = 0; i < accessor.count; i += 1) {
            std::memcpy(target, source, size);
            source += buffer_view.byteStride;
            target += size;
        }
        // using intermediate buffer
        output = intermediate_buffer.data();
        return true;
    }

    void Model_D3D11::setAmbient(Vector3F const& color, float brightness)
    {
        sunshine.ambient = DirectX::XMFLOAT4(color.x, color.y, color.z, brightness);
    }
    void Model_D3D11::setDirectionalLight(Vector3F const& direction, Vector3F const& color, float brightness)
    {
        sunshine.dir = DirectX::XMFLOAT4(direction.x, direction.y, direction.z, 0.0f);
        sunshine.color = DirectX::XMFLOAT4(color.x, color.y, color.z, brightness);
    }
    void Model_D3D11::addPointLight(Vector3F const& pos, Vector3F const& color, float brightness, float range)
    {
        if (point_lights.size() >= MAX_POINT_LIGHTS)
        {
            spdlog::warn("[core] Model_D3D11::addPointLight: maximum of {} point lights already reached", MAX_POINT_LIGHTS);
            return;
        }
        PointLight pl{};
        pl.pos = pos;
        pl.range = range;
        pl.color = color;
        pl.brightness = brightness;
        point_lights.push_back(pl);
    }
    void Model_D3D11::clearPointLights()
    {
        point_lights.clear();
    }
    std::vector<PointLight> Model_D3D11::takeEmbeddedLights()
    {
        return std::move(embedded_lights_);
    }
    void Model_D3D11::setScaling(Vector3F const& scale)
    {
        t_scale_ = DirectX::XMMatrixScaling(scale.x, scale.y, scale.z);
    }
    void Model_D3D11::setPosition(Vector3F const& pos)
    {
        t_trans_ = DirectX::XMMatrixTranslation(pos.x, pos.y, pos.z);
    }
    void Model_D3D11::setRotationRollPitchYaw(float roll, float pitch, float yaw)
    {
        t_mbrot_ = DirectX::XMMatrixRotationRollPitchYaw(pitch, yaw, roll);
    }
    void Model_D3D11::setRotationQuaternion(Vector4F const& quat)
    {
        DirectX::XMFLOAT4 const xq(quat.x, quat.y, quat.z, quat.w);
        t_mbrot_ = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&xq));
    }

    bool Model_D3D11::createImage(tinygltf::Model& model)
    {
        auto* device = m_device->GetD3D11Device();
        auto* context = m_device->GetD3D11DeviceContext();

        HRESULT hr = S_OK;

        // gltf: create

        image.resize(model.images.size());
        for (size_t idx = 0; idx < model.images.size(); idx += 1)
        {
            tinygltf::Image& img = model.images[idx];

            if (img.width <= 0 || img.height <= 0)
            {
                image[idx] = shared_->default_image; // 兄啊，你这纹理好怪哦
                spdlog::error("[core] 加载纹理 '{}' 失败", img.name);
                continue;
            }

            bool mipmap = true;
            D3D11_TEXTURE2D_DESC tex_def = {
                .Width = (UINT)img.width,
                .Height = (UINT)img.height,
                .MipLevels = mipmap ? (UINT)0 : (UINT)1,
                .ArraySize = 1,
                .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
                .SampleDesc = {
                    .Count = 1,
                    .Quality = 0,
                },
                .Usage = D3D11_USAGE_DEFAULT,
                .BindFlags = D3D11_BIND_SHADER_RESOURCE | (mipmap ? D3D11_BIND_RENDER_TARGET : (UINT)0),
                .CPUAccessFlags = 0,
                .MiscFlags = mipmap ? D3D11_RESOURCE_MISC_GENERATE_MIPS : (UINT)0,
            };
            D3D11_SUBRESOURCE_DATA dat_def = {
                .pSysMem = img.image.data(),
                .SysMemPitch = (UINT)(img.width * img.component * img.bits) / 8,
                .SysMemSlicePitch = (UINT)img.image.size(),
            };
            Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2d;
            hr = device->CreateTexture2D(&tex_def, mipmap ? NULL : &dat_def, &texture2d);
            if (FAILED(hr))
            {
                assert(false);
                return false;
            }
            D3D11_SHADER_RESOURCE_VIEW_DESC srv_def = {
                .Format = tex_def.Format,
                .ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
                .Texture2D = {
                    .MostDetailedMip = 0,
                    .MipLevels = mipmap ? UINT(-1) : tex_def.MipLevels,
                },
            };
            hr = device->CreateShaderResourceView(texture2d.Get(), &srv_def, &image[idx]);
            if (FAILED(hr))
            {
                assert(false);
                return false;
            }
            if (mipmap)
            {
                context->UpdateSubresource(texture2d.Get(), 0, NULL, dat_def.pSysMem, dat_def.SysMemPitch, dat_def.SysMemSlicePitch);
                context->GenerateMips(image[idx].Get());
            }
        }

        return true;
    }
    bool Model_D3D11::createSampler(tinygltf::Model& model)
    {
        auto* device = m_device->GetD3D11Device();

        HRESULT hr = S_OK;

        // gltf: create

        sampler.resize(model.samplers.size());
        for (size_t idx = 0; idx < model.samplers.size(); idx += 1)
        {
            tinygltf::Sampler& samp = model.samplers[idx];
            D3D11_SAMPLER_DESC samp_def = {
                .Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR,
                .AddressU = D3D11_TEXTURE_ADDRESS_WRAP,
                .AddressV = D3D11_TEXTURE_ADDRESS_WRAP,
                .AddressW = D3D11_TEXTURE_ADDRESS_WRAP,
                .MipLODBias = D3D11_DEFAULT_MIP_LOD_BIAS,
                .MaxAnisotropy = D3D11_DEFAULT_MAX_ANISOTROPY,
                .ComparisonFunc = D3D11_COMPARISON_ALWAYS,
                .BorderColor = {},
                .MinLOD = 0.0f,
                .MaxLOD = D3D11_FLOAT32_MAX,
            };
            map_sampler_to_d3d11(samp, samp_def);
            samp_def.Filter = D3D11_FILTER_ANISOTROPIC; // TODO: better?
            hr = device->CreateSamplerState(&samp_def, &sampler[idx]);
            if (FAILED(hr))
            {
                assert(false);
                return false;
            }
        }

        return true;
    }
    bool Model_D3D11::processNode(tinygltf::Model& model, tinygltf::Node& node)
    {
        auto* device = m_device->GetD3D11Device();

        HRESULT hr = S_OK;

        DirectX::XMMATRIX mTRS = get_local_transfrom_from_node(node);

        if (node.mesh >= 0)
        {
            tinygltf::Mesh& mesh = model.meshes[node.mesh];
            for (tinygltf::Primitive& prim : mesh.primitives)
            {
                ModelBlock mblock;
                DirectX::XMMATRIX mTRSw = mTRS;
                for (auto it = mTRS_stack.crbegin(); it != mTRS_stack.crend(); it++)
                {
                    mTRSw = DirectX::XMMatrixMultiply(mTRSw, *it);
                }
                mTRSw = DirectX::XMMatrixMultiply(mTRSw, DirectX::XMMatrixScaling(1.0f, 1.0f, -1.0f)); // to left-hand
                DirectX::XMStoreFloat4x4(&mblock.local_matrix, mTRSw);
                DirectX::XMStoreFloat4x4(&mblock.local_matrix_normal, DirectX::XMMatrixInverseTranspose(mTRSw)); // face normal
                if (prim.attributes.contains("POSITION"))
                {
                    tinygltf::Accessor& accessor = model.accessors[prim.attributes["POSITION"]];
                    uint8_t* buffer_ptr{};
                    size_t total_size_in_bytes{};
                    std::vector<uint8_t> intermediate_buffer;
                    if (!getBufferFromAccessor(model, accessor, buffer_ptr, total_size_in_bytes, intermediate_buffer))
                        return false;

                    D3D11_BUFFER_DESC vbo_def = {
                        .ByteWidth = static_cast<UINT>(total_size_in_bytes),
                        .Usage = D3D11_USAGE_DEFAULT,
                        .BindFlags = D3D11_BIND_VERTEX_BUFFER,
                        .CPUAccessFlags = 0,
                        .MiscFlags = 0,
                        .StructureByteStride = 0,
                    };
                    D3D11_SUBRESOURCE_DATA dat_def = {
                        .pSysMem = buffer_ptr,
                        .SysMemPitch = 0,
                        .SysMemSlicePitch = 0,
                    };
                    hr = device->CreateBuffer(&vbo_def, &dat_def, &mblock.vertex_buffer);
                    if (FAILED(hr))
                    {
                        assert(false);
                        return false;
                    }

                    mblock.draw_count = (UINT)accessor.count;
                }
                if (prim.attributes.contains("NORMAL"))
                {
                    tinygltf::Accessor& accessor = model.accessors[prim.attributes["NORMAL"]];
                    uint8_t* buffer_ptr{};
                    size_t total_size_in_bytes{};
                    std::vector<uint8_t> intermediate_buffer;
                    if (!getBufferFromAccessor(model, accessor, buffer_ptr, total_size_in_bytes, intermediate_buffer))
                        return false;

                    D3D11_BUFFER_DESC vbo_def = {
                        .ByteWidth = static_cast<UINT>(total_size_in_bytes),
                        .Usage = D3D11_USAGE_DEFAULT,
                        .BindFlags = D3D11_BIND_VERTEX_BUFFER,
                        .CPUAccessFlags = 0,
                        .MiscFlags = 0,
                        .StructureByteStride = 0,
                    };
                    D3D11_SUBRESOURCE_DATA dat_def = {
                        .pSysMem = buffer_ptr,
                        .SysMemPitch = 0,
                        .SysMemSlicePitch = 0,
                    };
                    hr = device->CreateBuffer(&vbo_def, &dat_def, &mblock.normal_buffer);
                    if (FAILED(hr))
                    {
                        assert(false);
                        return false;
                    }
                }
                if (prim.attributes.contains("COLOR_0"))
                {
                    tinygltf::Accessor& accessor = model.accessors[prim.attributes["COLOR_0"]];
                    uint8_t* buffer_ptr{};
                    size_t total_size_in_bytes{};
                    std::vector<uint8_t> intermediate_buffer;
                    if (!getBufferFromAccessor(model, accessor, buffer_ptr, total_size_in_bytes, intermediate_buffer))
                        return false;

                    D3D11_BUFFER_DESC vbo_def = {
                        .ByteWidth = static_cast<UINT>(total_size_in_bytes),
                        .Usage = D3D11_USAGE_DEFAULT,
                        .BindFlags = D3D11_BIND_VERTEX_BUFFER,
                        .CPUAccessFlags = 0,
                        .MiscFlags = 0,
                        .StructureByteStride = 0,
                    };
                    D3D11_SUBRESOURCE_DATA dat_def = {
                        .pSysMem = buffer_ptr,
                        .SysMemPitch = 0,
                        .SysMemSlicePitch = 0,
                    };
                    hr = device->CreateBuffer(&vbo_def, &dat_def, &mblock.color_buffer);
                    if (FAILED(hr))
                    {
                        assert(false);
                        return false;
                    }
                }
                if (prim.attributes.contains("TEXCOORD_0"))
                {
                    tinygltf::Accessor& accessor = model.accessors[prim.attributes["TEXCOORD_0"]];
                    uint8_t* buffer_ptr{};
                    size_t total_size_in_bytes{};
                    std::vector<uint8_t> intermediate_buffer;
                    if (!getBufferFromAccessor(model, accessor, buffer_ptr, total_size_in_bytes, intermediate_buffer))
                        return false;

                    D3D11_BUFFER_DESC vbo_def = {
                        .ByteWidth = static_cast<UINT>(total_size_in_bytes),
                        .Usage = D3D11_USAGE_DEFAULT,
                        .BindFlags = D3D11_BIND_VERTEX_BUFFER,
                        .CPUAccessFlags = 0,
                        .MiscFlags = 0,
                        .StructureByteStride = 0,
                    };
                    D3D11_SUBRESOURCE_DATA dat_def = {
                        .pSysMem = buffer_ptr,
                        .SysMemPitch = 0,
                        .SysMemSlicePitch = 0,
                    };
                    hr = device->CreateBuffer(&vbo_def, &dat_def, &mblock.uv_buffer);
                    if (FAILED(hr))
                    {
                        assert(false);
                        return false;
                    }
                }
                if (prim.indices >= 0)
                {
                    tinygltf::Accessor& accessor = model.accessors[prim.indices];
                    tinygltf::BufferView& bufferview = model.bufferViews[accessor.bufferView];
                    tinygltf::Buffer& buffer = model.buffers[bufferview.buffer];
                    if (bufferview.byteStride > 0) {
                        std::ignore = nullptr;
                    }

                    D3D11_BUFFER_DESC ibo_def = {
                        .ByteWidth = (UINT)tinygltf::GetComponentSizeInBytes(accessor.componentType) * (UINT)tinygltf::GetNumComponentsInType(accessor.type) * (UINT)accessor.count,
                        .Usage = D3D11_USAGE_DEFAULT,
                        .BindFlags = D3D11_BIND_INDEX_BUFFER,
                        .CPUAccessFlags = 0,
                        .MiscFlags = 0,
                        .StructureByteStride = 0,
                    };
                    D3D11_SUBRESOURCE_DATA dat_def = {
                        .pSysMem = buffer.data.data() + bufferview.byteOffset + accessor.byteOffset,
                        .SysMemPitch = 0,
                        .SysMemSlicePitch = 0,
                    };

                    int32_t index_size = tinygltf::GetComponentSizeInBytes(accessor.componentType);
                    std::vector<uint16_t> index_work;
                    if (index_size == 1)
                    {
                        index_work.resize(ibo_def.ByteWidth);
                        uint8_t* ptr = (uint8_t*)dat_def.pSysMem;
                        for (size_t i = 0; i < ibo_def.ByteWidth; i += 1)
                        {
                            index_work[i] = ptr[i];
                        }
                        index_size = 2;
                        ibo_def.ByteWidth *= 2;
                        dat_def.pSysMem = index_work.data();
                    }
                    assert(index_size == 2 || index_size == 4);
                    mblock.index_format = index_size == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;

                    hr = device->CreateBuffer(&ibo_def, &dat_def, &mblock.index_buffer);
                    if (FAILED(hr))
                    {
                        assert(false);
                        return false;
                    }

                    mblock.draw_count = (UINT)accessor.count;
                }
                if (prim.material >= 0)
                {
                    tinygltf::Material& material = model.materials[prim.material];
                    auto& bcc = material.pbrMetallicRoughness.baseColorFactor;
                #pragma warning(disable:4244)
                    // [Potential Overflow]
                    mblock.base_color = DirectX::XMFLOAT4(bcc[0], bcc[1], bcc[2], bcc[3]);
                #pragma warning(default:4244)
                    tinygltf::TextureInfo& texture_info = material.pbrMetallicRoughness.baseColorTexture;
                    if (texture_info.index >= 0)
                    {
                        tinygltf::Texture& texture = model.textures[texture_info.index];
                        mblock.image = image[texture.source];
                        if (texture.sampler >= 0)
                        {
                            mblock.sampler = sampler[texture.sampler];
                        }
                        else
                        {
                            mblock.sampler = shared_->default_sampler;
                        }
                    }
                    else
                    {
                        //mblock.image = shared_->default_image;
                        mblock.sampler = shared_->default_sampler;
                    }
                    if (material.alphaMode == "MASK")
                    {
                        mblock.alpha_mask = TRUE;
                    }
                    else if (material.alphaMode == "BLEND")
                    {
                        mblock.alpha_blend = TRUE;
                    }
                    // [Potential Overflow]
                #pragma warning(disable:4244)
                    mblock.alpha = material.alphaCutoff;
                #pragma warning(default:4244)
                    mblock.double_side = material.doubleSided;
                    mblock.material_name = material.name;
                }
                mblock.node_name = node.name;
                if (node.mesh >= 0 && node.mesh < static_cast<int>(model.meshes.size()))
                {
                    mblock.mesh_name = model.meshes[node.mesh].name;
                }
                map_primitive_topology_to_d3d11(prim, mblock.primitive_topology);
                model_block.emplace_back(mblock);
            }
        }

        mTRS_stack.push_back(mTRS);
        if (!node.children.empty())
        {
            for (auto const& child_node_idx : node.children)
            {
                if (!processNode(model, model.nodes[child_node_idx]))
                {
                    return false;
                }
            }
        }
        mTRS_stack.pop_back();

        return true;
    };
    bool Model_D3D11::createModelBlock(tinygltf::Model& model)
    {
        int default_scene = model.defaultScene;
        if (default_scene < 0) default_scene = 0;
        tinygltf::Scene& scene = model.scenes[default_scene];
        for (int const& node_idx : scene.nodes)
        {
            tinygltf::Node& node = model.nodes[node_idx];
            if (!processNode(model, node))
            {
                return false;
            }
        }
        return true;
    }

    bool Model_D3D11::createResources()
    {
        struct FileSystemWrapper
        {
            static bool FileExists(const std::string& abs_filename, void*)
            {
                return FileSystemManager::hasFile(abs_filename);
            }
            static bool ReadWholeFile(std::vector<unsigned char>* out, std::string* err, const std::string& filepath, void*)
            {
                // TODO: no copy x2
                SmartReference<IData> data;
                if (!FileSystemManager::readFile(filepath, data.put())) {
                    if (err) {
                        (*err) += "File load error : " + filepath + "\n";
                    }
                    return false;
                }
                out->resize(data->size());
                std::memcpy(out->data(), data->data(), data->size());
                return true;
            }
            static bool GetFileSizeInBytes(size_t* filesize_out, [[maybe_unused]] std::string* err, const std::string& abs_filename, void*) {
                *filesize_out = FileSystemManager::getFileSize(abs_filename);
                return *filesize_out > 0;
            }
        };
        tinygltf::FsCallbacks fs_cb = {
            .FileExists = &FileSystemWrapper::FileExists,
            .ExpandFilePath = &tinygltf::ExpandFilePath,
            .ReadWholeFile = &FileSystemWrapper::ReadWholeFile,
            .WriteWholeFile = &tinygltf::WriteWholeFile,
            .GetFileSizeInBytes = &FileSystemWrapper::GetFileSizeInBytes,
            .user_data = nullptr,
        };
        tinygltf::TinyGLTF gltf_ctx;
        gltf_ctx.SetStoreOriginalJSONForExtrasAndExtensions(true);
        gltf_ctx.SetFsCallbacks(fs_cb);

        tinygltf::Model model;
        std::string warn;
        std::string err;

        bool ret = false;
        if (gltf_path.ends_with(".gltf"))
        {
            ret = gltf_ctx.LoadASCIIFromFile(&model, &err, &warn, gltf_path.c_str());
        }
        else
        {
            ret = gltf_ctx.LoadBinaryFromFile(&model, &err, &warn, gltf_path.c_str());
        }
        if (!warn.empty())
        {
            spdlog::warn("[core] gltf model warning: {}", warn);
        }
        if (!err.empty())
        {
            spdlog::error("[core] gltf model error: {}", err);
        }
        if (!ret)
        {
            return false;
        }

        // load image to shader resource

        if (!createImage(model)) return false;

        // create sampler state

        if (!createSampler(model)) return false;

        // create model block

        if (!createModelBlock(model)) return false;

        loadLights(model);

        return true;
    }

    void Model_D3D11::loadLights(tinygltf::Model& model)
    {
        for (tinygltf::Node const& node : model.nodes)
        {
            if (node.light < 0 || node.light >= static_cast<int>(model.lights.size()))
                continue;

            tinygltf::Light const& light = model.lights[node.light];
            if (light.type != "point")
                continue;

            if (embedded_lights_.size() >= MAX_POINT_LIGHTS)
            {
                spdlog::warn("[core] gltf model '{}': more than {} point lights defined; excess lights are ignored",
                    gltf_path, MAX_POINT_LIGHTS);
                break;
            }

            float px = 0.0f, py = 0.0f, pz = 0.0f;
            if (node.translation.size() >= 3)
            {
            #pragma warning(disable:4244)
                px = (float)node.translation[0];
                py = (float)node.translation[1];
                pz = -(float)node.translation[2];
            #pragma warning(default:4244)
            }

            float cr = 1.0f, cg = 1.0f, cb = 1.0f;
            if (light.color.size() >= 3)
            {
            #pragma warning(disable:4244)
                cr = (float)light.color[0];
                cg = (float)light.color[1];
                cb = (float)light.color[2];
            #pragma warning(default:4244)
            }

            float brightness = (float)light.intensity;

            float range = (light.range > 0.0) ? (float)light.range : 50.0f;

            PointLight pl{};
            pl.pos = { px, py, pz };
            pl.range = range;
            pl.color = { cr, cg, cb };
            pl.brightness = brightness;
            embedded_lights_.push_back(pl);

            spdlog::info("[core] gltf model '{}': loaded point light '{}' pos=({},{},{}) range={} brightness={}",
                gltf_path, light.name, px, py, pz, range, brightness);
        }
    }
    void Model_D3D11::onDeviceCreate()
    {
        createResources();
    }
    void Model_D3D11::onDeviceDestroy()
    {
        image.clear();
        sampler.clear();

        model_block.clear();
    }

    void Model_D3D11::draw(IRenderer::FogState fog, std::span<PointLight const> scene_lights)
    {
        auto* context = m_device->GetD3D11DeviceContext();

        // common data

        struct LightCBuffer
        {
            DirectX::XMFLOAT4 ambient;
            DirectX::XMFLOAT4 sunshine_pos;
            DirectX::XMFLOAT4 sunshine_dir;
            DirectX::XMFLOAT4 sunshine_color;
            DirectX::XMFLOAT4 point_light_pos[MAX_POINT_LIGHTS]; // xyz=pos, w=range
            DirectX::XMFLOAT4 point_light_color[MAX_POINT_LIGHTS]; // rgb=color, a=brightness
            DirectX::XMFLOAT4 point_light_count; // x = count
        };
        static_assert(sizeof(LightCBuffer) == 515 * sizeof(DirectX::XMFLOAT4),
            "LightCBuffer size mismatch");

        LightCBuffer lcb{};
        lcb.ambient = sunshine.ambient;
        lcb.sunshine_pos = sunshine.pos;
        lcb.sunshine_dir = sunshine.dir;
        lcb.sunshine_color = sunshine.color;

        uint32_t slot = 0u;
        DirectX::XMMATRIX const t_locwo_ = DirectX::XMMatrixMultiply(DirectX::XMMatrixMultiply(t_scale_, t_mbrot_), t_trans_);
        auto write_light = [&](PointLight const& pl, bool model_local)
        {
            if (slot >= MAX_POINT_LIGHTS) return;
            DirectX::XMFLOAT3 wpos = { pl.pos.x, pl.pos.y, pl.pos.z };
            if (model_local)
            {
                DirectX::XMVECTOR v = DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&wpos), t_locwo_);
                DirectX::XMStoreFloat3(&wpos, v);
            }
            lcb.point_light_pos[slot] = DirectX::XMFLOAT4(wpos.x, wpos.y, wpos.z, pl.range);
            lcb.point_light_color[slot] = DirectX::XMFLOAT4(pl.color.x, pl.color.y, pl.color.z, pl.brightness);
            ++slot;
        };
        for (PointLight const& pl : point_lights)
            write_light(pl, true);
        for (PointLight const& pl : scene_lights)
            write_light(pl, false);
        lcb.point_light_count.x = static_cast<float>(slot);

        context->UpdateSubresource(shared_->cbo_light.Get(), 0, NULL, &lcb, 0, 0);

        auto set_state_matrix_from_block = [&](ModelBlock& mblock)
        {
            // IA

            if (mblock.color_buffer)
                context->IASetInputLayout(shared_->input_layout_vc.Get());
            else
                context->IASetInputLayout(shared_->input_layout.Get());

            // VS

            if (mblock.color_buffer)
                context->VSSetShader(shared_->shader_vertex_vc.Get(), NULL, 0);
            else
                context->VSSetShader(shared_->shader_vertex.Get(), NULL, 0);

            // PS

            if (!mblock.alpha_mask)
            {
                if (mblock.image)
                {
                    if (mblock.color_buffer)
                        context->PSSetShader(shared_->shader_pixel_vc[IDX(fog)].Get(), NULL, 0);
                    else
                        context->PSSetShader(shared_->shader_pixel[IDX(fog)].Get(), NULL, 0);
                }
                else
                {
                    if (mblock.color_buffer)
                        context->PSSetShader(shared_->shader_pixel_nt_vc[IDX(fog)].Get(), NULL, 0);
                    else
                        context->PSSetShader(shared_->shader_pixel_nt[IDX(fog)].Get(), NULL, 0);
                }
            }
            else
            {
                if (mblock.image)
                {
                    if (mblock.color_buffer)
                        context->PSSetShader(shared_->shader_pixel_alpha_vc[IDX(fog)].Get(), NULL, 0);
                    else
                        context->PSSetShader(shared_->shader_pixel_alpha[IDX(fog)].Get(), NULL, 0);
                }
                else
                {
                    if (mblock.color_buffer)
                        context->PSSetShader(shared_->shader_pixel_alpha_nt_vc[IDX(fog)].Get(), NULL, 0);
                    else
                        context->PSSetShader(shared_->shader_pixel_alpha_nt[IDX(fog)].Get(), NULL, 0);
                }
            }
        };
        auto upload_local_world_matrix = [&](ModelBlock& mblock)
        {
            struct
            {
                DirectX::XMFLOAT4X4 v1;
                DirectX::XMFLOAT4X4 v2;
            } v{};
            DirectX::XMMATRIX const t_total_ = DirectX::XMMatrixMultiply(DirectX::XMLoadFloat4x4(&mblock.local_matrix), t_locwo_);
            DirectX::XMStoreFloat4x4(&v.v1, t_total_);
            DirectX::XMStoreFloat4x4(&v.v2, DirectX::XMMatrixInverseTranspose(t_total_));
            context->UpdateSubresource(shared_->cbo_mlw.Get(), 0, NULL, &v, 0, 0);
        };
        auto set_alpha_mode_opaque = [&](ModelBlock& mblock)
        {
            // PS

            FLOAT const alpha[8] = {
                    mblock.base_color.x * model_color_.x,
                    mblock.base_color.y * model_color_.y,
                    mblock.base_color.z * model_color_.z,
                    mblock.base_color.w * model_color_.w,
                    0.5f, 0.0f, 0.0f, 0.0f,
            };
            context->UpdateSubresource(shared_->cbo_alpha.Get(), 0, NULL, alpha, 0, 0);
            ID3D11Buffer* ps_cbo[2] = {
                // camera position and look to vector are setup by Renderer at register(b0)
                shared_->cbo_alpha.Get(),
                shared_->cbo_light.Get(),
            };
            context->PSSetConstantBuffers(2, 2, ps_cbo);

            // OM
            context->OMSetDepthStencilState(shared_->state_ds.Get(), D3D11_DEFAULT_STENCIL_REFERENCE);
            FLOAT const blend_factor[4]{};
            context->OMSetBlendState(shared_->state_blend.Get(), blend_factor, D3D11_DEFAULT_SAMPLE_MASK);
        };
        auto set_alpha_mode_mask = [&](ModelBlock& mblock)
        {
            // PS

            FLOAT const alpha[8] = {
                    mblock.base_color.x * model_color_.x,
                    mblock.base_color.y * model_color_.y,
                    mblock.base_color.z * model_color_.z,
                    mblock.base_color.w * model_color_.w,
                    mblock.alpha, 0.0f, 0.0f, 0.0f,
            };
            context->UpdateSubresource(shared_->cbo_alpha.Get(), 0, NULL, alpha, 0, 0);
            ID3D11Buffer* ps_cbo[2] = {
                // camera position and look to vector are setup by Renderer at register(b0)
                shared_->cbo_alpha.Get(),
                shared_->cbo_light.Get(),
            };
            context->PSSetConstantBuffers(2, 2, ps_cbo);

            // OM
            context->OMSetDepthStencilState(shared_->state_ds.Get(), D3D11_DEFAULT_STENCIL_REFERENCE);
            FLOAT const blend_factor[4]{};
            context->OMSetBlendState(shared_->state_blend.Get(), blend_factor, D3D11_DEFAULT_SAMPLE_MASK);
        };
        auto set_alpha_mode_mask_custom = [&](ModelBlock& mblock, float const value)
        {
            // PS

            FLOAT const alpha[8] = {
                    mblock.base_color.x * model_color_.x,
                    mblock.base_color.y * model_color_.y,
                    mblock.base_color.z * model_color_.z,
                    mblock.base_color.w * model_color_.w,
                    value, 0.0f, 0.0f, 0.0f,
            };
            context->UpdateSubresource(shared_->cbo_alpha.Get(), 0, NULL, alpha, 0, 0);
            ID3D11Buffer* ps_cbo[2] = {
                // camera position and look to vector are setup by Renderer at register(b0)
                shared_->cbo_alpha.Get(),
                shared_->cbo_light.Get(),
            };
            context->PSSetConstantBuffers(2, 2, ps_cbo);
            ID3D11ShaderResourceView* active_image = mblock.override_image ? mblock.override_image.Get() : mblock.image.Get();
            if (active_image)
            {
                if (mblock.color_buffer)
                    context->PSSetShader(shared_->shader_pixel_alpha_vc[IDX(fog)].Get(), NULL, 0);
                else
                    context->PSSetShader(shared_->shader_pixel_alpha[IDX(fog)].Get(), NULL, 0);
            }
            else
            {
                if (mblock.color_buffer)
                    context->PSSetShader(shared_->shader_pixel_alpha_nt_vc[IDX(fog)].Get(), NULL, 0);
                else
                    context->PSSetShader(shared_->shader_pixel_alpha_nt[IDX(fog)].Get(), NULL, 0);
            }

            // OM
            context->OMSetDepthStencilState(shared_->state_ds.Get(), D3D11_DEFAULT_STENCIL_REFERENCE);
            FLOAT const blend_factor[4]{};
            context->OMSetBlendState(shared_->state_blend.Get(), blend_factor, D3D11_DEFAULT_SAMPLE_MASK);
        };
        auto set_alpha_mode_blend = [&](ModelBlock& mblock)
        {
            // PS

            FLOAT const alpha[8] = {
                    mblock.base_color.x * model_color_.x,
                    mblock.base_color.y * model_color_.y,
                    mblock.base_color.z * model_color_.z,
                    mblock.base_color.w * model_color_.w,
                    0.5f, 0.0f, 0.0f, 0.0f,
            };
            context->UpdateSubresource(shared_->cbo_alpha.Get(), 0, NULL, alpha, 0, 0);
            ID3D11Buffer* ps_cbo[2] = {
                // camera position and look to vector are setup by Renderer at register(b0)
                shared_->cbo_alpha.Get(),
                shared_->cbo_light.Get(),
            };
            context->PSSetConstantBuffers(2, 2, ps_cbo);

            ID3D11ShaderResourceView* active_image = mblock.override_image ? mblock.override_image.Get() : mblock.image.Get();
            if (active_image)
            {
                if (mblock.color_buffer)
                    context->PSSetShader(shared_->shader_pixel_vc[IDX(fog)].Get(), NULL, 0);
                else
                    context->PSSetShader(shared_->shader_pixel[IDX(fog)].Get(), NULL, 0);
            }
            else
            {
                if (mblock.color_buffer)
                    context->PSSetShader(shared_->shader_pixel_nt_vc[IDX(fog)].Get(), NULL, 0);
                else
                    context->PSSetShader(shared_->shader_pixel_nt[IDX(fog)].Get(), NULL, 0);
            }

            // OM
            context->OMSetDepthStencilState(shared_->state_ds_no_write.Get(), D3D11_DEFAULT_STENCIL_REFERENCE);
            FLOAT const blend_factor[4]{};
            ID3D11BlendState* blend_state = shared_->getBlendState(blend_mode_);
            context->OMSetBlendState(blend_state, blend_factor, D3D11_DEFAULT_SAMPLE_MASK);
        };
        auto set_alpha_mode_blend_overlay = [&](ModelBlock& mblock, float const exclude_value)
        {
            // PS

            FLOAT const alpha[8] = {
                    mblock.base_color.x * model_color_.x,
                    mblock.base_color.y * model_color_.y,
                    mblock.base_color.z * model_color_.z,
                    mblock.base_color.w * model_color_.w,
                    exclude_value, 0.0f, 0.0f, 0.0f,
            };
            context->UpdateSubresource(shared_->cbo_alpha.Get(), 0, NULL, alpha, 0, 0);
            ID3D11Buffer* ps_cbo[2] = {
                // camera position and look to vector are setup by Renderer at register(b0)
                shared_->cbo_alpha.Get(),
                shared_->cbo_light.Get(),
            };
            context->PSSetConstantBuffers(2, 2, ps_cbo);
            ID3D11ShaderResourceView* active_image = mblock.override_image ? mblock.override_image.Get() : mblock.image.Get();
            if (active_image)
            {
                if (mblock.color_buffer)
                    context->PSSetShader(shared_->shader_pixel_inv_alpha_vc[IDX(fog)].Get(), NULL, 0);
                else
                    context->PSSetShader(shared_->shader_pixel_inv_alpha[IDX(fog)].Get(), NULL, 0);
            }
            else
            {
                if (mblock.color_buffer)
                    context->PSSetShader(shared_->shader_pixel_inv_alpha_nt_vc[IDX(fog)].Get(), NULL, 0);
                else
                    context->PSSetShader(shared_->shader_pixel_inv_alpha_nt[IDX(fog)].Get(), NULL, 0);
            }

            // OM
            context->OMSetDepthStencilState(shared_->state_ds.Get(), D3D11_DEFAULT_STENCIL_REFERENCE);
            FLOAT const blend_factor[4]{};
            context->OMSetBlendState(shared_->state_blend_alpha.Get(), blend_factor, D3D11_DEFAULT_SAMPLE_MASK);
        };
        auto set_alpha_mode_screen_door = [&](ModelBlock& mblock)
        {
            // PS

            FLOAT const alpha[8] = {
                    mblock.base_color.x * model_color_.x,
                    mblock.base_color.y * model_color_.y,
                    mblock.base_color.z * model_color_.z,
                    mblock.base_color.w * model_color_.w,
                    0.5f, 0.0f, 0.0f, 0.0f,
            };
            context->UpdateSubresource(shared_->cbo_alpha.Get(), 0, NULL, alpha, 0, 0);
            ID3D11Buffer* ps_cbo[2] = {
                // camera position and look to vector are setup by Renderer at register(b0)
                shared_->cbo_alpha.Get(),
                shared_->cbo_light.Get(),
            };
            context->PSSetConstantBuffers(2, 2, ps_cbo);
            ID3D11ShaderResourceView* active_image = mblock.override_image ? mblock.override_image.Get() : mblock.image.Get();
            if (active_image)
            {
                if (mblock.color_buffer)
                    context->PSSetShader(shared_->shader_pixel_sd_vc[IDX(fog)].Get(), NULL, 0);
                else
                    context->PSSetShader(shared_->shader_pixel_sd[IDX(fog)].Get(), NULL, 0);
            }
            else
            {
                if (mblock.color_buffer)
                    context->PSSetShader(shared_->shader_pixel_sd_nt_vc[IDX(fog)].Get(), NULL, 0);
                else
                    context->PSSetShader(shared_->shader_pixel_sd_nt[IDX(fog)].Get(), NULL, 0);
            }

            // OM
            context->OMSetDepthStencilState(shared_->state_ds.Get(), D3D11_DEFAULT_STENCIL_REFERENCE);
            FLOAT const blend_factor[4]{};
            context->OMSetBlendState(shared_->state_blend.Get(), blend_factor, D3D11_DEFAULT_SAMPLE_MASK);
        };
        auto set_state_from_block = [&](ModelBlock& mblock)
        {
            set_state_matrix_from_block(mblock);

            // IA

            context->IASetPrimitiveTopology(mblock.primitive_topology);
            ID3D11Buffer* vbo[4] = { mblock.vertex_buffer.Get(), mblock.normal_buffer.Get(), mblock.uv_buffer.Get(), mblock.color_buffer.Get() };
            UINT stride[4] = { 3 * sizeof(float), 3 * sizeof(float), 2 * sizeof(float), 3 * sizeof(float) };
            UINT offset[4] = { 0, 0, 0, 0 };
            context->IASetVertexBuffers(0, 4, vbo, stride, offset);
            context->IASetIndexBuffer(mblock.index_buffer.Get(), mblock.index_format, 0);

            // VS

            upload_local_world_matrix(mblock);
            DirectX::XMFLOAT4 uv_cb[2] = { mblock.uv_transform, mblock.uv_rotation };
            context->UpdateSubresource(shared_->cbo_uv.Get(), 0, NULL, uv_cb, 0, 0);
            ID3D11Buffer* vs_cbo[2] = {
                // view-projection matrix setup by Renderer at register(b0)
                shared_->cbo_mlw.Get(),
                shared_->cbo_uv.Get(),
            };
            context->VSSetConstantBuffers(1, 2, vs_cbo);

            // RS

            if (mblock.double_side)
            {
                context->RSSetState(shared_->state_rs_cull_none.Get());
            }
            else
            {
                context->RSSetState(shared_->state_rs_cull_back.Get());
            }

            // PS

            ID3D11SamplerState* ps_samp[1] = { mblock.sampler.Get() };
            context->PSSetSamplers(0, 1, ps_samp);
            ID3D11ShaderResourceView* ps_srv[1] = { mblock.override_image ? mblock.override_image.Get() : mblock.image.Get() };
            context->PSSetShaderResources(0, 1, ps_srv);

            // OM & blend mode decision
            bool const is_blend = mblock.alpha_blend || (model_color_.w < 0.999f) ||
                (blend_mode_ != ModelBlendMode::Auto);

            if (is_blend) {
                if (blend_mode_ == ModelBlendMode::ScreenDoor) {
                    set_alpha_mode_screen_door(mblock);
                } else {
                    set_alpha_mode_blend(mblock);
                }
            }
            else if (mblock.alpha_mask) {
                set_alpha_mode_mask(mblock);
            }
            else {
                set_alpha_mode_opaque(mblock);
            }
        };
        auto draw_block = [&](ModelBlock& mblock)
        {
            if (mblock.index_buffer)
                context->DrawIndexed(mblock.draw_count, 0, 0);
            else
                context->Draw(mblock.draw_count, 0);
        };
        auto clear_state = [&]()
        {
            // IA

            ID3D11Buffer* vbo_null[4] = { NULL, NULL, NULL, NULL };
            UINT stride_zero[4] = { 0, 0, 0, 0 };
            UINT offset_zero[4] = { 0, 0, 0, 0 };
            context->IASetVertexBuffers(0, 4, vbo_null, stride_zero, offset_zero);
            context->IASetIndexBuffer(NULL, DXGI_FORMAT_R16_UINT, 0);

            // VS

            ID3D11Buffer* vs_cbo[2] = { NULL, NULL };
            context->VSSetConstantBuffers(1, 2, vs_cbo);

            // PS

            ID3D11SamplerState* ps_samp[1] = { NULL };
            context->PSSetSamplers(0, 1, ps_samp);
            ID3D11ShaderResourceView* ps_srv[1] = { NULL };
            context->PSSetShaderResources(0, 1, ps_srv);
            ID3D11Buffer* ps_cbo[2] = { NULL, NULL };
            context->PSSetConstantBuffers(2, 2, ps_cbo);
        };

        bool const force_blend_all = (model_color_.w < 0.999f) ||
            (blend_mode_ != ModelBlendMode::Auto);

        // pass 1 opaque object

        if (!force_blend_all)
        {
            for (auto& mblock : model_block)
            {
                if (!mblock.alpha_mask && !mblock.alpha_blend)
                {
                    set_state_from_block(mblock);
                    draw_block(mblock);
                }
            }
        }

        // pass 2 alpha mask object

        if (!force_blend_all)
        {
            for (auto& mblock : model_block)
            {
                if (mblock.alpha_mask)
                {
                    set_state_from_block(mblock);
                    draw_block(mblock);
                }
            }
        }

        // pass 3 alpha blend object

        for (auto& mblock : model_block)
        {
            if (force_blend_all || mblock.alpha_blend)
            {
                if (mblock.double_side && blend_mode_ != ModelBlendMode::ScreenDoor)
                {
                    set_state_from_block(mblock);
                    // Pass 3a: Draw back faces first
                    context->RSSetState(shared_->state_rs_cull_front.Get());
                    draw_block(mblock);
                    // Pass 3b: Draw front faces
                    context->RSSetState(shared_->state_rs_cull_back.Get());
                    draw_block(mblock);
                }
                else
                {
                    set_state_from_block(mblock);
                    draw_block(mblock);
                }
            }
        }
        
        // unbind

        clear_state();
    }

    void Model_D3D11::setColor(Vector4F const& color)
    {
        model_color_ = DirectX::XMFLOAT4(color.x, color.y, color.z, color.w);
    }
    void Model_D3D11::setAlpha(float alpha)
    {
        model_color_.w = alpha;
    }
    void Model_D3D11::setBlendMode(ModelBlendMode mode)
    {
        blend_mode_ = mode;
    }
    Vector4F Model_D3D11::getColor() const
    {
        return Vector4F(model_color_.x, model_color_.y, model_color_.z, model_color_.w);
    }
    ModelBlendMode Model_D3D11::getBlendMode() const
    {
        return blend_mode_;
    }

    uint32_t Model_D3D11::getSubmeshCount() const
    {
        return static_cast<uint32_t>(model_block.size());
    }
    StringView Model_D3D11::getSubmeshNodeName(uint32_t index) const
    {
        if (index < model_block.size())
            return model_block[index].node_name;
        return "";
    }
    StringView Model_D3D11::getSubmeshMeshName(uint32_t index) const
    {
        if (index < model_block.size())
            return model_block[index].mesh_name;
        return "";
    }
    StringView Model_D3D11::getSubmeshMaterialName(uint32_t index) const
    {
        if (index < model_block.size())
            return model_block[index].material_name;
        return "";
    }

    void Model_D3D11::setTexture(ITexture2D* p_texture, uint32_t submesh_index)
    {
        ID3D11ShaderResourceView* srv = p_texture ? static_cast<ID3D11ShaderResourceView*>(p_texture->getNativeHandle()) : nullptr;
        if (submesh_index == 0)
        {
            for (auto& block : model_block)
            {
                block.override_image = srv;
            }
        }
        else if (submesh_index <= model_block.size())
        {
            model_block[submesh_index - 1].override_image = srv;
        }
    }

    void Model_D3D11::setTextureByName(ITexture2D* p_texture, StringView name)
    {
        ID3D11ShaderResourceView* srv = p_texture ? static_cast<ID3D11ShaderResourceView*>(p_texture->getNativeHandle()) : nullptr;
        for (auto& block : model_block)
        {
            if (block.material_name == name || block.mesh_name == name || block.node_name == name)
            {
                block.override_image = srv;
            }
        }
    }

    void Model_D3D11::resetTexture(uint32_t submesh_index)
    {
        setTexture(nullptr, submesh_index);
    }

    void Model_D3D11::resetTextureByName(StringView name)
    {
        setTextureByName(nullptr, name);
    }

    void Model_D3D11::setUVTransform(float u_offset, float v_offset, float u_scale, float v_scale, float angle, uint32_t submesh_index)
    {
        float rad = DirectX::XMConvertToRadians(angle);
        DirectX::XMFLOAT4 uv_t(u_offset, v_offset, u_scale, v_scale);
        DirectX::XMFLOAT4 uv_r(std::cosf(rad), std::sinf(rad), 0.0f, 0.0f);

        if (submesh_index == 0)
        {
            for (auto& block : model_block)
            {
                block.uv_transform = uv_t;
                block.uv_rotation = uv_r;
            }
        }
        else if (submesh_index <= model_block.size())
        {
            model_block[submesh_index - 1].uv_transform = uv_t;
            model_block[submesh_index - 1].uv_rotation = uv_r;
        }
    }

    void Model_D3D11::setUVTransformByName(float u_offset, float v_offset, float u_scale, float v_scale, float angle, StringView name)
    {
        float rad = DirectX::XMConvertToRadians(angle);
        DirectX::XMFLOAT4 uv_t(u_offset, v_offset, u_scale, v_scale);
        DirectX::XMFLOAT4 uv_r(std::cosf(rad), std::sinf(rad), 0.0f, 0.0f);

        for (auto& block : model_block)
        {
            if (block.material_name == name || block.mesh_name == name || block.node_name == name)
            {
                block.uv_transform = uv_t;
                block.uv_rotation = uv_r;
            }
        }
    }

    void Model_D3D11::resetUVTransform(uint32_t submesh_index)
    {
        setUVTransform(0.0f, 0.0f, 1.0f, 1.0f, 0.0f, submesh_index);
    }

    void Model_D3D11::resetUVTransformByName(StringView name)
    {
        setUVTransformByName(0.0f, 0.0f, 1.0f, 1.0f, 0.0f, name);
    }

    Model_D3D11::Model_D3D11(Direct3D11::Device* p_device, ModelSharedComponent_D3D11* p_model_shared, StringView path)
        : m_device(p_device)
        , shared_(p_model_shared)
        , gltf_path(path)
    {
        t_scale_ = DirectX::XMMatrixIdentity();
        t_trans_ = DirectX::XMMatrixIdentity();
        t_mbrot_ = DirectX::XMMatrixIdentity();
        if (!createResources())
            throw std::runtime_error("Model_D3D11::Model_D3D11");
        m_device->addEventListener(this);
    }
    Model_D3D11::~Model_D3D11()
    {
        m_device->removeEventListener(this);
    }
}
