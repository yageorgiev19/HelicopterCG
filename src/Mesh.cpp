// Mesh.cpp - качване на геометрия в GPU и генериране на примитиви.
#include "Mesh.h"

#include <glm/gtc/constants.hpp>
#include <cmath>
#include <cstddef>   // offsetof

Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned>& indices)
    : indexCount(static_cast<GLsizei>(indices.size()))
{
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    // VAO "запомня" всички настройки по-долу (кой буфер, какъв формат).
    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex),
                 vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned),
                 indices.data(), GL_STATIC_DRAW);

    // Атрибут 0 = позиция, 1 = нормала, 2 = текстурни координати.
    // Данните са "преплетени" (interleaved): pos|normal|uv|pos|normal|uv...
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, uv));

    glBindVertexArray(0);
}

Mesh::~Mesh()
{
    glDeleteBuffers(1, &ebo);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
}

void Mesh::draw() const
{
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
}

//  Примитиви. Триъгълниците са подредени обратно на часовниковата стрелка
//  (CCW), гледано отвън - така работи отрязването на задните стени.
namespace Primitives {

Mesh* box()
{
    std::vector<Vertex> v;
    std::vector<unsigned> idx;

    // За всяка стена: нормала n и две оси u, v в равнината ѝ, като u x v = n.
    struct Face { glm::vec3 n, u, vv; };
    const Face faces[6] = {
        {{ 1, 0, 0}, { 0, 0,-1}, {0, 1, 0}},
        {{-1, 0, 0}, { 0, 0, 1}, {0, 1, 0}},
        {{ 0, 1, 0}, { 1, 0, 0}, {0, 0,-1}},
        {{ 0,-1, 0}, { 1, 0, 0}, {0, 0, 1}},
        {{ 0, 0, 1}, { 1, 0, 0}, {0, 1, 0}},
        {{ 0, 0,-1}, {-1, 0, 0}, {0, 1, 0}},
    };
    for (const Face& f : faces) {
        unsigned base = static_cast<unsigned>(v.size());
        glm::vec3 c = f.n * 0.5f;
        v.push_back({c - 0.5f * f.u - 0.5f * f.vv, f.n, {0, 0}});
        v.push_back({c + 0.5f * f.u - 0.5f * f.vv, f.n, {1, 0}});
        v.push_back({c + 0.5f * f.u + 0.5f * f.vv, f.n, {1, 1}});
        v.push_back({c - 0.5f * f.u + 0.5f * f.vv, f.n, {0, 1}});
        idx.insert(idx.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    }
    return new Mesh(v, idx);
}

Mesh* cylinder(int segments, float rBottom, float rTop, bool caps)
{
    std::vector<Vertex> v;
    std::vector<unsigned> idx;
    const float twoPi = glm::two_pi<float>();

    // Страничната повърхност. За пресечен конус нормалата е наклонена:
    // n ~ (cos t, rBottom - rTop, sin t) при височина 1.
    for (int j = 0; j <= segments; ++j) {
        float t = twoPi * j / segments;
        float c = std::cos(t), s = std::sin(t);
        glm::vec3 n = glm::normalize(glm::vec3(c, rBottom - rTop, s));
        float u = static_cast<float>(j) / segments;
        v.push_back({{rBottom * c, -0.5f, rBottom * s}, n, {u, 0}});
        v.push_back({{rTop * c,     0.5f, rTop * s},    n, {u, 1}});
    }
    for (int j = 0; j < segments; ++j) {
        unsigned b0 = 2 * j, t0 = b0 + 1, b1 = b0 + 2, t1 = b0 + 3;
        idx.insert(idx.end(), {t0, b1, b0, t0, t1, b1});
    }

    if (caps) {
        // Капаците са ветрила от триъгълници около централен връх.
        for (int side = 0; side < 2; ++side) {
            float y = side == 0 ? 0.5f : -0.5f;
            float r = side == 0 ? rTop : rBottom;
            glm::vec3 n(0, side == 0 ? 1.0f : -1.0f, 0);
            unsigned center = static_cast<unsigned>(v.size());
            v.push_back({{0, y, 0}, n, {0.5f, 0.5f}});
            for (int j = 0; j <= segments; ++j) {
                float t = twoPi * j / segments;
                float c = std::cos(t), s = std::sin(t);
                v.push_back({{r * c, y, r * s}, n, {0.5f + 0.5f * c, 0.5f + 0.5f * s}});
            }
            for (int j = 0; j < segments; ++j) {
                unsigned a = center + 1 + j, b = a + 1;
                if (side == 0) idx.insert(idx.end(), {center, b, a});
                else           idx.insert(idx.end(), {center, a, b});
            }
        }
    }
    return new Mesh(v, idx);
}

Mesh* sphere(int stacks, int slices)
{
    std::vector<Vertex> v;
    std::vector<unsigned> idx;
    const float pi = glm::pi<float>();

    // Сферични координати: phi от северния (0) до южния (pi) полюс,
    // theta - около оста Y. За единична сфера нормалата съвпада с позицията.
    for (int i = 0; i <= stacks; ++i) {
        float phi = pi * i / stacks;
        for (int j = 0; j <= slices; ++j) {
            float theta = 2.0f * pi * j / slices;
            glm::vec3 p(std::sin(phi) * std::cos(theta), std::cos(phi),
                        std::sin(phi) * std::sin(theta));
            v.push_back({p, p, {static_cast<float>(j) / slices,
                                1.0f - static_cast<float>(i) / stacks}});
        }
    }
    const unsigned row = slices + 1;
    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < slices; ++j) {
            unsigned a = i * row + j, b = a + row, c = b + 1, d = a + 1;
            idx.insert(idx.end(), {a, c, b, a, d, c});
        }
    }
    return new Mesh(v, idx);
}

Mesh* plane(float size, float uvRepeat)
{
    float h = size * 0.5f;
    glm::vec3 n(0, 1, 0);
    std::vector<Vertex> v = {
        {{-h, 0,  h}, n, {0, 0}},
        {{ h, 0,  h}, n, {uvRepeat, 0}},
        {{ h, 0, -h}, n, {uvRepeat, uvRepeat}},
        {{-h, 0, -h}, n, {0, uvRepeat}},
    };
    std::vector<unsigned> idx = {0, 1, 2, 0, 2, 3};
    return new Mesh(v, idx);
}

Mesh* disc(int segments)
{
    // Пръстен от вътрешен радиус 0.06 (главината) до 1.
    // u = радиус (0..1), v = ъгъл (0..1) - удобно за радиална текстура.
    std::vector<Vertex> v;
    std::vector<unsigned> idx;
    const float twoPi = glm::two_pi<float>();
    const float rIn = 0.06f;
    glm::vec3 n(0, 1, 0);
    for (int j = 0; j <= segments; ++j) {
        float t = twoPi * j / segments;
        float c = std::cos(t), s = std::sin(t);
        float vv = static_cast<float>(j) / segments;
        v.push_back({{rIn * c, 0, rIn * s}, n, {0.0f, vv}});
        v.push_back({{c, 0, s},             n, {1.0f, vv}});
    }
    for (int j = 0; j < segments; ++j) {
        unsigned i0 = 2 * j, o0 = i0 + 1, i1 = i0 + 2, o1 = i0 + 3;
        idx.insert(idx.end(), {i0, o1, o0, i0, i1, o1});
    }
    return new Mesh(v, idx);
}

} 