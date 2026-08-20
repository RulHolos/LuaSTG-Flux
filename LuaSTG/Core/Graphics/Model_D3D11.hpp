#pragma once
#include "core/implement/ReferenceCounted.hpp"
#include "Core/Graphics/Renderer.hpp"
#include "Core/Graphics/Direct3D11/Device.hpp"
#include "tiny_gltf.h"
#include <span>

#define IDX(x) (size_t)static_cast<uint8_t>(x)

namespace core::Graphics
{
    class ModelSharedComponent_D3D11
        : public implement::ReferenceCounted<IReferenceCounted>
        , public IDeviceEventListener
    {
        friend class Model_D3D11;
    private:
        SmartReference<Direct3D11::Device> m_device;

        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> default_image;
        Microsoft::WRL::ComPtr<ID3D11SamplerState> default_sampler;

        Microsoft::WRL::ComPtr<ID3D11InputLayout> input_layout;
        Microsoft::WRL::ComPtr<ID3D11InputLayout> input_layout_vc;
        Microsoft::WRL::ComPtr<ID3D11VertexShader> shader_vertex;
        Microsoft::WRL::ComPtr<ID3D11VertexShader> shader_vertex_vc;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> shader_pixel[IDX(IRenderer::FogState::MAX_COUNT)];
        Microsoft::WRL::ComPtr<ID3D11PixelShader> shader_pixel_alpha[IDX(IRenderer::FogState::MAX_COUNT)];
        Microsoft::WRL::ComPtr<ID3D11PixelShader> shader_pixel_inv_alpha[IDX(IRenderer::FogState::MAX_COUNT)];
        Microsoft::WRL::ComPtr<ID3D11PixelShader> shader_pixel_nt[IDX(IRenderer::FogState::MAX_COUNT)];
        Microsoft::WRL::ComPtr<ID3D11PixelShader> shader_pixel_alpha_nt[IDX(IRenderer::FogState::MAX_COUNT)];
        Microsoft::WRL::ComPtr<ID3D11PixelShader> shader_pixel_inv_alpha_nt[IDX(IRenderer::FogState::MAX_COUNT)];
        Microsoft::WRL::ComPtr<ID3D11PixelShader> shader_pixel_vc[IDX(IRenderer::FogState::MAX_COUNT)];
        Microsoft::WRL::ComPtr<ID3D11PixelShader> shader_pixel_alpha_vc[IDX(IRenderer::FogState::MAX_COUNT)];
        Microsoft::WRL::ComPtr<ID3D11PixelShader> shader_pixel_inv_alpha_vc[IDX(IRenderer::FogState::MAX_COUNT)];
        Microsoft::WRL::ComPtr<ID3D11PixelShader> shader_pixel_nt_vc[IDX(IRenderer::FogState::MAX_COUNT)];
        Microsoft::WRL::ComPtr<ID3D11PixelShader> shader_pixel_alpha_nt_vc[IDX(IRenderer::FogState::MAX_COUNT)];
        Microsoft::WRL::ComPtr<ID3D11PixelShader> shader_pixel_inv_alpha_nt_vc[IDX(IRenderer::FogState::MAX_COUNT)];

        Microsoft::WRL::ComPtr<ID3D11PixelShader> shader_pixel_sd[IDX(IRenderer::FogState::MAX_COUNT)];
        Microsoft::WRL::ComPtr<ID3D11PixelShader> shader_pixel_sd_nt[IDX(IRenderer::FogState::MAX_COUNT)];
        Microsoft::WRL::ComPtr<ID3D11PixelShader> shader_pixel_sd_vc[IDX(IRenderer::FogState::MAX_COUNT)];
        Microsoft::WRL::ComPtr<ID3D11PixelShader> shader_pixel_sd_nt_vc[IDX(IRenderer::FogState::MAX_COUNT)];

        Microsoft::WRL::ComPtr<ID3D11RasterizerState> state_rs_cull_none;
        Microsoft::WRL::ComPtr<ID3D11RasterizerState> state_rs_cull_back;
        Microsoft::WRL::ComPtr<ID3D11RasterizerState> state_rs_cull_front;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> state_ds_disable;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> state_ds;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> state_ds_no_write;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> state_ds_dl;
        Microsoft::WRL::ComPtr<ID3D11BlendState> state_blend;
        Microsoft::WRL::ComPtr<ID3D11BlendState> state_blend_alpha;
        Microsoft::WRL::ComPtr<ID3D11BlendState> state_blend_add;
        Microsoft::WRL::ComPtr<ID3D11BlendState> state_blend_sub;
        Microsoft::WRL::ComPtr<ID3D11BlendState> state_blend_revsub;
        Microsoft::WRL::ComPtr<ID3D11BlendState> state_blend_mul;
        Microsoft::WRL::ComPtr<ID3D11BlendState> state_blend_screen;
        Microsoft::WRL::ComPtr<ID3D11BlendState> state_blend_min;
        Microsoft::WRL::ComPtr<ID3D11BlendState> state_blend_max;
        Microsoft::WRL::ComPtr<ID3D11BlendState> state_blend_inv;
        Microsoft::WRL::ComPtr<ID3D11BlendState> state_blend_one;

        ID3D11BlendState* getBlendState(ModelBlendMode mode) const noexcept
        {
            switch (mode)
            {
            case ModelBlendMode::Add:
                return state_blend_add.Get();
            case ModelBlendMode::Sub:
                return state_blend_sub.Get();
            case ModelBlendMode::RevSub:
                return state_blend_revsub.Get();
            case ModelBlendMode::Mul:
                return state_blend_mul.Get();
            case ModelBlendMode::Screen:
                return state_blend_screen.Get();
            case ModelBlendMode::Min:
                return state_blend_min.Get();
            case ModelBlendMode::Max:
                return state_blend_max.Get();
            case ModelBlendMode::Inv:
                return state_blend_inv.Get();
            case ModelBlendMode::One:
                return state_blend_one.Get();
            case ModelBlendMode::Alpha:
            default:
                return state_blend_alpha.Get();
            }
        }

        Microsoft::WRL::ComPtr<ID3D11Buffer> cbo_mvp;
        Microsoft::WRL::ComPtr<ID3D11Buffer> cbo_mlw;
        Microsoft::WRL::ComPtr<ID3D11Buffer> cbo_caminfo;
        Microsoft::WRL::ComPtr<ID3D11Buffer> cbo_alpha;
        Microsoft::WRL::ComPtr<ID3D11Buffer> cbo_light;
        Microsoft::WRL::ComPtr<ID3D11Buffer> cbo_uv;

    private:
        bool createImage();
        bool createSampler();
        bool createShader();
        bool createConstantBuffer();
        bool createState();

        bool createResources();
        void onDeviceCreate();
        void onDeviceDestroy();

    public:
        ModelSharedComponent_D3D11(Direct3D11::Device* p_device);
        ~ModelSharedComponent_D3D11();
    };

