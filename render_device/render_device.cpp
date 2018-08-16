#include "common.h"
#include "shared_com_ptr.h"
#include "render_device.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <xnamath.h>

static const char vertex_shader_str[] =
   "struct VS_INPUT                     \
    {                                   \
        float4 Pos : POSITION;          \
        float2 Tex : TEXCOORD0;         \
    };                                  \
                                        \
    struct PS_INPUT                     \
    {                                   \
        float4 Pos : SV_POSITION;       \
        float2 Tex : TEXCOORD0;         \
    };                                  \
                                        \
    PS_INPUT main(VS_INPUT input)       \
    {                                   \
        PS_INPUT output = (PS_INPUT)0;  \
        output.Pos = input.Pos;         \
        output.Tex = input.Tex;         \
        return output;                  \
    }";

static const char pixel_shader_str[] =
   "Texture2D txDiffuse : register(t0);                 \
    SamplerState samLinear : register(s0);              \
                                                        \
    struct PS_INPUT                                     \
    {                                                   \
    float4 Pos : SV_POSITION;                           \
    float2 Tex : TEXCOORD0;                             \
    };                                                  \
                                                        \
    float4 main(PS_INPUT input) : SV_Target             \
    {                                                   \
        return txDiffuse.Sample(samLinear, input.Tex);  \
    }";

class shader_compiler
{
public:
    static bool compile_vertex_shader(ID3DBlob** blob)
    {
        auto compile = get_compiler();
        if (compile == NULL) return false;

        shared_com_ptr<ID3DBlob> error;
        HRESULT hr = compile(vertex_shader_str, sizeof(vertex_shader_str),
            "vertex_shader_string", nullptr, nullptr, "main",
            "vs_4_0", D3D10_SHADER_OPTIMIZATION_LEVEL1, 0, blob,
            &error);

        if (FAILED(hr)) {
            if (error != nullptr) {
                logger_error((char*)error->GetBufferPointer());
            } else {
                logger_error_va("compile vertex shader 0x%x", hr);
            }
            return false;
        }

        return true;
    }

    static bool compile_pixel_shader(ID3DBlob** blob)
    {
        auto compile = get_compiler();
        if (compile == NULL) return false;

        shared_com_ptr<ID3DBlob> error;
        HRESULT hr = compile(pixel_shader_str, sizeof(pixel_shader_str),
            "pixel_shader_string", nullptr, nullptr, "main",
            "ps_4_0", D3D10_SHADER_OPTIMIZATION_LEVEL1, 0, blob,
            &error);

        if (FAILED(hr)) {
            if (error != nullptr) {
                logger_error((char*)error->GetBufferPointer());
            } else {
                logger_error_va("compile pixel shader 0x%x", hr);
            }
            return false;
        }

        return true;
    }
private:
    static pD3DCompile get_compiler()
    {
        static pD3DCompile compile = NULL;
        if (compile != NULL) return compile;

        int  version = 49;
        char compiler[40];
        while (version-- > 30 && compile == NULL) {
            sprintf_s(compiler, 40, "D3DCompiler_%02d.dll", version);

            HMODULE module = LoadLibraryA(compiler);
            if (module != NULL) {
                compile = (pD3DCompile)GetProcAddress(module, "D3DCompile");
                if (compile != NULL) {
                    logger_info_va("compiler is %s", compiler);
                }
            }
        }

        return compile;
    }
};

class render_texture_impl : public render_texture
{
public:
    render_texture_impl(const shared_com_ptr<ID3D11ShaderResourceView>& resource,
                        const shared_com_ptr<ID3D11DeviceContext>& context,
                        int32_t width, int32_t height, bool transparent) :
        _width(width), _height(height),
        _resource(resource),
        _context(context),
        _transparent(transparent)
    {
    }

        render_texture_impl(const shared_com_ptr<ID3D11Texture2D> texture,
                            const shared_com_ptr<ID3D11Texture2D> stage_texture,
                            const shared_com_ptr<ID3D11ShaderResourceView>& resource,
                            const shared_com_ptr<ID3D11DeviceContext>& context,
                            int32_t width, int32_t height, bool transparent) :
            _texture(texture),
            _stage_texture(stage_texture),
            _width(width), _height(height),
            _resource(resource),
            _context(context),
            _transparent(transparent)
    {
    }
protected:
    void release()
    {
        delete this;
    }
protected:
    int32_t get_width()
    {
        return _width;
    }

