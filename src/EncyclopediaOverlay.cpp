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

// ===========================================================
// Destruktor
// ===========================================================
EncyclopediaOverlay::~EncyclopediaOverlay() {
    if (fontTex)  glDeleteTextures(1, &fontTex);
    if (quadVAO)  glDeleteVertexArrays(1, &quadVAO);
    if (quadVBO)  glDeleteBuffers(1, &quadVBO);
}

// ===========================================================
// Rysowanie quada pomocniczego (NDC, bez tekstury)
// Buduje kompletny quad per-call — brak stanu per-VAO
// ===========================================================
static void drawNDCQuad(unsigned int vao, unsigned int vbo,
                         float x0, float y0, float x1, float y1,
                         float u0, float v0, float u1, float v1)
{
    // Dwa trójkąty:
    //  (x0,y0,u0,v0)  (x1,y0,u1,v0)  (x1,y1,u1,v1)
    //  (x0,y0,u0,v0)  (x1,y1,u1,v1)  (x0,y1,u0,v1)
    float verts[24] = {
        x0,y0, u0,v0,  x1,y0, u1,v0,  x1,y1, u1,v1,
        x0,y0, u0,v0,  x1,y1, u1,v1,  x0,y1, u0,v1,
    };
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

// ===========================================================
// init
// ===========================================================
bool EncyclopediaOverlay::init(const std::string& fontPath) {
    // ---- Setup VAO/VBO ----
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, 24 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    buildEncyclopedia();

    // ---- Font ----
    std::vector<std::string> candidates;
    if (!fontPath.empty()) candidates.push_back(fontPath);
    for (const char* c : {
        "C:/Windows/Fonts/calibri.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/tahoma.ttf",
        "C:/Windows/Fonts/verdana.ttf"
    }) candidates.push_back(c);

    for (const auto& p : candidates)
        if (loadFont(p)) { break; }

    if (!fontTex) {
        std::cerr << "[Encyclopedia] Nie znaleziono czcionki!\n";
        return false;
    }

    // ---- Shader ----
    hudShader = createShaderProgramFromFiles(
        findAssetPath("Assets/shaders/hud.vert").c_str(),
        findAssetPath("Assets/shaders/hud.frag").c_str()
    );
    if (!hudShader) {
        std::cerr << "[Encyclopedia] Blad shadera HUD!\n";
        return false;
    }

    initialized = true;
    std::cout << "[Encyclopedia] OK. Kliknij LPM patrzac na rosline.\n";
    return true;
}

// ===========================================================
// loadFont — generuje atlas przez stb_truetype
// ===========================================================
bool EncyclopediaOverlay::loadFont(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;
    size_t sz = (size_t)f.tellg();
    f.seekg(0);
    std::vector<unsigned char> buf(sz);
    if (!f.read((char*)buf.data(), sz)) return false;

    stbtt_fontinfo info;
    if (!stbtt_InitFont(&info, buf.data(), stbtt_GetFontOffsetForIndex(buf.data(), 0))) {
        std::cerr << "[Encyclopedia] stbtt_InitFont FAIL: " << path << "\n";
        return false;
    }

    const float FONT_PX = ATLAS_FONT_PX;
    float sc = stbtt_ScaleForPixelHeight(&info, FONT_PX);

    int asc, desc, lg;
    stbtt_GetFontVMetrics(&info, &asc, &desc, &lg);
    fontAscent  = (int)std::ceil( asc * sc);
    fontDescent = (int)std::ceil(desc * sc);
    fontLineGap = (int)std::ceil(  lg * sc);

    // ---- Baking (bez pack API — proste i niezawodne) ----
    // Piekiemy glify 32..126 do atlasu 512x512 za pomocą stbtt_BakeFontBitmap
    std::vector<unsigned char> atlas(atlasW * atlasH, 0);
    stbtt_bakedchar baked[96];
    int bakeResult = stbtt_BakeFontBitmap(
        buf.data(), 0,          // font data, font index
        FONT_PX,               // pixel height
        atlas.data(), atlasW, atlasH,
        32, 96,                // first char, num chars
        baked
    );
    if (bakeResult == 0) {
        std::cerr << "[Encyclopedia] stbtt_BakeFontBitmap FAIL (za maly atlas?)\n";
        return false;
    }
    std::cout << "[Encyclopedia] BakeFontBitmap: " << bakeResult << " wierszy uzyte\n";

    // ---- Wypełnij GlyphInfo ----
    // stbtt_BakeFontBitmap generuje atlas w konwencji obrazkowej (Y↓, row0=gora)
    // OpenGL: tekstura uploaded bez flipu → row0 = dolny wiersz GL → UV.v=0 = góra obrazka
    //
    // Dlatego dla glypha (baked[i]) o granicach y0..y1 w atalsie (y0<y1, y0=gora):
    //   UV.v = (atlasH - y) / atlasH        (im wyżej w atalsie → niższe v)
    //   UV.v dla y0 (gora glypha) = (atlasH-y0)/atlasH  → duze v → gora GL tekstury
    //   UV.v dla y1 (dol glypha)  = (atlasH-y1)/atlasH  → male v → dol GL tekstury
    //
    // Mais... to właśnie powoduje że glyph jest ODWRÓCONY względem quada.
    // Rozwiązanie: flipujemy atlas pionowo przed uploadem do GL,
    // dzięki temu UV.v rośnie ku górze obrazka i glify nie są odwrócone.
    
    // Flip atlasu pionowo
    for (int row = 0; row < atlasH / 2; ++row) {
        unsigned char* a = atlas.data() + row * atlasW;
        unsigned char* b = atlas.data() + (atlasH - 1 - row) * atlasW;
        std::swap_ranges(a, a + atlasW, b);
    }

    for (int i = 0; i < 96; i++) {
        int c = i + 32;
        if (c >= 128) break;
        auto& g = glyphs[c];
        auto& bk = baked[i];

        // Po flipie: y0_flipped = atlasH - bk.y1, y1_flipped = atlasH - bk.y0
        // UV.v = y_flipped / atlasH  (standardowe GL UV po flipie)
        int y0f = atlasH - (int)bk.y1;
        int y1f = atlasH - (int)bk.y0;

        g.s0 = bk.x0 / (float)atlasW;
        g.s1 = bk.x1 / (float)atlasW;
        g.t0 = y0f   / (float)atlasH;   // dół glypha w GL UV
        g.t1 = y1f   / (float)atlasH;   // góra glypha w GL UV

        g.bx      = bk.xoff;
        g.by      = bk.yoff;   // ujemna = glyph ponad baseline
        g.advance = bk.xadvance;
        g.w       = (int)(bk.x1 - bk.x0);
        g.h       = (int)(bk.y1 - bk.y0);
    }

    // ---- Upload tekstury ----
    if (fontTex) glDeleteTextures(1, &fontTex);
    glGenTextures(1, &fontTex);
    glBindTexture(GL_TEXTURE_2D, fontTex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, atlasW, atlasH,
                 0, GL_RED, GL_UNSIGNED_BYTE, atlas.data());

    // WAŻNE: na niektórych driverach GL_RED wymaga swizzle żeby .r działało poprawnie
    GLint swizzle[] = { GL_RED, GL_RED, GL_RED, GL_RED };
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    std::cout << "[Encyclopedia] Font zaladowany: " << path << "\n";
    return true;
}

// ===========================================================
// Dane encyklopedii
// ===========================================================
void EncyclopediaOverlay::buildEncyclopedia() {
    encyclopedia["Moczarka Delikatna"] = {
        "Moczarka Delikatna", "Elodea nuttallii",
        "Drobna roslina zanurzona w wodzie. Pochodzi z Ameryki Polnocnej i zostala zawleczona do Europy jako roslina akwariowa w XIX wieku.",
        "Strefa litoralna jezior i kanaly o czystej lub umiarkowanie zeutrofizowanej wodzie.",
        "Tworzy geste lany stanowiace doskonale kryjowki dla narybku i drobnych bezkregowcow."
    };
    encyclopedia["Mech Zdrojek"] = {
        "Mech Zdrojek", "Fontinalis antipyretica",
        "Jeden z najwiekszych mchow wodnych w Europie. Rosnie przytwierdzona do kamieni, tworzac dlugie, ciemne wstegi.",
        "Czyste, zimne wody plynacie i jeziorowe, dobrze natlenione.",
        "Dawniej uzywana do uszczelniania szpar. Bardzo wrazliwa na zanieczyszczenia."
    };
    encyclopedia["Moczarka Kanadyjska"] = {
        "Moczarka Kanadyjska", "Elodea canadensis",
        "Popularny chwast wodny o trojlistnych okoldkach. Jedna z najszerzej rozprzestrzenionych inwazyjnych roslin w Europie.",
        "Stojace i wolno plynacie wody nasłonecznione lub polcieniste.",
        "Przybyła do Europy w XIX w. i bardzo szybko opanowała rzeki i jeziora kontynentu."
    };
    encyclopedia["Rogatek Sztywny"] = {
        "Rogatek Sztywny", "Ceratophyllum demersum",
        "Roslina zanurzona, bez korzeni — dryfuje lub zakotwicza sie miedzy roslinami. Liscie rozwidlone jak poroze.",
        "Wody stojace i wolno plynacie, bogate w zwiazki biogenne.",
        "W sloneczny dzien produkuje duze ilosci tlenu, co korzystnie wplywa na ryby."
    };
    encyclopedia["Rogatek Krotkoszyjkowy"] = {
        "Rogatek Krotkoszyjkowy", "Ceratophyllum submersum",
        "Podobny do rogatka sztywnego, ale o migkszych lisciach i mniejszej tolerancji na chlod.",
        "Plytkie, cieple i eutroficzne akweny, dobrze nasłonecznione.",
        "Rozmnaza sie glownie wegetatywnie przez fragmenty pedu."
    };
    encyclopedia["Tatarak"] = {
        "Tatarak Zwyczajny", "Acorus calamus",
        "Okazala roslina szuwarowa o lancetowatych, aromatycznych lisciach do ponad 1 m wysokosci.",
        "Brzegi jezior i rzek w strefie płytkowodnej.",
        "Klacze sa uzywane w ziololecznictwie. Olejki eteryczne dzialaja antyseptycznie."
    };
    encyclopedia["Osoka Woda"] = {
        "Osoka Aloesowata", "Stratiotes aloides",
        "Roslina plywajaca — latem wynurza sie z wody, zima opada na dno i przezimuje w spoczynku.",
        "Plytkie, czyste jeziora o wysokiej zawartosci wapnia.",
        "Chroniona w Polsce. Skupiska sa schronieniem dla wielu bezkregowcow."
    };
    encyclopedia["Osoka Brzeg"] = {
        "Osoka Aloesowata", "Stratiotes aloides",
        "Egzemplarze brzegowe. Liscie grubiaste z ostrymi kolcami — podobne do aloesa.",
        "Strefa plycizny przybrzeznej o czystej, bogatej w wapn wodzie.",
        "Im wiecej osoki w jeziorze, tym lepsza jakosc wody — naturalny wskaznik ekologiczny."
    };
}

// ===========================================================
// togglePanel
// ===========================================================
void EncyclopediaOverlay::togglePanel(const std::string& plant) {
    if (plant.empty() || lockedPlant == plant)
        lockedPlant = "";
    else
        lockedPlant = plant;
}

// ===========================================================
// drawRect (piksele → NDC przez screenW/H)
// ===========================================================
void EncyclopediaOverlay::drawRect(float px, float py, float pw, float ph,
                                    glm::vec4 col, int sw, int sh) {
    // NDC: x=[-1,1] y=[-1,1], ekranowe Y rośnie w dół → NDC Y maleje
    float x0 = (px       / sw) * 2.f - 1.f;
    float x1 = ((px+pw)  / sw) * 2.f - 1.f;
    float y1 =  1.f - (py       / sh) * 2.f;   // góra prostokąta w NDC
    float y0 =  1.f - ((py+ph)  / sh) * 2.f;   // dół prostokąta w NDC

    // solid color quad: UV nieważne, useTexture=0
    glUniform4fv(glGetUniformLocation(hudShader, "color"), 1, &col[0]);
    glUniform1i(glGetUniformLocation(hudShader, "useTexture"), 0);
    drawNDCQuad(quadVAO, quadVBO, x0, y0, x1, y1, 0,0,1,1);
}

// ===========================================================
// drawGlyph — jeden znak przy (pen_x, baseline_y) [piksele, Y↓]
// ===========================================================
void EncyclopediaOverlay::drawGlyph(unsigned char c, float pen_x, float baseline_y,
                                     float scale, glm::vec4 col, int sw, int sh) {
    if (c < 32 || c >= 128) return;
    const GlyphInfo& g = glyphs[c];
    if (g.w == 0 || g.h == 0) return;

    // Pozycja w pikselach (Y rośnie w dół)
    float gx = pen_x + g.bx * scale;          // lewy brzeg
    float gy = baseline_y + g.by * scale;     // górny brzeg (by ujemne = nad baseline)
    float gw = g.w * scale;
    float gh = g.h * scale;

    // NDC
    float x0 = (gx)       / sw * 2.f - 1.f;
    float x1 = (gx + gw)  / sw * 2.f - 1.f;
    float y1 =  1.f - (gy)       / sh * 2.f;   // góra glypha w NDC (duże Y)
    float y0 =  1.f - (gy + gh)  / sh * 2.f;   // dół glypha w NDC

    glUniform4fv(glGetUniformLocation(hudShader, "color"), 1, &col[0]);
    glUniform1i(glGetUniformLocation(hudShader, "useTexture"), 1);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fontTex);
    glUniform1i(glGetUniformLocation(hudShader, "hudTex"), 0);

    // UV: t0=dół glypha w GL, t1=góra glypha w GL (atlas flipowany pionowo)
    drawNDCQuad(quadVAO, quadVBO, x0, y0, x1, y1, g.s0, g.t0, g.s1, g.t1);
}

