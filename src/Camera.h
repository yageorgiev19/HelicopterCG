// Camera.h - камера, която обикаля около хеликоптера (orbit / chase).
//
// Позицията се задава в сферични координати спрямо целта:
//   азимут (около вертикалата), елевация (над хоризонта) и разстояние.
// В режим "преследване" азимутът е относителен спрямо посоката на носа,
// така камерата остава зад хеликоптера, докато той завива.
#pragma once

#include <glm/glm.hpp>

class Camera {
public:
    void update(float dt, const glm::vec3& heliCenter, float heliYaw);

    void rotate(float dxPixels, float dyPixels);   // влачене с мишката
    void zoom(float scrollSteps);                  // колелце на мишката
    void toggleMode() { chase = !chase; }
    bool isChase() const { return chase; }

    glm::mat4 view() const;
    glm::mat4 projection(float aspect) const;
    glm::vec3 eye() const { return eyePos; }

private:
    float azimuth   = 0.6f;    // [rad]
    float elevation = 0.28f;   // [rad]
    float distance  = 14.0f;   // [m]
    bool  chase     = true;

    float smoothYaw = 0.0f;    // изгладена посока на хеликоптера
    glm::vec3 target{0.0f, 1.5f, 0.0f};
    glm::vec3 eyePos{0.0f, 5.0f, 16.0f};
    bool first = true;
};
