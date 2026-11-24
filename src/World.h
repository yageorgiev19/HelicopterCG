// World.h - статичното обкръжение: земя с мрежа, площадка и стени по границите.
#pragma once

#include "Assets.h"
#include "SceneNode.h"

#include <memory>

std::unique_ptr<SceneNode> buildWorld(const Assets& assets);