    int32_t get_height()
    {
        return _height;
    }

    bool is_static()
    {
        return _texture == nullptr;
    }

    bool is_transparent()
    {
        return _transparent;
    }
protected:
    bool update(void* pixels)
    {
        dd_assert(_texture != nullptr && pixels != nullptr);

        D3D11_MAPPED_SUBRESOURCE resource;
        dd_checkd3d9_r(_context->Map(_stage_texture, 0, D3D11_MAP_WRITE, 0, &resource));
        if (resource.RowPitch == _width*sizeof(uint32_t)) {
            memcpy(resource.pData, pixels, _width*_height*sizeof(uint32_t));
        }
        _context->Unmap(_stage_texture, 0);
        _context->CopyResource(_texture, _stage_texture);
        return true;
    }
private:
    bool _transparent;
    int32_t _width;
    int32_t _height;
    shared_com_ptr<ID3D11Texture2D> _texture;
    shared_com_ptr<ID3D11Texture2D> _stage_texture;
    shared_com_ptr<ID3D11ShaderResourceView> _resource;
    shared_com_ptr<ID3D11DeviceContext> _context;
    friend class render_device_impl;
};

struct vertex
{
    XMFLOAT3 Pos;
    XMFLOAT2 Tex;
};

class render_device_impl : public render_device
{
public:
    render_device_impl() {}

    bool initialize(HWND hwnd, int32_t width, int32_t height);
protected:
    void release()
    {
        delete this;
    }
protected:
    render_texture* create_static_texture(int32_t w, int32_t h, bool transparent, void* pixels)
    {
        dd_assert(w > 0 && h > 0 && pixels != nullptr);

        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = w;
        desc.Height = h;
        desc.MipLevels = 1;
        desc.ArraySize = 1;

        if (transparent) {
            desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        } else {
            desc.Format = DXGI_FORMAT_B8G8R8X8_UNORM;
        }

        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA data = {};
        data.pSysMem = pixels;
        data.SysMemPitch = sizeof(uint32_t)*w;

        shared_com_ptr<ID3D11Texture2D> texture;
        dd_checkd3d9_p(_device->CreateTexture2D(&desc, &data, &texture));

        shared_com_ptr<ID3D11ShaderResourceView> resource;
        dd_checkd3d9_p(_device->CreateShaderResourceView(texture, NULL, &resource));

        auto result = new_ex render_texture_impl(resource, _context, w, h, transparent);
        dd_checkbool_p(result != nullptr);
        
        return result;
    }

    render_texture* create_dynamic_texture(int32_t w, int32_t h, bool transparent, void* pixels)
    {
        dd_assert(w > 0 && h > 0);

        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = w;
        desc.Height = h;
        desc.MipLevels = 1;
        desc.ArraySize = 1;

        if (transparent) {
            desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        } else {
            desc.Format = DXGI_FORMAT_B8G8R8X8_UNORM;
        }

        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        shared_com_ptr<ID3D11Texture2D> texture;
        if (pixels != nullptr) {
            D3D11_SUBRESOURCE_DATA data = {};
            data.pSysMem = pixels;
            data.SysMemPitch = sizeof(uint32_t)*w;

            dd_checkd3d9_p(_device->CreateTexture2D(&desc, &data, &texture));
        } else {
            dd_checkd3d9_p(_device->CreateTexture2D(&desc, NULL, &texture));
        }

        shared_com_ptr<ID3D11ShaderResourceView> resource;
        dd_checkd3d9_p(_device->CreateShaderResourceView(texture, NULL, &resource));

        shared_com_ptr<ID3D11Texture2D> stage_texture;
        dd_checkbool_p(create_stage_texture(w, h, desc.Format, &stage_texture));

        auto result = new_ex render_texture_impl(texture, stage_texture,
            resource, _context, w, h, transparent);
        dd_checkbool_p(result != nullptr);

        return result;
    }