// ===========================================================
// drawText  [baseline_y = linia bazowa, piksele Y↓]
// ===========================================================
void EncyclopediaOverlay::drawText(const std::string& text,
                                    float x, float baseline_y,
                                    float pixelH, glm::vec4 col,
                                    int sw, int sh) {
    if (!fontTex) return;
    float sc = pixelH / ATLAS_FONT_PX;
    float cx = x;
    for (unsigned char c : text) {
        if (c < 32 || c >= 128) { cx += pixelH * 0.35f; continue; }
        drawGlyph(c, cx, baseline_y, sc, col, sw, sh);
        cx += glyphs[c].advance * sc;
    }
}

float EncyclopediaOverlay::measureText(const std::string& text, float pixelH) {
    float sc = pixelH / ATLAS_FONT_PX;
    float w = 0.f;
    for (unsigned char c : text) {
        if (c < 32 || c >= 128) { w += pixelH * 0.35f; continue; }
        w += glyphs[c].advance * sc;
    }
    return w;
}

// ===========================================================
// drawWrapped — zawijanie słów, zwraca nowy baseline_y
// ===========================================================
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

// ===========================================================
// drawPanel
// ===========================================================
void EncyclopediaOverlay::drawPanel(const PlantInfo& info, float alpha, int sw, int sh) {
    const float PAD  = 14.f;
    const float PW   = 360.f;
    const float TH   = 20.f;   // rozmiar tytułu
    const float LH   = 13.f;   // łacina
    const float BH   = 12.f;   // body
    const float SH   = 10.5f;  // małe etykiety
    float asc_title  = fontAscent * (TH / ATLAS_FONT_PX);
    float asc_body   = fontAscent * (BH / ATLAS_FONT_PX);
    float asc_small  = fontAscent * (SH / ATLAS_FONT_PX);

    // Szacuj wysokość (bezpieczny margines)
    float estH = PAD * 2
        + TH + 6.f
        + LH + 10.f
        + BH * 1.45f * 5      // opis do 5 linii
        + SH + BH * 1.45f * 2 // siedlisko
        + SH + BH * 1.45f * 2 // ciekawostka
        + PAD;

    float px = (float)sw - PW - 16.f;
    float py = 16.f;

    // ---- Tła ----
    drawRect(px + 4, py + 4, PW, estH,
             {0.f, 0.f, 0.f, 0.30f * alpha}, sw, sh);
    drawRect(px, py, PW, estH,
             {0.05f, 0.13f, 0.09f, 0.92f * alpha}, sw, sh);
    drawRect(px, py, PW, asc_title + PAD * 1.6f,
             {0.07f, 0.26f, 0.14f, 0.97f * alpha}, sw, sh);
    drawRect(px, py, 4.f, estH,
             {0.22f, 0.80f, 0.40f, alpha}, sw, sh);
    drawRect(px + 4, py, PW - 4, 2.f,
             {0.22f, 0.80f, 0.40f, 0.55f * alpha}, sw, sh);

    float tx = px + PAD + 4.f;

    // ---- Tytuł ----
    float by = py + PAD * 0.7f + asc_title;
    drawText(info.polishName, tx, by, TH,
             {0.90f, 1.00f, 0.93f, alpha}, sw, sh);
    by += TH * 0.5f + 8.f;

    // ---- Łacina ----
    float asc_lat = fontAscent * (LH / ATLAS_FONT_PX);
    by += asc_lat;
    std::string lat = "[" + info.latinName + "]";
    drawText(lat, tx, by, LH, {0.50f, 0.86f, 0.62f, alpha * 0.88f}, sw, sh);
    by += LH * 0.5f + 10.f;

    // ---- Separator ----
    drawRect(tx, by + 2, PW - PAD * 2.f, 1.f,
             {0.22f, 0.70f, 0.36f, 0.40f * alpha}, sw, sh);
    by += 9.f;

    // ---- Opis ----
    by += asc_body;
    by = drawWrapped(info.description, tx, by, PW - PAD * 2.5f, BH,
                     {0.87f, 0.96f, 0.90f, alpha}, sw, sh);
    by += 6.f;

    // ---- Siedlisko ----
    float asc_s = fontAscent * (SH / ATLAS_FONT_PX);
    by += asc_s;
    drawText("Siedlisko:", tx, by, SH,
             {0.38f, 0.86f, 0.52f, alpha}, sw, sh);
    by += SH * 0.45f + 3.f + asc_body;
    by = drawWrapped(info.habitat, tx + 4.f, by, PW - PAD * 3.f, BH,
                     {0.78f, 0.93f, 0.82f, alpha * 0.90f}, sw, sh);
    by += 5.f;

    // ---- Ciekawostka ----
    by += asc_s;
    drawText("Ciekawostka:", tx, by, SH,
             {0.38f, 0.86f, 0.52f, alpha}, sw, sh);
    by += SH * 0.45f + 3.f + asc_body;
    drawWrapped(info.interesting, tx + 4.f, by, PW - PAD * 3.f, BH,
                {0.78f, 0.93f, 0.82f, alpha * 0.90f}, sw, sh);
}

