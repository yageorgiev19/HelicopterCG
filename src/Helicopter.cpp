// Helicopter.cpp - изграждане на модела, физика и анимация.
//
// Координатна система на модела: Y е нагоре, носът сочи към -Z,
// десният борд е към +X. Мерните единици са метри и секунди.
#include "Helicopter.h"
#include "WorldBounds.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace {

constexpr float kPivotY   = 1.5f;     // височина на центъра на масата над плъзгачите
constexpr float kGravity  = 9.81f;
constexpr float kPhysicsH = 1.0f / 120.0f;   // фиксирана стъпка на физиката [s]

// Позиция на детайл, зададена спрямо земята, превърната спрямо центъра на масата.
glm::vec3 at(float x, float y, float z) { return {x, y - kPivotY, z}; }

glm::quat axisAngle(float deg, const glm::vec3& axis)
{
    return glm::angleAxis(glm::radians(deg), axis);
}

const glm::vec3 X(1, 0, 0), Y(0, 1, 0), Z(0, 0, 1);

// Създава детайл (възел с геометрия) и го закача към родителя.
SceneNode* part(SceneNode* parent, const char* name, const Mesh* mesh,
                const glm::vec3& pos, const glm::vec3& scale, GLuint tex,
                const glm::quat& rot = glm::quat(1, 0, 0, 0))
{
    auto n = std::make_unique<SceneNode>(name);
    n->mesh = mesh;
    n->position = pos;
    n->shapeScale = scale;
    n->rotation = rot;
    n->material.texture = tex;
    return parent->addChild(std::move(n));
}

// Експоненциално приближаване към целева стойност. Не зависи от честотата
// на кадрите: след време tau се изминават ~63% от разстоянието до целта.
float approach(float current, float target, float tau, float dt)
{
    return current + (target - current) * (1.0f - std::exp(-dt / tau));
}

float smoothstep(float e0, float e1, float x)
{
    float t = std::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

} // namespace


Helicopter::Helicopter(const Assets& assets)
{
    build(assets);
    animate(0.0f);
}