    class Model_D3D11
        : public implement::ReferenceCounted<IModel>
        , public IDeviceEventListener
    {
    private:
        SmartReference<Direct3D11::Device> m_device;
        SmartReference<ModelSharedComponent_D3D11> shared_;

        DirectX::XMMATRIX t_scale_;
        DirectX::XMMATRIX t_trans_;
        DirectX::XMMATRIX t_mbrot_;

        std::string gltf_path;

        struct ModelBlock
        {
            std::string node_name;
            std::string mesh_name;
            std::string material_name;

            Microsoft::WRL::ComPtr<ID3D11Buffer> vertex_buffer;
            Microsoft::WRL::ComPtr<ID3D11Buffer> uv_buffer;
            Microsoft::WRL::ComPtr<ID3D11Buffer> normal_buffer;
            Microsoft::WRL::ComPtr<ID3D11Buffer> color_buffer;
            Microsoft::WRL::ComPtr<ID3D11Buffer> index_buffer;
            Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler;
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> image;
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> override_image;
            DirectX::XMFLOAT4 uv_transform = { 0.0f, 0.0f, 1.0f, 1.0f };
            DirectX::XMFLOAT4 uv_rotation = { 1.0f, 0.0f, 0.0f, 0.0f };
            DirectX::XMFLOAT4X4 local_matrix;
            DirectX::XMFLOAT4X4 local_matrix_normal; // notice: pair with local_matrix
            DirectX::XMFLOAT4 base_color;
            BOOL double_side = FALSE;
            BOOL alpha_blend = FALSE;
            BOOL alpha_mask = FALSE;
            FLOAT alpha = 0.5f;
            UINT draw_count = 0;
            DXGI_FORMAT index_format = DXGI_FORMAT_R16_UINT;
            D3D11_PRIMITIVE_TOPOLOGY primitive_topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            ModelBlock()
            {
                DirectX::XMStoreFloat4x4(&local_matrix, DirectX::XMMatrixIdentity());
                DirectX::XMStoreFloat4x4(&local_matrix_normal, DirectX::XMMatrixIdentity());
                base_color = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
            }
        };
        struct Sunshine
        {
            DirectX::XMFLOAT4 ambient;
            DirectX::XMFLOAT4 pos;
            DirectX::XMFLOAT4 dir;
            DirectX::XMFLOAT4 color;

            void setDir(float dir_deg, float upd_deg)
            {
                DirectX::XMFLOAT3 slfrom(1.0f, 0.0f, 0.0f);
                auto rotate_vec2 = [](float& x, float& y, float r)
                {
                    float sin_v = std::sinf(r);
                    float cos_v = std::cosf(r);
                    float xx = x * cos_v - y * sin_v;
                    float yy = x * sin_v + y * cos_v;
                    x = xx;
                    y = yy;
                };
                // up and down
                rotate_vec2(slfrom.x, slfrom.y, DirectX::XMConvertToRadians(upd_deg));
                // direction
                rotate_vec2(slfrom.x, slfrom.z, DirectX::XMConvertToRadians(dir_deg));
                // inverse
                dir.x = -slfrom.x;
                dir.y = -slfrom.y;
                dir.z = -slfrom.z;
                dir.w = 0.0f;
            }

            Sunshine()
            {
                ambient = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f); // full ambient light
                pos = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
                dir = DirectX::XMFLOAT4(0.0f, -1.0f, 0.0f, 0.0f);
                color = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f); // no directional light
            }
        };

        std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> image;
        std::vector<Microsoft::WRL::ComPtr<ID3D11SamplerState>> sampler;

        std::vector<ModelBlock> model_block;

        Sunshine sunshine;
        std::vector<PointLight> point_lights;
        std::vector<PointLight> embedded_lights_;

        DirectX::XMFLOAT4 model_color_{ 1.0f, 1.0f, 1.0f, 1.0f };
        ModelBlendMode blend_mode_{ ModelBlendMode::Auto };

        std::vector<DirectX::XMMATRIX> mTRS_stack;

        bool processNode(tinygltf::Model& model, tinygltf::Node& node);
        bool createImage(tinygltf::Model& model);
        bool createSampler(tinygltf::Model& model);
        bool createModelBlock(tinygltf::Model& model);
        void loadLights(tinygltf::Model& model);

        bool createResources();
        void onDeviceCreate();
        void onDeviceDestroy();

    public:

        static constexpr uint32_t MAX_POINT_LIGHTS = 255u;

        void setAmbient(Vector3F const& color, float brightness);
        void setDirectionalLight(Vector3F const& direction, Vector3F const& color, float brightness);
        void addPointLight(Vector3F const& pos, Vector3F const& color, float brightness, float range);
        void clearPointLights();
        std::vector<PointLight> takeEmbeddedLights();
        void setScaling(Vector3F const& scale);
        void setPosition(Vector3F const& pos);
        void setRotationRollPitchYaw(float roll, float pitch, float yaw);
        void setRotationQuaternion(Vector4F const& quat);

        void setColor(Vector4F const& color) override;
        void setAlpha(float alpha) override;
        void setBlendMode(ModelBlendMode mode) override;
        Vector4F getColor() const override;
        ModelBlendMode getBlendMode() const override;

        uint32_t getSubmeshCount() const override;
        StringView getSubmeshNodeName(uint32_t index) const override;
        StringView getSubmeshMeshName(uint32_t index) const override;
        StringView getSubmeshMaterialName(uint32_t index) const override;

        void setTexture(ITexture2D* p_texture, uint32_t submesh_index = 0) override;
        void setTextureByName(ITexture2D* p_texture, StringView name) override;
        void resetTexture(uint32_t submesh_index = 0) override;
        void resetTextureByName(StringView name) override;

        void setUVTransform(float u_offset, float v_offset, float u_scale = 1.0f, float v_scale = 1.0f, float angle = 0.0f, uint32_t submesh_index = 0) override;
        void setUVTransformByName(float u_offset, float v_offset, float u_scale, float v_scale, float angle, StringView name) override;
        void resetUVTransform(uint32_t submesh_index = 0) override;
        void resetUVTransformByName(StringView name) override;

        void draw(IRenderer::FogState fog, std::span<PointLight const> scene_lights);

    public:
        Model_D3D11(Direct3D11::Device* p_device, ModelSharedComponent_D3D11* p_model_shared, StringView path);
        ~Model_D3D11();
    };
}

#undef IDX
