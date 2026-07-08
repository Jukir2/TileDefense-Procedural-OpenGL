#include "WaveConfig.h"

std::vector<WaveConfig> createDefaultWaves(float baseEnemySize)
{
    float basicSize = baseEnemySize;
    float fastSize = baseEnemySize * 0.7f;
    float tankSize = baseEnemySize * 1.4f;

    std::vector<WaveConfig> waves;

    waves.push_back({ EnemyType::Basic, 10, 0.8f, basicSize });
    waves.push_back({ EnemyType::Basic, 8, 0.7f, basicSize });
    waves.push_back({ EnemyType::Fast,  10, 0.6f, fastSize });
    waves.push_back({ EnemyType::Tank,  6,  0.9f, tankSize });
    waves.push_back({ EnemyType::Basic, 15, 0.5f, basicSize });
    waves.push_back({ EnemyType::Fast,  16, 0.5f, fastSize });
    waves.push_back({ EnemyType::Tank,  8,  0.8f, tankSize });
    waves.push_back({ EnemyType::Basic, 20, 0.4f, basicSize });
    waves.push_back({ EnemyType::Fast,  20, 0.4f, fastSize });
    waves.push_back({ EnemyType::Tank,  12, 0.7f, tankSize });

    return waves;
}
