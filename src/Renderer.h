// Renderer.h - кадърът се рисува на три прохода:
//   1) карта на сенките (дълбочина от гледната точка на слънцето);
//   2) небе;
//   3) сцената - първо плътните обекти, после прозрачните (сортирани).
#pragma once

#include "SceneNode.h"
#include "Shader.h"

#include <glad/gl.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>

struct FrameParams {
    glm::mat4 view, proj;
    glm::vec3 cameraPos;
    glm::vec3 shadowCenter;          // около коя точка да е картата на сенките
    bool      spotOn = false;
    glm::vec3 spotPos{0}, spotDir{0, -1, 0};
    int       width = 1, height = 1;
};

class Renderer {
public:
    Renderer();
    ~Renderer();

    void render(const std::vector<DrawItem>& items, const FrameParams& f);
    void toggleNight() { night = !night; }
    bool isNight() const { return night; }

private:
    glm::mat4 computeLightSpace(const glm::vec3& center) const;
    void shadowPass(const std::vector<DrawItem>& items, const glm::mat4& lightSpace);
    void skyPass(const FrameParams& f);
    void scenePass(const std::vector<DrawItem>& items, const FrameParams& f,
                   const glm::mat4& lightSpace);
    void drawItem(const DrawItem& it);

    std::unique_ptr<Shader> sceneShader, depthShader, skyShader;

    static constexpr int kShadowSize = 2048;
    static constexpr float kShadowHalfExtent = 45.0f;   // покрива 90 x 90 m
    GLuint shadowFbo = 0, shadowTex = 0;
    GLuint emptyVao = 0;                                // за небето

    glm::vec3 sunDir;
    bool night = false;
};
