// Renderer.cpp
#include "Renderer.h"
#include "ShaderSources.h"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {
// Параметри на осветлението (в линейно пространство, преди гама корекцията).
struct Lighting {
    glm::vec3 sunColor, skyAmbient, groundAmbient, zenith, horizon, groundFar;
    float sunDisc;      // яркост на слънчевия (лунния) диск в небето
    float fogDensity;
};
const Lighting kDay   = {{2.4f, 2.2f, 1.9f}, {0.32f, 0.40f, 0.55f}, {0.14f, 0.13f, 0.10f},
                         {0.12f, 0.30f, 0.85f}, {0.75f, 0.85f, 1.00f}, {0.24f, 0.24f, 0.23f},
                         1.0f, 0.0045f};
const Lighting kNight = {{0.10f, 0.13f, 0.22f}, {0.025f, 0.035f, 0.06f}, {0.01f, 0.01f, 0.012f},
                         {0.004f, 0.008f, 0.025f}, {0.03f, 0.04f, 0.07f}, {0.01f, 0.012f, 0.01f},
                         0.08f, 0.0060f};
const glm::vec3 kSpotColor(9.0f, 8.6f, 7.5f);
}

Renderer::Renderer()
    : sunDir(glm::normalize(glm::vec3(0.45f, 0.75f, 0.30f)))
{
    sceneShader = std::make_unique<Shader>(ShaderSrc::sceneVS, ShaderSrc::sceneFS, "scene");
    depthShader = std::make_unique<Shader>(ShaderSrc::depthVS, ShaderSrc::depthFS, "depth");
    skyShader   = std::make_unique<Shader>(ShaderSrc::skyVS,   ShaderSrc::skyFS,   "sky");

    // --- Текстура за дълбочина + framebuffer за картата на сенките --------
    glGenTextures(1, &shadowTex);
    glBindTexture(GL_TEXTURE_2D, shadowTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, kShadowSize, kShadowSize, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // Извън картата: дълбочина 1 = "няма сянка".
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    const float border[4] = {1, 1, 1, 1};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    // Хардуерно сравнение: texture() в шейдъра връща 0 или 1 (или междинно
    // при линеен филтър), вместо самата дълбочина.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    glGenFramebuffers(1, &shadowFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowTex, 0);
    glDrawBuffer(GL_NONE);   // без цветен буфер
    glReadBuffer(GL_NONE);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::fprintf(stderr, "Framebuffer-ът за сенките е непълен.\n");
        std::exit(EXIT_FAILURE);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glGenVertexArrays(1, &emptyVao);   // Core профилът изисква VAO дори без атрибути
}

Renderer::~Renderer()
{
    glDeleteVertexArrays(1, &emptyVao);
    glDeleteFramebuffers(1, &shadowFbo);
    glDeleteTextures(1, &shadowTex);
}

// Ортографска проекция от посоката на слънцето. Центърът се "закрепва" към мрежата от тексели, за да не трептят ръбовете на сенките, когато  хеликоптерът (и заедно с него картата) се движи.
glm::mat4 Renderer::computeLightSpace(const glm::vec3& center) const
{
    glm::mat4 lightView = glm::lookAt(sunDir * 100.0f, glm::vec3(0), glm::vec3(0, 1, 0));
    glm::vec4 c = lightView * glm::vec4(center, 1.0f);
    float texel = 2.0f * kShadowHalfExtent / kShadowSize;
    c.x = std::floor(c.x / texel) * texel;
    c.y = std::floor(c.y / texel) * texel;
    const float h = kShadowHalfExtent;
    glm::mat4 lightProj = glm::ortho(c.x - h, c.x + h, c.y - h, c.y + h,
                                     -c.z - 150.0f, -c.z + 150.0f);
    return lightProj * lightView;
}

void Renderer::render(const std::vector<DrawItem>& items, const FrameParams& f)
{
    glm::mat4 lightSpace = computeLightSpace(f.shadowCenter);
    shadowPass(items, lightSpace);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, f.width, f.height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    skyPass(f);
    scenePass(items, f, lightSpace);
}

void Renderer::shadowPass(const std::vector<DrawItem>& items, const glm::mat4& lightSpace)
{
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo);
    glViewport(0, 0, kShadowSize, kShadowSize);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);                 // тънките детайли (перки) имат значение
    glEnable(GL_POLYGON_OFFSET_FILL);        // допълнително отместване на дълбочината
    glPolygonOffset(2.0f, 4.0f);

    depthShader->use();
    depthShader->set("uLightSpace", lightSpace);
    for (const DrawItem& it : items) {
        if (!it.material->castShadow || it.material->transparent) continue;
        depthShader->set("uModel", it.model);
        it.mesh->draw();
    }
    glDisable(GL_POLYGON_OFFSET_FILL);
}

