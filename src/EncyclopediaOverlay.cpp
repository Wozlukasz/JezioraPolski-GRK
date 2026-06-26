#include "EncyclopediaOverlay.h"
#include "Shader.h"
#include "Utils.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <cstring>

// stb_truetype — inicjalizacja jednorazowa
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

// ============================================================
// Destruktor
// ============================================================
EncyclopediaOverlay::~EncyclopediaOverlay() {
    if (fontTexture) glDeleteTextures(1, &fontTexture);
    if (quadVAO)     glDeleteVertexArrays(1, &quadVAO);
    if (quadVBO)     glDeleteBuffers(1, &quadVBO);
}

// ============================================================
// Inicjalizacja
// ============================================================
bool EncyclopediaOverlay::init(const std::string& fontPath) {
    setupQuad();
    buildEncyclopedia();

    // Próbuj załadować font z podanej ścieżki lub systemowych fontów Windows
    std::vector<std::string> candidates;
    if (!fontPath.empty()) candidates.push_back(fontPath);
    candidates.push_back("C:/Windows/Fonts/arial.ttf");
    candidates.push_back("C:/Windows/Fonts/segoeui.ttf");
    candidates.push_back("C:/Windows/Fonts/calibri.ttf");
    candidates.push_back("C:/Windows/Fonts/tahoma.ttf");
    candidates.push_back("C:/Windows/Fonts/verdana.ttf");

    bool fontLoaded = false;
    for (const auto& p : candidates) {
        if (loadFont(p)) { fontLoaded = true; break; }
    }

    if (!fontLoaded) {
        std::cerr << "[Encyclopedia] Nie znaleziono zadnego pliku .ttf! Uzyje fontu awaryjnego.\n";
        buildFallbackFont();
    }

    hudShader = createShaderProgramFromFiles(
        findAssetPath("Assets/shaders/hud.vert").c_str(),
        findAssetPath("Assets/shaders/hud.frag").c_str()
    );

    if (!hudShader) {
        std::cerr << "[Encyclopedia] Blad kompilacji shadera HUD!\n";
        return false;
    }

    initialized = true;
    std::cout << "[Encyclopedia] Overlay encyklopedii zainicjalizowany.\n";
    return true;
}

