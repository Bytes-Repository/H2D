# H2D

Biblioteka do pisania gier 2D w **H#**, na pakiet menedżera **bytes**.

Od wersji **0.2.0** H2D ma dwa silniki:

- **Silnik graficzny** (`h2d_gfx` i moduły pokrewne) — prawdziwe okno
  systemowe, renderowanie GPU (SDL2 + SDL_RenderCopyEx), tekstury
  PNG/JPG/BMP/GIF, wejście w czasie rzeczywistym, prawdziwy dźwięk.
  API celowo blisko [macroquad](https://macroquad.rs)'a: `init_window`,
  `clear_background`, `draw_rectangle`, `draw_texture`, `next_frame`,
  `is_key_down`, `get_frame_time`... To jest teraz **domyślny silnik H2D**
  i odpowiedź na "H2D nie ma prawdziwej grafiki" — już ma.
- **Silnik tekstowy** (`h2d_canvas` i moduły pokrewne) — oryginalny,
  w pełni działający silnik gier na siatce znaków w terminalu (ANSI
  truecolor). Zostaje, bez zmian, dla gier turowych, do których pasuje
  lepiej niż okno pikselowe (roguelike, sokobany, karcianki, gry
  planszowe).

Oba silniki współistnieją w jednej bibliotece, ale **nie mieszaj ich w
jednej grze** — różne przestrzenie współrzędnych (piksele vs komórki
siatki), różne pętle główne (real-time vs turowa), różne backendy
(okno SDL2 vs terminal). Wybierz jeden per gra.

Zobacz `docs/LIMITATIONS.md` po **dokładny, szczery status weryfikacji**
każdej części — co zostało realnie skompilowane i uruchomione w tym
repozytorium, a co jest napisane poprawnie względem źródeł H#, ale
jeszcze nie przepuszczone przez prawdziwy kompilator `h#`.

## Szybki start — silnik graficzny

```bash
# 1. zbuduj natywny backend (SDL2) — patrz native/README.md po pełne wymagania
sudo apt-get install libsdl2-dev libsdl2-image-dev libsdl2-mixer-dev libsdl2-gfx-dev
./native/build.sh

# 2. skompiluj i uruchom demo (Pong, dwuosobowy, W/S vs strzałki)
h# compile src/pong_main.h# -o pong -L build -lh2d_native
./pong
```

Minimalny przykład własnej gry (kwadrat sterowany strzałkami):

```h#
mod h2d_gfx
mod h2d_color
mod h2d_keys
mod h2d_input2d

fn main() is
    h2d_gfx::init_window(640, 480, "Moja gra")
    h2d_gfx::set_target_fps(60)

    let mut x: f64 = 300.0
    let mut y: f64 = 200.0
    let speed: f64 = 220.0

    while !h2d_gfx::window_should_close() is
        let dt: f64 = h2d_gfx::get_frame_time()
        let axis = h2d_input2d::wasd_axis()
        x += axis.0 * speed * dt
        y += axis.1 * speed * dt

        h2d_gfx::clear_background(h2d_color::dark_blue())
        h2d_gfx::draw_rectangle(x, y, 40.0, 40.0, h2d_color::gold())
        h2d_gfx::draw_text("WASD / strzalki, aby sie poruszac", 10.0, 10.0, 16, h2d_color::white())
        h2d_gfx::next_frame()
    end

    h2d_gfx::close_window()
end
```

## Moduły — silnik graficzny

| Moduł | Do czego |
|---|---|
| `h2d_gfx` | Okno, pętla klatek (`next_frame`), rysowanie prymitywów (prostokąt/koło/linia/trójkąt/tekst) |
| `h2d_color` | `Color { r, g, b, a }` + gotowa paleta nazwana |
| `h2d_texture` | `Texture2D`: wczytywanie (PNG/JPG/BMP/GIF przez SDL_image), rysowanie z obrotem/skalą/flip/tint, wycinki (sprite sheet) |
| `h2d_input2d` | Klawiatura i mysz w czasie rzeczywistym (`is_key_down`/`is_key_pressed`, pozycja/przyciski myszy) |
| `h2d_keys` | Stałe kodów klawiszy/przycisków myszy |
| `h2d_audio2` | Prawdziwy dźwięk (SDL2_mixer): efekty (`Sound`) i muzyka w tle (`Music`) |
| `h2d_camera2d` | Kamera pikselowa (target/offset/zoom), `to_screen`/`to_world`, `follow_clamped` |
| `h2d_entity2d` | ECS-lite dla przestrzeni pikselowej: `Entity2D` (pozycja/prędkość/obrót/tekstura/tint), `World2D` |
| `h2d_anim2d` | Animacja klatkowa ze spritesheet'a (wycinki tekstury po czasie rzeczywistym) |
| `h2d_collision2d` | AABB/koło w przestrzeni zmiennoprzecinkowej, najbliższy punkt, dystans |
| `h2d_app2d` | Wygodne opakowanie okna + `World2D` + `Camera2D` |

Pełny przykład grywalnej gry (**Pong, dwuosobowy**) jest w
`src/pong_main.h#`.

### Rysowanie tekstury z obrotem i skalą

```h#
mod h2d_texture
mod h2d_color

let player: h2d_texture::Texture2D = h2d_texture::load_texture("assets/player.png")
;; ... w pętli gry:
h2d_texture::draw_texture_ex(
    player, x, y, 64.0, 64.0,          ;; pozycja + rozmiar docelowy
    rotation_deg, 32.0, 32.0,          ;; obrot wokol srodka sprite'a
    false, false,                       ;; flip poziomy/pionowy
    h2d_color::white()                  ;; tint (bialy = bez zmian koloru)
)
```

### Animacja ze spritesheet'a

```h#
mod h2d_anim2d
mod h2d_texture

let sheet: h2d_texture::Texture2D = h2d_texture::load_texture("assets/walk.png")
let mut walk: h2d_anim2d::SpriteSheetAnim = h2d_anim2d::new_sheet_anim(sheet, 32.0, 32.0, 6, 6, 90, true)
;; w petli gry:
let dt_ms: int = __builtin_conv_float_to_int(h2d_gfx::get_frame_time() * 1000.0)
walk = h2d_anim2d::update(walk, dt_ms)
h2d_anim2d::draw(walk, x, y, 32.0, 32.0, h2d_color::white())
```

### Kamera przewijana

```h#
mod h2d_camera2d
mod h2d_vec2

let mut cam: h2d_camera2d::Camera2D = h2d_camera2d::new_camera2d_centered(h2d_gfx::screen_width(), h2d_gfx::screen_height())
cam = h2d_camera2d::follow_clamped(cam, player_pos, world_w, world_h, h2d_gfx::screen_width() + 0.0, h2d_gfx::screen_height() + 0.0)
let screen_pos: h2d_vec2::Vec2 = h2d_camera2d::to_screen(cam, player_pos)
h2d_gfx::draw_rectangle(screen_pos.x, screen_pos.y, 32.0, 32.0, h2d_color::red())
```

### Dźwięk

```h#
mod h2d_audio2

h2d_audio2::audio_init()
let jump: h2d_audio2::Sound = h2d_audio2::load_sound("assets/jump.wav")
let theme: h2d_audio2::Music = h2d_audio2::load_music("assets/theme.ogg")
h2d_audio2::play_music(theme, true)
;; przy skoku gracza:
h2d_audio2::play_sound(jump)
```

## Szybki start — silnik tekstowy (starszy, dalej wspierany)

```bash
h# preview src/sokoban_main.h#
```

uruchom z katalogu głównego repozytorium.

Sterowanie: `w a s d` (albo `up down left right`, albo `h j k l`),
`r` restart poziomu, `q` wyjście, samo Enter = czekaj w miejscu.

## Moduły — silnik tekstowy

| Moduł | Do czego |
|---|---|
| `h2d_canvas` | Bufor ekranu (siatka znaków), rysowanie tekstu/prostokątów/ramek/sprite'ów, wydajny redraw (tylko zmienione komórki) |
| `h2d_ansi` | Kolory 24-bit (truecolor), pogrubienie, podkreślenie, gotowa paleta |
| `h2d_sprite` | ASCII-art jako sprite'y (wielowierszowe stringi + kolor przezroczysty) |
| `h2d_anim` | Animacje klatkowe sprite'ów sterowane realnym czasem (ms) |
| `h2d_camera` | Przewijana kamera/viewport dla map większych niż ekran (siatka) |
| `h2d_entity` | Lekki ECS: `Entity` + `World`, spawn/despawn, wyszukiwanie po tagu/pozycji (siatka) |
| `h2d_tilemap` | Statyczna geometria poziomu jako tablica stringów |
| `h2d_collision` | AABB, punkt-w-prostokącie, odległość Manhattan, sąsiedzi kratowe (siatka) |
| `h2d_pathfind` | BFS po tilemapie (najkrótsza ścieżka w liczbie ruchów) |
| `h2d_input` | Wejście turowe (wpisz ruch, Enter) z normalizacją wasd/strzałek |
| `h2d_menu` | Proste menu tekstowe (góra/dół/Enter) pod tę samą pętlę wejścia |
| `h2d_audio` | Uczciwa atrapa dźwięku dla tego silnika — patrz `h2d_audio2` po prawdziwy dźwięk w silniku graficznym |
| `h2d_app` | Wygodne opakowanie `Canvas + World + Clock + running` |

Pełny przykład grywalnej gry (**Sokoban, 3 poziomy**) jest w
`src/sokoban_main.h#` + `src/sokoban_levels.h#`.

## Wspólne moduły (oba silniki)

| Moduł | Do czego |
|---|---|
| `h2d_vec2` | Podstawowa matematyka wektorowa (f64) |
| `h2d_timer` | Zegar (`now_ms`/`sleep_ms`), limiter FPS |
| `h2d_random` | Losowość oparta o prawdziwy natywny CSPRNG (kości, tasowanie, wybór) |

## Wzorzec API

**Funkcje H2D biorą wartość i zwracają nową/zmutowaną wersję**
(`canvas = h2d_canvas::set_cell(canvas, ...)`,
`world = h2d_entity2d::integrate_all(world, dt)`), bo tak w H# wygląda
mutacja pól struktury poprzez zmienną (`assign_lhs` write-back) — żadna
funkcja H2D nie modyfikuje niczego "w tle" bez przypisania wyniku z
powrotem do zmiennej. Dotyczy to obu silników jednakowo.

## Znane ograniczenia

Zobacz `docs/LIMITATIONS.md` — sekcja per silnik, z dokładnym rozbiciem
"co zweryfikowano jak" i co jest projektowym ograniczeniem na stałe
(np. FFI H# nie przenosi structów przez wartość, więc natywne API jest
płaskie pod spodem) w odróżnieniu od "jeszcze nie sprawdzone" (kod H#
silnika graficznego nie przeszedł jeszcze przez realny `h# compile` w
żadnym środowisku, bo to repo go nie ma — `native/h2d_native.c` po
stronie C natomiast tak, i to jest opisane dokładnie tam).

## Licencja

MIT — patrz `LICENSE`.
