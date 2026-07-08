#include "WaveSystem.h"
#include <algorithm> 
#include "WaveConfig.h"

WaveSystem::WaveSystem(Grid* grid)
    : grid_(grid)
{
}

void WaveSystem::setWaves(const std::vector<WaveConfig>& waves)
{
    waves_ = waves;
    reset();
}

void WaveSystem::reset()
{
    enemies_.clear();
    spawnTimer_ = 0.0f;
    spawnedInWave_ = 0;
    currentWaveIndex_ = -1;
    nextEnemyId_ = 0;

    if (waves_.empty())
        state_ = WaveState::Finished;
    else
        state_ = WaveState::Idle;
}

bool WaveSystem::canStartNextWave() const
{
    if (state_ == WaveState::Finished)
        return false;

    if (state_ == WaveState::Spawning || state_ == WaveState::Running)
        return false;

    if (!enemies_.empty())
        return false;

    return true;
}

void WaveSystem::startNextWave()
{
    if (!canStartNextWave())
        return;

    currentWaveIndex_++;
    if (currentWaveIndex_ >= static_cast<int>(waves_.size()))
    {
        state_ = WaveState::Finished;
        return;
    }

    enemies_.clear();
    spawnTimer_ = 0.0f;
    spawnedInWave_ = 0;
    state_ = WaveState::Spawning;
}

void WaveSystem::update(float dt, GameState& game)
{
    for (auto& e : enemies_)
        e->update(dt, game);

    enemies_.erase(
        std::remove_if(
            enemies_.begin(), enemies_.end(),
            [](const std::unique_ptr<Enemy>& e)
            {
                return !e->isActive();
            }),
        enemies_.end());

    if (state_ == WaveState::Spawning)
    {
        if (currentWaveIndex_ < 0 || currentWaveIndex_ >= static_cast<int>(waves_.size()))
            return;

        const WaveConfig& cfg = waves_[currentWaveIndex_];

        spawnTimer_ += dt;
        while (spawnedInWave_ < cfg.enemyCount && spawnTimer_ >= cfg.spawnInterval)
        {
            spawnTimer_ -= cfg.spawnInterval;

            auto enemy = std::make_unique<Enemy>(grid_, cfg.enemySize);
            enemy->setId(nextEnemyId_++);
            enemy->spawn(cfg.type);  
            enemies_.push_back(std::move(enemy));

            ++spawnedInWave_;
        }

        if (spawnedInWave_ >= cfg.enemyCount)
        {
            state_ = WaveState::Running;
        }
    }

    if (state_ == WaveState::Running)
    {
        if (enemies_.empty())
        {
            if (currentWaveIndex_ + 1 >= static_cast<int>(waves_.size()))
                state_ = WaveState::Finished;
            else
                state_ = WaveState::Idle;
        }
    }
}

void WaveSystem::draw(Shader& shader, GLuint vao)
{
    shader.use();
    glBindVertexArray(vao);

    GLint locPos = shader.loc("uPos");
    GLint locSize = shader.loc("uSize");
    GLint locType = shader.loc("uEnemyType");
    GLint locUseTex = shader.loc("uUseTexture");
    GLint locColor = shader.loc("uEnemyColor");

    for (auto& ePtr : enemies_)
    {
        Enemy* e = ePtr.get();
        if (!e->isActive())
            continue;

        float x, y;
        e->getPosition(x, y);

        float size = e->getSize();

        int typeIndex = 0;
        switch (e->getType())
        {
        case EnemyType::Basic: typeIndex = 0; break;
        case EnemyType::Fast:  typeIndex = 1; break;
        case EnemyType::Tank:  typeIndex = 2; break;
        }
        glUniform1i(locUseTex, 1);
        glUniform1i(locType, typeIndex); 
        glUniform2f(locPos, x, y);
        glUniform2f(locSize, size, size);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        float hp = e->getHP();
        float maxHP = e->getMaxHP();
        if (maxHP > 0.0f && hp > 0.0f)
        {
            float hpFrac = hp / maxHP;
            if (hpFrac < 0.0f) hpFrac = 0.0f;
            if (hpFrac > 1.0f) hpFrac = 1.0f;

            float fullW = size;
            float barH = size * 0.18f;
            float enemyTop = y + size * 0.5f;
            float centerY = enemyTop + barH * 0.8f;

            float leftX = x - fullW * 0.5f;
            float barW = fullW * hpFrac;
            float centerX = leftX + barW * 0.5f;

            glUniform1i(locUseTex, 0);
            glUniform3f(locColor, 1.0f, 0.0f, 0.0f);
            glUniform2f(locPos, centerX, centerY);
            glUniform2f(locSize, barW, barH);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        }
    }
}




Enemy* WaveSystem::getLastEnemy()
{
    Enemy* best = nullptr;
    float bestProg = -1.0f;

    for (auto& e : enemies_)
    {
        if (!e->isActive()) continue;
        float prog = e->getProgress();
        if (prog > bestProg)
        {
            bestProg = prog;
            best = e.get();
        }
    }
    return best;
}

const WaveConfig* WaveSystem::currentWaveConfig() const
{
    if (currentWaveIndex_ < 0 ||
        currentWaveIndex_ >= static_cast<int>(waves_.size()))
        return nullptr;
    return &waves_[currentWaveIndex_];
}

const WaveConfig* WaveSystem::nextWaveConfig() const
{
    if (waves_.empty())
        return nullptr;

    int idx = (currentWaveIndex_ < 0) ? 0 : currentWaveIndex_ + 1;
    if (idx < 0 || idx >= static_cast<int>(waves_.size()))
        return nullptr;

    return &waves_[idx];
}
