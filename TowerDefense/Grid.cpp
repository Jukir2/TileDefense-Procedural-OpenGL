#include "Grid.h"
#include <cmath>
#include <algorithm>
#include <random>
#include <cstdint>

namespace
{
    inline float fade(float t)
    {
        return t * t * t * (t * (t * 6.f - 15.f) + 10.f);
    }

    inline float lerp(float a, float b, float t)
    {
        return a + t * (b - a);
    }

    inline std::uint32_t hash2D(int x, int y)
    {
        std::uint32_t h = static_cast<std::uint32_t>(x) * 374761393u + static_cast<std::uint32_t>(y) * 668265263u;
        h = (h ^ (h >> 13)) * 1274126177u;
        return h ^ (h >> 16);
    }

    inline float grad2D(std::uint32_t h, float x, float y)
    {
        int hash = static_cast<int>(h & 7u);
        float u = (hash < 4) ? x : y;
        float v = (hash < 4) ? y : x;
        return ((hash & 1) ? -u : u) + ((hash & 2) ? -v : v);
    }

    float perlinNoise2D(float x, float y)
    {
        int X = static_cast<int>(std::floor(x));
        int Y = static_cast<int>(std::floor(y));

        float xf = x - static_cast<float>(X);
        float yf = y - static_cast<float>(Y);

        float u = fade(xf);
        float v = fade(yf);

        std::uint32_t h00 = hash2D(X, Y);
        std::uint32_t h10 = hash2D(X + 1, Y);
        std::uint32_t h01 = hash2D(X, Y + 1);
        std::uint32_t h11 = hash2D(X + 1, Y + 1);

        float n00 = grad2D(h00, xf, yf);
        float n10 = grad2D(h10, xf - 1.f, yf);
        float n01 = grad2D(h01, xf, yf - 1.f);
        float n11 = grad2D(h11, xf - 1.f, yf - 1.f);

        float x1 = lerp(n00, n10, u);
        float x2 = lerp(n01, n11, u);
        float n = lerp(x1, x2, v);

        return 0.5f * n + 0.5f;
    }

    float perlinFBM2D(float x, float y, int   octaves = 3, float lacunarity = 2.0f, float gain = 0.5f)
    {
        float amp = 1.0f;
        float freq = 1.0f;
        float sum = 0.0f;
        float norm = 0.0f;

        for (int i = 0; i < octaves; ++i)
        {
            sum += perlinNoise2D(x * freq, y * freq) * amp;
            norm += amp;
            amp *= gain;
            freq *= lacunarity;
        }
        return (norm > 0.0f) ? (sum / norm) : 0.0f;
    }

    inline float perlin2D(float x, float y)
    {
        return perlinFBM2D(x, y, 3, 2.0f, 0.5f);
    }
}

Grid::Grid(const GridParams& p)
    : params_(p),
    types_(p.cols* p.rows, 0)
{
    glGenTextures(1, &tex_);
    glBindTexture(GL_TEXTURE_2D, tex_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    generateProceduralLevel(false);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, params_.cols, params_.rows, 0, GL_RED, GL_UNSIGNED_BYTE, types_.data());
}

Grid::~Grid()
{
    if (tex_) glDeleteTextures(1, &tex_);
}

void Grid::bindTextureUnit(GLuint unit) const
{
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, tex_);
}

void Grid::setCell(int x, int y, unsigned char type)
{
    if (x < 0 || y < 0 || x >= params_.cols || y >= params_.rows) return;
    types_[y * params_.cols + x] = type;
    glBindTexture(GL_TEXTURE_2D, tex_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, 1, 1, GL_RED, GL_UNSIGNED_BYTE, &types_[y * params_.cols + x]);
}

unsigned char Grid::getCell(int x, int y) const
{
    if (x < 0 || y < 0 || x >= params_.cols || y >= params_.rows) return 255;
    return types_[y * params_.cols + x];
}

void Grid::uploadAll()
{
    glBindTexture(GL_TEXTURE_2D, tex_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, params_.cols, params_.rows, GL_RED, GL_UNSIGNED_BYTE, types_.data());
}

