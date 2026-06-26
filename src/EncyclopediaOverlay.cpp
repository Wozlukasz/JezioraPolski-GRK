#include "EncyclopediaOverlay.h"
#include "Shader.h"
#include "Utils.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cmath>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

// ============================================================
// Destruktor
// ============================================================
EncyclopediaOverlay::~EncyclopediaOverlay() {
    if (fontTex) glDeleteTextures(1, &fontTex);
    if (quadVAO) glDeleteVertexArrays(1, &quadVAO);
    if (quadVBO) glDeleteBuffers(1, &quadVBO);
}

// ============================================================
// init
// ============================================================
bool EncyclopediaOverlay::init(const std::string& fontPath) {
    // VAO/VBO z miejscem na MAX_BATCH_QUADS kwadrów
    const int VERT_FLOATS = MAX_BATCH_QUADS * 6 * 4; // quad=6 werteksow, werteks=4 floaty
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, VERT_FLOATS * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    textBatch.reserve(MAX_BATCH_QUADS * 6 * 4);

    buildEncyclopedia();

    // Szukanie czcionki
    for (const char* p : {
        fontPath.empty() ? nullptr : fontPath.c_str(),
        "C:/Windows/Fonts/calibri.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/tahoma.ttf",
        "C:/Windows/Fonts/verdana.ttf"
    }) {
        if (p && loadFont(p)) break;
    }

    if (!fontTex) {
        std::cerr << "[Encyclopedia] Nie znaleziono czcionki!\n";
        return false;
    }

    hudShader = createShaderProgramFromFiles(
        findAssetPath("Assets/shaders/hud.vert").c_str(),
        findAssetPath("Assets/shaders/hud.frag").c_str()
    );
    if (!hudShader) {
        std::cerr << "[Encyclopedia] Blad shadera HUD!\n";
        return false;
    }

    // Cache uniform locations — tylko RAZ, nie per-glyph!
    uColor  = glGetUniformLocation(hudShader, "color");
    uUseTex = glGetUniformLocation(hudShader, "useTexture");
    uHudTex = glGetUniformLocation(hudShader, "hudTex");

    glUseProgram(hudShader);
    glUniform1i(uHudTex, 0);  // unit 0 — ustawiamy raz na zawsze

    initialized = true;
    std::cout << "[Encyclopedia] OK. Kliknij LPM patrzac na rosline.\n";
    return true;
}

// ============================================================
// loadFont — BakeFontBitmap (proste i niezawodne)
// ============================================================
bool EncyclopediaOverlay::loadFont(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;
    size_t sz = (size_t)f.tellg();
    f.seekg(0);
    std::vector<unsigned char> buf(sz);
    if (!f.read((char*)buf.data(), sz)) return false;

    stbtt_fontinfo info;
    if (!stbtt_InitFont(&info, buf.data(), stbtt_GetFontOffsetForIndex(buf.data(), 0)))
        return false;

    float sc = stbtt_ScaleForPixelHeight(&info, ATLAS_FONT_PX);
    int asc, desc, lg;
    stbtt_GetFontVMetrics(&info, &asc, &desc, &lg);
    fontAscent  = (int)std::ceil(asc  * sc);
    fontDescent = (int)std::ceil(desc * sc);
    fontLineGap = (int)std::ceil(lg   * sc);

    std::vector<unsigned char> atlas(atlasW * atlasH, 0);
    stbtt_bakedchar baked[96];
    int rows = stbtt_BakeFontBitmap(
        buf.data(), 0, ATLAS_FONT_PX,
        atlas.data(), atlasW, atlasH,
        32, 96, baked
    );
    if (rows == 0) {
        std::cerr << "[Encyclopedia] BakeFontBitmap FAIL\n";
        return false;
    }

    // Flip pionowy atlasu (stb = Y↓, OpenGL texture = Y↑)
    for (int row = 0; row < atlasH / 2; ++row) {
        auto* a = atlas.data() + row * atlasW;
        auto* b = atlas.data() + (atlasH - 1 - row) * atlasW;
        std::swap_ranges(a, a + atlasW, b);
    }

    // Wypełnij GlyphInfo z uwzględnieniem flipa
    for (int i = 0; i < 96; i++) {
        int c = i + 32;
        if (c >= 128) break;
        auto& g  = glyphs[c];
        auto& bk = baked[i];
        // Po flipie: y0_nowe = atlasH - bk.y1, y1_nowe = atlasH - bk.y0
        int y0f = atlasH - (int)bk.y1;
        int y1f = atlasH - (int)bk.y0;
        g.s0 = bk.x0 / (float)atlasW;
        g.s1 = bk.x1 / (float)atlasW;
        g.t0 = y0f   / (float)atlasH;   // dół UV (GL)
        g.t1 = y1f   / (float)atlasH;   // góra UV (GL)
        g.bx = bk.xoff;
        g.by = bk.yoff;
        g.advance = bk.xadvance;
        g.w = (int)(bk.x1 - bk.x0);
        g.h = (int)(bk.y1 - bk.y0);
    }

    if (fontTex) glDeleteTextures(1, &fontTex);
    glGenTextures(1, &fontTex);
    glBindTexture(GL_TEXTURE_2D, fontTex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, atlasW, atlasH,
                 0, GL_RED, GL_UNSIGNED_BYTE, atlas.data());
    // Swizzle: GL_RED → czytamy .r poprawnie na każdym sterowniku
    const GLint swizzle[] = {GL_RED, GL_RED, GL_RED, GL_RED};
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    std::cout << "[Encyclopedia] Font: " << path << " (ascent=" << fontAscent << ")\n";
    return true;
}

