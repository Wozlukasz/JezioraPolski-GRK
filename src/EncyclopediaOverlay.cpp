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

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

// ===========================================================
// Destruktor
// ===========================================================
EncyclopediaOverlay::~EncyclopediaOverlay() {
    if (fontTex)   glDeleteTextures(1, &fontTex);
    if (quadVAO)   glDeleteVertexArrays(1, &quadVAO);
    if (quadVBO)   glDeleteBuffers(1, &quadVBO);
}

// ===========================================================
// init
// ===========================================================
bool EncyclopediaOverlay::init(const std::string& fontPath) {
    setupQuad();
    buildEncyclopedia();

    // Szukamy czcionki systemowej
    std::vector<std::string> candidates;
    if (!fontPath.empty()) candidates.push_back(fontPath);
    for (const char* f : {
        "C:/Windows/Fonts/calibri.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/tahoma.ttf",
        "C:/Windows/Fonts/verdana.ttf" })
        candidates.push_back(f);

    bool fontOK = false;
    for (const auto& p : candidates)
        if (loadFont(p)) { fontOK = true; break; }

    if (!fontOK) {
        std::cerr << "[Encyclopedia] Brak czcionki TTF — tekst bedzie niewidoczny.\n";
        // Tworzymy teksture zastepczą 1x1
        unsigned char w = 255;
        glGenTextures(1, &fontTex);
        glBindTexture(GL_TEXTURE_2D, fontTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE, &w);
    }

    hudShader = createShaderProgramFromFiles(
        findAssetPath("Assets/shaders/hud.vert").c_str(),
        findAssetPath("Assets/shaders/hud.frag").c_str()
    );
    if (!hudShader) { std::cerr << "[Encyclopedia] Blad shadera HUD!\n"; return false; }

    initialized = true;
    std::cout << "[Encyclopedia] Inicjalizacja OK. Kliknij LPM patrzac na rosline.\n";
    return true;
}