void Renderer::skyPass(const FrameParams& f)
{
    // Матрицата на изгледа без транслация -> небето е "безкрайно далече".
    glm::mat4 viewRot = glm::mat4(glm::mat3(f.view));
    skyShader->use();
    skyShader->set("uInvViewProj", glm::inverse(f.proj * viewRot));
    const Lighting& L = night ? kNight : kDay;
    skyShader->set("uSunDir", sunDir);
    skyShader->set("uSunIntensity", L.sunDisc);
    skyShader->set("uZenith", L.zenith);
    skyShader->set("uHorizon", L.horizon);
    skyShader->set("uGroundFar", L.groundFar);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glBindVertexArray(emptyVao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

void Renderer::drawItem(const DrawItem& it)
{
    const Material& m = *it.material;
    Shader& s = *sceneShader;
    s.set("uModel", it.model);
    // Нормалите се трансформират с обратната транспонирана матрица, иначе при нееднакво мащабиране (елипсоид) те престават да са перпендикулярни на повърхността.
    s.set("uNormalMat", glm::transpose(glm::inverse(glm::mat3(it.model))));
    s.set("uTint", m.tint);
    s.set("uUvScale", m.uvScale);
    s.set("uSpecular", m.specular);
    s.set("uShininess", m.shininess);
    s.set("uEmissive", m.emissive);
    s.set("uAlpha", m.alpha);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m.texture);
    it.mesh->draw();
}

void Renderer::scenePass(const std::vector<DrawItem>& items, const FrameParams& f,
                         const glm::mat4& lightSpace)
{
    const Lighting& L = night ? kNight : kDay;
    Shader& s = *sceneShader;
    s.use();
    s.set("uViewProj", f.proj * f.view);
    s.set("uLightSpace", lightSpace);
    s.set("uCamPos", f.cameraPos);
    s.set("uSunDir", sunDir);
    s.set("uSunColor", L.sunColor);
    s.set("uSkyAmbient", L.skyAmbient);
    s.set("uGroundAmbient", L.groundAmbient);
    s.set("uFogColor", L.horizon);
    s.set("uFogDensity", L.fogDensity);
    s.set("uSpotOn", f.spotOn ? 1 : 0);
    s.set("uSpotPos", f.spotPos);
    s.set("uSpotDir", f.spotDir);
    s.set("uSpotColor", kSpotColor);
    s.set("uSpotCosInner", std::cos(glm::radians(11.0f)));
    s.set("uSpotCosOuter", std::cos(glm::radians(17.0f)));
    s.set("uTex", 0);
    s.set("uShadowMap", 1);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadowTex);

    // Плътни обекти - с отрязване на задните стени (back-face culling).
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    std::vector<const DrawItem*> transparent;
    for (const DrawItem& it : items) {
        if (it.material->transparent) { transparent.push_back(&it); continue; }
        drawItem(it);
    }

    // Прозрачни обекти - от най-далечния към най-близкия (алгоритъм на художника), със смесване и без запис в буфера на дълбочината.
    std::sort(transparent.begin(), transparent.end(),
              [&](const DrawItem* a, const DrawItem* b) {
                  float da = glm::length(glm::vec3(a->model[3]) - f.cameraPos);
                  float db = glm::length(glm::vec3(b->model[3]) - f.cameraPos);
                  return da > db;
              });
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    for (const DrawItem* it : transparent) drawItem(*it);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

}
