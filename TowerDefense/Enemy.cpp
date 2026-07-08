#include "Enemy.h"

Enemy::Enemy(Grid* grid,
    float sizeNDC)
    : grid_(grid),
    size_(sizeNDC)
{
    buildPath();
    reset();
}

void Enemy::buildPath()
{
    path_.clear();

    if (!grid_)
    {
        active_ = false;
        return;
    }

    const auto& src = grid_->mainPath();
    if (src.size() < 2)
    {
        active_ = false;
        return;
    }

    path_ = src;
}

void Enemy::spawn(EnemyType type)
{
    type_ = type;
    stats_ = getEnemyStats(type_);
    hp_ = stats_.maxHP;

    segmentIndex_ = 0;
    t_ = 0.0f;
    active_ = !path_.empty();

    slowFactor_ = 1.0f;
    slowTimer_ = 0.0f;
}

void Enemy::reset()
{
    active_ = false;
    hp_ = 0.0f;
    segmentIndex_ = 0;
    t_ = 0.0f;
    slowFactor_ = 1.0f;
    slowTimer_ = 0.0f;
}

void Enemy::update(float dt, GameState& game)
{
    if (!active_ || path_.size() < 2)
        return;

    if (slowTimer_ > 0.0f)
    {
        slowTimer_ -= dt;
        if (slowTimer_ <= 0.0f)
        {
            slowTimer_ = 0.0f;
            slowFactor_ = 1.0f; 
        }
    }

    float distance = stats_.speed * slowFactor_ * dt;

    while (distance > 0.0f && active_)
    {
        if (segmentIndex_ >= static_cast<int>(path_.size()) - 1)
        {
            active_ = false;
            game.lives -= stats_.damageToPlayer;
            if (game.lives < 0) game.lives = 0;
            break;
        }

        float remaining = 1.0f - t_;
        if (distance < remaining)
        {
            t_ += distance;
            distance = 0.0f;
        }
        else
        {
            distance -= remaining;
            segmentIndex_++;
            t_ = 0.0f;

            if (segmentIndex_ >= static_cast<int>(path_.size()) - 1)
            {
                active_ = false;
                game.lives -= stats_.damageToPlayer;
                if (game.lives < 0) game.lives = 0;
                break;
            }
        }
    }

}

void Enemy::applyDamage(float dmg, GameState& game)
{
    if (!active_) return;

    hp_ -= dmg;
    if (hp_ <= 0.0f)
    {
        hp_ = 0.0f;
        active_ = false;
        game.gold += stats_.rewardGold;   
    }
}


void Enemy::getPositionNDC(float& x, float& y) const
{
    if (path_.empty())
    {
        x = y = 0.0f;
        return;
    }

    int i = segmentIndex_;
    if (i >= static_cast<int>(path_.size()) - 1)
        i = static_cast<int>(path_.size()) - 2;

    auto [gx0, gy0] = path_[i];
    auto [gx1, gy1] = path_[i + 1];

    float x0, y0, x1, y1;
    grid_->cellCenterNDC(gx0, gy0, x0, y0);
    grid_->cellCenterNDC(gx1, gy1, x1, y1);

    x = (1.0f - t_) * x0 + t_ * x1;
    y = (1.0f - t_) * y0 + t_ * y1;
}

void Enemy::draw(Shader& shader, GLuint vao)
{
    if (!active_)
        return;

    float x, y;
    getPositionNDC(x, y);

    shader.use();
    glBindVertexArray(vao);

    GLint locPos = shader.loc("uPos");
    GLint locSize = shader.loc("uSize");
    GLint locColor = shader.loc("uEnemyColor");


    float bodyR = 0.90f, bodyG = 0.25f, bodyB = 0.25f; 
    switch (type_)
    {
    case EnemyType::Basic:
        bodyR = 0.90f; bodyG = 0.25f; bodyB = 0.25f;  
        break;
    case EnemyType::Fast:
        bodyR = 0.25f; bodyG = 0.90f; bodyB = 0.25f;  
        break;
    case EnemyType::Tank:
        bodyR = 0.60f; bodyG = 0.40f; bodyB = 0.90f;  
        break;
    }

    glUniform3f(locColor, bodyR, bodyG, bodyB);
    glUniform2f(locPos, x, y);
    glUniform2f(locSize, size_, size_);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);


    if (stats_.maxHP > 0.0f && hp_ > 0.0f)
    {
        float hpFrac = hp_ / stats_.maxHP;
        if (hpFrac < 0.0f) hpFrac = 0.0f;
        if (hpFrac > 1.0f) hpFrac = 1.0f;

        float fullW = size_;
        float barH = size_ * 0.18f;         
        float enemyTop = y + size_ * 0.5f;    
        float centerY = enemyTop + barH * 0.8f; 

        float leftX = x - fullW * 0.5f;
        float barW = fullW * hpFrac;
        float centerX = leftX + barW * 0.5f;

        glUniform3f(locColor, 1.0f, 0.0f, 0.0f);
        glUniform2f(locPos, centerX, centerY);
        glUniform2f(locSize, barW, barH);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }
}


float Enemy::getProgress() const
{
    if (path_.empty()) return 0.0f;
    return static_cast<float>(segmentIndex_) + t_;
}

void Enemy::applySlow(float factor, float duration)
{
    if (factor < slowFactor_ || slowTimer_ <= 0.0f)
    {
        slowFactor_ = factor;
        slowTimer_ = duration;
    }
}