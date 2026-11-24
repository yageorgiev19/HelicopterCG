// World.cpp
#include "World.h"
#include "WorldBounds.h"

namespace {

SceneNode* object(SceneNode* parent, const Mesh* mesh, const glm::vec3& pos,
                  const glm::vec3& scale, GLuint tex)
{
    auto n = std::make_unique<SceneNode>();
    n->mesh = mesh;
    n->position = pos;
    n->shapeScale = scale;
    n->material.texture = tex;
    return parent->addChild(std::move(n));
}

} 

std::unique_ptr<SceneNode> buildWorld(const Assets& a)
{
    auto root = std::make_unique<SceneNode>("world");

    // Земя: равнина 3000 x 3000 m, покрита с мрежа от квадрати 5 x 5 m (текстурата с едно квадратче се повтаря 600 пъти). Мрежата показва движението при полет, а сянката на хеликоптера пада върху нея.
    SceneNode* g = object(root.get(), a.ground.get(), {0, -0.02f, 0}, glm::vec3(1), a.grid);
    g->material.tint = glm::vec3(0.75f);
    g->material.specular = 0.05f;
    g->material.shininess = 8;

    // Хеликоптерна площадка - плосък паралелепипед, горната му стена е на y = 0.
    SceneNode* pad = object(root.get(), a.box.get(), {0, -0.1f, 0}, {14, 0.2f, 14}, a.helipad);
    pad->material.specular = 0.15f;

    // Стени по границите на полето: 4 тънки полупрозрачни паралелепипеда с оранжева мрежа (квадрати 10 x 10 m). Показват докъде може да се лети; физиката (Helicopter::step) спира хеликоптера на същите граници.
    const float L = WorldBounds::kHalfSize, Hh = WorldBounds::kMaxAltitude;
    const glm::vec3 wallPos[4]   = {{0, Hh / 2, -L}, {0, Hh / 2, L}, {-L, Hh / 2, 0}, {L, Hh / 2, 0}};
    const glm::vec3 wallScale[4] = {{2 * L, Hh, 0.3f}, {2 * L, Hh, 0.3f},
                                    {0.3f, Hh, 2 * L}, {0.3f, Hh, 2 * L}};
    for (int i = 0; i < 4; ++i) {
        SceneNode* wall = object(root.get(), a.box.get(), wallPos[i], wallScale[i], a.barrier);
        wall->material.uvScale = {2 * L / 10.0f, Hh / 10.0f};
        wall->material.transparent = true;
        wall->material.castShadow = false;
        wall->material.specular = 0.0f;
        wall->material.emissive = {0.35f, 0.12f, 0.03f};   // вижда се и през нощта
    }

    return root;
}
