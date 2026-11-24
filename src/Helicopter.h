// Helicopter.h - модел (йерархия от примитиви), анимация и опростена физика.
#pragma once

#include "Assets.h"
#include "SceneNode.h"

#include <glm/glm.hpp>
#include <memory>

// Команди от клавиатурата за един кадър. Стойностите са в [-1, 1].
struct HeliInput {
    float collective = 0;   // W / S   - обща стъпка (нагоре / надолу)
    float yaw        = 0;   // A / D   - завъртане около вертикалата (опашен ротор)
    float pitch      = 0;   // стрелки нагоре/надолу - наклон напред/назад
    float roll       = 0;   // стрелки ляво/дясно   - наклон встрани
};

class Helicopter {
public:
    explicit Helicopter(const Assets& assets);

    void update(float dt, const HeliInput& input);

    void toggleEngine()      { engineOn = !engineOn; }
    void toggleSearchlight() { searchlightOn = !searchlightOn; }

    SceneNode* root() const { return rootNode.get(); }

    // Състояние, нужно на камерата, осветлението и заглавието на прозореца.
    glm::vec3 position{0.0f};      // точка на допир на плъзгачите със земята
    glm::vec3 velocity{0.0f};
    float yaw = 0.0f;              // [rad] посока на носа
    float rotorSpeed = 0.0f;       // 0 = спрян, 1 = номинални обороти
    bool  engineOn = false;
    bool  searchlightOn = true;

    glm::mat4 bodyMatrix() const;                 // световна матрица на корпуса
    glm::vec3 centerOfMass() const;
    glm::vec3 searchlightPosition() const;
    glm::vec3 searchlightDirection() const;
    bool onGround() const { return grounded; }

private:
    void build(const Assets& a);
    void step(float h, const HeliInput& in);       // една стъпка на физиката
    void animate(float dt);                        // обновява възлите в графа

    std::unique_ptr<SceneNode> rootNode;
    // Указатели към възлите, които се анимират.
    SceneNode* body = nullptr;
    SceneNode* mainRotor = nullptr;
    SceneNode* bladePivots[4] = {};
    SceneNode* blades[4] = {};
    SceneNode* tailRotor = nullptr;
    SceneNode* mainBlur = nullptr;
    SceneNode* tailBlur = nullptr;
    SceneNode* beacon = nullptr;
    SceneNode* strobe = nullptr;
    SceneNode* lamp = nullptr;

    // Вътрешно състояние на физиката.
    float pitch = 0.0f, roll = 0.0f;     // [rad] текущ наклон на корпуса
    float yawRate = 0.0f;                // [rad/s]
    float collective = 0.5f;             // 0..1, 0.5 ~ равновесие при висене
    bool  grounded = true;
    float accumulator = 0.0f;            // за фиксираната стъпка на физиката

    // Анимация.
    float mainAngle = 0.0f, tailAngle = 0.0f, time = 0.0f;
};