void Helicopter::build(const Assets& a)
{
    rootNode = std::make_unique<SceneNode>("helicopter");

    // "body" е шарнирът, около който корпусът се накланя и завърта.
    body = rootNode->addChild("body");
    body->position = {0, kPivotY, 0};

    // --- Фюзелаж и кабина ------------------------------------------------
    SceneNode* n = part(body, "fuselage", a.sphere.get(), at(0, 1.55f, 0),
                        {1.15f, 1.05f, 2.1f}, a.camo);
    n->material.uvScale = {2, 1};

    n = part(body, "cockpit", a.sphere.get(), at(0, 1.72f, -1.30f),
             {0.98f, 0.82f, 1.05f}, a.glass);
    n->material.specular = 1.2f;
    n->material.shininess = 96;

    n = part(body, "nose", a.sphere.get(), at(0, 1.25f, -1.75f),
             {0.75f, 0.55f, 0.6f}, a.camo);

    // --- Двигател, изпускателна тръба, мачта ------------------------------
    n = part(body, "engine", a.sphere.get(), at(0, 2.45f, 0.45f),
             {0.62f, 0.40f, 1.15f}, a.metal);
    n->material.tint = {0.75f, 0.8f, 0.72f};
    n->material.specular = 0.6f;

    n = part(body, "exhaust", a.cylinder.get(), at(0, 2.45f, 1.55f),
             {0.20f, 0.35f, 0.20f}, a.metal, axisAngle(90, X));
    n->material.tint = glm::vec3(0.25f);

    n = part(body, "mast", a.cylinder.get(), at(0, 2.95f, 0.1f),
             {0.11f, 0.6f, 0.11f}, a.metal);

    // --- Опашна греда, кил, стабилизатор ----------------------------------
    // Пресеченият конус е по оста Y; завъртане на +90 около X го насочва към +Z.
    part(body, "tailBoom", a.taperedCylinder.get(), at(0, 1.78f, 4.2f),
         {0.40f, 4.6f, 0.40f}, a.camo, axisAngle(90, X))->material.uvScale = {2, 4};

    part(body, "fin", a.box.get(), at(0, 2.35f, 6.25f),
         {0.08f, 1.35f, 0.85f}, a.camo, axisAngle(25, X));

    part(body, "stabilizer", a.box.get(), at(0, 1.82f, 5.55f),
         {1.9f, 0.06f, 0.5f}, a.camo);

    // --- Плъзгачи (ски) ----------------------------------------------------
    for (int side = -1; side <= 1; side += 2) {
        float x = 0.85f * side;
        part(body, "skid", a.cylinder.get(), at(x, 0.06f, 0.0f),
             {0.06f, 3.6f, 0.06f}, a.metal, axisAngle(90, X))->material.tint = glm::vec3(0.35f);
        part(body, "skidTip", a.cylinder.get(), at(x, 0.21f, -2.0f),
             {0.06f, 0.5f, 0.06f}, a.metal, axisAngle(-53, X))->material.tint = glm::vec3(0.35f);
        for (int fb = -1; fb <= 1; fb += 2) {
            // Подпорите са наклонени навътре: горният край е по-близо до оста.
            part(body, "strut", a.cylinder.get(), at(0.78f * side, 0.52f, 0.85f * fb),
                 {0.05f, 0.95f, 0.05f}, a.metal, axisAngle(15.0f * side, Z))
                ->material.tint = glm::vec3(0.35f);
        }
    }

    // --- Главен ротор -----------------------------------------------------
    // mainRotor се върти около Y. Всяка перка е закачена на собствен шарнир
    // (bladePivot), завъртян на k*90 градуса - така 4 еднакви перки
    // се получават от една и съща геометрия.
    mainRotor = body->addChild("mainRotor");
    mainRotor->position = at(0, 3.27f, 0.1f);
    part(mainRotor, "hub", a.cylinder.get(), {0, 0, 0}, {0.32f, 0.2f, 0.32f}, a.metal);

    const float bladeLen = 5.0f;
    for (int k = 0; k < 4; ++k) {
        bladePivots[k] = mainRotor->addChild("bladePivot");
        blades[k] = part(bladePivots[k], "blade", a.box.get(),
                         {0.25f + bladeLen * 0.5f, 0.0f, 0.0f},
                         {bladeLen, 0.05f, 0.32f}, a.blade);
        blades[k]->material.specular = 0.4f;
    }

    // Полупрозрачен диск - имитация на размазването при бързо въртене.
    // Закачен е към корпуса (не към ротора), за да не се върти с перките.
    mainBlur = part(body, "mainBlur", a.disc.get(), at(0, 3.29f, 0.1f),
                    {5.3f, 1.0f, 5.3f}, a.rotorBlur);
    mainBlur->material.transparent = true;
    mainBlur->material.castShadow = false;
    mainBlur->material.specular = 0.0f;

    // --- Опашен ротор (върти се около X) -----------------------------------
    tailRotor = body->addChild("tailRotor");
    tailRotor->position = at(0.16f, 2.72f, 6.62f);
    part(tailRotor, "tailHub", a.cylinder.get(), {0, 0, 0}, {0.1f, 0.18f, 0.1f},
         a.metal, axisAngle(90, Z));
    for (int k = 0; k < 2; ++k)
        part(tailRotor, "tailBlade", a.box.get(), {0.05f, 0, 0}, {0.03f, 1.7f, 0.14f},
             a.white, axisAngle(90.0f * k, X))->material.tint = glm::vec3(0.15f);

    tailBlur = part(body, "tailBlur", a.disc.get(), at(0.2f, 2.72f, 6.62f),
                    {0.9f, 1.0f, 0.9f}, a.rotorBlur, axisAngle(90, Z));
    tailBlur->material.transparent = true;
    tailBlur->material.castShadow = false;

    // --- Светлини ------------------------------------------------------------
    // Навигационни: червена вляво, зелена вдясно (авиационен стандарт).
    n = part(body, "navRed", a.sphere.get(), at(-0.95f, 1.82f, 5.55f), glm::vec3(0.07f), a.white);
    n->material.tint = {1, 0.1f, 0.1f};
    n->material.emissive = {2.5f, 0.1f, 0.05f};
    n->material.castShadow = false;
    n = part(body, "navGreen", a.sphere.get(), at(0.95f, 1.82f, 5.55f), glm::vec3(0.07f), a.white);
    n->material.tint = {0.1f, 1, 0.2f};
    n->material.emissive = {0.1f, 2.5f, 0.3f};
    n->material.castShadow = false;

    beacon = part(body, "beacon", a.sphere.get(), at(0, 2.88f, 1.05f), glm::vec3(0.09f), a.white);
    beacon->material.tint = {1, 0.15f, 0.1f};
    beacon->material.castShadow = false;

    strobe = part(body, "strobe", a.sphere.get(), at(0, 1.78f, 6.62f), glm::vec3(0.07f), a.white);
    strobe->material.castShadow = false;

    // Прожектор под носа.
    lamp = part(body, "searchlight", a.cylinder.get(), at(0, 0.78f, -1.75f),
                {0.16f, 0.18f, 0.16f}, a.metal, axisAngle(-60, X));
    lamp->material.castShadow = false;
}

