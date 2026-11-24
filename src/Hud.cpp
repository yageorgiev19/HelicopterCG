// Hud.cpp
#include "Hud.h"
#include "FontData.h"

#include <cstddef>

namespace {

const char* hudVS = R"GLSL(
#version 330 core
layout(location = 0) in vec2 aPos;     // пиксели, (0,0) = горе вляво
layout(location = 1) in vec2 aUV;      // координати в атласа на шрифта
layout(location = 2) in vec4 aColor;
uniform vec2 uScreen;
out vec2 vUV;
out vec4 vColor;
void main()
{
    vec2 ndc = vec2(aPos.x / uScreen.x * 2.0 - 1.0, 1.0 - aPos.y / uScreen.y * 2.0);
    vUV = aUV;
    vColor = aColor;
    gl_Position = vec4(ndc, 0.0, 1.0);
}
)GLSL";

const char* hudFS = R"GLSL(
#version 330 core
in vec2 vUV;
in vec4 vColor;
out vec4 FragColor;
uniform sampler2D uFont;
void main()
{
    float a = texture(uFont, vUV).r;   // атласът пази само яркостта на символа
    FragColor = vec4(vColor.rgb, vColor.a * a);
}
)GLSL";


unsigned nextCodepoint(const std::string& s, size_t& i)
{
    unsigned char c = static_cast<unsigned char>(s[i]);
    unsigned cp = c;
    int extra = 0;
    if      (c >= 0xF0) { cp = c & 0x07; extra = 3; }
    else if (c >= 0xE0) { cp = c & 0x0F; extra = 2; }
    else if (c >= 0xC0) { cp = c & 0x1F; extra = 1; }
    ++i;
    while (extra-- > 0 && i < s.size()) {
        cp = (cp << 6) | (static_cast<unsigned char>(s[i]) & 0x3F);
        ++i;
    }
    return cp;
}

} 

Hud::Hud()
{
    shader = std::make_unique<Shader>(hudVS, hudFS, "hud");

    
    std::vector<unsigned char> pixels(FontData::kAtlasW * FontData::kAtlasH);
    for (size_t i = 0; i < pixels.size(); ++i) {
        unsigned char b = FontData::kPixels4[i / 2];
        unsigned char n = (i % 2 == 0) ? (b >> 4) : (b & 0x0F);
        pixels[i] = static_cast<unsigned char>(n * 17);   // 0..15 -> 0..255
    }
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, FontData::kAtlasW, FontData::kAtlasH, 0,
                 GL_RED, GL_UNSIGNED_BYTE, pixels.data());
    // NEAREST: при цял мащаб символите остават остри, без размазване.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V, x));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V, u));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V, r));
    glBindVertexArray(0);
}

Hud::~Hud()
{
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteTextures(1, &tex);
}

void Hud::begin(int w, int h, int s)
{
    screenW = w; screenH = h; scale = s < 1 ? 1 : s;
    verts.clear();
}

float Hud::lineHeight() const { return static_cast<float>(FontData::kCellH * scale); }

void Hud::quad(float x0, float y0, float x1, float y1,
               float u0, float v0, float u1, float v1, const glm::vec4& c)
{
    V a{x0, y0, u0, v0, c.r, c.g, c.b, c.a}, b{x1, y0, u1, v0, c.r, c.g, c.b, c.a};
    V d{x1, y1, u1, v1, c.r, c.g, c.b, c.a}, e{x0, y1, u0, v1, c.r, c.g, c.b, c.a};
    verts.insert(verts.end(), {a, b, d, a, d, e});
}

int Hud::glyphIndex(unsigned cp) const
{
    const int n = static_cast<int>(sizeof(FontData::kCodepoints) / sizeof(FontData::kCodepoints[0]));
    for (int i = 0; i < n; ++i)
        if (FontData::kCodepoints[i] == cp) return i;
    return '?' - 32;   // непознат символ -> "?"
}

float Hud::textWidth(const std::string& s) const
{
    size_t i = 0;
    int count = 0;
    while (i < s.size()) { nextCodepoint(s, i); ++count; }
    return static_cast<float>(count * FontData::kCellW * scale);
}

float Hud::text(float x, float y, const std::string& s, const glm::vec4& color)
{
    const float cw = static_cast<float>(FontData::kCellW * scale);
    const float ch = static_cast<float>(FontData::kCellH * scale);
    float endX = x;

    // Два прохода: първо тъмна сянка, отместена с 1 px надясно и надолу,
    // после самият текст. Сянката прави буквите четими и без фон,
    // както върху светлата земя, така и върху тъмното нощно небе.
    const float off = static_cast<float>(scale);
    const glm::vec4 shadow(0.0f, 0.0f, 0.0f, 0.85f * color.a);
    for (int pass = 0; pass < 2; ++pass) {
        const float dx = pass == 0 ? off : 0.0f;
        const glm::vec4& c = pass == 0 ? shadow : color;
        float cx = x;
        size_t i = 0;
        while (i < s.size()) {
            unsigned cp = nextCodepoint(s, i);
            if (cp != ' ') {
                int g = glyphIndex(cp);
                float u0 = static_cast<float>((g % FontData::kCols) * FontData::kCellW) / FontData::kAtlasW;
                float v0 = static_cast<float>((g / FontData::kCols) * FontData::kCellH) / FontData::kAtlasH;
                float u1 = u0 + static_cast<float>(FontData::kCellW) / FontData::kAtlasW;
                float v1 = v0 + static_cast<float>(FontData::kCellH) / FontData::kAtlasH;
                quad(cx + dx, y + dx, cx + cw + dx, y + ch + dx, u0, v0, u1, v1, c);
            }
            cx += cw;
        }
        endX = cx;
    }
    return endX;
}

void Hud::end()
{
    if (verts.empty()) return;

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(V), verts.data(), GL_DYNAMIC_DRAW);

    // Над всичко: без дълбочина, със смесване (краищата на буквите са полупрозрачни).
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    shader->use();
    shader->set("uScreen", glm::vec2(static_cast<float>(screenW), static_cast<float>(screenH)));
    shader->set("uFont", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(verts.size()));

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}