bool Grid::pickCellFromNDC(float xNDC, float yNDC, int& outX, int& outY) const
{
    float cx = (params_.cols - 1) * 0.5f;
    float cy = (params_.rows - 1) * 0.5f;
    float spanX = params_.tileW + params_.gapX;
    float spanY = params_.tileH + params_.gapY;

    float localX = xNDC - offsetX_;
    float localY = yNDC - offsetY_;

    float colf = localX / spanX + cx;
    float rowf = localY / spanY + cy;

    int col = (int)std::round(colf);
    int row = (int)std::round(rowf);

    if (col < 0 || row < 0 || col >= params_.cols || row >= params_.rows) return false;

    float centerX = (col - cx) * spanX;
    float centerY = (row - cy) * spanY;

    if (std::fabs(localX - centerX) > params_.tileW * 0.5f) return false;
    if (std::fabs(localY - centerY) > params_.tileH * 0.5f) return false;

    outX = col;
    outY = row;
    return true;
}

void Grid::cellCenterNDC(int x, int y, float& outX, float& outY) const
{
    float cx = (params_.cols - 1) * 0.5f;
    float cy = (params_.rows - 1) * 0.5f;
    float spanX = params_.tileW + params_.gapX;
    float spanY = params_.tileH + params_.gapY;

    outX = offsetX_ + (x - cx) * spanX;
    outY = offsetY_ + (y - cy) * spanY;
}


