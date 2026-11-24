// Assets.h - общи ресурси: примитиви и текстури, създадени веднъж при старта.
#pragma once

#include "Mesh.h"

#include <glad/gl.h>
#include <memory>

struct Assets {
    // Геометрия (една и съща мрежа се използва от много възли).
    std::unique_ptr<Mesh> box, cylinder, taperedCylinder, sphere, disc, ground;

    // Процедурни текстури.
    GLuint camo = 0, metal = 0, blade = 0, glass = 0, helipad = 0,
           grid = 0, barrier = 0, rotorBlur = 0, white = 0;
};