    bool create_stage_texture(uint32_t width, uint32_t height, DXGI_FORMAT format, ID3D11Texture2D** stage_texture)
    {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = format;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        HRESULT hr = _device->CreateTexture2D(&desc, NULL, stage_texture);
        if (FAILED(hr)) {
            logger_error_va("error create stage texture, %u", hr);
            return false;
        }

        return true;
    }
protected:
    bool render_begin()
    {
        float ClearColor[4] = { 0.f, 0.f, 0.f, 1.f };
        _context->ClearRenderTargetView(_rendertarget, ClearColor);

        _context->VSSetShader(_vertexshader, NULL, 0);
        _context->PSSetShader(_pixelshader, NULL, 0);
        _context->PSSetSamplers(0, 1, &_sampler);
        return true;
    }

    void render_end()
    {
        _swapchain->Present(1, 0);
    }

    bool draw_texture(render_texture* texture,
        float left, float top, float right, float bottom)
    {
        float vertex_x0 = -1.f + 2.f * left   / _width;
        float vertex_x1 = -1.f + 2.f * right  / _width;
        float vertex_y1 = -1.f + 2.f * (_height - top) / _height;
        float vertex_y0 = -1.f + 2.f *  (_height - bottom) / _height;

        vertex vertexes[] = {
            { XMFLOAT3( vertex_x0, vertex_y0, 0.0f ), XMFLOAT2( 0.0f, 1.0f) },
            { XMFLOAT3( vertex_x0, vertex_y1, 0.0f ), XMFLOAT2( 0.0f, 0.0f) },
            { XMFLOAT3( vertex_x1, vertex_y0, 0.0f ), XMFLOAT2( 1.0f, 1.0f) },
            { XMFLOAT3( vertex_x1, vertex_y1, 0.0f ), XMFLOAT2( 1.0f, 0.0f) },
        };

        D3D11_MAPPED_SUBRESOURCE resc;
        dd_checkd3d9_r(_context->Map(_vertexbuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &resc));

        memcpy(resc.pData, vertexes, sizeof(vertex)*4);
        _context->Unmap(_vertexbuffer, 0);

        if (texture->is_transparent()) {
            _context->OMSetBlendState(_blend, 0, 0xFFFFFFFF);
        } else {
            _context->OMSetBlendState(NULL, 0, 0xFFFFFFFF);
        }

        render_texture_impl* impl = (render_texture_impl*)texture;
        _context->PSSetShaderResources(0, 1, &impl->_resource);
        _context->DrawIndexed(6, 0, 0);
        return true;
    }
private:
    void set_viewport(int32_t w, int32_t h);

    bool create_vertex_shader();
    bool create_vertex_layout(ID3DBlob* blob);

    bool create_pixel_shader();
    
    bool create_vertex_buffer();
    bool create_index_buffer();
    
    bool create_sampler();
    bool create_blend_state();

    shared_com_ptr<ID3D11Device> _device;
    shared_com_ptr<ID3D11DeviceContext> _context;
    
    shared_com_ptr<IDXGISwapChain> _swapchain;
    shared_com_ptr<ID3D11RenderTargetView> _rendertarget;

    shared_com_ptr<ID3D11VertexShader> _vertexshader;
    shared_com_ptr<ID3D11InputLayout> _vertexlayout;

    shared_com_ptr<ID3D11PixelShader> _pixelshader;
    
    shared_com_ptr<ID3D11Buffer> _vertexbuffer;
    shared_com_ptr<ID3D11Buffer> _indexbuffer;
    
    shared_com_ptr<ID3D11SamplerState> _sampler;
    shared_com_ptr<ID3D11BlendState> _blend;

    int32_t _width;
    int32_t _height;
};

bool render_device_impl::initialize(HWND hwnd, int32_t width, int32_t height)
{
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = width;
    sd.BufferDesc.Height = height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE,
        NULL, 0, featureLevels, 3, D3D11_SDK_VERSION, &sd,
        &_swapchain, &_device, &featureLevel, &_context);
    dd_checkd3d9_r(hr);

    shared_com_ptr<ID3D11Texture2D> back_buffer;
    dd_checkd3d9_r(_swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&back_buffer));
    dd_checkd3d9_r(_device->CreateRenderTargetView(back_buffer, NULL, &_rendertarget));
    _context->OMSetRenderTargets(1, &_rendertarget, NULL);

    _width = width; _height = height;
    set_viewport(width, height);

    return create_vertex_shader()
        && create_pixel_shader()
        && create_vertex_buffer()
        && create_index_buffer()
        && create_sampler()
        && create_blend_state();
}

