#pragma once

enum class CellType { Empty, Road, Block };

struct Selection
{
    bool hasCell = false;
    int  gx = -1;
    int  gy = -1;
    CellType cellType = CellType::Empty;
};

inline const char* cellTypeToString(CellType t)
{
    switch (t)
    {
    case CellType::Empty: return "Empty tile";
    case CellType::Road:  return "Road tile";
    case CellType::Block: return "Blocked";
    default:              return "Unknown";
    }
}

inline CellType classifyCell(unsigned char v)
{
    if (v == 1) return CellType::Road;
    if (v == 2) return CellType::Block;
    return CellType::Empty;
}

enum class TowerType { Basic, Slow, Fast };

inline const char* towerTypeToString(TowerType t)
{
    switch (t)
    {
    case TowerType::Basic: return "Basic";
    case TowerType::Slow:  return "Slow";
    case TowerType::Fast:  return "Fast";
    default:               return "Unknown";
    }
}

struct TowerStats
{
    float damage;          
    float range;           
    float fireRate;        
    float projectileSpeed; 
    int   cost;            
};

const TowerStats& getTowerStats(TowerType type);


enum class EnemyType { Basic, Fast, Tank };

inline const char* enemyTypeToString(EnemyType t)
{
    switch (t)
    {
    case EnemyType::Basic: return "Basic";
    case EnemyType::Fast:  return "Fast";
    case EnemyType::Tank:  return "Tank";
    default:               return "Unknown";
    }
}

struct EnemyStats
{
    float maxHP;         
    float speed;         
    int   rewardGold;   
    int   damageToPlayer; 
};

const EnemyStats& getEnemyStats(EnemyType type);


inline int getTowerUpgradeCost(TowerType type, int currentLevel)
{
    const TowerStats& base = getTowerStats(type);

    return base.cost * currentLevel;
}

inline float towerDamageMultiplier(int level)
{
    return 1.0f + 0.6f * (level - 1);
}

inline float towerRangeMultiplier(int level)
{
    return 1.0f + 0.2f * (level - 1);
}

inline float towerFireRateMultiplier(int level)
{
    return 1.0f + 0.3f * (level - 1);
}

struct SlowEffect
{
    float factor;   
    float duration; 
};

inline SlowEffect getSlowEffect(TowerType type)
{
    switch (type)
    {
    case TowerType::Slow:
        return { 0.5f, 1.2f };
    default:
        return { 1.0f, 0.0f }; 
    }
}