// ===========================================================
// render
// ===========================================================
void EncyclopediaOverlay::render(int sw, int sh, float dt) {
    if (!initialized) return;

    const float FADE = 5.f;
    bool vis = !lockedPlant.empty();
    panelAlpha = vis
        ? std::min(1.f, panelAlpha + dt * FADE)
        : std::max(0.f, panelAlpha - dt * FADE);

    if (panelAlpha < 0.005f) return;

    // Wyświetlamy lockedPlant lub ostatnio pokazany (podczas fade-out)
    auto it = encyclopedia.find(lockedPlant);
    if (it == encyclopedia.end()) {
        // lockedPlant właśnie wyczyszczony (fade-out) — szukamy dowolnego wpisu
        // żeby zanik był płynny
        if (panelAlpha < 0.005f) return;
        // Brak danych — przerywamy
        return;
    }

    // Zachowaj stan GL
    GLboolean depthTest, cullFace, blend;
    glGetBooleanv(GL_DEPTH_TEST, &depthTest);
    glGetBooleanv(GL_CULL_FACE,  &cullFace);
    glGetBooleanv(GL_BLEND,      &blend);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(hudShader);

    drawPanel(it->second, panelAlpha, sw, sh);

    // Przywróć stan GL
    if (depthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (cullFace)  glEnable(GL_CULL_FACE);  else glDisable(GL_CULL_FACE);
    if (!blend)    glDisable(GL_BLEND);
}
