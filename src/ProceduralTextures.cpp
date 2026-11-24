// ProceduralTextures.cpp - генератори на процедурни текстури.
#include "ProceduralTextures.h"

#include <algorithm>
#include <cmath>

//  Помощни функции
static float clamp01(float x) { return std::min(1.0f, std::max(0.0f, x)); }
static float mix(float a, float b, float t) { return a + (b - a) * t; }
static float smoothstep(float e0, float e1, float x)
{
    float t = clamp01((x - e0) / (e1 - e0));
    return t * t * (3.0f - 2.0f * t);
}
static float fract(float x) { return x - std::floor(x); }

void Image::set(int x, int y, float r, float g, float b, float a)
{
    size_t i = (static_cast<size_t>(y) * width + x) * 4;
    rgba[i + 0] = static_cast<std::uint8_t>(clamp01(r) * 255.0f + 0.5f);
    rgba[i + 1] = static_cast<std::uint8_t>(clamp01(g) * 255.0f + 0.5f);
    rgba[i + 2] = static_cast<std::uint8_t>(clamp01(b) * 255.0f + 0.5f);
    rgba[i + 3] = static_cast<std::uint8_t>(clamp01(a) * 255.0f + 0.5f);
}

GLuint uploadTexture(const Image& img, bool srgb, bool repeat)
{
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8,
                 img.width, img.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, img.rgba.data());

    // Mip-нива: намалени копия 1/2, 1/4, ... Използват се, когато обектът е далече, и премахват "трептенето" (алиасинг) на ситните детайли.
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // трилинейно
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    GLint wrap = repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
    return tex;
}

namespace ProcTex {

// Целочислен хеш -> псевдослучайно число в [0,1). Едни и същи входни данни винаги дават един и същи резултат, затова текстурите са детерминирани.
static float hash2(int x, int y, unsigned seed)
{
    unsigned h = static_cast<unsigned>(x) * 374761393u
               + static_cast<unsigned>(y) * 668265263u + seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (h & 0xFFFFFFu) / 16777216.0f;
}

// Value noise: случайни стойности във възлите на решетка и гладка (smoothstep) билинейна интерполация между тях. Координатите на възлите се вземат по модул period, затова шумът се повтаря безшевно.
static float valueNoise2(float x, float y, int px, int py, unsigned seed)
{
    int x0 = static_cast<int>(std::floor(x)), y0 = static_cast<int>(std::floor(y));
    float fx = x - x0, fy = y - y0;
    auto wrap = [](int a, int p) { return ((a % p) + p) % p; };
    int xa = wrap(x0, px), xb = wrap(x0 + 1, px);
    int ya = wrap(y0, py), yb = wrap(y0 + 1, py);
    float sx = fx * fx * (3 - 2 * fx), sy = fy * fy * (3 - 2 * fy);
    float top = mix(hash2(xa, ya, seed), hash2(xb, ya, seed), sx);
    float bot = mix(hash2(xa, yb, seed), hash2(xb, yb, seed), sx);
    return mix(top, bot, sy);
}

float valueNoise(float x, float y, int period, unsigned seed)
{
    return valueNoise2(x, y, period, period, seed);
}

// fBm с различни периоди по X и Y (за "разтеглени" шарки като шлифован метал).
static float fbm2(float x, float y, int octaves, int px, int py, unsigned seed)
{
    float sum = 0, amp = 0.5f, norm = 0;
    for (int o = 0; o < octaves; ++o) {
        sum  += amp * valueNoise2(x, y, px, py, seed + o * 101u);
        norm += amp;
        x *= 2; y *= 2; px *= 2; py *= 2;   // двойна честота
        amp *= 0.5f;                       // половин амплитуда
    }
    return sum / norm;
}

float fbm(float x, float y, int octaves, int period, unsigned seed)
{
    return fbm2(x, y, octaves, period, period, seed);
}


Image camouflage(int size)
{
    Image img(size, size);
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
            float u = static_cast<float>(x) / size, v = static_cast<float>(y) / size;
            float n1 = fbm(u * 4, v * 4, 5, 4, 11u);
            float n2 = fbm(u * 4, v * 4, 5, 4, 29u);
            float grain = valueNoise(u * 128, v * 128, 128, 7u) * 0.06f - 0.03f;

            // Праговете превръщат плавния шум в петна с ясни граници.
            float r = 0.33f, g = 0.37f, b = 0.21f;                 // маслинено
            if (n1 > 0.58f)      { r = 0.20f; g = 0.24f; b = 0.13f; } // тъмно
            else if (n2 > 0.60f) { r = 0.49f; g = 0.45f; b = 0.31f; } // пясъчно
            else if (n1 < 0.38f) { r = 0.11f; g = 0.12f; b = 0.10f; } // почти черно

            // Панелни линии (стикове на ламарината) и нитове по тях.
            float shade = 1.0f;
            float pu = fract(u * 8), pv = fract(v * 6);
            if (pu < 0.012f || pv < 0.016f) shade = 0.55f;
            float dv = fract(v * 48) - 0.5f;
            if (pu > 0.022f && pu < 0.034f && std::fabs(dv) < 0.15f) shade = 1.3f; // нит

            img.set(x, y, (r + grain) * shade, (g + grain) * shade, (b + grain) * shade);
        }
    return img;
}