// ============================================================
// Dane encyklopedii
// ============================================================
void EncyclopediaOverlay::buildEncyclopedia() {
    encyclopedia["Moczarka Delikatna"] = {
        "Moczarka Delikatna", "Elodea nuttallii",
        "Drobna roslina zanurzona w wodzie. Pochodzi z Ameryki Polnocnej i zostala zawleczona do Europy jako roslina akwariowa w XIX w.",
        "Strefa litoralna jezior i kanaly o czystej lub umiarkowanie zeutrofizowanej wodzie.",
        "Tworzy geste lany stanowiace doskonale kryjowki dla narybku i drobnych bezkregowcow."
    };
    encyclopedia["Mech Zdrojek"] = {
        "Mech Zdrojek", "Fontinalis antipyretica",
        "Jeden z najwiekszych mchow wodnych w Europie. Rosnie przytwierdzona do kamieni, tworzac dlugie ciemne wstegi.",
        "Czyste, zimne wody plynacie i jeziorowe, dobrze natlenione.",
        "Dawniej uzywana do uszczelniania szpar w budowlach. Bardzo wrazliwa na zanieczyszczenia."
    };
    encyclopedia["Moczarka Kanadyjska"] = {
        "Moczarka Kanadyjska", "Elodea canadensis",
        "Popularny chwast wodny o trojlistnych okoldkach. Jedna z najszerzej rozprzestrzenionych inwazyjnych roslin w Europie.",
        "Stojace i wolno plynacie wody, miejsca nasłonecznione lub polcieniste.",
        "Przybyła do Europy w XIX w. i bardzo szybko opanowała rzeki i jeziora calego kontynentu."
    };
    encyclopedia["Rogatek Sztywny"] = {
        "Rogatek Sztywny", "Ceratophyllum demersum",
        "Roslina zanurzona, bez korzeni. Dryfuje lub zakotwicza sie miedzy roslinami. Liscie rozwidlone jak poroze.",
        "Wody stojace i wolno plynacie, bogate w zwiazki biogenne.",
        "W sloneczny dzien produkuje duze ilosci tlenu, co korzystnie wplywa na zycie w jeziorze."
    };
    encyclopedia["Rogatek Krotkoszyjkowy"] = {
        "Rogatek Krotkoszyjkowy", "Ceratophyllum submersum",
        "Podobny do rogatka sztywnego, ale o migkszych lisciach i mniejszej tolerancji na chlod.",
        "Plytkie, cieple i eutroficzne akweny, dobrze nasłonecznione.",
        "Rozmnaza sie glownie wegetatywnie przez oderwane fragmenty pedu."
    };
    encyclopedia["Tatarak"] = {
        "Tatarak Zwyczajny", "Acorus calamus",
        "Okazala roslina szuwarowa o lancetowatych, aromatycznych lisciach, ponad 1 m wysokosci.",
        "Brzegi jezior i rzek w strefie płytkowodnej.",
        "Klacze sa uzywane w ziololecznictwie i perfumerstwie. Olejki eteryczne dzialaja antyseptycznie."
    };
    encyclopedia["Osoka Woda"] = {
        "Osoka Aloesowata", "Stratiotes aloides",
        "Roslina plywajaca — latem wynurza sie z wody, zima opada na dno i przezimuje w spoczynku. Liscie kolczaste.",
        "Plytkie, czyste jeziora o wysokiej zawartosci wapnia.",
        "Chroniona w Polsce. Skupiska sa schronieniem dla wielu bezkregowcow."
    };
    encyclopedia["Osoka Brzeg"] = {
        "Osoka Aloesowata", "Stratiotes aloides",
        "Egzemplarze brzegowe z grubiastymi lisciami zakonzonymi ostrymi kolcami, podobnymi do aloesa.",
        "Strefa plycizny przybrzeznej o czystej, bogatej w wapn wodzie.",
        "Im wiecej osoki w jeziorze, tym lepsza jakosc wody — naturalny wskaznik ekologiczny."
    };
}

