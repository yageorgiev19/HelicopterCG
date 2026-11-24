// ===========================================================================
//  Анимиран текстуриран хеликоптер
//  Курсов проект по BCSE51 Компютърна графика, ТУ-София
//  Автор: Явор Калинов Георгиев, фак. № 121223073, гр. 39, КСИ
//
//  C++17, OpenGL 3.3 Core, GLFW (прозорец и вход), GLM (математика),
//  glad (зареждане на OpenGL функциите).
// ===========================================================================
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "Assets.h"
#include "Camera.h"
#include "Helicopter.h"
#include "Hud.h"
#include "ProceduralTextures.h"
#include "Renderer.h"
#include "World.h"

#include <cstdio>
#include <algorithm>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#ifdef _WIN32
// За да се виждат правилно българските букви в конзолата на Windows.
extern "C" __declspec(dllimport) int __stdcall SetConsoleOutputCP(unsigned int);
#endif


struct App {
    std::unique_ptr<Helicopter> heli;
    std::unique_ptr<Renderer>   renderer;
    Camera camera;
    bool   dragging = false;
    double lastX = 0, lastY = 0;
    bool   paused = false;
    bool   showHud = true;     
};

static void printControls()
{
    std::printf(
        "\n=== Управление ===\n"
        "  E              - старт / стоп на двигателя\n"
        "  W / S          - обща стъпка: изкачване / спускане\n"
        "  A / D          - завъртане наляво / надясно\n"
        "  Стрелки        - наклон напред/назад и встрани (движение)\n"
        "  L              - прожектор вкл./изкл.\n"
        "  C              - камера: преследване / свободна орбита\n"
        "  N              - ден / нощ\n"
        "  Мишка (влачене)- завъртане на камерата, колелце - приближаване\n"
        "  P              - пауза,  H - контролите на екрана,  Esc - изход\n\n");
}


static void onKey(GLFWwindow* w, int key, int, int action, int)
{
    if (action != GLFW_PRESS) return;
    App& app = *static_cast<App*>(glfwGetWindowUserPointer(w));
    switch (key) {
        case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(w, GLFW_TRUE); break;
        case GLFW_KEY_E:      app.heli->toggleEngine(); break;
        case GLFW_KEY_L:      app.heli->toggleSearchlight(); break;
        case GLFW_KEY_C:      app.camera.toggleMode(); break;
        case GLFW_KEY_N:      app.renderer->toggleNight(); break;
        case GLFW_KEY_P:      app.paused = !app.paused; break;
        case GLFW_KEY_H:      app.showHud = !app.showHud; break;
        default: break;
    }
}

static void onMouseButton(GLFWwindow* w, int button, int action, int)
{
    App& app = *static_cast<App*>(glfwGetWindowUserPointer(w));
    if (button == GLFW_MOUSE_BUTTON_LEFT || button == GLFW_MOUSE_BUTTON_RIGHT) {
        app.dragging = (action == GLFW_PRESS);
        glfwGetCursorPos(w, &app.lastX, &app.lastY);
    }
}

static void onCursor(GLFWwindow* w, double x, double y)
{
    App& app = *static_cast<App*>(glfwGetWindowUserPointer(w));
    if (app.dragging)
        app.camera.rotate(static_cast<float>(x - app.lastX), static_cast<float>(y - app.lastY));
    app.lastX = x;
    app.lastY = y;
}

static void onScroll(GLFWwindow* w, double, double dy)
{
    App& app = *static_cast<App*>(glfwGetWindowUserPointer(w));
    app.camera.zoom(static_cast<float>(dy));
}

// Чете задържаните клавиши -> команди за хеликоптера.
static HeliInput readInput(GLFWwindow* w)
{
    auto key = [w](int k) { return glfwGetKey(w, k) == GLFW_PRESS ? 1.0f : 0.0f; };
    HeliInput in;
    in.collective = key(GLFW_KEY_W) - key(GLFW_KEY_S);
    in.yaw        = key(GLFW_KEY_A) - key(GLFW_KEY_D);
    in.pitch      = key(GLFW_KEY_UP) - key(GLFW_KEY_DOWN);
    in.roll       = key(GLFW_KEY_RIGHT) - key(GLFW_KEY_LEFT);
    return in;
}


