#pragma once
#include <vector>
#include "GameTypes.h"

struct WaveConfig
{
    EnemyType type;
    int       enemyCount;
    float     spawnInterval;
    float     enemySize;
};

std::vector<WaveConfig> createDefaultWaves(float baseEnemySize);