// ===========================================================================
//  Физика: опростен модел на тяга, гравитация и съпротивление на въздуха.
// ===========================================================================
void Helicopter::update(float dt, const HeliInput& in)
{
    // Фиксирана стъпка: физиката се смята винаги с h = 1/120 s, независимо
    // от FPS. Така поведението е еднакво на бърз и на бавен компютър.
    accumulator += dt;
    int steps = 0;
    while (accumulator >= kPhysicsH && steps < 24) {
        step(kPhysicsH, in);
        accumulator -= kPhysicsH;
        ++steps;
    }
    animate(dt);
}

void Helicopter::step(float h, const HeliInput& in)
{
    // 1) Обороти на ротора - инерция при включване и изключване.
    rotorSpeed = approach(rotorSpeed, engineOn ? 1.0f : 0.0f, engineOn ? 2.5f : 6.0f, h);

    // 2) Обща стъпка със "стабилизация на вертикалната скорост": W/S задават
    //    желана вертикална скорост (+-5 m/s), а без натиснат клавиш тя е 0 и
    //    хеликоптерът виси на място. Пропорционален регулатор (P-регулатор):
    //    стъпка = стъпка_за_висене + K * (желана - текуща скорост).
    float command;
    if (grounded && in.collective <= 0.0f) {
        command = 0.25f;                                  // на земята, без тяга за излитане
    } else {
        float hover = 0.5f / std::max(rotorSpeed * rotorSpeed, 0.25f);
        float vyTarget = 5.0f * in.collective;
        command = hover + 0.12f * (vyTarget - velocity.y);
    }
    collective = approach(collective, std::clamp(command, 0.0f, 1.0f), 0.25f, h);

    // 3) Наклон (циклична стъпка). На земята корпусът стои хоризонтално.
    const float maxTilt = glm::radians(20.0f);
    float pitchTarget = grounded ? 0.0f : -in.pitch * maxTilt;  // нос надолу = напред
    float rollTarget  = grounded ? 0.0f : -in.roll  * maxTilt;
    pitch = approach(pitch, pitchTarget, 0.45f, h);
    roll  = approach(roll,  rollTarget,  0.45f, h);

    // 4) Завъртане (опашен ротор) - работи само при въртящ се ротор.
    yawRate = approach(yawRate, in.yaw * glm::radians(70.0f) * rotorSpeed, 0.3f, h);
    yaw += yawRate * h;

    // 5) Сили. Тягата е по локалната ос "нагоре" на корпуса - при наклон
    //    тя има хоризонтална компонента и хеликоптерът се движи.
    glm::quat q = glm::angleAxis(yaw, Y) * glm::angleAxis(pitch, X) * glm::angleAxis(roll, Z);
    glm::vec3 up = q * Y;
    float tiltComp = 1.0f / std::max(std::cos(pitch) * std::cos(roll), 0.7f);
    float thrust = 2.0f * kGravity * rotorSpeed * rotorSpeed * collective * tiltComp;

    glm::vec3 acc = up * thrust + glm::vec3(0, -kGravity, 0);
    float speed = glm::length(velocity);
    acc -= velocity * (0.08f + 0.0025f * speed);      // линейно + квадратично съпротивление

    // 6) Полу-неявен метод на Ойлер: първо скоростта, после позицията.
    velocity += acc * h;
    position += velocity * h;

    // 7) Контакт със земята.
    if (position.y <= 0.0f) {
        position.y = 0.0f;
        if (velocity.y < 0.0f) velocity.y = 0.0f;
        float friction = std::exp(-6.0f * h);
        velocity.x *= friction;
        velocity.z *= friction;
        grounded = true;
    } else {
        grounded = false;
    }

    // Граници на полето (стените). Хеликоптерът спира 8 m преди стената
    // (перките са с радиус ~5.3 m, опашката е ~6.6 m зад центъра), а скоростта
    // към стената се нулира, за да не "залепва" в нея.
    const float L = WorldBounds::kHalfSize - 8.0f;
    if (position.x >  L) { position.x =  L; velocity.x = std::min(velocity.x, 0.0f); }
    if (position.x < -L) { position.x = -L; velocity.x = std::max(velocity.x, 0.0f); }
    if (position.z >  L) { position.z =  L; velocity.z = std::min(velocity.z, 0.0f); }
    if (position.z < -L) { position.z = -L; velocity.z = std::max(velocity.z, 0.0f); }
    if (position.y > WorldBounds::kMaxAltitude) {
        position.y = WorldBounds::kMaxAltitude;
        velocity.y = std::min(velocity.y, 0.0f);
    }
}