// ============================================================
// togglePanel
// ============================================================
void EncyclopediaOverlay::togglePanel(const std::string& plant) {
    if (plant.empty() || lockedPlant == plant)
        lockedPlant = "";
    else
        lockedPlant = plant;
}

// ============================================================
// drawRect — 1 draw call, solid color
// ============================================================
void EncyclopediaOverlay::drawRect(float px, float py, float pw, float ph,
                                    glm::vec4 col, int sw, int sh) {
    float x0 =  (px)       / sw * 2.f - 1.f;
    float x1 =  (px + pw)  / sw * 2.f - 1.f;
    float y1 = 1.f - (py)       / sh * 2.f;
    float y0 = 1.f - (py + ph)  / sh * 2.f;

    float v[24] = {
        x0,y0, 0,0,  x1,y0, 1,0,  x1,y1, 1,1,
        x0,y0, 0,0,  x1,y1, 1,1,  x0,y1, 0,1,
    };
    glUniform4fv(uColor,  1, &col[0]);
    glUniform1i(uUseTex, 0);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

// ============================================================
// addGlyph — TYLKO CPU, zero GL calls
// ============================================================
void EncyclopediaOverlay::addGlyph(unsigned char c, float pen_x, float baseline_y,
                                    float scale, int sw, int sh) {
    if (c < 32 || c >= 128) return;
    const GlyphInfo& g = glyphs[c];
    if (g.w == 0 || g.h == 0) return;

    float gx = pen_x     + g.bx * scale;
    float gy = baseline_y + g.by * scale;
    float gw = g.w * scale;
    float gh = g.h * scale;

    float x0 =  gx       / sw * 2.f - 1.f;
    float x1 = (gx + gw) / sw * 2.f - 1.f;
    float y1 = 1.f -  gy       / sh * 2.f;
    float y0 = 1.f - (gy + gh) / sh * 2.f;

    // 6 werteksów (2 trójkąty), każdy: x, y, u, v
    float q[24] = {
        x0,y0, g.s0,g.t0,   x1,y0, g.s1,g.t0,   x1,y1, g.s1,g.t1,
        x0,y0, g.s0,g.t0,   x1,y1, g.s1,g.t1,   x0,y1, g.s0,g.t1,
    };
    textBatch.insert(textBatch.end(), q, q + 24);
}

// ============================================================
// flushText — 1 glBufferSubData + 1 glDrawArrays dla CAŁEGO tekstu
// ============================================================
void EncyclopediaOverlay::flushText(glm::vec4 col) {
    if (textBatch.empty()) return;

    glUniform4fv(uColor,  1, &col[0]);
    glUniform1i(uUseTex, 1);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fontTex);

    int nVerts = (int)(textBatch.size() / 4);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    textBatch.size() * sizeof(float), textBatch.data());
    glDrawArrays(GL_TRIANGLES, 0, nVerts);

    textBatch.clear();
}

// ============================================================
// drawText — zbiera glify do batcha, potem flushuje
// ============================================================
void EncyclopediaOverlay::drawText(const std::string& text,
                                    float x, float baseline_y,
                                    float pixelH, glm::vec4 col,
                                    int sw, int sh) {
    if (!fontTex) return;
    float sc = pixelH / ATLAS_FONT_PX;
    float cx = x;
    for (unsigned char c : text) {
        if (c < 32 || c >= 128) { cx += pixelH * 0.35f; continue; }
        addGlyph(c, cx, baseline_y, sc, sw, sh);
        cx += glyphs[c].advance * sc;
    }
    flushText(col); // jeden draw call na cały string
}

float EncyclopediaOverlay::measureText(const std::string& text, float pixelH) {
    float sc = pixelH / ATLAS_FONT_PX, w = 0.f;
    for (unsigned char c : text) {
        if (c < 32 || c >= 128) { w += pixelH * 0.35f; continue; }
        w += glyphs[c].advance * sc;
    }
    return w;
}

// ============================================================
// drawWrapped
// ============================================================
float EncyclopediaOverlay::drawWrapped(const std::string& text,
                                        float x, float baseline_y,
                                        float maxW, float pixelH,
                                        glm::vec4 col, int sw, int sh) {
    float lineH = pixelH * 1.45f;
    std::istringstream iss(text);
    std::string word, line;
    float cy = baseline_y;
    while (iss >> word) {
        std::string test = line.empty() ? word : line + " " + word;
        if (measureText(test, pixelH) > maxW && !line.empty()) {
            drawText(line, x, cy, pixelH, col, sw, sh);
            cy += lineH;
            line = word;
        } else {
            line = test;
        }
    }
    if (!line.empty()) {
        drawText(line, x, cy, pixelH, col, sw, sh);
        cy += lineH;
    }
    return cy;
}