void render_device_impl::set_viewport(int32_t w, int32_t h)
{
    D3D11_VIEWPORT vp;
    vp.Width = (FLOAT)w;
    vp.Height = (FLOAT)h;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    _context->RSSetViewports(1, &vp);
}

bool render_device_impl::create_vertex_shader()
{
    shared_com_ptr<ID3DBlob> blob;
    dd_checkbool_r(shader_compiler::compile_vertex_shader(&blob));
    
    auto data = blob->GetBufferPointer();
    auto size = blob->GetBufferSize();
    dd_checkd3d9_r(_device->CreateVertexShader(data, size, NULL, &_vertexshader));
    return create_vertex_layout(blob);
}

bool render_device_impl::create_vertex_layout(ID3DBlob* blob)
{
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    auto data = blob->GetBufferPointer();
    auto size = blob->GetBufferSize();
    dd_checkd3d9_r(_device->CreateInputLayout(layout, 2, data, size, &_vertexlayout));

    _context->IASetInputLayout(_vertexlayout);
    return true;
}

bool render_device_impl::create_pixel_shader()
{
    shared_com_ptr<ID3DBlob> blob;
    dd_checkbool_r(shader_compiler::compile_pixel_shader(&blob));
    
    auto data = blob->GetBufferPointer();
    auto size = blob->GetBufferSize();
    dd_checkd3d9_r(_device->CreatePixelShader(data, size, NULL, &_pixelshader));
    return true;
}

bool render_device_impl::create_vertex_buffer()
{
    vertex vertices[] = {
        { XMFLOAT3(-1.0f, -1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) },
        { XMFLOAT3(-1.0f,  1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3( 1.0f, -1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) },
        { XMFLOAT3( 1.0f,  1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) }
    };

    D3D11_BUFFER_DESC desc = {};
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.ByteWidth = sizeof(vertex)*4;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    D3D11_SUBRESOURCE_DATA data = {};
    data.pSysMem = vertices;
    dd_checkd3d9_r(_device->CreateBuffer(&desc, &data, &_vertexbuffer));

    UINT stride = sizeof(vertex);
    UINT offset = 0;
    _context->IASetVertexBuffers(0, 1, &_vertexbuffer, &stride, &offset);
    return true;
}

bool render_device_impl::create_index_buffer()
{
    WORD indices[] = { 0,1,2, 2,1,3 };

    D3D11_BUFFER_DESC desc = {};
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.ByteWidth = sizeof(WORD)*6;
    desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA data = {};
    data.pSysMem = indices;
    dd_checkd3d9_r(_device->CreateBuffer( &desc, &data, &_indexbuffer));

    _context->IASetIndexBuffer(_indexbuffer, DXGI_FORMAT_R16_UINT, 0);
    _context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    return true;
}

bool render_device_impl::create_sampler()
{
    D3D11_SAMPLER_DESC desc = {};
    desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    desc.MinLOD = 0;
    desc.MaxLOD = D3D11_FLOAT32_MAX;
    dd_checkd3d9_r(_device->CreateSamplerState(&desc, &_sampler));
    return true;
}

bool render_device_impl::create_blend_state()
{
    D3D11_BLEND_DESC desc = {};
    auto& target = desc.RenderTarget[0];
    target.BlendEnable = TRUE;
    target.SrcBlend = D3D11_BLEND_SRC_ALPHA;
    target.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    target.BlendOp = D3D11_BLEND_OP_ADD;
    target.SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA; 
    target.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA; 
    target.BlendOpAlpha = D3D11_BLEND_OP_ADD; 
    target.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    dd_checkd3d9_r(_device->CreateBlendState(&desc, &_blend));
    return true;
}

render_device* create_render_device(HWND hwnd, int32_t width, int32_t height)
{
    auto device = new_ex render_device_impl();
    if (device->initialize(hwnd, width, height))
        return device;

    delete device;
    return nullptr;
}
