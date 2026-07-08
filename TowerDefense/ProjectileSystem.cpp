#include "ProjectileSystem.h"
#include "WaveSystem.h"
#include "Enemy.h"
#include <cmath>
#include <algorithm>

ProjectileSystem::ProjectileSystem(Grid* grid)
    : grid_(grid)
{
}

void ProjectileSystem::update(float dt, WaveSystem& waves, GameState& game)
{
    auto& towers = game.towers;
    const auto& params = grid_->params();
    float tileSpan = params.tileW + params.gapX;

    const auto& enemies = waves.enemies();

    for (auto& t : towers)
        if (t.cooldown > 0.0f)
            t.cooldown -= dt;

    for (auto& tower : towers)
    {
        if (tower.cooldown > 0.0f)
            continue;

        const TowerStats& base = getTowerStats(tower.type);

        float dmgMul = towerDamageMultiplier(tower.level);
        float rngMul = towerRangeMultiplier(tower.level);
        float rateMul = towerFireRateMultiplier(tower.level);

        float damage = base.damage * dmgMul;
        float range = base.range * rngMul;
        float fireRate = base.fireRate * rateMul;

        float tx, ty;
        grid_->cellCenterNDC(tower.gx, tower.gy, tx, ty);

        float maxDist = range * tileSpan;

        Enemy* bestTarget = nullptr;
        float  bestProg = -1.0f;

        for (auto& ePtr : enemies)
        {
            Enemy* e = ePtr.get();
            if (!e->isActive()) continue;

            float ex, ey;
            e->getPosition(ex, ey);

            float dx = ex - tx;
            float dy = ey - ty;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist > maxDist) continue;

            float prog = e->getProgress();  
            if (prog > bestProg)
            {
                bestProg = prog;
                bestTarget = e;
            }
        }

        if (!bestTarget)
            continue;

        Projectile p{};
        p.x = tx;
        p.y = ty;
        p.speed = base.projectileSpeed * tileSpan;
        p.damage = damage;
        p.sourceType = tower.type;              
        p.targetId = bestTarget->getId();     
        p.active = true;

        switch (tower.type)
        {
        case TowerType::Basic:
            p.r = 1.0f; p.g = 0.2f; p.b = 0.2f;
            break;
        case TowerType::Fast:
            p.r = 1.0f; p.g = 1.0f; p.b = 0.2f;
            break;
        case TowerType::Slow:
            p.r = 0.2f; p.g = 0.4f; p.b = 1.0f;
            break;
        }

        bullets_.push_back(p);
        tower.cooldown = 1.0f / fireRate;
    }

    for (auto& b : bullets_)
    {
        if (!b.active) continue;

        Enemy* target = nullptr;
        for (auto& ePtr : enemies)
        {
            Enemy* e = ePtr.get();
            if (!e->isActive()) continue;
            if (e->getId() == b.targetId)
            {
                target = e;
                break;
            }
        }

        if (!target)
        {
            b.active = false;
            continue;
        }

        float ex, ey;
        target->getPosition(ex, ey);

        float dx = ex - b.x;
        float dy = ey - b.y;
        float dist = std::sqrt(dx * dx + dy * dy);

        float step = b.speed * dt;
        float enemyRadius = target->getSize() * 0.6f;

        auto onHit = [&]()
            {
                target->applyDamage(b.damage, game);

                SlowEffect se = getSlowEffect(b.sourceType);
                if (se.factor < 1.0f && se.duration > 0.0f)
                {
                    target->applySlow(se.factor, se.duration);
                }

                b.active = false;
            };

        if (dist < 1e-5f)
        {
            onHit();
            continue;
        }

        if (dist <= step || dist <= enemyRadius)
        {
            onHit();
            continue;
        }

        dx /= dist;
        dy /= dist;
        b.x += dx * step;
        b.y += dy * step;
    }

    bullets_.erase(
        std::remove_if(bullets_.begin(), bullets_.end(),
            [](const Projectile& b)
            {
                if (!b.active) return true;
                if (b.x < -1.3f || b.x > 1.3f || b.y < -1.3f || b.y > 1.3f)
                    return true;
                return false;
            }),
        bullets_.end());
}

void ProjectileSystem::draw(Shader& shader, GLuint vao, GLint colorLoc)
{
    if (!grid_) return;

    shader.use();
    glBindVertexArray(vao);

    const auto& params = grid_->params();
    float projectileSize = params.tileW * 0.30f;

    GLint locPos = shader.loc("uPos");
    GLint locSize = shader.loc("uSize");

    for (const auto& b : bullets_)
    {
        if (!b.active) continue;

        glUniform3f(colorLoc, b.r, b.g, b.b);
        glUniform2f(locPos, b.x, b.y);
        glUniform2f(locSize, projectileSize, projectileSize);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }
}


void ProjectileSystem::reset()
{
    bullets_.clear();
}