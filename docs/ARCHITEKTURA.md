# Architektura nowego MiniSharp

Cel: MiniSharp jest **podstawą niezależną od gier**. Rdzeń nie wie nic o żadnej grze ani projekcie, który na nim
powstaje. Gry (ToEE, Fallout…) i większe dodatki (3D, Iso) są modułami, które dołączają się do rdzenia, a nie
odwrotnie. Platformy: Nintendo 3DS, przeglądarka (ten sam C jako WebAssembly) i Android (wersja web w WebView,
bez osobnego kodu C); build PC służy tylko testom.

## Warstwy

Każda warstwa korzysta tylko z niższych. Niższa nie zna wyższej.

```
5. aplikacje      menu, edytor kodu, edytor grafu, terminal, Help   (app/ – 3DS, web/ – przeglądarka)
4. moduły         3d, iso, …  (opcjonalne, każdy w swoim folderze)   (modules/<nazwa>/)
   graf           model grafu + generator (graf → AST → kod), bez UI (graph/) – obok modułów, korzysta tylko z języka
3. platforma      Gfx, Input, Audio, Time, App, File – 3DS, web, PC-test (platform/3ds, web, pc)
2. biblioteka     Console, Math, Mathf, Random, string, char, List…  (lib/)
1. język          lekser, parser, kompilator, VM, GC, korutyny       (lang/)
```

Zasady:
- **Język (1)** nie dołącza żadnego nagłówka platformy ani grafiki; kompiluje się i testuje sam.
- **Funkcje wbudowane** każda warstwa i moduł rejestruje **swoją tabelą** (`ms_register(tabela)`), zamiast jednej
  tabeli w `natives.c`. Kompilator widzi to, co zarejestrowano przed kompilacją.
- **Moduł** = folder z kodem C, tabelą funkcji, opisem API (`api.cs` – Help i podpowiedzi) i testami. Włącza się go
  przy budowaniu (lista modułów w pliku konfiguracyjnym builda). Moduły gier żyją w repozytoriach gier.
- **Aplikacje (5)** nie zawierają logiki języka; korzystają z publicznego API (`ms.h`): kompiluj, uruchom klatkę,
  zapytaj indeks kodu, pobierz błąd z linią.
- **Logika raz, w C.** Przeglądarka ma własny interfejs (HTML/JS/SVG), ale podpowiedzi, indeks kodu, model grafu
  i generator kodu wywołuje z C (wasm) – nie pisze ich drugi raz w JS.
- Żadnych ścieżek ani nazw gier w rdzeniu (`toee/`, `Toee`, `Actor`…). Zasoby gier czyta moduł gry.

## Edytor kodu a język

Edytor jest **zwykłym edytorem tekstu** (bufor, kursor, przewijanie, zaznaczanie, klawiatura, pliki) i nic nie wie
o MiniSharpie. O język pyta przez wąską **usługę języka** (`lang/service.h`), którą dostarcza rdzeń:
kolory linii, podpowiedzi w miejscu kursora, sygnatura wywołania, typ pod kursorem, błędy z liniami, uruchom.
Dziś te punkty są rozsiane w `editor.c` (słowa kluczowe, `ci_*`, `ms_complete`, `project_*`, `help_*`).
Zysk: ten sam edytor otwiera też pliki bez języka (`uses.txt`, dane), a przeglądarka woła tę samą usługę przez wasm.
Kolejność prac się nie zmienia: usługa stoi na rdzeniu, więc najpierw język i biblioteka (etapy 1–2), edytor w etapie 4.

## Układ folderów

