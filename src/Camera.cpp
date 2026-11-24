// Camera.cpp
#include "Camera.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

void Camera::update(float dt, const glm::vec3& heliCenter, float heliYaw)
{
    if (first) { target = heliCenter; smoothYaw = heliYaw; first = false; }

    // Експоненциално изглаждане - камерата "догонва" целта плавно,
    // вместо да е закована за нея (иначе всяко трепване се вижда).
    float k = 1.0f - std::exp(-dt * 6.0f);
    target += (heliCenter - target) * k;

    // Изглаждане на ъгъла по най-късия път (за да не се завърти на 350°).
    float diff = std::remainder(heliYaw - smoothYaw, glm::two_pi<float>());
    smoothYaw += diff * (1.0f - std::exp(-dt * 2.5f));

    float az = chase ? smoothYaw + azimuth : azimuth;
    glm::vec3 offset(std::sin(az) * std::cos(elevation),
                     std::sin(elevation),
                     std::cos(az) * std::cos(elevation));
    eyePos = target + offset * distance;
    eyePos.y = std::max(eyePos.y, 0.4f);   // да не влиза под земята
}

void Camera::rotate(float dx, float dy)
{
    azimuth   -= dx * 0.005f;
    elevation += dy * 0.005f;
    elevation = std::clamp(elevation, -0.15f, 1.45f);
}

void Camera::zoom(float steps)
{
    distance *= std::pow(0.9f, steps);
    distance = std::clamp(distance, 5.0f, 80.0f);
}

glm::mat4 Camera::view() const
{
    return glm::lookAt(eyePos, target, glm::vec3(0, 1, 0));
}

glm::mat4 Camera::projection(float aspect) const
{
    // Перспективна проекция: зрителен ъгъл 50°, близка равнина 0.1 m, далечна 800 m.
    return glm::perspective(glm::radians(50.0f), aspect, 0.1f, 800.0f);
}
