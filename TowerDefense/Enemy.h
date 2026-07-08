#pragma once
#include <vector>
#include <utility>
#include <glad/glad.h>

#include "Grid.h"
#include "Shader.h"
#include "GameTypes.h"
#include "GameState.h"

class Enemy
{
public:
    Enemy(Grid* grid, float sizeNDC);

    EnemyType getType() const { return type_; }

    void spawn(EnemyType type);
    void reset();

    void update(float dt, GameState& game);
    void draw(Shader& shader, GLuint vao);

    bool  isActive()    const { return active_; }
    float getHP()       const { return hp_; }
    float getMaxHP()    const { return stats_.maxHP; }
    float getProgress() const;

    void getPosition(float& x, float& y) const { getPositionNDC(x, y); }

    int  getId() const { return id_; }
    void setId(int id) { id_ = id; }

    void applyDamage(float dmg, GameState& game);

    float getSize() const { return size_; }

    void applySlow(float factor, float duration);

private:
    Grid* grid_ = nullptr;
    float size_ = 0.0f;

    std::vector<std::pair<int, int>> path_;

    EnemyType  type_ = EnemyType::Basic;
    EnemyStats stats_{};
    float      hp_ = 0.0f;

    int   segmentIndex_ = 0;
    float t_ = 0.0f;
    bool  active_ = false;

    int   id_ = -1;

    float slowFactor_ = 1.0f;
    float slowTimer_ = 0.0f;

    void buildPath();
    void getPositionNDC(float& x, float& y) const;
};
