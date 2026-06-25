# Jeziora Polski — Interaktywna scena podwodna w OpenGL

Aplikacja graficzna 3D w C++/OpenGL/GLSL, przedstawiająca podwodny ekosystem polskiego jeziora Strzeszynek.
Scena zawiera teren z realistycznym dnem, roślinnością wodną, rybami i efektami podwodnymi.

## Autorzy (w kolejności alfabetycznej):
- Julian Szamotuła
- Kajetan Szamotuła
- Łukasz Woźniak

---

## Zaimplementowane metody

### Metody obowiązkowe
| Metoda | Opis implementacji |
|---|---|
| **Normal Mapping** | Mapy normalne na terenie (mud, soil, grass) i rybach (płoć, ukleja) z poprawną przestrzenią styczną TBN |
| **PBR Lighting** | Pełny Cook-Torrance BRDF: GGX NDF, Smith Geometry, Schlick Fresnel. Parametry: albedo, metallic, roughness z tekstur. Widoczne na terenie i rybach |
| **Quaternion Camera Control** | Kamera sterowana kwaternionami — obroty yaw/pitch bez gimbal lock, płynny ruch |
| **Shadow Mapping** | Mapa cieni z jednego światła kierunkowego, PCF 3×3, bias adaptacyjny, border clamping |
| **Parallel Transport Frames** | Ramki transportu równoległego wyznaczające orientację ryb na ścieżkach Catmull-Rom bez nagłych obrotów (double reflection method) |
| **Underwater Skybox/Cubemap** | Proceduralny skybox renderowany do 6-ściannej cubemapy (`GL_TEXTURE_CUBE_MAP`). Pod wodą: efekt Snella, scatter, gradient głębinowy. Nad wodą: niebo ze słońcem |

### Metoda A — Masowe renderowanie z LOD
- **Instancing**: tysiące roślin renderowanych przez `glDrawArraysInstanced` z macierzami instancji
- **LOD**: dwa poziomy — szczegółowe modele 3D w bliskim zasięgu, flat billboardy (krzyżowe) w dalszym zasięgu
- **Frustum culling**: chunk-based z 6-plane test
- **Chunking**: rośliny pogrupowane w chunki 10×10m dla efektywnego culling

### Metoda B — Wczytywanie zewnętrznych modeli z materiałami
- Modele OBJ ryb: **płoć** (~1.7MB, ~20k trójkątów) i **ukleja** (~314KB)
- Pełne PBR materiały: diffuse, normal map, roughness map
- 7 gatunków roślin z modelami OBJ i wariantami
- Model terenu i wody z OBJ z teksturami

---

## Interakcje

| Klawisz | Akcja |
|---|---|
| `WASD` | Ruch kamery (przód/tył/lewo/prawo) |
| `Mysz` | Obrót kamery (kwaternionowy) |
| `C` | Toggle przechwytywania myszy |
| **`F`** | **Toggle latarka podwodna** — spot light zamocowany do kamery, oświetla scenę ciepłym żółtym światłem |
| **`B`** | **Toggle bąbelki powietrza** — emituje pęcherzyki powietrza z pozycji kamery, unoszą się do góry z efektem dryftu |
| **`E`** | **Toggle karmienie ryb** — przyciąga ryby w kierunku kamery, ryby przyspieszają i zmieniają ścieżkę |
| `ESC` | Zamknij aplikację |

---

## Efekty wizualne
- Kaustyki podwodne (animowane plamy światła na dnie)
- God rays (promienie światła przechodzące przez wodę)
- Mgła eksponencjalna (realistyczne zanikanie widoczności pod wodą)
- Absorpcja barw pod wodą (ciepły zielony ton)
- Falowanie powierzchni wody (wielowarstwowe fale sinusoidalne)
- Fresnel na powierzchni wody
- Subsurface scattering na roślinach
- Bąbelki z iridescencją i efektem Fresnela
- Snell's cone na podwodnym skyboxie

---

## Budowanie

### Wymagania
- C++17
- CMake ≥ 3.20
- OpenGL 3.3
- GLFW 3 (`brew install glfw` na macOS)
- GLM (header-only, w include/)
- stb_image (header-only, w include/)

### Kompilacja
```bash
mkdir -p cmake-build-release
cd cmake-build-release
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

### Uruchomienie
```bash
cd ..  # wróć do katalogu głównego projektu (wymóg dla ścieżek do Assets)
./cmake-build-release/JezioraPolski-GRK
```

---

## Struktura projektu
```
├── main.cpp                    # Główna pętla renderowania
├── include/
│   ├── Camera.h                # Kamera kwaternionowa
│   ├── Model.h                 # Wczytywanie OBJ z tangent/bitangent
│   ├── ParallelTransport.h     # PTF + Catmull-Rom spline
│   ├── FishManager.h           # Zarządzanie rybami
│   ├── BubbleSystem.h          # System cząsteczek bąbelków
│   ├── CubemapGenerator.h      # Generator cubemapy
│   ├── PlantManager.h          # Instancyjne renderowanie roślin
│   └── Shader.h / Texture.h / Utils.h
├── src/                        # Implementacje .cpp
├── Assets/
│   ├── shaders/                # Shadery GLSL (terrain, water, fish, bubble, skybox, shadow)
│   ├── models/                 # Modele OBJ (teren, woda, ryby, rośliny)
│   └── materials/              # Tekstury PBR (diffuse, normal, roughness)
└── CMakeLists.txt
```