// ============================================================
// drawPanel
// ============================================================
void EncyclopediaOverlay::drawPanel(const PlantInfo& info, float alpha, int sw, int sh) {
    const float PAD = 20.f;
    const float PW  = 480.f;
    const float TH  = 26.f;
    const float LH  = 16.f;
    const float BH  = 15.f;
    const float SH  = 13.5f;

    float asc_title = fontAscent * (TH / ATLAS_FONT_PX);
    float asc_body  = fontAscent * (BH / ATLAS_FONT_PX);
    float asc_small = fontAscent * (SH / ATLAS_FONT_PX);

    float estH = PAD * 2
        + TH + 6.f
        + LH + 10.f
        + BH * 1.45f * 5
        + SH + BH * 1.45f * 2
        + SH + BH * 1.45f * 2
        + PAD;

    float px = (float)sw - PW - 16.f;
    float py = 16.f;

    // Tła (stałe kolory — każdy to 1 draw call)
    drawRect(px+4, py+4, PW,  estH, {0.f,  0.f,  0.f,  0.30f*alpha}, sw, sh);
    drawRect(px,   py,   PW,  estH, {0.05f,0.13f,0.09f,0.92f*alpha}, sw, sh);
    drawRect(px,   py,   PW,  asc_title+PAD*1.6f, {0.07f,0.26f,0.14f,0.97f*alpha}, sw, sh);
    drawRect(px,   py,   4.f, estH, {0.22f,0.80f,0.40f,alpha},        sw, sh);
    drawRect(px+4, py,   PW-4, 2.f, {0.22f,0.80f,0.40f,0.55f*alpha},  sw, sh);

    float tx = px + PAD + 4.f;
    float by = py + PAD * 0.7f + asc_title;

    // Tytuł
    drawText(info.polishName, tx, by, TH,
             {0.90f,1.00f,0.93f,alpha}, sw, sh);
    by += TH * 0.5f + 8.f;

    // Łacina
    by += fontAscent * (LH / ATLAS_FONT_PX);
    drawText("[" + info.latinName + "]", tx, by, LH,
             {0.50f,0.86f,0.62f,alpha*0.88f}, sw, sh);
    by += LH * 0.5f + 10.f;

    // Separator
    drawRect(tx, by+2, PW-PAD*2.f, 1.f, {0.22f,0.70f,0.36f,0.40f*alpha}, sw, sh);
    by += 9.f;

    // Opis
    by += asc_body;
    by = drawWrapped(info.description, tx, by, PW-PAD*2.5f, BH,
                     {0.87f,0.96f,0.90f,alpha}, sw, sh);
    by += 6.f;

    // Siedlisko
    by += asc_small;
    drawText("Siedlisko:", tx, by, SH,
             {0.38f,0.86f,0.52f,alpha}, sw, sh);
    by += SH*0.45f + 3.f + asc_body;
    by = drawWrapped(info.habitat, tx+4.f, by, PW-PAD*3.f, BH,
                     {0.78f,0.93f,0.82f,alpha*0.90f}, sw, sh);
    by += 5.f;

    // Ciekawostka
    by += asc_small;
    drawText("Ciekawostka:", tx, by, SH,
             {0.38f,0.86f,0.52f,alpha}, sw, sh);
    by += SH*0.45f + 3.f + asc_body;
    drawWrapped(info.interesting, tx+4.f, by, PW-PAD*3.f, BH,
                {0.78f,0.93f,0.82f,alpha*0.90f}, sw, sh);
}

// ============================================================
// render
// ============================================================
void EncyclopediaOverlay::render(int sw, int sh, float dt) {
    if (!initialized) return;

    const float FADE = 5.f;
    bool vis = !lockedPlant.empty();
    panelAlpha = vis
        ? std::min(1.f, panelAlpha + dt * FADE)
        : std::max(0.f, panelAlpha - dt * FADE);

    if (panelAlpha < 0.005f) return;

    auto it = encyclopedia.find(lockedPlant);
    if (it == encyclopedia.end()) return;

    // Zapisz stan GL
    GLboolean depthTest, cullFace, blend;
    glGetBooleanv(GL_DEPTH_TEST, &depthTest);
    glGetBooleanv(GL_CULL_FACE,  &cullFace);
    glGetBooleanv(GL_BLEND,      &blend);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(hudShader);
    glBindVertexArray(quadVAO);
    glActiveTexture(GL_TEXTURE0);

    drawPanel(it->second, panelAlpha, sw, sh);

    glBindVertexArray(0);

    // Przywróć stan GL
    if (depthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (cullFace)  glEnable(GL_CULL_FACE);  else glDisable(GL_CULL_FACE);
    if (!blend)    glDisable(GL_BLEND);
}
