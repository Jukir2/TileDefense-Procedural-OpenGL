#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>

#include "Shader.h"
#include "Grid.h"
#include "Enemy.h"
#include "WaveSystem.h"
#include "GameTypes.h"
#include "GameState.h"
#include "Tower.h"
#include "ProjectileSystem.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using namespace std;

static int towerSpriteIndex(TowerType type)
{
    switch (type)
    {
    case TowerType::Basic: return 0;
    case TowerType::Slow:  return 1;
    case TowerType::Fast:  return 2;
    default:               return 0;
    }
}


struct RectNDC
{
    float cx, cy; 
    float sx, sy; 
};

static void drawRect(const RectNDC& r, Shader& sh, GLuint vao,
    float rC, float gC, float bC)
{
    sh.use();
    glUniform2f(sh.loc("uPos"), r.cx, r.cy);
    glUniform2f(sh.loc("uSize"), r.sx, r.sy);
    glUniform3f(sh.loc("uEnemyColor"), rC, gC, bC);
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

static const char* gridVS =
"#version 330 core\n"
"layout (location = 0) in vec2 aPos;\n"
"uniform ivec2 uGrid;\n"
"uniform vec2 uTileSize;\n"
"uniform vec2 uGap;\n"
"uniform vec2 uOffset;\n"
"flat out ivec2 vCell;\n"
"out vec2 vUV;\n"
"void main(){\n"
"    int col = gl_InstanceID % uGrid.x;\n"
"    int row = gl_InstanceID / uGrid.x;\n"
"    vCell = ivec2(col,row);\n"
"\n"
"    vec2 gridCenter = (vec2(uGrid) - 1.0) * 0.5;\n"
"    vec2 offset = (vec2(col,row) - gridCenter) * (uTileSize + uGap) + uOffset;\n"
"    gl_Position = vec4(aPos * uTileSize + offset, 0.0, 1.0);\n"
"\n"
"    vUV = aPos + vec2(0.5);\n"
"}\n";



static const char* gridFS =
"#version 330 core\n"
"flat in ivec2 vCell;\n"
"in vec2 vUV;\n"
"\n"
"uniform sampler2D uTypes;\n"
"uniform sampler2D uGrassAtlas;\n"
"uniform sampler2D uRoadAtlas;\n"
"uniform sampler2D uStartGoalAtlas;\n" 
"uniform vec3 uColorRoad;\n"    
"uniform vec3 uColorBlock;\n"
"uniform float uSeed;\n"
"uniform ivec2 uGrid;\n"
"uniform ivec2 uStartCell;\n"
"uniform ivec2 uGoalCell;\n"
"uniform sampler2D uObstacleAtlas;\n"
"\n"
"out vec4 FragColor;\n"
"\n"
"float hash2(vec2 p)\n"
"{\n"
"    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);\n"
"}\n"
"\n"
"int getCellType(ivec2 c)\n"
"{\n"
"    if (c.x < 0 || c.y < 0 || c.x >= uGrid.x || c.y >= uGrid.y)\n"
"        return 0;\n"
"    float t = texelFetch(uTypes, c, 0).r;\n"
"    return int(round(t * 255.0));\n"
"}\n"
"\n"
"vec4 sampleGrass()\n"
"{\n"
"    const float tilesX = 4.0;\n"
"    const float tilesY = 2.0;\n"
"\n"
"    float r = hash2(vec2(vCell) + vec2(uSeed, uSeed * 1.37));\n"
"    int idx = int(floor(r * 8.0)) % 8;\n"
"    int ix = idx % int(tilesX);\n"
"    int iy = idx / int(tilesX);\n"
"\n"
"    vec2 tileScale = vec2(1.0 / tilesX, 1.0 / tilesY);\n"
"    vec2 baseUV = vec2(float(ix), float(iy)) * tileScale;\n"
"\n"
"    vec2 local = vec2(vUV.x, 1.0 - vUV.y);\n"
"\n"
"    vec2 atlasUV = baseUV + local * tileScale;\n"
"    vec3 col = texture(uGrassAtlas, atlasUV).rgb;\n"
"    return vec4(col, 1.0);\n"
"}\n"
"\n"
"vec4 sampleRoad()\n"
"{\n"
"    int up    = getCellType(vCell + ivec2(0, 1));\n"
"    int down  = getCellType(vCell + ivec2(0,-1));\n"
"    int left  = getCellType(vCell + ivec2(-1,0));\n"
"    int right = getCellType(vCell + ivec2(1, 0));\n"
"\n"
"    bool bUp    = (up    == 1);\n"
"    bool bDown  = (down  == 1);\n"
"    bool bLeft  = (left  == 1);\n"
"    bool bRight = (right == 1);\n"
"\n"
"    int neighbors = int(bUp) + int(bDown) + int(bLeft) + int(bRight);\n"
"\n"
"    const int IDX_H  = 0;\n"
"    const int IDX_DL = 1;\n"
"    const int IDX_UR = 2;\n"
"    const int IDX_LU = 3;\n"
"    const int IDX_V  = 4;\n"
"    const int IDX_RD = 5;\n"
"\n"
"    int roadIdx = IDX_H;\n"
"\n"
"    if (neighbors <= 1)\n"
"    {\n"
"        if (bLeft || bRight)\n"
"            roadIdx = IDX_H;\n"
"        else\n"
"            roadIdx = IDX_V;\n"
"    }\n"
"    else if (neighbors == 2)\n"
"    {\n"
"        bool horizontal = (bLeft && bRight);\n"
"        bool vertical   = (bUp   && bDown);\n"
"        bool onlyLR     = (bLeft || bRight) && !(bUp || bDown);\n"
"        bool onlyUD     = (bUp   || bDown)  && !(bLeft || bRight);\n"
"\n"
"        if (horizontal || onlyLR)\n"
"        {\n"
"            roadIdx = IDX_H;\n"
"        }\n"
"        else if (vertical || onlyUD)\n"
"        {\n"
"            roadIdx = IDX_V;\n"
"        }\n"
"        else\n"
"        {\n"
"            if (bUp && bRight)        roadIdx = IDX_UR;\n"
"            else if (bRight && bDown) roadIdx = IDX_RD;\n"
"            else if (bDown && bLeft)  roadIdx = IDX_DL;\n"
"            else if (bLeft && bUp)    roadIdx = IDX_LU;\n"
"        }\n"
"    }\n"
"    else\n"
"    {\n"
"        roadIdx = IDX_V;\n"
"    }\n"
"\n"
"    const float tilesX = 3.0;\n"
"    const float tilesY = 2.0;\n"
"\n"
"    int ix = roadIdx % int(tilesX);\n"
"    int iy = roadIdx / int(tilesX);\n"
"\n"
"    vec2 tileScale = vec2(1.0 / tilesX, 1.0 / tilesY);\n"
"    vec2 baseUV = vec2(float(ix), float(iy)) * tileScale;\n"
"\n"
"    vec2 local = vec2(vUV.x, 1.0 - vUV.y);\n"
"    vec2 atlasUV = baseUV + local * tileScale;\n"
"    vec3 col = texture(uRoadAtlas, atlasUV).rgb;\n"
"    return vec4(col, 1.0);\n"
"}\n"
"vec4 sampleObstacle()\n"
"{\n"
"    const float tilesX = 2.0;\n"
"    const float tilesY = 2.0;\n"
"\n"
"    float r = hash2(vec2(vCell) + vec2(uSeed * 2.17, uSeed * 3.31));\n"
"    int idx = int(floor(r * 4.0)) % 4;\n"
"\n"
"    int ix = idx % int(tilesX);\n"
"    int iy = idx / int(tilesX);\n"
"\n"
"    vec2 tileScale = vec2(1.0 / tilesX, 1.0 / tilesY);\n"
"    vec2 baseUV = vec2(float(ix), float(iy)) * tileScale;\n"
"\n"
"    vec2 local = vec2(vUV.x, 1.0 - vUV.y);\n"
"    vec2 atlasUV = baseUV + local * tileScale;\n"
"\n"
"    vec3 col = texture(uObstacleAtlas, atlasUV).rgb;\n"
"    return vec4(col, 1.0);\n"
"}\n"
"vec4 sampleStartGoal(bool isStart)\n"
"{\n"
"    const float tilesX = 1.0;\n"
"    const float tilesY = 2.0;\n"
"\n"
"    int idx = isStart ? 0 : 1;\n"
"    int ix = 0;\n"
"    int iy = idx;\n"
"\n"
"    vec2 tileScale = vec2(1.0 / tilesX, 1.0 / tilesY);\n"
"    vec2 baseUV = vec2(float(ix), float(iy)) * tileScale;\n"
"\n"
"    vec2 local = vec2(vUV.x, 1.0 - vUV.y);\n"
"    vec2 atlasUV = baseUV + local * tileScale;\n"
"    return texture(uStartGoalAtlas, atlasUV);\n"
"}\n"


"\n"
"void main()\n"
"{\n"
"    int ty = getCellType(vCell);\n"
"\n"
"    bool isStart = all(equal(vCell, uStartCell));\n"
"    bool isGoal  = all(equal(vCell, uGoalCell));\n"
"\n"
"    if (ty == 1)\n"
"    {\n"
"        if (isStart)\n"
"        {\n"
"            FragColor = sampleStartGoal(true);\n"
"            return;\n"
"        }\n"
"        if (isGoal)\n"
"        {\n"
"            FragColor = sampleStartGoal(false);\n"
"            return;\n"
"        }\n"
"\n"
"        FragColor = sampleRoad();\n"
"        return;\n"
"    }\n"
"    if (ty == 2)\n"
"    {\n"
"        FragColor = sampleObstacle();\n"
"        return;\n"
"    }\n"
"\n"
"    FragColor = sampleGrass();\n"
"}\n";






static const char* enemyVS =
"#version 330 core\n"
"layout (location = 0) in vec2 aPos;\n"
"uniform vec2 uPos;\n"
"uniform vec2 uSize;\n"
"out vec2 vUV;\n"
"void main(){\n"
"    vUV = aPos + vec2(0.5);\n"
"    gl_Position = vec4(aPos * uSize + uPos, 0.0, 1.0);\n"
"}\n";


static const char* enemyFS =
"#version 330 core\n"
"in vec2 vUV;\n"
"uniform sampler2D uBasicEnemy;\n"
"uniform sampler2D uFastEnemy;\n"
"uniform sampler2D uTankEnemy;\n"
"uniform int  uEnemyType;\n"
"uniform bool uUseTexture;\n"
"uniform vec3 uEnemyColor;\n"
"out vec4 FragColor;\n"
"void main(){\n"
"    if (!uUseTexture)\n"
"    {\n"
"        FragColor = vec4(uEnemyColor, 1.0);\n"
"        return;\n"
"    }\n"
"\n"
"    vec2 uv = vec2(vUV.x, 1.0 - vUV.y);\n"
"    vec4 col;\n"
"    if (uEnemyType == 0)\n"
"        col = texture(uBasicEnemy, uv);\n"
"    else if (uEnemyType == 1)\n"
"        col = texture(uFastEnemy,  uv);\n"
"    else\n"
"        col = texture(uTankEnemy,  uv);\n"
"\n"
"    if (col.a < 0.1)\n"
"        discard;\n"
"\n"
"    FragColor = col;\n"
"}\n";



static const char* projectileVS =
"#version 330 core\n"
"layout (location = 0) in vec2 aPos;\n"
"uniform vec2 uPos;\n"
"uniform vec2 uSize;\n"
"out vec2 vLocal;\n"
"void main(){\n"
"    vLocal = aPos;\n"
"    gl_Position = vec4(aPos * uSize + uPos, 0.0, 1.0);\n"
"}\n";

static const char* projectileFS =
"#version 330 core\n"
"in vec2 vLocal;\n"
"uniform vec3 uColor;\n"
"out vec4 FragColor;\n"
"void main(){\n"
"    float r = length(vLocal * 2.0);\n"
"    if (r > 1.0)\n"
"        discard;\n"
"\n"
"    FragColor = vec4(uColor, 1.0);\n"
"}\n";

static const char* rangeVS =
"#version 330 core\n"
"layout (location = 0) in vec2 aPos;\n"
"uniform vec2 uPos;\n"
"uniform vec2 uSize;\n"
"out vec2 vLocal;\n"
"void main(){\n"
"    vLocal = aPos;\n"
"    gl_Position = vec4(aPos * uSize + uPos, 0.0, 1.0);\n"
"}\n";

static const char* rangeFS =
"#version 330 core\n"
"in vec2 vLocal;\n"
"out vec4 FragColor;\n"
"void main(){\n"
"    float r = length(vLocal * 2.0);\n"
"    float thickness = 0.08;\n"
"\n"
"    if (r > 1.0 || r < 1.0 - thickness)\n"
"        discard;\n"
"\n"
"    FragColor = vec4(0.0, 0.0, 0.0, 1.0);\n"
"}\n";

static const char* highlightVS =
"#version 330 core\n"
"layout (location = 0) in vec2 aPos;\n"
"uniform vec2 uPos;\n"
"uniform vec2 uSize;\n"
"out vec2 vLocal;\n"
"void main(){\n"
"    vLocal = aPos;\n"
"    gl_Position = vec4(aPos * uSize + uPos, 0.0, 1.0);\n"
"}\n";

static const char* highlightFS =
"#version 330 core\n"
"in vec2 vLocal;\n"
"uniform vec3 uColor;\n"
"uniform float uThickness;\n"
"out vec4 FragColor;\n"
"void main(){\n"
"    float halfSize = 0.5;\n"
"    float ax = abs(vLocal.x);\n"
"    float ay = abs(vLocal.y);\n"
"\n"
"    if (ax < halfSize - uThickness && ay < halfSize - uThickness)\n"
"        discard;\n"
"\n"
"    FragColor = vec4(uColor, 1.0);\n"
"}\n";


static const char* towerVS =
"#version 330 core\n"
"layout (location = 0) in vec2 aPos;\n"
"uniform vec2 uPos;\n"
"uniform vec2 uSize;\n"
"out vec2 vUV;\n"
"void main(){\n"
"    vUV = aPos + vec2(0.5);\n"
"    gl_Position = vec4(aPos * uSize + uPos, 0.0, 1.0);\n"
"}\n";

static const char* towerFS =
"#version 330 core\n"
"in vec2 vUV;\n"
"uniform sampler2D uTowerAtlas;\n"
"uniform int uSpriteIndex;\n"
"out vec4 FragColor;\n"
"void main(){\n"
"    const float tilesX = 2.0;\n"
"    const float tilesY = 2.0;\n"
"\n"
"    int ix = uSpriteIndex % int(tilesX);\n"
"    int iy = uSpriteIndex / int(tilesX);\n"
"\n"
"    vec2 tileScale = vec2(1.0 / tilesX, 1.0 / tilesY);\n"
"    vec2 baseUV = vec2(float(ix), float(iy)) * tileScale;\n"
"\n"
"    vec2 local = vec2(vUV.x, 1.0 - vUV.y);\n"
"    vec2 atlasUV = baseUV + local * tileScale;\n"
"\n"
"    FragColor = texture(uTowerAtlas, atlasUV);\n"
"}\n";

static const char* panelVS =
"#version 330 core\n"
"layout (location = 0) in vec2 aPos;\n"
"uniform vec2 uPos;\n"
"uniform vec2 uSize;\n"
"void main(){\n"
"    gl_Position = vec4(aPos * uSize + uPos, 0.0, 1.0);\n"
"}\n";

static const char* panelFS =
"#version 330 core\n"
"uniform vec3 uEnemyColor;\n"
"out vec4 FragColor;\n"
"void main(){\n"
"    FragColor = vec4(uEnemyColor, 1.0);\n"
"}\n";


static GLuint loadTexture2D(const char* path)
{
    int w, h, channels;
    stbi_set_flip_vertically_on_load(0); 
    unsigned char* data = stbi_load(path, &w, &h, &channels, 4); 

    if (!data)
    {
        std::cerr << "Failed to load texture: " << path << "\n";
        return 0;
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA8,
        w, h, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, data
    );

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    stbi_image_free(data);
    return tex;
}


static const int   COST_BASIC = 100;
static const int   COST_SLOW = 120;
static const int   COST_FAST = 90;

static constexpr float SPLIT_X = 0.66f;

struct App
{
    int width = 1280;
    int height = 720;

    GLuint VAO = 0, VBO = 0, EBO = 0;

    Shader* shGrid = nullptr;
    Shader* shEnemy = nullptr;
    Shader* shRange = nullptr;     
    Shader* shTower = nullptr;

    Grid* grid = nullptr;
    WaveSystem* waves = nullptr;
    ProjectileSystem* projectiles = nullptr;

    GameState game;  

    GLint g_locGrid = -1;
    GLint g_locTile = -1;
    GLint g_locGap = -1;
    GLint g_locEmpty = -1;
    GLint g_locRoad = -1;
    GLint g_locBlock = -1;
    GLint g_locTypes = -1;
    GLint g_locOffset = -1;

    GLuint grassTex = 0;
    GLuint roadTex = 0;
    GLuint startGoalTex = 0; 
    GLuint obstacleTex = 0;        

    GLint  g_locGrassAtlas = -1;
    GLint  g_locRoadAtlas = -1;
    GLint  g_locStartGoalAtlas = -1;
    GLint  g_locObstacleAtlas = -1;  

    GLint g_locSeed = -1;
    GLint  g_locStartCell = -1;      
    GLint  g_locGoalCell = -1;      
    float grassSeed = 0.0f;

    GLint e_locPos = -1;
    GLint e_locSize = -1;
    GLint e_locColor = -1;

    GLint r_locPos = -1;
    GLint r_locSize = -1;


    double lastTime = 0.0;

    GLuint towerTex = 0;

    GLint  t_locPos = -1;
    GLint  t_locSize = -1;
    GLint  t_locAtlas = -1;
    GLint  t_locSpriteIndex = -1;

    Shader* shProjectile = nullptr;

    GLint p_locPos = -1;
    GLint p_locSize = -1;
    GLint p_locColor = -1;

    Shader* shPanel = nullptr;

    GLuint enemyTexBasic = 0;
    GLuint enemyTexFast = 0;
    GLuint enemyTexTank = 0;

    Shader* shHighlight = nullptr;

    GLint  h_locPos = -1;
    GLint  h_locSize = -1;
    GLint  h_locColor = -1;
    GLint  h_locThickness = -1;

    bool inMainMenu = true; 
    bool paused = false;
    bool showInGameMenu = false;
    bool showHowTo = false;
    bool pauseHowTo = false;

};

static void framebuffer_size_callback(GLFWwindow* w, int width, int height)
{
    App* app = (App*)glfwGetWindowUserPointer(w);
    if (app)
    {
        app->width = width;
        app->height = height;
    }
    glViewport(0, 0, width, height);
}

static void mouse_button_callback(GLFWwindow* w, int button, int action, int mods)
{
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS)
        return;

    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse)
        return; 

    App* app = (App*)glfwGetWindowUserPointer(w);
    if (!app || !app->grid)
        return;

    Selection& sel = app->game.selection;

    double mx, my;
    glfwGetCursorPos(w, &mx, &my);

    float xNDC = float(mx / app->width * 2.0 - 1.0);
    float yNDC = float(1.0 - my / app->height * 2.0);

    int gx, gy;
    if (app->grid->pickCellFromNDC(xNDC, yNDC, gx, gy))
    {
        unsigned char cell = app->grid->getCell(gx, gy);
        sel.hasCell = true;
        sel.gx = gx;
        sel.gy = gy;
        sel.cellType = classifyCell(cell);   
    }
    else
    {
        sel.hasCell = false;
    }
}