// ===========================================================
// Konfiguracja quada jednostkowego (pozycja + UV)
// ===========================================================
void EncyclopediaOverlay::setupQuad() {
    // Pozycja w [0,1]^2,  UV w [0,1]^2
    // aPos(0,0)=dol-lewy, aPos(1,1)=gora-prawy (konwencja shaderów)
    float v[] = {
        0.f,0.f,  0.f,0.f,
        1.f,0.f,  1.f,0.f,
        1.f,1.f,  1.f,1.f,
        0.f,0.f,  0.f,0.f,
        1.f,1.f,  1.f,1.f,
        0.f,1.f,  0.f,1.f,
    };
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    // GL_DYNAMIC_DRAW — będziemy aktualizować per-glyph
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

// ===========================================================
// Ładowanie TTF przez stb_truetype
// ===========================================================
bool EncyclopediaOverlay::loadFont(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;
    size_t sz = (size_t)f.tellg();
    f.seekg(0);
    std::vector<unsigned char> buf(sz);
    if (!f.read((char*)buf.data(), sz)) return false;

    stbtt_fontinfo info;
    if (!stbtt_InitFont(&info, buf.data(), 0)) return false;

    // Metryki fontu przy rozmiarze referencyjnym
    float sc = stbtt_ScaleForPixelHeight(&info, ATLAS_FONT_PX);
    int asc, desc, lg;
    stbtt_GetFontVMetrics(&info, &asc, &desc, &lg);
    fontAscent  = (int)std::ceil(asc  * sc);
    fontDescent = (int)std::ceil(desc * sc);
    fontLineGap = (int)std::ceil(lg   * sc);

    // Atlas bitmapowy — pakowanie glifów ASCII 32..127
    std::vector<unsigned char> atlas(atlasW * atlasH, 0);
    stbtt_pack_context pc;
    stbtt_PackBegin(&pc, atlas.data(), atlasW, atlasH, 0, 2, nullptr);
    stbtt_PackSetOversampling(&pc, 2, 2);

    stbtt_packedchar pdata[96];
    stbtt_PackFontRange(&pc, buf.data(), 0, ATLAS_FONT_PX, 32, 96, pdata);
    stbtt_PackEnd(&pc);

    // -----------------------------------------------------------
    // Kluczowa kwestia UVów:
    // stb_truetype generuje atlas w konwencji obrazkowej (Y w dół,
    // wiersz 0 = góra obrazka). OpenGL przechowuje tekstury odwrotnie
    // (pierwszy element tablicy = dolny-lewy = UV.t=0).
    // Dlatego obraz jest "przekręcony" względem konwencji UV.
    //
    // Mapowanie: wiersz_obrazka → UV.t = 1 - wiersz_obrazka / atlasH
    //   glyph.t_top    = 1 - p.y0 / atlasH   (wysoka wartość t = góra tekstury GL)
    //   glyph.t_bottom = 1 - p.y1 / atlasH   (niska wartość t = dół tekstury GL)
    // -----------------------------------------------------------
    for (int i = 0; i < 96; i++) {
        int c = i + 32;
        if (c >= 128) break;
        auto& g  = glyphs[c];
        auto& p  = pdata[i];

        g.s0 =       (float)p.x0 / atlasW;   // lewa krawędź (U nie wymaga flipowania)
        g.s1 =       (float)p.x1 / atlasW;   // prawa
        g.t0 = 1.f - (float)p.y1 / atlasH;   // DOLNA krawędź glypha w UV-OpenGL (p.y1 = dół obrazka)
        g.t1 = 1.f - (float)p.y0 / atlasH;   // GÓRNA krawędź glypha w UV-OpenGL (p.y0 = góra obrazka)

        g.bx      = p.xoff;
        g.by      = p.yoff;    // ujemna wartość = glyph wystaje ponad baseline
        g.advance = p.xadvance;
        g.w       = p.x1 - p.x0;
        g.h       = p.y1 - p.y0;
    }

    // Upload do GPU jako jednokanałowa R8 (oszczędność VRAM)
    if (fontTex) glDeleteTextures(1, &fontTex);
    glGenTextures(1, &fontTex);
    glBindTexture(GL_TEXTURE_2D, fontTex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, atlasW, atlasH, 0,
                 GL_RED, GL_UNSIGNED_BYTE, atlas.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    std::cout << "[Encyclopedia] Czcionka: " << path << "\n";
    return true;
}

// ===========================================================
// Dane encyklopedii roślin (bez polskich znaków = stabilniejsze)
// ===========================================================
void EncyclopediaOverlay::buildEncyclopedia() {
    encyclopedia["Moczarka Delikatna"] = {
        "Moczarka Delikatna", "Elodea nuttallii",
        "Drobna roslina naczeniowa calkowicie zanurzona w wodzie. Pochodzi z Ameryki Polnocnej i zostala zawleczona do Europy jako roslina akwariowa w XIX wieku.",
        "Strefa litoralna jezior, rzeki i kanaly o czystej lub umiarkowanie zeutrofizowanej wodzie.",
        "Tworzy geste lany, tworzac doskonale kryjowki dla narybku i drobnych bezkregowcow wodnych."
    };
    encyclopedia["Mech Zdrojek"] = {
        "Mech Zdrojek", "Fontinalis antipyretica",
        "Jeden z najwiekszych mchow wodnych w Europie. Rosnie przytwierdzona do kamieni i zanurzonego drewna, tworzac dlugie, ciemne wstegi.",
        "Czyste, zimne wody plynacie i jeziorowe, dobrze natlenione.",
        "Dawniej uzywana do uszczelniania szpar. Bardzo wrazliwa na zanieczyszczenia organiczne."
    };
    encyclopedia["Moczarka Kanadyjska"] = {
        "Moczarka Kanadyjska", "Elodea canadensis",
        "Popularny chwast wodny o trojlistnych okoldkach. Jedna z najszerzej rozprzestrzenionych inwazyjnych roslin wodnych w Europie.",
        "Stojace i wolno plynacie wody, miejsca nasłonecznione i półcieniste.",
        "Przybyła do Europy w XIX w. i bardzo szybko opanowała rzeki i jeziora calego kontynentu."
    };
    encyclopedia["Rogatek Sztywny"] = {
        "Rogatek Sztywny", "Ceratophyllum demersum",
        "Roslina calkowicie zanurzona, pozbawiona korzeni — dryfuje lub zakotwicza miedzy roslinami. Liscie rozwidlone jak poroze jelenia.",
        "Wody stojace i wolno plynacie, bogate w zwiazki biogenne.",
        "W sloneczny dzien produkuje duze ilosci tlenu, co korzystnie wplywa na ryby w jeziorze."
    };
    encyclopedia["Rogatek Krotkoszyjkowy"] = {
        "Rogatek Krotkoszyjkowy", "Ceratophyllum submersum",
        "Podobny do rogatka sztywnego, lecz o migkszych lisciach i mniejszej tolerancji na chlod. Wystepuje rzadziej od swojego krewniaka.",
        "Plytkie, cieple i eutroficzne akweny, dobrze nasłonecznione.",
        "Rozmnaza sie glownie wegetatywnie — przez fragmenty pedu odrywane przez pradami lub zwierzetami."
    };
    encyclopedia["Tatarak"] = {
        "Tatarak Zwyczajny", "Acorus calamus",
        "Okazala roslina szuwarowa o lancetowatych, aromatycznych lisciach. Moze osiagac ponad metr wysokosci ponad lustrem wody.",
        "Brzegi jezior, rzek, rowow i bagien — w strefie płytkowodnej.",
        "Klacze tataraku sa uzywane w ziololecznictwie i perfumerstwie. Olejki eteryczne dzialaja antyseptycznie."
    };
    encyclopedia["Osoka Woda"] = {
        "Osoka Aloesowata", "Stratiotes aloides",
        "Interesujaca roslina plywajaca — latem wynurza sie z wody, zima opada na dno i przezimuje w spoczynku. Liscie kolczaste.",
        "Plytkie, czyste jeziora i starorzecza o wysokiej zawartosci wapnia.",
        "Objeta ochrona scisla w Polsce. Tworzy skupiska bedace schronieniem dla wielu bezkregowcow."
    };
    encyclopedia["Osoka Brzeg"] = {
        "Osoka Aloesowata", "Stratiotes aloides",
        "Egzemplarze rosnace na plytkich skrajach brzegu. Liscie grubiaste i zakonczone ostrymi kolcami — podobne do aloesa.",
        "Strefa plycizny przybrzeznej o czystej, bogatej w wapn wodzie.",
        "Im wiecej osoki w jeziorze, tym lepsza jakosc wody — to naturalny wskaznik ekologiczny."
    };
}

// ===========================================================
// Przełączanie panelu (wywoływane po kliknięciu)
// ===========================================================
void EncyclopediaOverlay::togglePanel(const std::string& plantName) {
    if (plantName.empty()) {
        // Kliknięto w powietrze — zamknij panel
        lockedPlant = "";
    } else if (lockedPlant == plantName) {
        // Ta sama roślina — toggle (zamknij)
        lockedPlant = "";
    } else {
        // Nowa roślina — pokaż ją
        lockedPlant = plantName;
    }
}

// ===========================================================
// drawRect — wypełniony prostokąt (piksele ekranowe)
// ===========================================================
void EncyclopediaOverlay::drawRect(float x, float y, float w, float h,
                                    glm::vec4 color, int sw, int sh) {
    // Konwersja pikseli → NDC. Uwaga: ekranowe Y rośnie w dół, NDC Y w górę.
    float nx = (x / sw) * 2.f - 1.f;
    float ny = 1.f - (y / sh) * 2.f;          // NDC górna krawędź prostokąta
    float nw = (w / sw) * 2.f;
    float nh = (h / sh) * 2.f;

    // Shader oczekuje: aPos*(scale) + offset → offset = dół-lewy
    GLint locOff = glGetUniformLocation(hudShader, "offset");
    GLint locSc  = glGetUniformLocation(hudShader, "scale");
    GLint locCol = glGetUniformLocation(hudShader, "color");
    GLint locUT  = glGetUniformLocation(hudShader, "useTexture");

    glUniform2f(locOff, nx, ny - nh);
    glUniform2f(locSc,  nw, nh);
    glUniform4fv(locCol, 1, &color[0]);
    glUniform1i(locUT, 0);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

// ===========================================================
// drawGlyph — jeden znak przy baseline_y
// ===========================================================
void EncyclopediaOverlay::drawGlyph(unsigned char c, float pen_x, float baseline_y,
                                     float scale, glm::vec4 color, int sw, int sh) {
    if (c < 32 || c >= 128) return;
    const auto& g = glyphs[c];
    if (g.w == 0 || g.h == 0) return;

    // Pozycja glypha w pikselach ekranu
    float gx = pen_x + g.bx * scale;
    float gy = baseline_y + g.by * scale;  // gy = góra glypha (by jest ujemne = nad baseline)
    float gw = g.w * scale;
    float gh = g.h * scale;

    // NDC
    float nx = (gx / sw) * 2.f - 1.f;
    float ny = 1.f - (gy / sh) * 2.f;          // NDC górna krawędź glypha
    float nw = (gw / sw) * 2.f;
    float nh = (gh / sh) * 2.f;

    // Quad: aPos(0,0)=dol-lewy  aPos(1,1)=gora-prawy
    // UV: t0=dol-glypha (w GL), t1=gora-glypha (w GL)  — poprawiony flip!
    float verts[] = {
        0.f,0.f,  g.s0,g.t0,
        1.f,0.f,  g.s1,g.t0,
        1.f,1.f,  g.s1,g.t1,
        0.f,0.f,  g.s0,g.t0,
        1.f,1.f,  g.s1,g.t1,
        0.f,1.f,  g.s0,g.t1,
    };

    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

    GLint locOff = glGetUniformLocation(hudShader, "offset");
    GLint locSc  = glGetUniformLocation(hudShader, "scale");
    GLint locCol = glGetUniformLocation(hudShader, "color");
    GLint locUT  = glGetUniformLocation(hudShader, "useTexture");

    glUniform2f(locOff, nx, ny - nh);   // dół-lewy w NDC
    glUniform2f(locSc,  nw, nh);
    glUniform4fv(locCol, 1, &color[0]);
    glUniform1i(locUT, 1);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fontTex);
    glUniform1i(glGetUniformLocation(hudShader, "hudTex"), 0);

    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

// ===========================================================
// drawText — tekst przy podanym baseline_y (piksele, Y↓)
// ===========================================================
void EncyclopediaOverlay::drawText(const std::string& text, float x, float baseline_y,
                                    float pixelH, glm::vec4 color, int sw, int sh) {
    if (!fontTex) return;
    float scale = pixelH / ATLAS_FONT_PX;
    float cx = x;
    for (unsigned char c : text) {
        if (c == ' ') { cx += glyphs[' '].advance * scale; continue; }
        if (c < 32 || c >= 128) { cx += pixelH * 0.4f; continue; }
        drawGlyph(c, cx, baseline_y, scale, color, sw, sh);
        cx += glyphs[c].advance * scale;
    }
}

float EncyclopediaOverlay::measureText(const std::string& text, float pixelH) {
    float sc = pixelH / ATLAS_FONT_PX;
    float w = 0.f;
    for (unsigned char c : text) {
        if (c < 32 || c >= 128) { w += pixelH * 0.4f; continue; }
        w += glyphs[c].advance * sc;
    }
    return w;
}

// ===========================================================
// drawWrapped — tekst z zawijaniem, zwraca nowy baseline_y
// ===========================================================
float EncyclopediaOverlay::drawWrapped(const std::string& text, float x, float baseline_y,
                                        float maxW, float pixelH, glm::vec4 color,
                                        int sw, int sh) {
    float lineH = pixelH * 1.4f;
    float cy = baseline_y;
    std::istringstream iss(text);
    std::string word, line;
    while (iss >> word) {
        std::string test = line.empty() ? word : line + " " + word;
        if (measureText(test, pixelH) > maxW && !line.empty()) {
            drawText(line, x, cy, pixelH, color, sw, sh);
            cy += lineH;
            line = word;
        } else {
            line = test;
        }
    }
    if (!line.empty()) {
        drawText(line, x, cy, pixelH, color, sw, sh);
        cy += lineH;
    }
    return cy;
}

// ===========================================================
// drawPanel — rysuje panel encyklopedii
// ===========================================================
void EncyclopediaOverlay::drawPanel(const PlantInfo& info, float alpha, int sw, int sh) {
    const float PAD    = 14.f;
    const float PW     = 360.f;   // szerokość panelu
    const float TH     = 21.f;    // tytuł
    const float LH     = 13.5f;   // łacina
    const float BH     = 12.5f;   // body
    const float SH     = 11.f;    // małe etykiety
    float lineH_body   = BH * 1.4f;
    float lineH_small  = SH * 1.4f;
    float scale = BH / ATLAS_FONT_PX;

    // Szacowanie wysokości
    float estH = PAD
        + (TH + 4.f)
        + (LH + 8.f)
        + lineH_body * 4   // opis (4 linie)
        + lineH_small * 2  // siedlisko
        + lineH_small * 2  // ciekawostka
        + PAD;

    float px = (float)sw - PW - 16.f;
    float py = 16.f;

    // === Tło ===
    // Cień zewnętrzny
    drawRect(px + 4, py + 4, PW, estH,
             {0.f, 0.f, 0.f, 0.35f * alpha}, sw, sh);
    // Główne tło
    drawRect(px, py, PW, estH,
             {0.05f, 0.14f, 0.10f, 0.93f * alpha}, sw, sh);
    // Pasek tytułowy
    drawRect(px, py, PW, TH + PAD * 1.4f,
             {0.08f, 0.28f, 0.16f, 0.97f * alpha}, sw, sh);
    // Akcent lewej krawędzi
    drawRect(px, py, 4.f, estH,
             {0.25f, 0.82f, 0.42f, alpha}, sw, sh);
    // Subtelna górna linia
    drawRect(px + 4, py, PW - 4, 2.f,
             {0.25f, 0.82f, 0.42f, 0.6f * alpha}, sw, sh);

    // === Tekst ===
    // Baseline = py + PAD + fontAscent*scale_title
    float tx = px + PAD + 6.f;

    // Tytuł — baseline przy górze paska
    float asc_title = fontAscent * (TH / ATLAS_FONT_PX);
    float by = py + PAD * 0.7f + asc_title;
    drawText(info.polishName, tx, by, TH,
             {0.90f, 1.00f, 0.93f, alpha}, sw, sh);
    by += TH + 5.f;

    // Łacina
    float asc_latin = fontAscent * (LH / ATLAS_FONT_PX);
    by += asc_latin * 0.1f;
    std::string lat = "[" + info.latinName + "]";
    drawText(lat, tx, by, LH,
             {0.55f, 0.88f, 0.65f, alpha * 0.88f}, sw, sh);
    by += LH + 10.f;

    // Separator
    drawRect(tx, by - 4, PW - PAD * 2, 1.f,
             {0.25f, 0.72f, 0.38f, 0.45f * alpha}, sw, sh);

    // Opis
    by = drawWrapped(info.description, tx, by, PW - PAD * 2.5f, BH,
                     {0.87f, 0.96f, 0.90f, alpha}, sw, sh);
    by += 6.f;

    // Etykieta — Siedlisko
    drawText("Siedlisko:", tx, by, SH,
             {0.40f, 0.88f, 0.55f, alpha}, sw, sh);
    by += SH * 1.3f;
    by = drawWrapped(info.habitat, tx + 4.f, by, PW - PAD * 2.8f, BH - 1.f,
                     {0.78f, 0.92f, 0.82f, alpha * 0.9f}, sw, sh);
    by += 5.f;

    // Etykieta — Ciekawostka
    drawText("Ciekawostka:", tx, by, SH,
             {0.40f, 0.88f, 0.55f, alpha}, sw, sh);
    by += SH * 1.3f;
    drawWrapped(info.interesting, tx + 4.f, by, PW - PAD * 2.8f, BH - 1.f,
                {0.78f, 0.92f, 0.82f, alpha * 0.9f}, sw, sh);
}

// ===========================================================
// render — wywołuj co klatkę
// ===========================================================
void EncyclopediaOverlay::render(int sw, int sh, float dt) {
    if (!initialized) return;

    // Animacja fade-in / fade-out
    const float FADE = 4.5f;
    bool visible = !lockedPlant.empty();
    if (visible)
        panelAlpha = std::min(1.f, panelAlpha + dt * FADE);
    else
        panelAlpha = std::max(0.f, panelAlpha - dt * FADE);

    if (panelAlpha < 0.01f) return;

    auto it = encyclopedia.find(lockedPlant.empty() ? "" : lockedPlant);
    if (!visible) {
        // W trakcie zanikania trzymamy ostatnie dane — szukamy po czymkolwiek
        // Nie rysujemy jeśli nie ma danych (co nie powinno się zdarzyć)
        if (panelAlpha < 0.01f) return;
        // Szukamy ostatniego wpisu
        it = encyclopedia.begin(); // fallback
        if (it == encyclopedia.end()) return;
    } else {
        it = encyclopedia.find(lockedPlant);
        if (it == encyclopedia.end()) { lockedPlant = ""; return; }
    }

    // Zachowaj stan GL
    GLboolean depth, cull, blend;
    glGetBooleanv(GL_DEPTH_TEST, &depth);
    glGetBooleanv(GL_CULL_FACE,  &cull);
    glGetBooleanv(GL_BLEND,      &blend);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(hudShader);
    drawPanel(it->second, panelAlpha, sw, sh);

    // Przywróć stan GL
    if (depth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (cull)  glEnable(GL_CULL_FACE);  else glDisable(GL_CULL_FACE);
    if (!blend) glDisable(GL_BLEND);
}
