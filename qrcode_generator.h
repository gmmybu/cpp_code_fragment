#pragma once
#include <stdint.h>
#include <vector>

// https://github.com/fukuchi/libqrencode
#include "qrencode.h"

class qrcode_generator
{
    enum
    {
        PIXEL_BLACK = 0x000000,
        PIXEL_WHITE = 0xffffff,
    };
public:
    static bool generate(const std::string& str, uint32_t pixel_count_per_dot, uint32_t border_dot_count, std::vector<uint32_t>& pixels, uint32_t& bitmap_width)
    {
        QRcode* qr = QRcode_encodeString(str.c_str(), 0, QR_ECLEVEL_M, QR_MODE_8, 1);
        if (qr == NULL) return false;

        class qrcode_deleter
        {
        public:
            qrcode_deleter(QRcode* qr) : _qr(qr) {}
            ~qrcode_deleter() { QRcode_free(_qr); }
        private:
            QRcode* _qr;
        };

        class qrcode_painter
        {
        public:
            qrcode_painter(uint32_t qrcode_width, uint32_t pixel_count_per_dot, uint32_t border_dot_count) :
                _qrcode_width(qrcode_width), _pixel_count_per_dot(pixel_count_per_dot), _border_dot_count(border_dot_count)
            {
                _bitmap_width = (_qrcode_width + _border_dot_count * 2) * _pixel_count_per_dot;
                _pixels.resize(_bitmap_width * _bitmap_width, PIXEL_WHITE);
            }

            void set_pixel(uint32_t x, uint32_t y, uint32_t color)
            {
                uint32_t start_x = (x + _border_dot_count) * _pixel_count_per_dot;
                uint32_t start_y = (y + _border_dot_count) * _pixel_count_per_dot;
                for (uint32_t j = 0; j < _pixel_count_per_dot; j++) {
                    uint32_t y = start_y + j;
                    for (uint32_t i = 0; i < _pixel_count_per_dot; i++) {
                        uint32_t x = start_x + i;
                        _pixels[y * _bitmap_width + x] = color;
                    }
                }
            }

            void get_result(std::vector<uint32_t>& pixels, uint32_t& bitmap_width)
            {
                bitmap_width = _bitmap_width;
                pixels = std::move(_pixels);
            }
        private:
            uint32_t _pixel_count_per_dot;

            uint32_t _border_dot_count;

            uint32_t _qrcode_width;

            uint32_t _bitmap_width;

            std::vector<uint32_t> _pixels;
        };

        qrcode_deleter deleter(qr);
        qrcode_painter painter(qr->width, pixel_count_per_dot, border_dot_count);

        uint8_t* data = qr->data;
        for (int y = 0; y < qr->width; y++) {
            for (int x = 0; x < qr->width; x++) {
                uint8_t pixel = *data++;
                if (pixel&1) {
                    painter.set_pixel(x, y, PIXEL_BLACK);
                } else {
                    painter.set_pixel(x, y, PIXEL_WHITE);
                }
            }
        }

        painter.get_result(pixels, bitmap_width);
        return true;
    }
};
