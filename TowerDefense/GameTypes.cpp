#include "GameTypes.h"


static const TowerStats BASIC_TOWER = {
    10.0f,  
    2.5f,  
    1.0f,   
    4.0f,   
    100     
};

static const TowerStats SLOW_TOWER = {
    4.0f,  
    2.0f,  
    0.6f,  
    4.0f,   
    120
};

static const TowerStats FAST_TOWER = {
    2.0f,  
    1.8f,   
    3.0f,   
    6.0f,  
    90
};

const TowerStats& getTowerStats(TowerType type)
{
    switch (type)
    {
    case TowerType::Basic: return BASIC_TOWER;
    case TowerType::Slow:  return SLOW_TOWER;
    case TowerType::Fast:  return FAST_TOWER;
    default:               return BASIC_TOWER;
    }
}

static const EnemyStats BASIC_ENEMY = {
    30.0f,  
    2.0f,   
    15,     
    1       
};

static const EnemyStats FAST_ENEMY = {
    18.0f, 
    3.2f,   
    12,     
    1
};

static const EnemyStats TANK_ENEMY = {
    80.0f, 
    1.2f,   
    30,     
    1
};

const EnemyStats& getEnemyStats(EnemyType type)
{
    switch (type)
    {
    case EnemyType::Basic: return BASIC_ENEMY;
    case EnemyType::Fast:  return FAST_ENEMY;
    case EnemyType::Tank:  return TANK_ENEMY;
    default:               return BASIC_ENEMY;
    }
}
