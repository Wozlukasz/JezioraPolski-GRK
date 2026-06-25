# Projekt GRK: Podwodny Świat (Jezioro Strzeszynek)

**Motyw projektu:** Interaktywny świat podwodny (Grafika Komputerowa).  
Projekt to napisana w języku C++ z użyciem OpenGL i GLSL zaawansowana aplikacja graficzna 3D. Aplikacja renderuje w czasie rzeczywistym mroczny, gęsty las wodorostów rosnący na dnie jeziora Strzeszynek wraz z ławicami pływających ryb.

## Skład grupy
*Tutaj wpiszcie imiona i nazwiska członków grupy*
- Julian Szamotuła
- Kajetan Szamotuła
- Łukasz Woźniak

---

## 🛠 Zaimplementowane Metody Obowiązkowe

1. **Normal mapping** – zastosowany na materiałach terenu, roślin oraz modelach ryb (przestrzeń styczna TBN jest przeliczana w vertex shaderze).
2. **PBR lighting (Metallic/Roughness)** – kompleksowy model oświetlenia w oparciu o Cook-Torrance BRDF uwzględniający światło otoczenia (ambient), rozproszone (diffuse), lustrzane (specular GGX) oraz podpierzchniowe rozpraszanie światła (Subsurface Scattering na liściach). 
3. **Quaternion camera control** – całkowicie zmodernizowana kamera, w której obrót odbywa się za pomocą matematyki kwaternionów, gwarantując płynny ruch i absolutny brak zjawiska "Gimbal Lock".
4. **Shadow mapping** – dynamiczne generowanie cieni (Depth Map) z wykorzystaniem techniki PCF (Percentage-Closer Filtering 3x3) w celu wygładzenia krawędzi oraz Shadow Bias by uniknąć artefaktów (shadow acne). 
5. **Parallel Transport Frames (PTF)** – skomplikowana animacja ryb wzdłuż gładkich krzywych Catmull-Rom. Algorytm PTF dba o to, by wektory kierunkowe na krzywej (styczna, normalna, binormalna) zachowywały płynne przejścia, dzięki czemu ryby gładko skręcają.
6. **Underwater skybox/cubemap** – dynamicznie generowana mapa sześcienna (Cubemap) symulująca powierzchnię wody oraz mgłę z perspektywy podwodnej. Służy również jako tło (skybox) maskujące horyzont.

---

## 🚀 Metody Dodatkowe (Wybór grupy)

Zgodnie z wymaganiami technicznymi, zaimplementowano poniższe techniki dodatkowe:

### Metoda A: Masowe renderowanie powtarzalnych obiektów z poziomami szczegółowości
- Autorski system `PlantManager` za pomocą tzw. **Hardware Instancing** potrafi wyrenderować do 400 000 roślin. Obszar podzielono na chunki 10x10.
- System posiada dynamiczny **Level of Detail (LOD)** ze strefą płynnego przenikania (Crossfade/Dithering LOD). Blisko kamery modele to bogata geometria 3D, z kolei modele na dalszym planie stają się "płaskimi" billboardami 2D, redukując uderzenie w wydajność karty graficznej.

### Metoda B: Wczytywanie zewnętrznych modeli z materiałami
- System obsługujący ładowanie skomplikowanych modeli (Wavefront OBJ) wraz z kompletem załączonych materiałów i tekstur proceduralnych. Technika pozwala m.in. na zróżnicowanie podwodnego stada ryb (wczytywane Płocie i Ukleje).

---

## 🎮 Interakcje i Sterowanie (Klawiszologia)

Aplikacja zawiera aż 3 unikalne formy interakcji z oprogramowanym światem.

### Ruch i Kamera (Quaternions)
- `W` / `S` / `A` / `D` – Podstawowy ruch (przód, tył, na boki)
- `Spacja` / `Lewy Shift` – Ruch w osi Y (wynurzenie / zanurzenie)
- `Lewy CTRL` – Pływanie sprintem (x3 prędkość)
- `Myszka` – Sterowanie kierunkiem patrzenia (Kwaterniony)
- `C` – Uwolnienie kursora myszki / Powrót do gry

### Interakcje ze sceną
- `[F]` **Latarka** – Aktywowanie światła punktowego (Flashlight/Spotlight) doczepionego do maski nurka. Skutkuje to dynamiczną zmianą warunków oświetleniowych oraz wymuszeniem przeliczenia modeli zgaszonego PBR dookoła obiektywu.
- `[B]` **Bąbelki** – Aktywowanie sprzętowego systemu cząsteczkowego (Particle System). Z okolic gracza w stronę tafli jeziora zaczną unosić się bąble powietrza generowane za pomocą shaderów. 
- `[E]` **Karmienie ryb (Wabik)** – Genialna interakcja modyfikująca logikę wrogów/stworzeń w locie. Kliknięcie klawisza sprawia, że sztuczna inteligencja ławic ryb modyfikuje w locie wektory krzywych Catmull-Rom. Zaczynają one szybko płynąć w Twoją stronę. Kolejne kliknięcie cofa tę czynność, pozwalając im spokojnie dryfować w swojej nowej strefie.

---

## 💿 Uruchomienie projektu
1. Sklonuj repozytorium GitHub na dysk twardy.
2. Zbuduj aplikację (C++) w środowisku obsługującym CMake (np. CLion / Visual Studio).
3. Projekt wymaga bibliotek systemowych do renderowania okna (dostarczonych jako statyczne w repozytorium bądź zależnych w systemie paczek).
4. Wszystkie asety graficzne znajdują się w folderze `Assets`. Upewnij się, że "Working Directory" ustawione jest na korzeń (root) repozytorium.