// ===========================================================================
//  Анимация: превръща състоянието в трансформации на възлите.
// ===========================================================================
void Helicopter::animate(float dt)
{
    time += dt;
    const float twoPi = glm::two_pi<float>();

    // Ъгълът се натрупва с ъгловата скорост: 5 об/s главен, 22 об/s опашен.
    mainAngle = std::fmod(mainAngle + rotorSpeed * twoPi * 5.0f * dt, twoPi);
    tailAngle = std::fmod(tailAngle + rotorSpeed * twoPi * 22.0f * dt, twoPi);

    rootNode->position = position;
    body->rotation = glm::angleAxis(yaw, Y) * glm::angleAxis(pitch, X) * glm::angleAxis(roll, Z);

    mainRotor->rotation = glm::angleAxis(mainAngle, Y);
    tailRotor->rotation = glm::angleAxis(tailAngle, X);

    // Конусност: при тяга перките се огъват нагоре. Ъгълът на атака
    // (feathering) расте с общата стъпка.
    float coning = glm::radians(6.0f) * rotorSpeed * rotorSpeed * collective;
    float bladePitch = glm::radians(1.0f + 11.0f * collective);
    for (int k = 0; k < 4; ++k) {
        bladePivots[k]->rotation = glm::angleAxis(k * glm::half_pi<float>(), Y) *
                                   glm::angleAxis(coning, Z);
        blades[k]->rotation = glm::angleAxis(bladePitch, X);
    }

    // Размазан диск: появява се плавно с нарастване на оборотите.
    float blur = smoothstep(0.3f, 0.85f, rotorSpeed);
    mainBlur->material.alpha = blur;
    tailBlur->material.alpha = blur;
    mainBlur->visible = tailBlur->visible = blur > 0.01f;

    // Мигащи светлини (само при работещ двигател).
    float beaconPhase = std::fmod(time * 1.2f, 1.0f);
    beacon->material.emissive = (engineOn && beaconPhase < 0.12f)
        ? glm::vec3(4.0f, 0.25f, 0.1f) : glm::vec3(0.15f, 0.0f, 0.0f);
    float strobePhase = std::fmod(time * 0.8f, 1.0f);
    bool flash = strobePhase < 0.04f || (strobePhase > 0.10f && strobePhase < 0.14f);
    strobe->material.emissive = (engineOn && flash) ? glm::vec3(6.0f) : glm::vec3(0.1f);

    lamp->material.emissive = searchlightOn ? glm::vec3(5.0f, 5.0f, 4.2f) : glm::vec3(0.0f);
}

glm::mat4 Helicopter::bodyMatrix() const
{
    return rootNode->localMatrix() * body->localMatrix();
}

glm::vec3 Helicopter::centerOfMass() const { return position + glm::vec3(0, kPivotY, 0); }

glm::vec3 Helicopter::searchlightPosition() const
{
    return glm::vec3(bodyMatrix() * glm::vec4(at(0, 0.62f, -1.85f), 1.0f));
}

glm::vec3 Helicopter::searchlightDirection() const
{
    return glm::normalize(body->rotation * glm::vec3(0, -0.55f, -1.0f));
}
