#pragma once
#include <vector>
#include <glad/glad.h>

#include "Grid.h"
#include "Shader.h"
#include "GameTypes.h"
#include "GameState.h"

class WaveSystem;   
class Enemy;        

struct Projectile
{
    float x, y;      
    float speed;     
    float damage;
    float r, g, b;   
    bool  active = true;
    int   targetId = -1;  
    TowerType sourceType;
};

class ProjectileSystem
{
public:
    explicit ProjectileSystem(Grid* grid);

    void update(float dt, WaveSystem& waves, GameState& game);

    void draw(Shader& shader, GLuint vao, GLint colorLoc);

    void reset();

private:
    Grid* grid_;
    std::vector<Projectile> bullets_;
};
