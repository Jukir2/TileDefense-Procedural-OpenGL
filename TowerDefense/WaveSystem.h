#pragma once
#include <vector>
#include <memory>

#include "Grid.h"
#include "Enemy.h"
#include "Shader.h"
#include "WaveConfig.h"


enum class WaveState
{
    Idle,       
    Spawning,   
    Running,    
    Finished    
};

class WaveSystem
{
public:
    explicit WaveSystem(Grid* grid);

    void setWaves(const std::vector<WaveConfig>& waves);
    void reset();        
    void startNextWave();

    void update(float dt, GameState& game);     
    void draw(Shader& enemyShader, GLuint vao); 

    WaveState state() const { return state_; }
    int currentWaveIndex() const { return currentWaveIndex_; } 
    int totalWaves() const { return static_cast<int>(waves_.size()); }
    int enemiesAlive() const { return static_cast<int>(enemies_.size()); }

    bool canStartNextWave() const;

    Enemy* getLastEnemy();

    const std::vector<std::unique_ptr<Enemy>>& enemies() const { return enemies_; }

    const WaveConfig* currentWaveConfig() const;
    const WaveConfig* nextWaveConfig() const;

private:
    Grid* grid_ = nullptr;

    std::vector<WaveConfig> waves_;
    std::vector<std::unique_ptr<Enemy>> enemies_;

    WaveState state_ = WaveState::Idle;
    int   currentWaveIndex_ = -1;
    float spawnTimer_ = 0.0f;
    int   spawnedInWave_ = 0;

    int   nextEnemyId_ = 0;
};
