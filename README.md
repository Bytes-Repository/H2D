# H2D

Biblioteka do pisania poważnych gier 2D w **H#**, na pakiet menedżera
**bytes**.

H2D nie udaje, że H# ma dziś prawdziwą grafikę okienkową — bo nie ma
(zobacz `docs/LIMITATIONS.md` po dokładne uzasadnienie, zweryfikowane w
kodzie źródłowym interpretera i kompilatora H#, nie tylko w README).
Zamiast tego H2D to **w pełni działający silnik gier na siatce znaków,
renderowany w terminalu przez prawdziwy ANSI truecolor** — z encjami,
mapami kafelkowymi, kolizjami, zegarem i wejściem turowym. To
wystarczająca podstawa pod prawdziwe gry: roguelike, sokobany,
strategie turowe, puzzle, karcianki, gry planszowe.

## Co jest w środku

| Moduł | Do czego |
|---|---|
| `h2d_canvas` | Bufor ekranu (siatka znaków), rysowanie tekstu/prostokątów/ramek/sprite'ów, wydajny redraw (tylko zmienione komórki) |
| `h2d_ansi` | Kolory 24-bit (truecolor), pogrubienie, podkreślenie, gotowa paleta |
| `h2d_sprite` | ASCII-art jako sprite'y (wielowierszowe stringi + kolor przezroczysty) |
| `h2d_anim` | Animacje klatkowe sprite'ów sterowane realnym czasem (ms) |
| `h2d_camera` | Przewijana kamera/viewport dla map większych niż ekran |
| `h2d_entity` | Lekki ECS: `Entity` + `World`, spawn/despawn, wyszukiwanie po tagu/pozycji |
| `h2d_tilemap` | Statyczna geometria poziomu jako tablica stringów |
| `h2d_collision` | AABB, punkt-w-prostokącie, odległość Manhattan, sąsiedzi kratowe |
| `h2d_pathfind` | BFS po tilemapie (najkrótsza ścieżka w liczbie ruchów) |
| `h2d_input` | Wejście turowe (wpisz ruch, Enter) z normalizacją wasd/strzałek |
| `h2d_menu` | Proste menu tekstowe (góra/dół/Enter) pod tę samą pętlę wejścia |
| `h2d_random` | Losowość oparta o prawdziwy natywny CSPRNG (kości, tasowanie, wybór) |
| `h2d_audio` | Uczciwa atrapa dźwięku — stabilny interfejs pod przyszły realny backend |
| `h2d_timer` | Zegar (`now_ms`/`sleep_ms`), limiter FPS dla animacji niezależnych od tur |
| `h2d_vec2` | Podstawowa matematyka wektorowa (f64) |
| `h2d_app` | Wygodne opakowanie `Canvas + World + Clock + running` |

Pełny przykład grywalnej gry (**Sokoban, 3 poziomy**) jest w
`src/sokoban_main.h#` + `src/sokoban_levels.h#`.

## Szybki start

```bash
h# preview src/sokoban_main.h#
```

uruchom z katalogu głównego repozytorium (patrz `docs/LIMITATIONS.md`,
sekcja o `mod`, po wyjaśnienie dlaczego to ważne).

Sterowanie: `w a s d` (albo `up down left right`, albo `h j k l`),
`r` restart poziomu, `q` wyjście, samo Enter = czekaj w miejscu.

## Użycie w swojej grze

```h#
mod h2d_lib   ;; albo pojedynczo: mod h2d_canvas / mod h2d_entity / ...

fn main() is
    let mut canvas: h2d_canvas::Canvas = h2d_canvas::new_canvas(40, 20)
    let mut world:  h2d_entity::World  = h2d_entity::new_world()

    let spawned = h2d_entity::spawn(world, 5, 5, "@", h2d_ansi::cyan(), "player")
    world = spawned.0
    let player_id: int = spawned.1

    let mut running: bool = true
    while running is
        canvas = h2d_canvas::clear(canvas, " ", "")
        let idx: int = h2d_entity::find_index_by_id(world, player_id)
        let p: h2d_entity::Entity = world.entities[idx]
        canvas = h2d_canvas::set_cell(canvas, p.x, p.y, p.glyph, p.style)
        canvas = h2d_canvas::present(canvas)

        let action: string = h2d_input::read_action("Ruch: ")
        if action == "quit" is
            running = false
        else is
            let delta = h2d_input::action_delta(action)
            world = h2d_entity::move_index(world, idx, delta.0, delta.1)
        end
    end
    h2d_canvas::shutdown(canvas)
end
```

Wzorzec ogólny: **funkcje H2D biorą wartość i zwracają nową/zmutowaną
wersję** (`canvas = h2d_canvas::set_cell(canvas, ...)`), bo tak w H#
wygląda mutacja pól struktury poprzez zmienną (`assign_lhs`
write-back) — żadna funkcja H2D nie modyfikuje niczego "w tle" bez
przypisania wyniku z powrotem do zmiennej.

## Rysowanie sprite'ów

```h#
let rows: [string] = h2d_sprite::pad_rows([
    " /\\ ",
    "/  \\",
    "|db|",
    "\\__/",
], " ")
let goblin: h2d_sprite::Sprite = h2d_sprite::sprite_from_lines(rows, " ")
canvas = h2d_canvas::blit_sprite(canvas, x, y, goblin.rows, h2d_ansi::green(), goblin.transparent_char)
```

## Mapy kafelkowe

```h#
let mut rows: [string] = []
rows.push("#####")
rows.push("#   #")
rows.push("# @ #")
rows.push("#####")
let map: h2d_tilemap::Tilemap = h2d_tilemap::from_rows(rows)
let start = h2d_tilemap::find_char(map, "@")   ;; (x, y)
```

## Nowe moduły — szybkie przykłady

**Losowość** (realny CSPRNG, nie stub):
```h#
mod h2d_random
let roll: int = h2d_random::dice(6)
let crit: bool = h2d_random::chance(15)   ;; 15% szans
```

**Pathfinding (BFS)**:
```h#
mod h2d_pathfind
let result = h2d_pathfind::bfs_path(map, enemy_x, enemy_y, player_x, player_y, "#")
if result.0 is
    let next_x: int = result.1[1]   ;; [0] to pozycja startowa, [1] pierwszy krok
    let next_y: int = result.2[1]
end
```

**Kamera dla dużych map**:
```h#
mod h2d_camera
let mut cam: h2d_camera::Camera = h2d_camera::new_camera(40, 20)
cam = h2d_camera::follow(cam, player_x, player_y, map.width, map.height)
canvas = h2d_canvas::set_cell(canvas, h2d_camera::world_to_screen_x(cam, player_x), h2d_camera::world_to_screen_y(cam, player_y), "@", h2d_ansi::cyan())
```

**Animacja sprite'a**:
```h#
mod h2d_anim
let frames: [h2d_sprite::Sprite] = [h2d_sprite::sprite_glyph("|"), h2d_sprite::sprite_glyph("/"), h2d_sprite::sprite_glyph("-"), h2d_sprite::sprite_glyph("\\")]
let mut spin: h2d_anim::Animation = h2d_anim::new_animation(frames, 120, true)
;; w pętli gry:
let tick = h2d_timer::tick(clock)
clock = tick.0
spin = h2d_anim::update(spin, tick.1)
let sprite: h2d_sprite::Sprite = h2d_anim::current_sprite(spin)
```

**Menu**:
```h#
mod h2d_menu
let mut menu: h2d_menu::Menu = h2d_menu::new_menu(["Nowa gra", "Wczytaj", "Wyjście"])
canvas = h2d_menu::draw(canvas, menu, 4, 4, h2d_ansi::gray(), h2d_ansi::style2(h2d_ansi::white(), h2d_ansi::bold()))
let action: string = h2d_input::read_action("")
let result = h2d_menu::handle_action(menu, action)
menu = result.0
if result.1 is
    ;; Enter na podświetlonej pozycji — h2d_menu::selected_label(menu)
end
```

**Dźwięk (atrapa z logiem debugowym)**:
```h#
mod h2d_audio
let audio: h2d_audio::AudioState = h2d_audio::new_audio(true)
h2d_audio::play(audio, "coin")   ;; dziś: nic nie gra, ale wypisze [h2d_audio] play: coin
```

## Znane ograniczenia (przeczytaj przed dużym projektem)

- **Brak grafiki pikselowej/okna** — tylko siatka znaków ANSI w
  terminalu. Zobacz `docs/LIMITATIONS.md`.
- **Brak wejścia w czasie rzeczywistym** — gry są turowe (Enter po
  ruchu), bo `read_key()` w H# to dziś zaślepka.
- **Brak dźwięku** — `std -> audio` w H# to dziś zaślepka; H2D go nie
  owija, bo nie ma czego owijać.
- Kod napisano i zweryfikowano względem źródeł interpretera/kompilatora
  H#, ale nie uruchomiono go przez realny `h#` w tym środowisku (brak
  pełnego środowiska budowania). Uruchom `h# check` i `h# preview`
  zanim zbudujesz na tym coś większego — i zgłoś, jeśli coś nie
  zadziała tak, jak opisano.

## Licencja

MIT — patrz `LICENSE`.
