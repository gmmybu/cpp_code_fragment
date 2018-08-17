#pragma once
#include <stdint.h>
#include <windows.h>

// only support BGRA or BGRX
class render_texture
{
public:
    virtual void release() = 0;
public:
    virtual int32_t get_width() = 0;
    virtual int32_t get_height() = 0;
    virtual bool is_static() = 0;
    virtual bool is_transparent() = 0;
public:
    virtual bool update(void* pixels) = 0;
};

class render_device
{
public:
    virtual void release() = 0;
public:
    virtual render_texture* create_static_texture(int32_t w, int32_t h, bool transparent, void* pixels) = 0;
    virtual render_texture* create_dynamic_texture(int32_t w, int32_t h, bool transparent, void* pixels = nullptr) = 0;
public:
    virtual bool render_begin() = 0;
    virtual void render_end() = 0;

    virtual bool draw_texture(render_texture* texture,
        float left, float top, float right, float bottom) = 0;
};

render_device* create_render_device(HWND hwnd, int32_t width, int32_t height);
