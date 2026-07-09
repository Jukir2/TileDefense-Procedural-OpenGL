# Tile Defense - Procedural OpenGL Tower Defense

**Tile Defense** is a 2D Tower Defense game written in C++ using OpenGL 3.3.  
The project focuses on procedural map generation, real-time rendering and basic gameplay systems such as enemy waves, tower building, tower upgrades and projectiles.

The game was created as part of an engineering thesis project.

---

## Screenshots

> Screenshots will be added here.

<!-- Example after adding screenshots:
![Main menu](screenshots/main_menu.png)
![Gameplay](screenshots/gameplay.png)
![Procedural map](screenshots/procedural_map.png)
-->

---

## Main Features

- 2D Tower Defense gameplay
- Procedurally generated enemy path
- Terrain and obstacle generation based on Perlin noise
- Grid-based map system
- Three tower types:
  - Basic Tower
  - Slow Tower
  - Fast Tower
- Enemy wave system
- Tower upgrades and selling
- Projectile system
- Win and lose conditions
- In-game menu and "How to play" window
- Texture-based rendering using OpenGL
- User interface created with Dear ImGui

---

## Technologies

The project uses:

- C++
- OpenGL 3.3 Core Profile
- GLFW
- GLAD
- Dear ImGui
- stb_image
- GLSL shader code embedded directly in the source code
- Microsoft Visual Studio / Visual Studio Code

---

## Project Structure

```text
TileDefense-Procedural-OpenGL/
│
├── TowerDefense.sln
├── README.md
├── .gitignore
│
├── docs/
│   └── Praca_inzynierska_Jakub_Michalski.pdf
│
├── screenshots/
│
└── TowerDefense/
    ├── main.cpp
    ├── Shader.cpp / Shader.h
    ├── Grid.cpp / Grid.h
    ├── GameState.cpp / GameState.h
    ├── Enemy.cpp / Enemy.h
    ├── Tower.cpp / Tower.h
    ├── WaveSystem.cpp / WaveSystem.h
    ├── ProjectileSystem.cpp / ProjectileSystem.h
    └── assets/