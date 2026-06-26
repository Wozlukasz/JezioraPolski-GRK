#pragma once
#include <string>
#include <map>
#include <glad/glad.h>
#include <glm/glm.hpp>

struct PlantInfo {
    std::string polishName;   // Polska nazwa wyświetlana w tytule
    std::string latinName;    // Nazwa łacińska
    std::string description;  // Krótki opis (2-3 zdania)
    std::string habitat;      // Siedlisko
    std::string interesting;  // Ciekawostka
};

// Pojedynczy glyph z atlasu fontu
struct GlyphInfo {
    float x0, y0, x1, y1;     // Współrzędne UV w atlasie
    float bx, by;              // Bearing (przesunięcie)
    float advance;             // Postęp kursora
    int w, h;                  // Wymiary glypha w pikselach
};

class EncyclopediaOverlay {
public:
    EncyclopediaOverlay() = default;
    ~EncyclopediaOverlay();

    bool init(const std::string& fontPath = "");
    void render(const std::string& plantSpeciesName, int screenW, int screenH, float deltaTime);

private:
    unsigned int quadVAO = 0, quadVBO = 0;
    unsigned int fontTexture = 0;
    unsigned int hudShader = 0;
    bool initialized = false;

    GlyphInfo glyphs[128];
    float fontScale = 0.0f;
    int atlasWidth = 512, atlasHeight = 512;
    int fontAscent = 0, fontDescent = 0, fontLineGap = 0;

    // Dane encyklopedii — wypełniane przy inicjalizacji
    std::map<std::string, PlantInfo> encyclopedia;

    // Animacja pojawiania się panelu
    float panelAlpha = 0.0f;         // 0.0 = niewidoczny, 1.0 = pełna widoczność
    std::string currentPlant = "";
    std::string lastPlant = "";

    void setupQuad();
    void buildEncyclopedia();
    bool loadFont(const std::string& fontPath);
    void buildFallbackFont();

    // Rysowanie
    void drawRect(float x, float y, float w, float h, glm::vec4 color, int screenW, int screenH);
    void drawText(const std::string& text, float x, float y, float pixelHeight,
                  glm::vec4 color, int screenW, int screenH);
    float measureTextWidth(const std::string& text, float pixelHeight);
    void drawPanel(const PlantInfo& info, float alpha, int screenW, int screenH);
    float drawWrappedText(const std::string& text, float x, float y,
                          float maxWidth, float pixelHeight, glm::vec4 color,
                          int screenW, int screenH);
};