int main()
{
    if (!glfwInit())
    {
        std::cerr << "GLFW init failed\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window =
        glfwCreateWindow(1280, 720, "Tile Defense", nullptr, nullptr);
    if (!window)
    {
        std::cerr << "Window creation failed\n";
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "GLAD load failed\n";
        glfwTerminate();
        return 1;
    }

    srand(static_cast<unsigned>(std::time(nullptr)));

    App app;
    glfwSetWindowUserPointer(window, &app);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    float vertices[] = {
        -0.5f,-0.5f,
         0.5f,-0.5f,
         0.5f, 0.5f,
        -0.5f, 0.5f
    };
    unsigned int indices[] = { 0,1,2, 2,3,0 };

    glGenVertexArrays(1, &app.VAO);
    glGenBuffers(1, &app.VBO);
    glGenBuffers(1, &app.EBO);

    glBindVertexArray(app.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, app.VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, app.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    Shader shGrid(gridVS, gridFS);
    Shader shPanel(panelVS, panelFS);
    Shader shEnemy(enemyVS, enemyFS);
    app.shGrid = &shGrid;
    app.shPanel = &shPanel;
    app.shEnemy = &shEnemy;
    Shader shRange(rangeVS, rangeFS);
    app.shRange = &shRange;

    Shader shProjectile(projectileVS, projectileFS);
    app.shProjectile = &shProjectile;

    Shader shTower(towerVS, towerFS);
    app.shTower = &shTower;

    Shader shHighlight(highlightVS, highlightFS);
    app.shHighlight = &shHighlight;

    shTower.use();
    app.t_locPos = shTower.loc("uPos");
    app.t_locSize = shTower.loc("uSize");
    app.t_locAtlas = shTower.loc("uTowerAtlas");
    app.t_locSpriteIndex = shTower.loc("uSpriteIndex");
    glUniform1i(app.t_locAtlas, 5);

    shEnemy.use();
    glUniform1i(shEnemy.loc("uBasicEnemy"), 6);
    glUniform1i(shEnemy.loc("uFastEnemy"), 7);
    glUniform1i(shEnemy.loc("uTankEnemy"), 8);

    shHighlight.use();
    app.h_locPos = shHighlight.loc("uPos");
    app.h_locSize = shHighlight.loc("uSize");
    app.h_locColor = shHighlight.loc("uColor");
    app.h_locThickness = shHighlight.loc("uThickness");



    //USTAWIENIA SIATKI
    GridParams gp{ 28, 25, 0.12f, 0.12f, 0.00f, 0.00f };

    float marginX = 0.02f;
    float marginY = 0.05f;

    float regionMinX = -1.0f + marginX;
    float regionMaxX = SPLIT_X - marginX;
    float regionMinY = -1.0f + marginY;
    float regionMaxY = 1.0f - marginY;

    float regionW = regionMaxX - regionMinX;
    float regionH = regionMaxY - regionMinY;

    float baseW = gp.cols * (gp.tileW + gp.gapX) - gp.gapX;
    float baseH = gp.rows * (gp.tileH + gp.gapY) - gp.gapY;

    float scaleX = regionW / baseW;
    float scaleY = regionH / baseH;
    float scale = std::min(scaleX, scaleY);

    gp.tileW *= scale;
    gp.tileH *= scale;
    gp.gapX *= scale;
    gp.gapY *= scale;

    float offsetX = (regionMinX + regionMaxX) * 0.5f;
    float offsetY = (regionMinY + regionMaxY) * 0.5f;

    Grid grid(gp);
    app.grid = &grid;
    grid.setOffset(offsetX, offsetY);

    WaveSystem waves(&grid);
    app.waves = &waves;

    ProjectileSystem projectiles(&grid);
    app.projectiles = &projectiles;

    float baseEnemySize = gp.tileW * 0.7f;
    waves.setWaves(createDefaultWaves(baseEnemySize));

    shGrid.use();
    app.g_locGrid = shGrid.loc("uGrid");
    app.g_locTile = shGrid.loc("uTileSize");
    app.g_locGap = shGrid.loc("uGap");
    app.g_locEmpty = shGrid.loc("uColorEmpty");
    app.g_locRoad = shGrid.loc("uColorRoad");
    app.g_locBlock = shGrid.loc("uColorBlock");
    app.g_locTypes = shGrid.loc("uTypes");
    app.g_locOffset = shGrid.loc("uOffset");
    app.g_locGrassAtlas = shGrid.loc("uGrassAtlas");
    app.g_locRoadAtlas = shGrid.loc("uRoadAtlas");
    app.g_locSeed = shGrid.loc("uSeed");
    app.g_locStartGoalAtlas = shGrid.loc("uStartGoalAtlas");
    app.g_locStartCell = shGrid.loc("uStartCell");
    app.g_locGoalCell = shGrid.loc("uGoalCell");
    app.g_locObstacleAtlas = shGrid.loc("uObstacleAtlas");




    glUniform2i(app.g_locGrid, gp.cols, gp.rows);
    glUniform2f(app.g_locTile, gp.tileW, gp.tileH);
    glUniform2f(app.g_locGap, gp.gapX, gp.gapY);
    glUniform2f(app.g_locOffset, offsetX, offsetY);

    glUniform3f(app.g_locEmpty, 0.13f, 0.18f, 0.24f);
    glUniform3f(app.g_locRoad, 0.84f, 0.77f, 0.30f);
    glUniform3f(app.g_locBlock, 0.20f, 0.52f, 0.86f);

    app.grassSeed = static_cast<float>(std::rand() % 10000);
    glUniform1f(app.g_locSeed, app.grassSeed);

    glUniform1i(app.g_locTypes, 0);
    glUniform1i(app.g_locGrassAtlas, 1);
    glUniform1i(app.g_locRoadAtlas, 2);
    glUniform1i(app.g_locStartGoalAtlas, 3);
    glUniform1i(app.g_locObstacleAtlas, 4);

    const auto& path = grid.mainPath();
    if (!path.empty())
    {
        auto startCell = path.front();
        auto goalCell = path.back();

        glUniform2i(app.g_locStartCell, startCell.first, startCell.second);
        glUniform2i(app.g_locGoalCell, goalCell.first, goalCell.second);
    }

    shProjectile.use();
    app.p_locPos = shProjectile.loc("uPos");
    app.p_locSize = shProjectile.loc("uSize");
    app.p_locColor = shProjectile.loc("uColor");

    shEnemy.use();
    app.e_locPos = shEnemy.loc("uPos");
    app.e_locSize = shEnemy.loc("uSize");

    shRange.use();
    app.r_locPos = shRange.loc("uPos");
    app.r_locSize = shRange.loc("uSize");

    glClearColor(0.08f, 0.10f, 0.12f, 1.0f);
    app.lastTime = glfwGetTime();

    RectNDC panelBG{
        (1.0f + SPLIT_X) * 0.5f,
        0.0f,
        (1.0f - SPLIT_X),
        2.0f
    };

    app.grassTex = loadTexture2D("assets/tiles.png");
    if (!app.grassTex)
    {
        std::cerr << "Brak tiles.png albo blad ladowania!\n";
    }

    app.roadTex = loadTexture2D("assets/road_tiles.png");
    if (!app.roadTex)
    {
        std::cerr << "Brak road_tiles.png albo blad ladowania!\n";
    }

    app.startGoalTex = loadTexture2D("assets/start_goal.png");
    if (!app.startGoalTex)
    {
        std::cerr << "Brak start_goal.png albo blad ladowania!\n";
    }

    app.obstacleTex = loadTexture2D("assets/obstacles.png");
    if (!app.obstacleTex)
    {
        std::cerr << "Brak obstacles.png albo blad ladowania!\n";
    }

    app.towerTex = loadTexture2D("assets/towers.png");
    if (!app.towerTex)
    {
        std::cerr << "Brak towers.png albo blad ladowania!\n";
    }

    app.enemyTexBasic = loadTexture2D("assets/basic_enemy.png");
    if (!app.enemyTexBasic)
        std::cerr << "Brak basic_enemy.png albo blad ladowania!\n";

    app.enemyTexFast = loadTexture2D("assets/fast_enemy.png");
    if (!app.enemyTexFast)
        std::cerr << "Brak fast_enemy.png albo blad ladowania!\n";

    app.enemyTexTank = loadTexture2D("assets/tank_enemy.png");
    if (!app.enemyTexTank)
        std::cerr << "Brak tank_enemy.png albo blad ladowania!\n";


    while (!glfwWindowShouldClose(window))
    {
        double now = glfwGetTime();
        float dt = float(now - app.lastTime);
        app.lastTime = now;

        glfwPollEvents();


        bool isPlaying = !app.inMainMenu && !app.game.gameOver && !app.game.victory && !app.paused;

        if (isPlaying)
        {
            app.waves->update(dt, app.game);
            app.projectiles->update(dt, *app.waves, app.game);

            if (app.game.lives <= 0)
            {
                app.game.lives = 0;
                app.game.gameOver = true;
            }
            else if (app.waves->state() == WaveState::Finished &&
                app.waves->enemiesAlive() == 0)
            {
                app.game.victory = true;
            }
        }


        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (app.inMainMenu)
        {
            ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2((float)app.width, (float)app.height), ImGuiCond_Always);

            ImGui::Begin("Main Menu", nullptr,
                ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoCollapse);

            ImVec2 winSize = ImGui::GetWindowSize();
            float buttonWidth = winSize.x * 0.4f;
            float buttonHeight = 40.0f;
            float spacing = 20.0f;

            float totalHeight = buttonHeight * 3.0f + spacing * 2.0f;
            float startX = (winSize.x - buttonWidth) * 0.5f;
            float startY = (winSize.y - totalHeight) * 0.5f;

            ImGui::SetCursorPos(ImVec2(startX, startY));
            if (ImGui::Button("Start game", ImVec2(buttonWidth, buttonHeight)))
            {
                app.game = GameState{};
                app.waves->reset();
                app.projectiles->reset();

                app.grid->generateProceduralLevel();

                const auto& path2 = app.grid->mainPath();
                app.shGrid->use();
                if (!path2.empty())
                {
                    auto startCell = path2.front();
                    auto goalCell = path2.back();
                    glUniform2i(app.g_locStartCell, startCell.first, startCell.second);
                    glUniform2i(app.g_locGoalCell, goalCell.first, goalCell.second);
                }

                app.grassSeed = static_cast<float>(std::rand() % 10000);
                if (app.g_locSeed != -1)
                {
                    glUniform1f(app.g_locSeed, app.grassSeed);
                }

                glActiveTexture(GL_TEXTURE4);
                glBindTexture(GL_TEXTURE_2D, app.obstacleTex);

                app.inMainMenu = false;
                app.paused = false;
            }


            ImGui::SetCursorPos(ImVec2(startX, startY + (buttonHeight + spacing)));
            if (ImGui::Button("How to play?", ImVec2(buttonWidth, buttonHeight)))
            {
                app.showHowTo = true;
            }

            ImGui::SetCursorPos(ImVec2(startX, startY + 2.0f * (buttonHeight + spacing)));
            if (ImGui::Button("Exit game", ImVec2(buttonWidth, buttonHeight)))
            {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }

            ImGui::End();

            if (app.inMainMenu && app.showHowTo)
            {
                ImVec2 popupSize((float)app.width * 0.6f, (float)app.height * 0.6f);
                ImVec2 popupPos(((float)app.width - popupSize.x) * 0.5f,
                    ((float)app.height - popupSize.y) * 0.5f);

                ImGui::SetNextWindowPos(popupPos, ImGuiCond_Always);
                ImGui::SetNextWindowSize(popupSize, ImGuiCond_Always);

                ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoCollapse;

                ImGui::Begin("How to play", &app.showHowTo, flags);

                ImGui::TextWrapped("How to play:");
                ImGui::Separator();

                ImGui::BulletText("Enemies walk along the brown road from the portal to the castle.");
                ImGui::BulletText("If an enemy reaches the castle, you lose HP. At 0 HP the game is over.");
                ImGui::BulletText("Click any tile to select it.");
                ImGui::BulletText("Build towers on tiles that have no road or no obstacles (little ponds and rocks).");
                ImGui::BulletText("Use the buttons in the right panel to build different types of towers.");
                ImGui::BulletText("Each tower has its own damage, range and fire rate. You can upgrade and sell towers.");
                ImGui::BulletText("Use 'Next wave' to start the next enemy wave when you are ready.");
                ImGui::BulletText("Earn gold by killing enemies and use it to build or upgrade towers.");
                ImGui::BulletText("Win by surviving all waves!");

                ImGui::Spacing();
                if (ImGui::Button("Back", ImVec2(-FLT_MIN, 0)))
                {
                    app.showHowTo = false;
                }

                ImGui::End();
            }

        }
        else
        {
            float splitPixel = (SPLIT_X + 1.0f) * 0.5f * app.width;
            float margin = 4.0f;
            float panelX = splitPixel + margin;
            float panelWidth = app.width - panelX - margin;

            ImGui::SetNextWindowPos(ImVec2(panelX, 4.0f), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(panelWidth, app.height - 8.0f),
                ImGuiCond_Always);

            ImGui::Begin("Control Panel", nullptr,
                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

            ImGui::Text("Gold: %d", app.game.gold);
            ImGui::Text("HP:   %d / %d", app.game.lives, app.game.maxLives);
            if (app.game.gameOver)
                ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "GAME OVER");
            else if (app.game.victory)
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "VICTORY!");
            ImGui::Separator();

            WaveState st = app.waves->state();
            int waveIdx = app.waves->currentWaveIndex();
            int total = app.waves->totalWaves();
            int alive = app.waves->enemiesAlive();

            const char* stText = "";
            switch (st) {
            case WaveState::Idle:     stText = "Idle (waiting)"; break;
            case WaveState::Spawning: stText = "Spawning";       break;
            case WaveState::Running:  stText = "Running";        break;
            case WaveState::Finished: stText = "Finished";       break;
            }

            if (waveIdx < 0)
                ImGui::Text("Wave: - / %d", total);
            else
                ImGui::Text("Wave: %d / %d", waveIdx + 1, total);

            ImGui::Text("Enemies alive: %d", alive);
            ImGui::Text("State: %s", stText);

            bool canNext = app.waves->canStartNextWave() && !app.game.gameOver && !app.game.victory;
            if (!canNext) ImGui::BeginDisabled();
            if (ImGui::Button("Next wave", ImVec2(-FLT_MIN, 0)))
                app.waves->startNextWave();
            if (!canNext) ImGui::EndDisabled();

            ImGui::Separator();
            if (ImGui::Button("Menu", ImVec2(-FLT_MIN, 0)))
            {
                app.paused = true;
                app.showInGameMenu = true;
                app.pauseHowTo = false;
            }


            ImGui::Separator();
            const WaveConfig* curCfg = app.waves->currentWaveConfig();
            if (curCfg)
            {
                const EnemyStats& es = getEnemyStats(curCfg->type);
                ImGui::Text("Current wave:");
                ImGui::Text("  Type:   %s", enemyTypeToString(curCfg->type));
                ImGui::Text("  Count:  %d", curCfg->enemyCount);
                ImGui::Text("  HP:     %.0f", es.maxHP);
                ImGui::Text("  Speed:  %.2f", es.speed);
                ImGui::Text("  Reward: %d", es.rewardGold);
            }
            else
            {
                ImGui::Text("Current wave: none");
            }

            ImGui::Separator();
            const WaveConfig* nextCfg = app.waves->nextWaveConfig();
            if (nextCfg)
            {
                const EnemyStats& esNext = getEnemyStats(nextCfg->type);
                ImGui::Text("Next wave:");
                ImGui::Text("  Type:   %s", enemyTypeToString(nextCfg->type));
                ImGui::Text("  Count:  %d", nextCfg->enemyCount);
                ImGui::Text("  HP:     %.0f", esNext.maxHP);
                ImGui::Text("  Speed:  %.2f", esNext.speed);
                ImGui::Text("  Reward: %d", esNext.rewardGold);
            }
            else
            {
                ImGui::Text("Next wave: none (last wave)");
            }

            ImGui::Separator();

            Selection& sel = app.game.selection;
            if (!sel.hasCell)
            {
                ImGui::Text("Selection: none");
            }
            else
            {
                const char* kindText =
                    (sel.cellType == CellType::Empty) ? "Empty tile" :
                    (sel.cellType == CellType::Road) ? "Road tile" :
                    "Blocked";
                ImGui::Text("Selection: %s (%d,%d)",
                    kindText, sel.gx, sel.gy);
            }

            Tower* selectedTower = nullptr;
            if (sel.hasCell)
                selectedTower = app.game.findTower(sel.gx, sel.gy);

            bool tileOccupied =
                (selectedTower != nullptr);

            bool baseSpotOk =
                sel.hasCell &&
                sel.cellType == CellType::Empty &&
                !tileOccupied;

            bool canBasic = baseSpotOk && app.game.gold >= COST_BASIC;
            bool canSlow = baseSpotOk && app.game.gold >= COST_SLOW;
            bool canFast = baseSpotOk && app.game.gold >= COST_FAST;

            auto buildTower = [&](TowerType type, int cost)
                {
                    if (!baseSpotOk)        return;
                    if (app.game.gold < cost) return;

                    app.game.gold -= cost;

                    Tower t;
                    t.type = type;
                    t.gx = sel.gx;
                    t.gy = sel.gy;
                    t.cooldown = 0.0f;
                    t.level = 1;
                    t.goldInvested = cost;

                    app.game.towers.push_back(t);
                };

            ImGui::Separator();
            ImGui::Text("Build towers:");

            if (!canBasic) ImGui::BeginDisabled();
            if (ImGui::Button("Build BASIC (100)", ImVec2(-FLT_MIN, 0)))
                buildTower(TowerType::Basic, COST_BASIC);
            if (!canBasic) ImGui::EndDisabled();

            if (!canSlow) ImGui::BeginDisabled();
            if (ImGui::Button("Build SLOW (120)", ImVec2(-FLT_MIN, 0)))
                buildTower(TowerType::Slow, COST_SLOW);
            if (!canSlow) ImGui::EndDisabled();

            if (!canFast) ImGui::BeginDisabled();
            if (ImGui::Button("Build FAST (90)", ImVec2(-FLT_MIN, 0)))
                buildTower(TowerType::Fast, COST_FAST);
            if (!canFast) ImGui::EndDisabled();

            if (!baseSpotOk)
            {
                if (sel.hasCell)
                {
                    if (tileOccupied)
                        ImGui::TextDisabled("Cannot build: tower already here");
                    else if (sel.cellType == CellType::Road)
                        ImGui::TextDisabled("Cannot build on path");
                    else if (sel.cellType == CellType::Block)
                        ImGui::TextDisabled("Cannot build on obstacle");
                }
                else
                {
                    ImGui::TextDisabled("Click a tile on the map");
                }
            }

            if (selectedTower)
            {
                ImGui::Separator();
                const TowerStats& baseStats = getTowerStats(selectedTower->type);
                int level = selectedTower->level;

                ImGui::Text("Selected tower:");
                ImGui::Text("  Type:  %s", towerTypeToString(selectedTower->type));
                ImGui::Text("  Level: %d", level);

                float dmgMul = towerDamageMultiplier(level);
                float rngMul = towerRangeMultiplier(level);
                float rateMul = towerFireRateMultiplier(level);

                ImGui::Text("  Damage:   %.1f", baseStats.damage * dmgMul);
                ImGui::Text("  Range:    %.1f", baseStats.range * rngMul);
                ImGui::Text("  FireRate: %.2f", baseStats.fireRate * rateMul);

                int upgradeCost = static_cast<int>(baseStats.cost * 0.7f * level);
                ImGui::Text("Upgrade cost: %d", upgradeCost);

                int sellValue = static_cast<int>(selectedTower->goldInvested * 0.75f);
                ImGui::Text("Sell value: %d", sellValue);

                bool canUpgrade = (app.game.gold >= upgradeCost);
                if (!canUpgrade) ImGui::BeginDisabled();
                if (ImGui::Button("Upgrade tower", ImVec2(-FLT_MIN, 0)))
                {
                    app.game.gold -= upgradeCost;
                    selectedTower->level += 1;
                    selectedTower->goldInvested += upgradeCost;
                }
                if (!canUpgrade) ImGui::EndDisabled();

                if (ImGui::Button("Sell tower", ImVec2(-FLT_MIN, 0)))
                {
                    app.game.gold += sellValue;

                    auto& towersVec = app.game.towers;
                    auto it = std::find_if(
                        towersVec.begin(), towersVec.end(),
                        [&](const Tower& t) { return &t == selectedTower; }
                    );
                    if (it != towersVec.end())
                    {
                        towersVec.erase(it);
                    }

                    app.game.selection.hasCell = false;
                    selectedTower = nullptr;
                }
            }


            ImGui::End();

            if (app.showInGameMenu)
            {
                ImGui::OpenPopup("InGameMenuPopup");
            }

            if (app.game.gameOver)
            {
                ImGui::OpenPopup("GameOverPopup");
            }
            if (app.game.victory)
            {
                ImGui::OpenPopup("VictoryPopup");
            }

            ImVec2 display = ImGui::GetIO().DisplaySize;

            ImVec2 menuSize(display.x * 0.42f, display.y * 0.55f);

            ImVec2 howToSize(display.x * 0.75f, display.y * 0.80f);

            ImVec2 winSize = app.pauseHowTo ? howToSize : menuSize;

            winSize.x = std::clamp(winSize.x, 420.0f, display.x - 40.0f);
            winSize.y = std::clamp(winSize.y, 320.0f, display.y - 40.0f);

            ImVec2 winPos((display.x - winSize.x) * 0.5f, (display.y - winSize.y) * 0.5f);


            ImGuiWindowFlags popupFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

            ImGui::SetNextWindowPos(winPos, ImGuiCond_Always);
            ImGui::SetNextWindowSize(winSize, ImGuiCond_Always);

            if (ImGui::BeginPopupModal("InGameMenuPopup", nullptr, popupFlags))
            {

                float buttonWidth = winSize.x * 0.8f;
                float buttonHeight = 30.0f;
                float spacing = 10.0f;
                float startX = (winSize.x - buttonWidth) * 0.5f;

                if (!app.pauseHowTo)
                {
                    ImGui::Text("Game menu");
                    ImGui::Separator();

                    float totalH = buttonHeight * 5.0f + spacing * 4.0f;
                    float startY = (winSize.y - totalH) * 0.5f;

                    ImGui::SetCursorPos(ImVec2(startX, startY));
                    if (ImGui::Button("Resume", ImVec2(buttonWidth, buttonHeight)))
                    {
                        app.paused = false;
                        app.showInGameMenu = false;
                        app.pauseHowTo = false;
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::SetCursorPos(ImVec2(startX, startY + (buttonHeight + spacing) * 1.0f));
                    if (ImGui::Button("Restart", ImVec2(buttonWidth, buttonHeight)))
                    {
                        app.game = GameState{};
                        app.waves->reset();
                        app.projectiles->reset();

                        app.paused = false;
                        app.showInGameMenu = false;
                        app.pauseHowTo = false;

                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::SetCursorPos(ImVec2(startX, startY + (buttonHeight + spacing) * 2.0f));
                    if (ImGui::Button("New map", ImVec2(buttonWidth, buttonHeight)))
                    {
                        app.game = GameState{};
                        app.waves->reset();
                        app.projectiles->reset();

                        app.grid->generateProceduralLevel();

                        const auto& path2 = app.grid->mainPath();
                        app.shGrid->use();
                        if (!path2.empty())
                        {
                            auto startCell = path2.front();
                            auto goalCell = path2.back();
                            glUniform2i(app.g_locStartCell, startCell.first, startCell.second);
                            glUniform2i(app.g_locGoalCell, goalCell.first, goalCell.second);
                        }

                        app.grassSeed = static_cast<float>(std::rand() % 10000);
                        if (app.g_locSeed != -1)
                            glUniform1f(app.g_locSeed, app.grassSeed);

                        glActiveTexture(GL_TEXTURE4);
                        glBindTexture(GL_TEXTURE_2D, app.obstacleTex);

                        app.paused = false;
                        app.showInGameMenu = false;
                        app.pauseHowTo = false;

                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::SetCursorPos(ImVec2(startX, startY + (buttonHeight + spacing) * 3.0f));
                    if (ImGui::Button("How to play?", ImVec2(buttonWidth, buttonHeight)))
                    {
                        app.pauseHowTo = true;
                    }

                    ImGui::SetCursorPos(ImVec2(startX, startY + (buttonHeight + spacing) * 4.0f));
                    if (ImGui::Button("Go back to main menu", ImVec2(buttonWidth, buttonHeight)))
                    {
                        app.game = GameState{};
                        app.waves->reset();
                        app.projectiles->reset();

                        app.inMainMenu = true;
                        app.paused = false;
                        app.showInGameMenu = false;
                        app.pauseHowTo = false;

                        ImGui::CloseCurrentPopup();
                    }
                }
                else
                {
                    ImGui::TextWrapped("How to play:");
                    ImGui::Separator();

                    ImGui::BeginChild("HowToScroll", ImVec2(0, -45.0f), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);

                    ImGui::BulletText("Enemies walk along the brown road from the portal to the castle.");
                    ImGui::BulletText("If an enemy reaches the castle, you lose HP. At 0 HP the game is over.");
                    ImGui::BulletText("Click any tile to select it.");
                    ImGui::BulletText("Build towers on tiles that have no road or no obstacles (little ponds and rocks).");
                    ImGui::BulletText("Use the buttons in the right panel to build different types of towers.");
                    ImGui::BulletText("Each tower has its own damage, range and fire rate. You can upgrade and sell towers.");
                    ImGui::BulletText("Use 'Next wave' to start the next enemy wave when you are ready.");
                    ImGui::BulletText("Earn gold by killing enemies and use it to build or upgrade towers.");
                    ImGui::BulletText("Win by surviving all waves!");

                    ImGui::EndChild();

                    if (ImGui::Button("Back to menu", ImVec2(-FLT_MIN, 0)))
                    {
                        app.pauseHowTo = false;
                    }
                }

                ImGui::EndPopup();
            }


            if (ImGui::BeginPopupModal("GameOverPopup", nullptr, popupFlags))
            {
                ImGui::SetWindowPos(winPos, ImGuiCond_Always);
                ImGui::SetWindowSize(winSize, ImGuiCond_Always);

                ImGui::Text("Game Over!");
                ImGui::Separator();

                float buttonWidth = winSize.x * 0.8f;
                float buttonHeight = 30.0f;
                float spacing = 10.0f;
                float totalH = buttonHeight * 3.0f + spacing * 2.0f;
                float startX = (winSize.x - buttonWidth) * 0.5f;
                float startY = (winSize.y - totalH) * 0.5f;

                ImGui::SetCursorPos(ImVec2(startX, startY));
                if (ImGui::Button("Try again?", ImVec2(buttonWidth, buttonHeight)))
                {
                    app.game = GameState{};
                    app.waves->reset();
                    app.projectiles->reset();

                    app.game.gameOver = false;
                    app.game.victory = false;

                    ImGui::CloseCurrentPopup();
                }

                ImGui::SetCursorPos(ImVec2(startX, startY + buttonHeight + spacing));
                if (ImGui::Button("New map", ImVec2(buttonWidth, buttonHeight)))
                {
                    app.game = GameState{};
                    app.waves->reset();
                    app.projectiles->reset();

                    app.grid->generateProceduralLevel();

                    const auto& path = app.grid->mainPath();
                    app.shGrid->use();
                    if (!path.empty())
                    {
                        auto startCell = path.front();
                        auto goalCell = path.back();
                        glUniform2i(app.g_locStartCell, startCell.first, startCell.second);
                        glUniform2i(app.g_locGoalCell, goalCell.first, goalCell.second);
                    }

                    app.grassSeed = static_cast<float>(std::rand() % 10000);
                    if (app.g_locSeed != -1)
                    {
                        glUniform1f(app.g_locSeed, app.grassSeed);
                    }

                    glActiveTexture(GL_TEXTURE4);
                    glBindTexture(GL_TEXTURE_2D, app.obstacleTex);

                    glBindVertexArray(app.VAO);
                    glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, gp.cols * gp.rows);

                    app.game.gameOver = false;
                    app.game.victory = false;

                    ImGui::CloseCurrentPopup();
                }

                ImGui::SetCursorPos(ImVec2(startX, startY + 2.0f * (buttonHeight + spacing)));
                if (ImGui::Button("Main menu", ImVec2(buttonWidth, buttonHeight)))
                {
                    app.game = GameState{};
                    app.waves->reset();
                    app.projectiles->reset();

                    app.inMainMenu = true;
                    app.game.gameOver = false;
                    app.game.victory = false;

                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }

            if (ImGui::BeginPopupModal("VictoryPopup", nullptr, popupFlags))
            {
                ImGui::SetWindowPos(winPos, ImGuiCond_Always);
                ImGui::SetWindowSize(winSize, ImGuiCond_Always);

                ImGui::Text("Victory!");
                ImGui::Separator();

                float buttonWidth = winSize.x * 0.8f;
                float buttonHeight = 30.0f;
                float spacing = 10.0f;
                float totalH = buttonHeight * 3.0f + spacing * 2.0f;
                float startX = (winSize.x - buttonWidth) * 0.5f;
                float startY = (winSize.y - totalH) * 0.5f;

                ImGui::SetCursorPos(ImVec2(startX, startY));
                if (ImGui::Button("Replay?", ImVec2(buttonWidth, buttonHeight)))
                {
                    app.game = GameState{};
                    app.waves->reset();
                    app.projectiles->reset();

                    app.game.gameOver = false;
                    app.game.victory = false;

                    ImGui::CloseCurrentPopup();
                }

                ImGui::SetCursorPos(ImVec2(startX, startY + buttonHeight + spacing));
                if (ImGui::Button("New map", ImVec2(buttonWidth, buttonHeight)))
                {
                    app.game = GameState{};
                    app.waves->reset();
                    app.projectiles->reset();

                    app.grid->generateProceduralLevel();

                    const auto& path = app.grid->mainPath();
                    app.shGrid->use();
                    if (!path.empty())
                    {
                        auto startCell = path.front();
                        auto goalCell = path.back();
                        glUniform2i(app.g_locStartCell, startCell.first, startCell.second);
                        glUniform2i(app.g_locGoalCell, goalCell.first, goalCell.second);
                    }

                    app.grassSeed = static_cast<float>(std::rand() % 10000);
                    if (app.g_locSeed != -1)
                    {
                        glUniform1f(app.g_locSeed, app.grassSeed);
                    }

                    glActiveTexture(GL_TEXTURE4);
                    glBindTexture(GL_TEXTURE_2D, app.obstacleTex);

                    glBindVertexArray(app.VAO);
                    glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, gp.cols * gp.rows);

                    app.game.gameOver = false;
                    app.game.victory = false;

                    ImGui::CloseCurrentPopup();
                }

                ImGui::SetCursorPos(ImVec2(startX, startY + 2.0f * (buttonHeight + spacing)));
                if (ImGui::Button("Main menu", ImVec2(buttonWidth, buttonHeight)))
                {
                    app.game = GameState{};
                    app.waves->reset();
                    app.projectiles->reset();

                    app.inMainMenu = true;
                    app.game.gameOver = false;
                    app.game.victory = false;

                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }
        }


        glClear(GL_COLOR_BUFFER_BIT);

        if (!app.inMainMenu)
        {
            app.shGrid->use();

            app.grid->bindTextureUnit(0);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, app.grassTex);

            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, app.roadTex);

            glActiveTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_2D, app.startGoalTex);

            glActiveTexture(GL_TEXTURE4);
            glBindTexture(GL_TEXTURE_2D, app.obstacleTex);

            glBindVertexArray(app.VAO);
            glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, gp.cols * gp.rows);

            {
                Selection& sel = app.game.selection;
                if (sel.hasCell)
                {
                    float cx, cy;
                    app.grid->cellCenterNDC(sel.gx, sel.gy, cx, cy);

                    float sx = gp.tileW;
                    float sy = gp.tileH;

                    app.shHighlight->use();
                    glBindVertexArray(app.VAO);

                    glUniform2f(app.h_locPos, cx, cy);
                    glUniform2f(app.h_locSize, sx, sy);
                    glUniform3f(app.h_locColor, 0.0f, 0.0f, 0.0f);
                    glUniform1f(app.h_locThickness, 0.08f);

                    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
                }
            }

            drawRect(panelBG, *app.shPanel, app.VAO, 0.03f, 0.04f, 0.05f);

            {
                Selection& sel = app.game.selection;
                if (sel.hasCell)
                {
                    Tower* tw = app.game.findTower(sel.gx, sel.gy);
                    if (tw)
                    {
                        const auto& params = app.grid->params();
                        float tileSpan = params.tileW + params.gapX;

                        const TowerStats& base = getTowerStats(tw->type);
                        float rngMul = towerRangeMultiplier(tw->level);
                        float range = base.range * rngMul;

                        float radiusNDC = range * tileSpan;

                        float cx, cy;
                        app.grid->cellCenterNDC(tw->gx, tw->gy, cx, cy);

                        app.shRange->use();
                        glBindVertexArray(app.VAO);
                        glUniform2f(app.r_locPos, cx, cy);
                        glUniform2f(app.r_locSize, radiusNDC * 2.0f, radiusNDC * 2.0f);

                        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
                    }
                }
            }

            app.shEnemy->use();
            glBindVertexArray(app.VAO);

            glActiveTexture(GL_TEXTURE6);
            glBindTexture(GL_TEXTURE_2D, app.enemyTexBasic);

            glActiveTexture(GL_TEXTURE7);
            glBindTexture(GL_TEXTURE_2D, app.enemyTexFast);

            glActiveTexture(GL_TEXTURE8);
            glBindTexture(GL_TEXTURE_2D, app.enemyTexTank);

            app.waves->draw(*app.shEnemy, app.VAO);

            app.projectiles->draw(*app.shProjectile, app.VAO, app.p_locColor);

            app.shTower->use();
            glBindVertexArray(app.VAO);

            glActiveTexture(GL_TEXTURE5);
            glBindTexture(GL_TEXTURE_2D, app.towerTex);

            for (const auto& t : app.game.towers)
            {
                float cx, cy;
                app.grid->cellCenterNDC(t.gx, t.gy, cx, cy);
                float sx = gp.tileW * 0.8f;
                float sy = gp.tileH * 0.8f;

                glUniform2f(app.t_locPos, cx, cy);
                glUniform2f(app.t_locSize, sx, sy);
                glUniform1i(app.t_locSpriteIndex, towerSpriteIndex(t.type));

                glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            }
        }




        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    glDeleteVertexArrays(1, &app.VAO);
    glDeleteBuffers(1, &app.VBO);
    glDeleteBuffers(1, &app.EBO);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
