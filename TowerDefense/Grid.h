#pragma once
#include <vector>
#include <utility>
#include <glad/glad.h>

struct GridParams {
    int cols;
    int rows;
    float tileW;
    float tileH;
    float gapX;
    float gapY;
};

class Grid {
public:
    Grid(const GridParams& p);
    ~Grid();

    void bindTextureUnit(GLuint unit) const;
    void setCell(int x, int y, unsigned char type);
    unsigned char getCell(int x, int y) const;

    bool pickCellFromNDC(float xNDC, float yNDC, int& outX, int& outY) const;
    void uploadAll();
    void cellCenterNDC(int x, int y, float& outX, float& outY) const;

    const GridParams& params() const { return params_; }
    GLuint texture() const { return tex_; }

    void setOffset(float ox, float oy) { offsetX_ = ox; offsetY_ = oy; }

    const std::vector<std::pair<int, int>>& mainPath() const { return mainPath_; }

    void generateProceduralLevel(bool uploadTexture = true);


private:
    GridParams params_;
    std::vector<unsigned char> types_;
    GLuint tex_{ 0 };

    float offsetX_ = 0.0f;
    float offsetY_ = 0.0f;

    std::vector<std::pair<int, int>> mainPath_;
};