void Grid::generateProceduralLevel(bool uploadTexture)
{
    int cols = params_.cols;
    int rows = params_.rows;
    if (cols <= 0 || rows <= 0)
        return;

    auto index = [cols](int x, int y)
        {
            return y * cols + x;
        };

    auto wouldCreateSquare = [&](int nx, int ny,
        const std::vector<unsigned char>& visited) -> bool
        {
            if (cols < 2 || rows < 2) return false;

            for (int tx = nx - 1; tx <= nx; ++tx)
            {
                for (int ty = ny - 1; ty <= ny; ++ty)
                {
                    if (tx < 0 || ty < 0 || tx + 1 >= cols || ty + 1 >= rows)
                        continue;

                    int idx00 = index(tx, ty);
                    int idx10 = index(tx + 1, ty);
                    int idx01 = index(tx, ty + 1);
                    int idx11 = index(tx + 1, ty + 1);

                    auto isVisitedOrNew = [&](int cx, int cy, int idx)
                        {
                            if (cx == nx && cy == ny)
                                return true;
                            return visited[idx] != 0;
                        };

                    bool v00 = isVisitedOrNew(tx, ty, idx00);
                    bool v10 = isVisitedOrNew(tx + 1, ty, idx10);
                    bool v01 = isVisitedOrNew(tx, ty + 1, idx01);
                    bool v11 = isVisitedOrNew(tx + 1, ty + 1, idx11);

                    if (v00 && v10 && v01 && v11)
                        return true;
                }
            }
            return false;
        };

    std::fill(types_.begin(), types_.end(), 0);
    mainPath_.clear();

    static std::mt19937 rng(std::random_device{}());


    std::uniform_real_distribution<float> noiseShiftDist(0.0f, 1000.0f);
    float noiseShiftX = noiseShiftDist(rng);
    float noiseShiftY = noiseShiftDist(rng);

    const int maxAttempts = 200;              
    const int maxSteps = cols * rows;      
    const int minPathLen = (cols + rows) / 2; 

    for (int attempt = 0; attempt < maxAttempts; ++attempt)
    {
        std::vector<unsigned char>          visited(cols * rows, 0);
        std::vector<std::pair<int, int>>    tmpPath;
        tmpPath.reserve(cols * rows);

        int startYMin = (rows > 2) ? 1 : 0;
        int startYMax = (rows > 2) ? rows - 2 : rows - 1;
        std::uniform_int_distribution<int> startRowDist(startYMin, startYMax);

        int sx = 0;
        int sy = startRowDist(rng);

        int exitSide = 0;
        if (rows > 2)
        {
            std::uniform_int_distribution<int> sideDist(0, 1);
            exitSide = sideDist(rng);
        }

        int ex = cols - 1;
        int ey = sy; 

        if (exitSide == 0)
        {
            std::uniform_int_distribution<int> exitRowDist(startYMin, startYMax);
            ex = cols - 1;
            ey = exitRowDist(rng);
        }
        else
        {
            int minExitX = cols / 2;
            std::uniform_int_distribution<int> exitColDist(minExitX, cols - 1);
            ex = exitColDist(rng);
            ey = 0;
        }

        int x = sx;
        int y = sy;
        visited[index(x, y)] = 1;
        tmpPath.push_back({ x, y });

        bool reachedExit = false;
        std::uniform_real_distribution<float> prob01(0.0f, 1.0f);

        for (int step = 0; step < maxSteps; ++step)
        {
            if (exitSide == 0)
            {
                if (x == cols - 1 && (int)tmpPath.size() >= minPathLen)
                {
                    reachedExit = true;
                    break;
                }
            }
            else
            {
                if (x == ex && y == 0 && (int)tmpPath.size() >= minPathLen)
                {
                    reachedExit = true;
                    break;
                }
            }

            struct Candidate { int nx, ny, dist; };
            std::vector<Candidate> closer;
            std::vector<Candidate> farther;

            auto consider = [&](int nx, int ny)
                {
                    if (nx < 0 || ny < 0 || nx >= cols || ny >= rows)
                        return;

                    if (visited[index(nx, ny)])
                        return;

                    if (wouldCreateSquare(nx, ny, visited))
                        return;

                    const int dx[4] = { 1, -1, 0, 0 };
                    const int dy[4] = { 0, 0, 1, -1 };
                    for (int k = 0; k < 4; ++k)
                    {
                        int ax = nx + dx[k];
                        int ay = ny + dy[k];
                        if (ax < 0 || ay < 0 || ax >= cols || ay >= rows)
                            continue;

                        if (!visited[index(ax, ay)])
                            continue;

                        if (ax == x && ay == y)
                            continue;

                        return;
                    }

                    int dNew = std::abs(nx - ex) + std::abs(ny - ey);
                    int dCurr = std::abs(x - ex) + std::abs(y - ey);

                    Candidate c{ nx, ny, dNew };
                    if (dNew <= dCurr)
                        closer.push_back(c);
                    else
                        farther.push_back(c);
                };


            consider(x + 1, y);
            consider(x - 1, y);
            consider(x, y + 1);
            consider(x, y - 1);

            if (closer.empty() && farther.empty())
                break; 

            std::vector<Candidate>* pool = nullptr;
            float r = prob01(rng);

            if (!closer.empty() && (r < 0.7f || farther.empty()))
                pool = &closer;
            else
                pool = &farther;

            std::uniform_int_distribution<int> pick(0, (int)pool->size() - 1);
            Candidate chosen = (*pool)[pick(rng)];

            x = chosen.nx;
            y = chosen.ny;
            visited[index(x, y)] = 1;
            tmpPath.push_back({ x, y });
        }

        if (reachedExit)
        {
            mainPath_ = tmpPath;

            for (auto [px, py] : mainPath_)
                types_[index(px, py)] = 1;

            float noiseScale = 0.15f;
            float threshold = 0.55f;

            for (int yy = 0; yy < rows; ++yy)
            {
                for (int xx = 0; xx < cols; ++xx)
                {
                    if (types_[index(xx, yy)] != 0)
                        continue;

                    float nx = xx * noiseScale + noiseShiftX;
                    float ny = yy * noiseScale + noiseShiftY;

                    float n = perlin2D(nx, ny); 

                    if (n > threshold)
                        types_[index(xx, yy)] = 2;
                }
            }

            if (uploadTexture)
                uploadAll();

            return;
        }
    }

    std::fill(types_.begin(), types_.end(), 0);
    mainPath_.clear();

    int midY = rows / 2;
    for (int x = 0; x < cols; ++x)
    {
        mainPath_.push_back({ x, midY });
        types_[index(x, midY)] = 1;
    }

    if (uploadTexture)
        uploadAll();
}
