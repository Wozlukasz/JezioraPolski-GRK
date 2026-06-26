#pragma once
#include <string>
#include <map>
#include <vector>
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

    // ---- Uniform locations (cache raz, nie per-glyph) ----
    GLint uColor   = -1;
    GLint uUseTex  = -1;
    GLint uHudTex  = -1;

    // ---- Text batching ----
    // Gromadzimy werteksy w CPU, jeden draw call na flush
    static constexpr int MAX_BATCH_QUADS = 1024; // max znakow na jeden flush
    std::vector<float> textBatch;  // (x,y,u,v) * 6 * N

    // ---- Font ----
    static constexpr float ATLAS_FONT_PX = 24.0f;
    int atlasW = 512, atlasH = 512;
    int fontAscent = 0, fontDescent = 0, fontLineGap = 0;
    GlyphInfo glyphs[128];

    // ---- Stan panelu ----
    std::string lockedPlant = "";
    float panelAlpha = 0.0f;

    std::map<std::string, PlantInfo> encyclopedia;

    // ---- Inicjalizacja ----
    void setupQuad();
    void buildEncyclopedia();
    bool loadFont(const std::string& path);

    // ---- Rysowanie ----
    void drawRect(float x, float y, float w, float h, glm::vec4 color, int sw, int sh);

    // Batch text API:
    // addGlyph dodaje werteksy do textBatch (brak GL calls)
    void addGlyph(unsigned char c, float pen_x, float baseline_y,
                  float scale, int sw, int sh);
    // flushText wrzuca batch na GPU i rysuje jednym draw callem
    void flushText(glm::vec4 color);

    // Wysokościowe API:
    void drawText(const std::string& text, float x, float baseline_y,
                  float pixelH, glm::vec4 color, int sw, int sh);
    float measureText(const std::string& text, float pixelH);
    float drawWrapped(const std::string& text, float x, float baseline_y,
                      float maxW, float pixelH, glm::vec4 color, int sw, int sh);
    void drawPanel(const PlantInfo& info, float alpha, int sw, int sh);
};
