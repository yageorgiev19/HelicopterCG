// Mesh.h - геометрия в паметта на видеокартата и процедурни примитиви.
//
// Всеки връх съдържа позиция, нормала и текстурни координати (u, v).
// Върховете се записват в VBO, индексите в EBO, а форматът им се описва
// веднъж във VAO. При рисуване е достатъчно да се "върже" VAO и да се
// извика glDrawElements.
#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>
#include <vector>

struct Vertex {
    glm::vec3 position;  // позиция в локалната (моделна) координатна система
    glm::vec3 normal;    // единична нормала - нужна за осветлението
    glm::vec2 uv;        // текстурни координати
};

class Mesh {
public:
    Mesh() = default;
    Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned>& indices);
    ~Mesh();

    // Обектът притежава OpenGL ресурси, затова не се копира, а само се мести.
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    void draw() const;
    unsigned triangleCount() const { return indexCount / 3; }

private:
    GLuint vao = 0, vbo = 0, ebo = 0;
    GLsizei indexCount = 0;
};

// ---------------------------------------------------------------------------
// Процедурни примитиви. Всички са центрирани в началото на координатната
// система и имат размер около 1, а реалният размер се задава с мащабиране
// в йерархията на сцената.
// ---------------------------------------------------------------------------
namespace Primitives {
    // Куб със страна 1. Всяка стена има собствени 4 върха, за да има
    // остри ръбове (отделни нормали) и собствени текстурни координати.
    Mesh* box();

    // Пресечен конус (цилиндър при rBottom == rTop) с височина 1 по оста Y.
    Mesh* cylinder(int segments, float rBottom, float rTop, bool caps);

    // UV сфера с радиус 1. Мащабирана нееднакво дава елипсоид.
    Mesh* sphere(int stacks, int slices);

    // Хоризонтална равнина size x size, текстурата се повтаря uvRepeat пъти.
    Mesh* plane(float size, float uvRepeat);

    // Плосък диск с радиус 1 (за размазания диск на въртящия се ротор).
    Mesh* disc(int segments);
}