static void drawHud(const App& app, Hud& hud, int w, int h)
{
    struct Row { const char* key; const char* action; std::string state; glm::vec4 color; };
    const glm::vec4 on(0.45f, 1.0f, 0.45f, 1.0f), off(0.70f, 0.70f, 0.70f, 1.0f),
                    warn(1.0f, 0.40f, 0.35f, 1.0f), info(0.55f, 0.80f, 1.0f, 1.0f);
    const Row rows[] = {
        {"E", "старт / стоп на двигателя", app.heli->engineOn ? "ВКЛ" : "ИЗКЛ", app.heli->engineOn ? on : off},
        {"L", "прожектор вкл./изкл.", app.heli->searchlightOn ? "ВКЛ" : "ИЗКЛ", app.heli->searchlightOn ? on : off},
        {"C", "камера: преследване / свободна орбита", app.camera.isChase() ? "преследване" : "орбита", info},
        {"N", "ден / нощ", app.renderer->isNight() ? "нощ" : "ден", info},
        {"P", "пауза", app.paused ? "ПАУЗА" : "", warn},
    };

    // Цял мащаб на шрифта според височината на прозореца (по-едър на 4K).
    hud.begin(w, h, h >= 1400 ? 2 : 1);
    const float pad = 10, lh = hud.lineHeight(), gap = hud.textWidth("  ");
    const float keyW = hud.textWidth("E  – ");
    float actionW = 0;
    for (const Row& r : rows) actionW = std::max(actionW, hud.textWidth(r.action));
    const float x0 = 12, y0 = 12;   

    float y = y0 + pad;
    float x = hud.text(x0 + pad, y, "Управление", glm::vec4(1.0f, 0.85f, 0.3f, 1.0f));
    hud.text(x, y, "  (H – скрий)", off);
    y += lh * 1.4f;
    for (const Row& r : rows) {
        float xk = hud.text(x0 + pad, y, r.key, glm::vec4(1.0f, 0.85f, 0.3f, 1.0f));
        hud.text(xk, y, "  – ", off);
        hud.text(x0 + pad + keyW, y, r.action, glm::vec4(0.95f, 0.95f, 0.95f, 1.0f));
        if (!r.state.empty())
            hud.text(x0 + pad + keyW + actionW + gap, y, "[" + r.state + "]", r.color);
        y += lh;
    }
    hud.end();
}

static void createAssets(Assets& a)
{
    a.box.reset(Primitives::box());
    a.cylinder.reset(Primitives::cylinder(28, 1.0f, 1.0f, true));
    a.taperedCylinder.reset(Primitives::cylinder(28, 1.0f, 0.45f, true));
    a.sphere.reset(Primitives::sphere(24, 36));
    a.disc.reset(Primitives::disc(64));
    a.ground.reset(Primitives::plane(3000.0f, 600.0f));

    a.camo       = uploadTexture(ProcTex::camouflage(512), true);
    a.metal      = uploadTexture(ProcTex::metal(256), true);
    a.blade      = uploadTexture(ProcTex::rotorBlade(256), true);
    a.glass      = uploadTexture(ProcTex::glass(256), true);
    a.helipad    = uploadTexture(ProcTex::helipad(1024), true, false);
    a.grid       = uploadTexture(ProcTex::groundGrid(256), true);
    a.barrier    = uploadTexture(ProcTex::barrier(128), true);
    a.rotorBlur  = uploadTexture(ProcTex::rotorBlur(256), true, false);
    a.white      = uploadTexture(ProcTex::white(), true);
}

