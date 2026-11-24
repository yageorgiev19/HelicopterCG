// Hud.h - текст върху екрана (head-up display) със сянка за четимост.
//
// Текстът се рисува с растерен шрифт (FontData.h): всеки символ е
// правоъгълник (2 триъгълника) с текстурни координати към клетката на
// символа в атласа. Всички правоъгълници за кадъра се събират в един
// буфер и се рисуват с едно извикване в екранни координати (пиксели).
#pragma once

#include "Shader.h"

#include <glad/gl.h>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

class Hud {
public:
    Hud();
    ~Hud();

    // Започва нов кадър; scale = цял мащаб на шрифта (1 = 9x18 px на символ).
    void begin(int screenW, int screenH, int scale);

    // Текст в UTF-8 (с тъмна сянка отдолу); връща x след последния символ.
    // Координатите са в пиксели от горния ляв ъгъл на прозореца.
    float text(float x, float y, const std::string& utf8, const glm::vec4& color);

    // Ширина на текста в пиксели (шрифтът е моноширинен).
    float textWidth(const std::string& utf8) const;
    float lineHeight() const;

    void end();   // качва буфера и рисува всичко

private:
    struct V { float x, y, u, v, r, g, b, a; };
    void quad(float x0, float y0, float x1, float y1,
              float u0, float v0, float u1, float v1, const glm::vec4& c);
    int glyphIndex(unsigned codepoint) const;

    std::unique_ptr<Shader> shader;
    GLuint vao = 0, vbo = 0, tex = 0;
    std::vector<V> verts;
    int screenW = 1, screenH = 1, scale = 1;
};