// ============================================================
// Konfiguracja quada HUD (jednostkowy kwadrat [0,1]x[0,1])
// ============================================================
void EncyclopediaOverlay::setupQuad() {
    // Quad: pozycja (x,y) + UV (u,v) — przestrzeń [0,1]
    float verts[] = {
        0.0f, 0.0f,   0.0f, 1.0f,
        1.0f, 0.0f,   1.0f, 1.0f,
        1.0f, 1.0f,   1.0f, 0.0f,

        0.0f, 0.0f,   0.0f, 1.0f,
        1.0f, 1.0f,   1.0f, 0.0f,
        0.0f, 1.0f,   0.0f, 0.0f,
    };

    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

// ============================================================
// Dane encyklopedii
// ============================================================
void EncyclopediaOverlay::buildEncyclopedia() {
    encyclopedia["Moczarka Delikatna"] = {
        "Moczarka Delikatna",
        "Elodea nuttallii",
        "Drobna roslina naczeniowa calkowicie zanurzona w wodzie. Pochodzi z Ameryki Polnocnej i zostala zawleczona do Europy jako roslina akwariowa.",
        "Strefa litoralna jezior, rzeki i kanaly o czystej lub umiarkowanie zeutrofizowanej wodzie.",
        "Tworzy geste lany pod woda, co stwarza doskonale kryjowki dla narybku i drobnych bezkregowcow."
    };
    encyclopedia["Mech Zdrojek"] = {
        "Mech Zdrojek",
        "Fontinalis antipyretica",
        "Jeden z najwiekszych mchow wodnych w Europie. Rosnie przytwierdzona do kamieni i zanurzonego drewna, tworzac dlugie wstegi.",
        "Czystemzimnewody plynacie i jeziorowe, dobrze natlenione.",
        "Dawniej uzywana jako material uszczelniajacy (zdrojek = woda biezaca). Bardzo wrazliwa na zanieczyszczenia organiczne."
    };
    encyclopedia["Moczarka Kanadyjska"] = {
        "Moczarka Kanadyjska",
        "Elodea canadensis",
        "Popularny chwast wodny o trojlistnych okoldkach. Jedna z najszerzej rozprzestrzenionych inwazyjnych roslin wodnych w Europie.",
        "Stojace i wolno plynacie wody, preferuje miejsca nasłonecznione.",
        "Masowo stosowana w akwariach na calym swiecie. W Europie przybyła w XIX w. i bardzo szybko opanowala rzeki i jeziora."
    };
    encyclopedia["Rogatek Sztywny"] = {
        "Rogatek Sztywny",
        "Ceratophyllum demersum",
        "Roslina calkowicie zanurzona w wodzie, pozbawiona korzeni — dryfuje lub zakotwicza sie miedzy innymi roslinami. Liscie sa rozwidlone jak poroze jelenia.",
        "Wody stojace i wolno plynacie, bogane w zwiazki biogenne.",
        "Bardzo wazna roslina tlenowa — w ciagu letniego dnia produkuje duze ilosci tlenu, co korzystnie wplywa na ryby."
    };
    encyclopedia["Rogatek Krotkoszyjkowy"] = {
        "Rogatek Krotkoszyjkowy",
        "Ceratophyllum submersum",
        "Podobny do rogatka sztywnego, lecz o migkszych lisciach i mniejszej tolerancji na chlod. Rzadszy od swojego krewniaka.",
        "Plytkie, cieple i eutroficzne akweny, dobrze nastonecznione.",
        "W przeciwienstwie do wielu roslin wodnych rozmnaza sie glownie wegtatywnie — przez fragmenty pedu."
    };
    encyclopedia["Tatarak"] = {
        "Tatarak Zwyczajny",
        "Acorus calamus",
        "Okazala, szybko rosnaca roslina szuwarowa o lancetowatych, aromatycznych lisciach. Moze osiagac ponad 1 metr wysokosci.",
        "Brzegi jezior, rzek, rowow i bagien — w strefie platko wodnej.",
        "Klocz tataraku (klacze) jest uzywany w ziololecznictwie i przemysle perfumeryjnym. Olejki eteryczne zawarte w roslinie dzialaja antyseptycznie."
    };
    encyclopedia["Osoka Woda"] = {
        "Osoka Aloesowata",
        "Stratiotes aloides",
        "Interesujaca roslina plywajaca — latem wynurza sie z wody, zima opada na dno i przezimuje tam w spoczynku. Liscie sa zaostrzone i kolczaste.",
        "Plytkie, czyste jeziora i starorzecza o wysokiej zasobnosci wapnia.",
        "Ochrona scisl w Polsce. Tworzy zwarte skupiska, w ktorych rozwinelo sie wiele specjalistycznych bezkregowcow."
    };
    encyclopedia["Osoka Brzeg"] = {
        "Osoka Aloesowata",
        "Stratiotes aloides",
        "Egzemplarze rosnace na plytkich skrajach linii brzegowej. Liscie osoki sa czerow odobne do aloesa — grubiaste i zakonczone ostrymi kraweddziami.",
        "Strefa plycizny przybrzeznej o czystej wodzie.",
        "Gatunek chroniony. Ogolnie rzecz biorac, im wiecej osoki w jeziorze, tym lepsza jakosc wody."
    };
}

// ============================================================
// Ladowanie fontu TTF przez stb_truetype
// ============================================================
bool EncyclopediaOverlay::loadFont(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;

    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<unsigned char> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) return false;

    stbtt_fontinfo fontInfo;
    if (!stbtt_InitFont(&fontInfo, buffer.data(), stbtt_GetFontOffsetForIndex(buffer.data(), 0))) {
        std::cerr << "[Encyclopedia] stbtt_InitFont nieudane dla: " << path << "\n";
        return false;
    }

    // Rozmiar wzorcowy do wypalenia atlasu (pixels na em)
    const float PIXEL_HEIGHT = 24.0f;
    fontScale = stbtt_ScaleForPixelHeight(&fontInfo, PIXEL_HEIGHT);

    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&fontInfo, &ascent, &descent, &lineGap);
    fontAscent  = (int)std::ceil(ascent  * fontScale);
    fontDescent = (int)std::ceil(descent * fontScale);
    fontLineGap = (int)std::ceil(lineGap * fontScale);

    // Atlas bitmapowy
    std::vector<unsigned char> atlas(atlasWidth * atlasHeight, 0);

    stbtt_pack_context pc;
    stbtt_PackBegin(&pc, atlas.data(), atlasWidth, atlasHeight, 0, 1, nullptr);
    stbtt_PackSetOversampling(&pc, 2, 2);

    stbtt_packedchar pdata[96]; // ASCII 32..127
    stbtt_PackFontRange(&pc, buffer.data(), 0, PIXEL_HEIGHT, 32, 96, pdata);
    stbtt_PackEnd(&pc);

    // Wypełnij tablicę glyphs[]
    for (int i = 0; i < 96; i++) {
        int c = i + 32;
        if (c >= 128) break;
        auto& g = glyphs[c];
        auto& p = pdata[i];

        g.x0 = (float)p.x0 / atlasWidth;
        g.y0 = (float)p.y0 / atlasHeight;
        g.x1 = (float)p.x1 / atlasWidth;
        g.y1 = (float)p.y1 / atlasHeight;
        g.bx = p.xoff;
        g.by = p.yoff;
        g.advance = p.xadvance;
        g.w = p.x1 - p.x0;
        g.h = p.y1 - p.y0;
    }

    // Upload do GPU (R8 — jeden kanał)
    if (fontTexture) glDeleteTextures(1, &fontTexture);
    glGenTextures(1, &fontTexture);
    glBindTexture(GL_TEXTURE_2D, fontTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, atlasWidth, atlasHeight, 0, GL_RED, GL_UNSIGNED_BYTE, atlas.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4); // Przywróć default

    std::cout << "[Encyclopedia] Font zaladowany: " << path << "\n";
    return true;
}