int main()
{
#ifdef _WIN32
    SetConsoleOutputCP(65001);   // UTF-8
#endif
    if (!glfwInit()) {
        std::fprintf(stderr, "GLFW не може да се инициализира.\n");
        return EXIT_FAILURE;
    }
    // OpenGL 3.3 Core профил + 4x MSAA (изглаждане на ръбовете).
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
    glfwWindowHint(GLFW_SAMPLES, 4);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Анимиран текстуриран хеликоптер", nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "Неуспешно създаване на прозорец (нужен е OpenGL 3.3).\n");
        glfwTerminate();
        return EXIT_FAILURE;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);   // вертикална синхронизация

    if (!gladLoadGL(glfwGetProcAddress)) {
        std::fprintf(stderr, "glad не успя да зареди OpenGL функциите.\n");
        return EXIT_FAILURE;
    }
    std::printf("OpenGL: %s\nGPU:    %s\n",
                reinterpret_cast<const char*>(glGetString(GL_VERSION)),
                reinterpret_cast<const char*>(glGetString(GL_RENDERER)));

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);

    
    {
        Assets assets;
        createAssets(assets);

        App app;
        app.heli = std::make_unique<Helicopter>(assets);
        app.renderer = std::make_unique<Renderer>();
        Hud hud;
        std::unique_ptr<SceneNode> world = buildWorld(assets);

        glfwSetWindowUserPointer(window, &app);
        glfwSetKeyCallback(window, onKey);
        glfwSetMouseButtonCallback(window, onMouseButton);
        glfwSetCursorPosCallback(window, onCursor);
        glfwSetScrollCallback(window, onScroll);

        printControls();

        std::vector<DrawItem> items;
        double last = glfwGetTime(), titleTimer = 0;
        int frames = 0;

        // Основен цикъл 
        while (!glfwWindowShouldClose(window)) {
            double now = glfwGetTime();
            float dt = static_cast<float>(now - last);
            last = now;
            if (dt > 0.1f) dt = 0.1f;          // защита при "замръзване"

            // 1) Вход и обновяване на състоянието.
            if (!app.paused) app.heli->update(dt, readInput(window));
            app.camera.update(dt, app.heli->centerOfMass(), app.heli->yaw);

            // 2) Обхождане на графа на сцената -> списък за рисуване.
            items.clear();
            world->collect(glm::mat4(1.0f), items);
            app.heli->root()->collect(glm::mat4(1.0f), items);

            // 3) Рисуване.
            FrameParams f;
            glfwGetFramebufferSize(window, &f.width, &f.height);
            if (f.width > 0 && f.height > 0) {
                f.view = app.camera.view();
                f.proj = app.camera.projection(static_cast<float>(f.width) / f.height);
                f.cameraPos = app.camera.eye();
                f.shadowCenter = app.heli->centerOfMass();
                f.spotOn = app.heli->searchlightOn;
                f.spotPos = app.heli->searchlightPosition();
                f.spotDir = app.heli->searchlightDirection();
                app.renderer->render(items, f);
                if (app.showHud) drawHud(app, hud, f.width, f.height);
            }

            // 4) Табло в заглавието на прозореца (4 пъти в секунда).
            ++frames;
            titleTimer += dt;
            if (titleTimer >= 0.25) {
                const Helicopter& h = *app.heli;
                char title[256];
                std::snprintf(title, sizeof(title),
                    "Хеликоптер | Двигател: %s | Обороти: %3.0f%% | Височина: %5.1f m | "
                    "Скорост: %5.1f km/h | Камера: %s | %3.0f FPS%s",
                    h.engineOn ? "ВКЛ" : "ИЗКЛ", h.rotorSpeed * 100.0f, h.position.y,
                    glm::length(h.velocity) * 3.6f,
                    app.camera.isChase() ? "преследване" : "орбита",
                    frames / titleTimer, app.paused ? " | ПАУЗА" : "");
                glfwSetWindowTitle(window, title);
                titleTimer = 0;
                frames = 0;
            }

            glfwSwapBuffers(window);
            glfwPollEvents();
        }
        // Текстурите не са в Mesh/Shader обекти - изтриват се ръчно.
        GLuint tex[] = {assets.camo, assets.metal, assets.blade, assets.glass,
                        assets.helipad, assets.grid, assets.barrier, assets.rotorBlur, assets.white};
        glDeleteTextures(sizeof(tex) / sizeof(tex[0]), tex);
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
