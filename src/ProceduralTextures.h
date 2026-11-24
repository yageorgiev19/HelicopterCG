// ProceduralTextures.h - текстури, генерирани изцяло с код (без файлове).
//
// Всяка текстура се изчислява пиксел по пиксел в RAM като RGBA масив,
// след което се качва във видеокартата с mip-нива. Основата е
// "value noise" + fBm (fractal Brownian motion) - сума от няколко октави
// шум с нарастваща честота и намаляваща амплитуда.
#pragma once

#include <glad/gl.h>
#include <cstdint>
#include <vector>

struct Image {
    int width = 0, height = 0;
    std::vector<std::uint8_t> rgba;          // width * height * 4 байта
    Image(int w, int h) : width(w), height(h), rgba(static_cast<size_t>(w) * h * 4, 255) {}
    void set(int x, int y, float r, float g, float b, float a = 1.0f);
};

// Качва изображението като 2D текстура. srgb = true за цветни текстури
// (данните са в sRGB гама и GPU ги линеаризира при четене).
GLuint uploadTexture(const Image& img, bool srgb, bool repeat = true);

namespace ProcTex {
    // Шум в [0,1], периодичен с период period (за безшевно повтаряне).
    float valueNoise(float x, float y, int period, unsigned seed);
    float fbm(float x, float y, int octaves, int period, unsigned seed);

    Image camouflage(int size);    // тяло: петниста камуфлажна боя + панели + нитове
    Image metal(int size);         // шлифован метал (двигател, мачта, плъзгачи)
    Image rotorBlade(int size);    // перка: тъмна, с жълти/бели върхове
    Image glass(int size);         // стъкло на кабината с отблясък
    Image helipad(int size);       // бетонна площадка с "H" и кръг
    Image groundGrid(int size);    // земя: едно квадратче с тъмен контур (мрежа)
    Image barrier(int size);       // полупрозрачна оранжева мрежа за стените на полето
    Image rotorBlur(int size);     // полупрозрачен радиален диск за ротора
    Image white();                 // 1x1 бяла текстура (само цвят от материала)
}