// ============================================================
// Awaryjny font — 8x8 bitmapy ASCII (klasyczny PC-BIOS)
// ============================================================
void EncyclopediaOverlay::buildFallbackFont() {
    // Minimalna konfiguracja — każdy glif ma stały rozmiar 8x8
    const float GLYPH_W = 8.0f;
    const float GLYPH_H = 16.0f;
    fontAscent  = (int)GLYPH_H;
    fontDescent = 0;
    fontLineGap = 2;
    fontScale   = 1.0f;

    for (int c = 32; c < 128; c++) {
        auto& g = glyphs[c];
        int idx = c - 32;
        int col = idx % 16;
        int row = idx / 16;
        g.x0 = col / 16.0f;
        g.y0 = row / 6.0f;
        g.x1 = (col + 1) / 16.0f;
        g.y1 = (row + 1) / 6.0f;
        g.bx = 0; g.by = 0;
        g.advance = GLYPH_W + 1;
        g.w = (int)GLYPH_W; g.h = (int)GLYPH_H;
    }

    // Biała tekstura 1x1 jako placeholder
    unsigned char white = 255;
    if (fontTexture) glDeleteTextures(1, &fontTexture);
    glGenTextures(1, &fontTexture);
    glBindTexture(GL_TEXTURE_2D, fontTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE, &white);
}