```
MiniSharp2/
  lang/         ms.h (publiczne API), vm.h (wnętrze: bajtkod, Program, VM), parser, kompilator (kilka plików), vm, gc, korutyny, disasm, printer (AST → kod),
                service.h/.c – usługa języka dla edytorów (kolory, podpowiedzi, błędy; z codeindex)
  lib/          funkcje wbudowane niezależne od platformy; Console = API jak w C# + siatka znaków (kursor, kolory,
                przewijanie); platforma tylko rysuje siatkę i podaje wpisaną linię
  platform/
    platform.h  czego warstwy wyżej chcą od hosta
    3ds/        libctru + citro2d/citro3d: ekran, obrazki, dźwięk, dotyk, rysowanie do obrazka
    web/        host w wasm: platform.h przez funkcje JS (canvas, dźwięk, dotyk)
    pc/         host testów: konsola, programowy bufor ekranów (--frames, --press, --ppm)
  modules/      3d/, iso/ (opcjonalne)
  graph/        json.c, fgraph.c – model grafu, katalog węzłów, generator (graf → AST → printer); bez UI, testy na PC,
                ten sam kod w 3DS i w wasm
  app/          aplikacja natywna w C (dziś 3DS, później np. Switch): menu, editor, bolt (edytor grafu), terminal (repl – klient
                Console, nie część języka), help,
                keyboard; rysuje i czyta wejście tylko przez app/ui.h (bez libctru) – nowa konsola = nowy platform/<konsola>
  web/          strona: edytor HTML/JS, widok grafu SVG, budowanie wasm (Zig), publikacja
  android/      projekt Gradle: WebView z wersją web (prepare-assets.sh kopiuje web/dist)
  .github/      workflow: testy PC, .3dsx, .apk, wydania 3ds-latest / android-latest
  sdk/          Help.txt, api/*.cs (opis API wg warstw i modułów, dla podpowiedzi i VS Code)
  tools/        gen_help.py, gen_samples.py, make_atlas.py, ms.ps1, build/test skrypty
  examples/     przykładowe programy (tylko ogólne)
  tests/        testy języka, biblioteki, projektów, aplikacji (graf), modułów (w ich folderach)
  docs/         ten opis, przegląd, plan, formaty
  build/        wyniki budowania (nie w gicie)
```

## Publiczne API rdzenia (szkic, do ustalenia w etapie 1)

```c
MsProgram* ms_compile(const MsSource* files, int n, MsError* err);   // projekt = kilka plików
MsVM*      ms_vm_new(MsProgram* p);
int        ms_entry(p, MS_ENTRY_MAIN) / ms_hook(p, "Update");        // indeks funkcji dla vm_call; 0 = błąd (ms_vm_error)
int        ms_register(const NativeDef* table, int n);               // warstwy i moduły dokładają funkcje (jest: lang/register.c)
int        ms_register_hook("Update");                               // pętla gry jest warstwy wyżej: rdzeń zna tylko Main
int        ms_register_source(const char* cs);                       // klasy C# dokładane przez warstwę (np. Node grafu)
```

Rdzeń nie wie o pętli gry: `Update`/`Draw` rejestruje host albo aplikacja (`ms_register_hook`), on też decyduje, czy program
bez `Main` i bez `Update` jest błędem. Kompilator dla każdej zarejestrowanej nazwy robi jedną funkcję wołającą `static void X()`
wszystkich klas (kolejność `[Order(n)]`). Tak samo klasy potrzebne warstwie (np. `abstract class Node` dla grafu) dokłada
ona sama przez `ms_register_source` – program może mieć własną klasę o tej nazwie.

Szczegóły (nazwy, co jest w `ms.h`, a co wewnętrzne) ustalamy przy przenoszeniu języka – testy stare mają przejść
bez zmian w kodzie MiniSharp (programach `.cs`).

## Jak gry korzystają z MiniSharp

Repo gry (np. `toee-models`) zawiera swój moduł (`modules/toee/`: `Actor`, `Item`, formaty, dźwięki) i swoje
biblioteki `.cs` (`Toee.Inventory`…). Build gry = rdzeń + wybrane moduły + moduł gry → osobny `.3dsx`
(np. `MiniSharp-ToEE.3dsx`). Rdzeń buduje się sam, bez żadnej gry.