Image metal(int size)
{
    Image img(size, size);
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
            float u = static_cast<float>(x) / size, v = static_cast<float>(y) / size;
            // Нисък честотен шум по u, висок по v -> надлъжни "драскотини".
            float streak = fbm2(u * 2, v * 64, 4, 2, 64, 3u);
            float blot = fbm(u * 3, v * 3, 4, 3, 17u);
            float c = 0.42f + 0.18f * streak + 0.10f * (blot - 0.5f);
            float seam = fract(v * 4) < 0.01f ? 0.6f : 1.0f;
            img.set(x, y, c * seam, c * seam * 1.01f, c * seam * 1.04f);
        }
    return img;
}

Image rotorBlade(int size)
{
    Image img(size, size / 4);
    for (int y = 0; y < img.height; ++y)
        for (int x = 0; x < size; ++x) {
            float u = static_cast<float>(x) / size;
            float n = valueNoise(u * 64, static_cast<float>(y) / img.height * 4, 64, 5u);
            float r = 0.12f + 0.04f * n, g = r, b = r * 1.05f;
            if (u > 0.90f)      { r = 0.90f; g = 0.72f; b = 0.08f; }  // жълт връх
            else if (u > 0.86f) { r = 0.92f; g = 0.92f; b = 0.90f; }  // бяла ивица
            img.set(x, y, r, g, b);
        }
    return img;
}

Image glass(int size)
{
    Image img(size, size);
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
            float u = static_cast<float>(x) / size, v = static_cast<float>(y) / size;
            float r = mix(0.05f, 0.18f, v), g = mix(0.10f, 0.28f, v), b = mix(0.14f, 0.36f, v);
            // Диагонален отблясък - имитация на отражение на небето.
            float d = std::fabs(fract(u * 2 + v * 0.8f) - 0.5f);
            float hl = 1.0f - smoothstep(0.02f, 0.07f, d);
            img.set(x, y, r + 0.45f * hl, g + 0.5f * hl, b + 0.55f * hl);
        }
    return img;
}

Image helipad(int size)
{
    Image img(size, size);
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
            float u = static_cast<float>(x) / size, v = static_cast<float>(y) / size;
            float n = fbm(u * 16, v * 16, 4, 16, 61u);
            float c = 0.50f + 0.12f * (n - 0.5f);
            float r = c, g = c, b = c * 0.98f;

            // Фуги между бетонните плочи.
            if (fract(u * 4) < 0.006f || fract(v * 4) < 0.006f) { r *= 0.7f; g *= 0.7f; b *= 0.7f; }

            // Жълт кръг.
            float d = std::sqrt((u - 0.5f) * (u - 0.5f) + (v - 0.5f) * (v - 0.5f));
            if (d > 0.37f && d < 0.41f) { r = 0.85f; g = 0.68f; b = 0.10f; }

            // Бяла буква "H".
            bool leftBar  = u > 0.36f && u < 0.43f && v > 0.32f && v < 0.68f;
            bool rightBar = u > 0.57f && u < 0.64f && v > 0.32f && v < 0.68f;
            bool middle   = u > 0.36f && u < 0.64f && v > 0.47f && v < 0.53f;
            if (leftBar || rightBar || middle) { r = 0.92f; g = 0.92f; b = 0.90f; }

            // Бял бордюр.
            float e = std::min(std::min(u, 1 - u), std::min(v, 1 - v));
            if (e > 0.02f && e < 0.035f) { r = 0.9f; g = 0.9f; b = 0.88f; }

            img.set(x, y, r, g, b);
        }
    return img;
}

Image groundGrid(int size)
{
    // Текстурата е ЕДНО квадратче от мрежата: светъл бетон с тъмна линия по ръбовете. При повтаряне (GL_REPEAT) линиите на съседните копия се допират и се получава мрежа. Така при полет се вижда как земята "тече" под хеликоптера и се усеща скоростта.
    Image img(size, size);
    const int line = 3;                       // половин дебелина на линията (px)
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
            float u = static_cast<float>(x) / size, v = static_cast<float>(y) / size;
            float n = valueNoise(u * 16, v * 16, 16, 131u);
            float c = 0.60f + 0.05f * (n - 0.5f);
            bool edge = x < line || x >= size - line || y < line || y >= size - line;
            if (edge) c = 0.28f;
            img.set(x, y, c, c, c * 0.98f);
        }
    return img;
}

Image barrier(int size)
{
    // Едно квадратче от стената: почти прозрачен оранжев фон и плътна рамка.
    // Повторено върху стената, дава мрежа, през която се вижда.
    Image img(size, size);
    const int line = 4;
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
            bool edge = x < line || x >= size - line || y < line || y >= size - line;
            img.set(x, y, 1.0f, 0.45f, 0.08f, edge ? 0.85f : 0.12f);
        }
    return img;
}

Image rotorBlur(int size)
{
    // X = радиус (0..1), Y = ъгъл (0..1). Алфа каналът определя
    // прозрачността: в средата и при ръба дискът изчезва плавно.
    Image img(size, size);
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
            float r = static_cast<float>(x) / (size - 1);
            float ang = static_cast<float>(y) / size;
            float a = 0.50f * smoothstep(0.05f, 0.18f, r) * (1.0f - smoothstep(0.93f, 1.0f, r));
            a *= 0.85f + 0.15f * std::cos(ang * 6.2831853f * 4);       // 4 перки
            float cr = 0.12f, cg = 0.12f, cb = 0.13f;
            if (r > 0.90f && r < 0.97f) { cr = 0.8f; cg = 0.65f; cb = 0.1f; a *= 1.2f; } // жълт пръстен
            img.set(x, y, cr, cg, cb, a);
        }
    return img;
}

Image white()
{
    return Image(1, 1);   
}

} // namespace ProcTex