// ============================================================
// Rysowanie quada w przestrzeni ekranu (piksele)
// ============================================================
void EncyclopediaOverlay::drawRect(float x, float y, float w, float h, glm::vec4 color,
                                    int screenW, int screenH) {
    // Konwersja pikseli → NDC (x: -1..1, y: -1..1, oś Y w górę)
    float ndcX = (x / screenW) * 2.0f - 1.0f;
    float ndcY = 1.0f - (y / screenH) * 2.0f;   // odwrócona Y
    float ndcW = (w / screenW) * 2.0f;
    float ndcH = (h / screenH) * 2.0f;

    glUseProgram(hudShader);
    glUniform2f(glGetUniformLocation(hudShader, "offset"), ndcX, ndcY - ndcH);
    glUniform2f(glGetUniformLocation(hudShader, "scale"),  ndcW, ndcH);
    glUniform4fv(glGetUniformLocation(hudShader, "color"), 1, &color[0]);
    glUniform1i(glGetUniformLocation(hudShader, "useTexture"), 0);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

// ============================================================
// Rysowanie tekstu przez atlas fontu
// ============================================================
void EncyclopediaOverlay::drawText(const std::string& text, float x, float y,
                                    float pixelHeight, glm::vec4 color,
                                    int screenW, int screenH) {
    if (!fontTexture) return;

    // Skalowanie względem rozmiaru wzorcowego atlasu (24px)
    const float ATLAS_SIZE = 24.0f;
    float scale = pixelHeight / ATLAS_SIZE;

    glUseProgram(hudShader);
    glUniform1i(glGetUniformLocation(hudShader, "useTexture"), 1);
    glUniform4fv(glGetUniformLocation(hudShader, "color"), 1, &color[0]);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fontTexture);
    glUniform1i(glGetUniformLocation(hudShader, "hudTex"), 0);

    float cx = x;
    for (unsigned char c : text) {
        if (c < 32 || c >= 128) { cx += pixelHeight * 0.5f; continue; }
        const auto& g = glyphs[c];

        float gx = cx + g.bx * scale;
        float gy = y  + g.by * scale;
        float gw = g.w * scale;
        float gh = g.h * scale;

        // Konwersja → NDC
        float nx = (gx / screenW) * 2.0f - 1.0f;
        float ny = 1.0f - (gy / screenH) * 2.0f;
        float nw = (gw / screenW) * 2.0f;
        float nh = (gh / screenH) * 2.0f;

        // Dynamiczny quad z UV glypha — nadpisz VBO
        float verts[] = {
            0.0f, 0.0f,   g.x0, g.y1,
            1.0f, 0.0f,   g.x1, g.y1,
            1.0f, 1.0f,   g.x1, g.y0,

            0.0f, 0.0f,   g.x0, g.y1,
            1.0f, 1.0f,   g.x1, g.y0,
            0.0f, 1.0f,   g.x0, g.y0,
        };

        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

        glUniform2f(glGetUniformLocation(hudShader, "offset"), nx, ny - nh);
        glUniform2f(glGetUniformLocation(hudShader, "scale"),  nw, nh);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        cx += g.advance * scale;
    }

    // Przywróć domyślny quad (identyczny dla tła)
    float defaultVerts[] = {
        0.f,0.f, 0.f,1.f,  1.f,0.f, 1.f,1.f,  1.f,1.f, 1.f,0.f,
        0.f,0.f, 0.f,1.f,  1.f,1.f, 1.f,0.f,  0.f,1.f, 0.f,0.f,
    };
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(defaultVerts), defaultVerts);
    glBindVertexArray(0);
}

float EncyclopediaOverlay::measureTextWidth(const std::string& text, float pixelHeight) {
    const float ATLAS_SIZE = 24.0f;
    float scale = pixelHeight / ATLAS_SIZE;
    float w = 0.0f;
    for (unsigned char c : text) {
        if (c < 32 || c >= 128) { w += pixelHeight * 0.5f; continue; }
        w += glyphs[c].advance * scale;
    }
    return w;
}

// ============================================================
// Tekst z zawijaniem słów
// ============================================================
float EncyclopediaOverlay::drawWrappedText(const std::string& text, float x, float y,
                                            float maxWidth, float pixelHeight,
                                            glm::vec4 color, int screenW, int screenH) {
    const float lineH = pixelHeight * 1.35f;
    float curY = y;

    std::istringstream iss(text);
    std::string word;
    std::string line;

    while (iss >> word) {
        std::string testLine = line.empty() ? word : line + " " + word;
        if (measureTextWidth(testLine, pixelHeight) > maxWidth && !line.empty()) {
            drawText(line, x, curY, pixelHeight, color, screenW, screenH);
            curY += lineH;
            line = word;
        } else {
            line = testLine;
        }
    }
    if (!line.empty()) {
        drawText(line, x, curY, pixelHeight, color, screenW, screenH);
        curY += lineH;
    }
    return curY;
}

