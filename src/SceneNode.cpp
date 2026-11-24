// SceneNode.cpp
#include "SceneNode.h"

#include <glm/gtc/matrix_transform.hpp>

SceneNode* SceneNode::addChild(std::unique_ptr<SceneNode> child)
{
    children.push_back(std::move(child));
    return children.back().get();
}

SceneNode* SceneNode::addChild(const std::string& childName)
{
    return addChild(std::make_unique<SceneNode>(childName));
}

glm::mat4 SceneNode::localMatrix() const
{
    // Първо завъртане, после транслация: M = T * R.
    return glm::translate(glm::mat4(1.0f), position) * glm::mat4_cast(rotation);
}

void SceneNode::collect(const glm::mat4& parentWorld, std::vector<DrawItem>& out) const
{
    if (!visible) return;

    glm::mat4 world = parentWorld * localMatrix();

    if (mesh)   // мащабът на формата се добавя само за собствената геометрия
        out.push_back({mesh, glm::scale(world, shapeScale), &material});

    for (const auto& c : children)
        c->collect(world, out);   // децата получават world БЕЗ shapeScale
}
