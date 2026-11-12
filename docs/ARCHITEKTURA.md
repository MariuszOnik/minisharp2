# Architektura nowego MiniSharp

Cel: MiniSharp jest **podstawą niezależną od gier**. Rdzeń nie wie nic o żadnej grze ani projekcie, który na nim
powstaje. Gry (ToEE, Fallout…) i większe dodatki (3D, Iso) są modułami, które dołączają się do rdzenia, a nie
odwrotnie. Platformy: Nintendo 3DS i przeglądarka (ten sam C jako WebAssembly); build PC służy tylko testom.

## Warstwy

Każda warstwa korzysta tylko z niższych. Niższa nie zna wyższej.

```
5. aplikacje      menu, edytor kodu, edytor grafu, Help             (app/)
4. moduły         3d, iso, …  (opcjonalne, każdy w swoim folderze)   (modules/<nazwa>/)
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

## Układ folderów (propozycja)

```
minisharp-core/
  lang/         ms.h (publiczne API), parser, kompilator (kilka plików), vm, gc, korutyny, disasm,
                service.h/.c – usługa języka dla edytorów (kolory, podpowiedzi, błędy; z codeindex)
  lib/          funkcje wbudowane niezależne od platformy
  platform/
    platform.h  czego warstwy wyżej chcą od hosta
    3ds/        libctru + citro2d/citro3d: ekran, obrazki, dźwięk, dotyk, rysowanie do obrazka
    web/        host w wasm: platform.h przez funkcje JS (canvas, dźwięk, dotyk)
    pc/         host testów: konsola, programowy bufor ekranów (--frames, --press, --ppm)
  modules/      3d/, iso/ (opcjonalne)
  app/          menu, editor, graph, help, keyboard, ui, terminal (jeśli zostaje) – wersja 3DS w C
  web/          strona: edytor HTML/JS, widok grafu SVG, budowanie wasm (Zig), publikacja
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
int        ms_call_main / ms_call_update / ms_call_draw(MsVM* vm);   // 0 = błąd (ms_vm_error)
void       ms_register(const NativeDef* table, int n);               // warstwy i moduły dokładają funkcje
```

Szczegóły (nazwy, co jest w `ms.h`, a co wewnętrzne) ustalamy przy przenoszeniu języka – testy stare mają przejść
bez zmian w kodzie MiniSharp (programach `.cs`).

## Jak gry korzystają z MiniSharp

Repo gry (np. `toee-models`) zawiera swój moduł (`modules/toee/`: `Actor`, `Item`, formaty, dźwięki) i swoje
biblioteki `.cs` (`Toee.Inventory`…). Build gry = rdzeń + wybrane moduły + moduł gry → osobny `.3dsx`
(np. `MiniSharp-ToEE.3dsx`). Rdzeń buduje się sam, bez żadnej gry.