// ============================================================
// Rysowanie panelu encyklopedii
// ============================================================
void EncyclopediaOverlay::drawPanel(const PlantInfo& info, float alpha, int screenW, int screenH) {
    const float PANEL_W   = 370.0f;
    const float PADDING   = 16.0f;
    const float TITLE_H   = 22.0f;
    const float LATIN_H   = 14.0f;
    const float BODY_H    = 13.0f;
    const float LABEL_H   = 11.5f;
    const float LINE_SPACING = 4.0f;

    // Wylicz wysokość panelu (szacowana)
    float estimatedH = PADDING
        + TITLE_H + LINE_SPACING
        + LATIN_H + LINE_SPACING * 2
        + BODY_H * 5 + LINE_SPACING * 2
        + LABEL_H * 2 + LINE_SPACING
        + LABEL_H * 2 + PADDING;

    float px = screenW - PANEL_W - 20.0f;   // Prawy górny róg
    float py = 20.0f;

    // ----- Tło z efektem glassmorphism -----
    // Zewnętrzna warstwa (ciemna): szklany efekt
    drawRect(px - 2, py - 2, PANEL_W + 4, estimatedH + 4,
             glm::vec4(0.05f, 0.12f, 0.08f, 0.92f * alpha), screenW, screenH);
    // Wewnętrzny panel (jaśniejszy)
    drawRect(px, py, PANEL_W, estimatedH,
             glm::vec4(0.08f, 0.20f, 0.12f, 0.88f * alpha), screenW, screenH);

    // Pasek tytułowy (akcentowy kolor)
    drawRect(px, py, PANEL_W, TITLE_H + PADDING * 1.5f,
             glm::vec4(0.10f, 0.35f, 0.18f, 0.95f * alpha), screenW, screenH);
    // Lewy akcent — pionowa kreska
    drawRect(px, py, 4.0f, estimatedH,
             glm::vec4(0.30f, 0.85f, 0.45f, alpha), screenW, screenH);

    float tx = px + PADDING + 4.0f;
    float ty = py + PADDING * 0.8f;

    // ----- Tytuł -----
    drawText(info.polishName, tx, ty, TITLE_H,
             glm::vec4(0.90f, 1.00f, 0.92f, alpha), screenW, screenH);
    ty += TITLE_H + LINE_SPACING;

    // ----- Nazwa łacińska (kursywa — uproszczona) -----
    std::string latinStr = "(" + info.latinName + ")";
    drawText(latinStr, tx, ty, LATIN_H,
             glm::vec4(0.60f, 0.90f, 0.68f, alpha * 0.9f), screenW, screenH);
    ty += LATIN_H + LINE_SPACING * 2.5f;

    // ----- Separator -----
    drawRect(tx, ty - 3, PANEL_W - PADDING * 2, 1,
             glm::vec4(0.30f, 0.75f, 0.40f, alpha * 0.5f), screenW, screenH);

    // ----- Opis -----
    ty = drawWrappedText(info.description, tx, ty, PANEL_W - PADDING * 2.5f,
                         BODY_H, glm::vec4(0.88f, 0.96f, 0.90f, alpha), screenW, screenH);
    ty += LINE_SPACING * 1.5f;

    // ----- Siedlisko -----
    drawText("Siedlisko:", tx, ty, LABEL_H,
             glm::vec4(0.45f, 0.90f, 0.58f, alpha), screenW, screenH);
    ty += LABEL_H + 2.0f;
    ty = drawWrappedText(info.habitat, tx + 6, ty, PANEL_W - PADDING * 3.0f,
                         BODY_H, glm::vec4(0.80f, 0.93f, 0.84f, alpha * 0.9f), screenW, screenH);
    ty += LINE_SPACING;

    // ----- Ciekawostka -----
    drawText("Ciekawostka:", tx, ty, LABEL_H,
             glm::vec4(0.45f, 0.90f, 0.58f, alpha), screenW, screenH);
    ty += LABEL_H + 2.0f;
    drawWrappedText(info.interesting, tx + 6, ty, PANEL_W - PADDING * 3.0f,
                    BODY_H, glm::vec4(0.80f, 0.93f, 0.84f, alpha * 0.9f), screenW, screenH);
}

// ============================================================
// Główna metoda render — wywoływana co klatkę
// ============================================================
void EncyclopediaOverlay::render(const std::string& plantSpeciesName,
                                  int screenW, int screenH, float deltaTime) {
    if (!initialized) return;

    // Animacja fade-in/out
    const float FADE_SPEED = 3.5f;
    if (!plantSpeciesName.empty()) {
        currentPlant = plantSpeciesName;
        panelAlpha = std::min(1.0f, panelAlpha + deltaTime * FADE_SPEED);
    } else {
        panelAlpha = std::max(0.0f, panelAlpha - deltaTime * FADE_SPEED);
        if (panelAlpha <= 0.0f) currentPlant = "";
    }

    if (panelAlpha <= 0.01f || currentPlant.empty()) return;

    // Wyszukaj dane w encyklopedii
    auto it = encyclopedia.find(currentPlant);
    if (it == encyclopedia.end()) return;

    // Zachowaj stan OpenGL
    GLboolean depthTest, cullFace, blend;
    glGetBooleanv(GL_DEPTH_TEST, &depthTest);
    glGetBooleanv(GL_CULL_FACE, &cullFace);
    glGetBooleanv(GL_BLEND,     &blend);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    drawPanel(it->second, panelAlpha, screenW, screenH);

    // Przywróć stan
    if (depthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (cullFace)  glEnable(GL_CULL_FACE);  else glDisable(GL_CULL_FACE);
    if (!blend)    glDisable(GL_BLEND);
}
