#pragma once
#include "GameTypes.h"

struct Tower
{
    TowerType type;
    int gx;
    int gy;
    int   level = 1;
    float cooldown = 0.0f;  
    int goldInvested;
};

inline bool towerOnCell(const Tower& t, int gx, int gy)
{
    return t.gx == gx && t.gy == gy;
}
