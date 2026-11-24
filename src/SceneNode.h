// SceneNode.h - йерархичен граф на сцената (scene graph).
//
// Всеки възел има локална трансформация спрямо родителя си:
//     M_local = T(position) * R(rotation)
// Световната матрица се получава като произведение по веригата от корена:
//     M_world = M_parent_world * M_local
// Така, ако завъртим главината на ротора, перките (нейни деца) се въртят
// заедно с нея, а ако преместим целия хеликоптер - всичко се мести заедно.
//
// shapeScale мащабира САМО геометрията на конкретния възел и не се
// предава на децата. Иначе мащабирането на тялото като елипсоид би
// изкривило и перките, плъзгачите и т.н.
#pragma once

#include "Mesh.h"

#include <glad/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <string>
#include <vector>

struct Material {
    GLuint    texture     = 0;              // процедурна текстура
    glm::vec3 tint        {1.0f};           // умножава се с цвета от текстурата
    glm::vec2 uvScale     {1.0f};           // колко пъти се повтаря текстурата
    float     specular    = 0.25f;          // сила на огледалния отблясък
    float     shininess   = 32.0f;          // "острота" на отблясъка (Blinn-Phong)
    glm::vec3 emissive    {0.0f};           // собствено светене (навигационни светлини)
    float     alpha       = 1.0f;           // прозрачност (1 = плътен)
    bool      transparent = false;          // рисува се след плътните обекти
    bool      castShadow  = true;           // участва ли в картата на сенките
};

// Един елемент за рисуване: геометрия + крайна матрица + материал.
struct DrawItem {
    const Mesh*     mesh;
    glm::mat4       model;
    const Material* material;
};

class SceneNode {
public:
    explicit SceneNode(std::string n = "") : name(std::move(n)) {}

    std::string name;
    glm::vec3 position  {0.0f};
    glm::quat rotation  {1.0f, 0.0f, 0.0f, 0.0f};   // кватернион (без завъртане)
    glm::vec3 shapeScale{1.0f};
    bool      visible = true;                       // скрива възела и децата му

    const Mesh* mesh = nullptr;   // може да е nullptr - "празен" възел-шарнир
    Material    material;

    SceneNode* addChild(std::unique_ptr<SceneNode> child);
    SceneNode* addChild(const std::string& childName);   // празен възел

    glm::mat4 localMatrix() const;

    // Обхожда дървото в дълбочина и събира всичко за рисуване.
    void collect(const glm::mat4& parentWorld, std::vector<DrawItem>& out) const;

private:
    std::vector<std::unique_ptr<SceneNode>> children;
};
