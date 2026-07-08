#include "GameState.h"

bool GameState::isTileOccupied(int gx, int gy) const
{
    for (const auto& t : towers)
        if (t.gx == gx && t.gy == gy)
            return true;
    return false;
}

const Tower* GameState::findTower(int gx, int gy) const
{
    for (const auto& t : towers)
        if (t.gx == gx && t.gy == gy)
            return &t;
    return nullptr;
}

Tower* GameState::findTower(int gx, int gy)
{
    return const_cast<Tower*>(
        static_cast<const GameState*>(this)->findTower(gx, gy)
        );
}
