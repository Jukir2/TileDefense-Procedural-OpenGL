#pragma once
#include <vector>

#include "Tower.h"
#include "GameTypes.h"

struct GameState
{
    int gold = 300;
    int lives = 20;
    int maxLives = 20;
    int wave = 0;

    bool gameOver = false;
    bool victory = false;

    std::vector<Tower> towers;
    Selection selection;

    bool isTileOccupied(int gx, int gy) const;
    const Tower* findTower(int gx, int gy) const;
    Tower* findTower(int gx, int gy);
};
