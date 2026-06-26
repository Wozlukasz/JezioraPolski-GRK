#pragma once
#include <string>
#include <map>
#include <glad/glad.h>
#include <glm/glm.hpp>

struct PlantInfo {
    std::string polishName;
    std::string latinName;
    std::string description;
    std::string habitat;
    std::string interesting;
};

// Dane glypha z bitmapowego atlasu fontu
struct GlyphInfo {
    float s0, t0, s1, t1; // UV (s=x, t=y) w atlasie w przestrzeni OpenGL (t rośnie ku górze)
    float bx, by;          // Bearing względem baseline (w pikselach przy rozmiarze referencyjnym)
    float advance;         // Postęp kursora (piksele)
    int   w, h;            // Wymiary glypha w pikselach atlasu
};

class EncyclopediaOverlay {
public:
    EncyclopediaOverlay() = default;
    ~EncyclopediaOverlay();

    bool init(const std::string& fontPath = "");

    // Wywołaj po wykryciu kliknięcia — przełącza panel dla danej rośliny
    void togglePanel(const std::string& plantSpeciesName);

    // Renderowanie per-klatka
    void render(int screenW, int screenH, float deltaTime);

    bool isPanelVisible() const { return panelAlpha > 0.01f; }

private:
    unsigned int quadVAO  = 0, quadVBO = 0;
    unsigned int fontTex  = 0;
    unsigned int hudShader = 0;
    bool initialized      = false;

    // ---- Font ----
    static constexpr float ATLAS_FONT_PX = 24.0f; // Rozmiar referencyjny atlasu
    int atlasW = 512, atlasH = 512;
    int fontAscent = 0, fontDescent = 0, fontLineGap = 0;
    GlyphInfo glyphs[128]; // ASCII 32..127

    // ---- Stan panelu ----
    std::string lockedPlant = ""; // Gatunek aktualnie wyświetlany
    float panelAlpha = 0.0f;     // Animacja fade-in/out

    // ---- Dane encyklopedii ----
    std::map<std::string, PlantInfo> encyclopedia;

    // ---- Inicjalizacja wewnętrzna ----
    void setupQuad();
    void buildEncyclopedia();
    bool loadFont(const std::string& path);

    // ---- Rysowanie ----
    void drawRect(float x, float y, float w, float h, glm::vec4 color,
                  int sw, int sh);
    void drawGlyph(unsigned char c, float gx, float gy_baseline,
                   float scale, glm::vec4 color, int sw, int sh);
    void drawText(const std::string& text, float x, float y_baseline,
                  float pixelH, glm::vec4 color, int sw, int sh);
    float measureText(const std::string& text, float pixelH);
    float drawWrapped(const std::string& text, float x, float y_baseline,
                      float maxW, float pixelH, glm::vec4 color, int sw, int sh);
    void drawPanel(const PlantInfo& info, float alpha, int sw, int sh);
};
