# Przegląd starego MiniSharp (stan na 2026-11-12)

Źródło: `D:\Programowanie\FalloutDS\minisharp`, gałąź `toee-3d` (commit `fa01cec`), ok. 24 tys. linii własnego kodu
(bez `stb_image.h`), 59 grup testów (195 sprawdzeń na PC, 3 padają od dawna). Gałęzie: `main`, `jezyk-v4`,
`fallout-port`, `toee-3d`; zdalne repo na GitHubie (z publikacją wersji web na GitHub Pages).

Stare repo zostaje **archiwum**: nic w nim nie zmieniamy, kod przenosimy stąd plik po pliku.

**Werdykty:** **zostaje** (przenieść, przejrzeć, drobne poprawki) · **poprawić** (przenieść z wyraźną przebudową) ·
**moduł** (poza rdzeniem – osobny, opcjonalny moduł albo repo gry) · **wyrzucić** · **decyzja** (wybór użytkownika).

## Decyzje użytkownika (2026-11-12)

- Nowe repo w nowym folderze, czysta historia; stare repo jako archiwum.
- Przenoszenie z przeglądem (nie pisanie od zera); testy idą razem z kodem.
- Platformy: **3DS i przeglądarka (web)**. Build PC zostaje wyłącznie jako narzędzie do testów automatycznych (bez okna
  i bez wspierania jako platforma). Web zostaje, bo rdzeń jest w C (wasm) – różni się tylko warstwa platformy
  (decyzja zmieniona tego samego dnia: najpierw „tylko 3DS”).
- Edytor kodu i edytor grafu zostają; **który edytor grafu – decyzja po porównaniu** (sekcja niżej).

## Rdzeń języka

| Plik | Linie | Co robi | Werdykt | Uwagi |
|---|---:|---|---|---|
| `src/ms.h` | 202 | wartości, obiekty, bajtkod, Program, VM, typy funkcji wbudowanych | poprawić | jeden nagłówek na wszystko: rozdzielić publiczne API (kompiluj, uruchom, zarejestruj funkcje) od wnętrza VM i bajtkodu |
| `src/ast.h` | 144 | tokeny, AST, alokator areny | zostaje | |
| `src/parser.c` | 1378 | lekser + parser zstępujący | zostaje | przejrzeć komunikaty błędów |
| `src/compiler.c` | 3090 | sprawdzanie typów + generowanie bajtkodu | poprawić | największy plik: podzielić (typy i nazwy, wyrażenia, instrukcje, klasy/przestrzenie nazw) bez zmiany zachowania – testy pilnują |
| `src/vm.c` | 1537 | maszyna stosowa, GC mark & sweep, korutyny | zostaje | |
| `src/disasm.c` | 61 | wydruk bajtkodu | zostaje | narzędzie |
| `src/printer.c` | 445 | AST → tekst C# | decyzja | potrzebny grafowi A; poza tym może być formatowaniem kodu w edytorze |
| `src/repl.c` | 175 | linia po linii (terminal) | decyzja | razem z terminalem na 3DS |
| `src/project.c` | 238 | projekt = folder, `uses.txt`, `using X;` dołącza `../x/lib` | zostaje | mechanizm ogólny (nic o ToEE w kodzie, tylko w komentarzach) |
| `src/codeindex.c` | 608 | indeks kodu dla podpowiedzi edytora | zostaje | komentarze z przykładami `Toee.*` zamienić na ogólne |

## Biblioteka wbudowana i platforma

| Plik | Linie | Co robi | Werdykt | Uwagi |
|---|---:|---|---|---|
| `src/natives.c` | 1073 | **jedna tabela** wszystkich funkcji wbudowanych: rdzeń (`Console`, `Math`, `Mathf`, `string`, `char`, `int`…), platforma (`Gfx`, `Input`, `Audio`, `Time`, `App`, `File`) i moduły (`Actor` 47, `Gfx3D` 37, `Item` 17, `Iso` 15, `Cam` 13) | poprawić | 129 z ok. 300 wpisów to moduły gier. Podział na pliki wg warstwy + **rejestracja tabel przez moduły** (moduł dodaje swoją tabelę, rdzeń nic o nim nie wie) |
| `src/platform.h` | 83 | czego rdzeń chce od hosta (ekran, obrazki, dźwięk, tekst) | zostaje | przejrzeć: część to API modułu 3D |
| `src/media.c` + `external/stb_image.h` | 130 | dekodowanie PNG/JPG/BMP, WAV | zostaje | |
| `src/gfx3d.c` + `.h` | 2294 | postacie ToEE (siatki, szkielety, animacje, ubrania, bronie), dźwięki gry, tabela przedmiotów, kamera, prymitywy 3D | moduł | rozdzielić: **ogólne 3D** (`Gfx3D`, `Cam`, prymitywy, rysowanie siatek) i **moduł ToEE** (`Actor`, `Item`, formaty `.msh/.anm/.evt`, dźwięki, `items.txt`) w repo `toee-models` |
| `src/iso.c` + `.h` | 395 | sortowanie izometryczne (`Iso`) | moduł | ogólne i przydatne, ale opcjonalne |
| `src/json.c` + `.h` | 359 | drzewo JSON | decyzja | potrzebne tylko grafowi B |

## Aplikacja na 3DS (`source3ds/`)

| Plik | Linie | Co robi | Werdykt | Uwagi |
|---|---:|---|---|---|
| `main_3ds.c` | 392 | menu plików, kompilacja, pętla gry, autorun, log | poprawić | menu i uruchamianie gry w jednym pliku; rozdzielić. Przewijanie menu przy trzymaniu (2026-11-12) zostaje |
| `platform_3ds.c/.h` | 304 | warstwa nad libctru + citro2d | zostaje | |
| `media_3ds.c` | 349 | obrazki (tekstury), dźwięk (ndsp), rysowanie do obrazka | zostaje | `Gfx.NewImage/BeginImage/EndImage` (2026-11-12) zostaje |
| `gfx3d_3ds.c` | 189 | rysowanie 3D na GPU (citro3d) | moduł | razem z ogólnym 3D |
| `editor.c/.h` | 941 | edytor kodu: kolory, numery linii, podpowiedzi, błędy | zostaje | przejrzeć; prawdopodobnie podzielić (widok, edycja, podpowiedzi) |
| `keyboard.c/.h` | 190 | klawiatura dotykowa | zostaje | |
| `ui.c/.h` | 169 | wspólne elementy UI | zostaje | |
| `help.c/.h` + `help_data.h` | 539 | Help w edytorze (generowany z `sdk/`) | zostaje | treść bez wzmianek o grach; moduły dokładają swoje strony |
| `samples.h` | 1191 | przykłady wbudowane w aplikację (generowane) | zostaje | generator `tools/gen_samples.py` |
| `terminal.c/.h` | 228 | REPL na konsoli | decyzja | |
| `grapheditor.c/.h`, `gview.c/.h` | 577 | graf A na konsoli | decyzja | |
| `graphview.c/.h` | 194 | ekran pomiaru rysowania grafu (prototyp) | wyrzucić | spełnił zadanie (pomiar w `docs/graph-model.md`) |
| `bolt.c/.h` | 1049 | graf B („mini Bolt”) na konsoli | decyzja | |
| `*.pica` | | shadery (teren, model 3D) | moduł / zostaje | teren (`Gfx.DrawTerrain`) zostaje, model 3D do modułu 3D |

## PC (tylko testy)

| Plik | Linie | Werdykt | Uwagi |
|---|---:|---|---|
| `pc/main_pc.c` | 205 | zostaje | uruchamianie testów, `--frames`, `--press`, `--ppm` (podgląd klatki) |
| `pc/platform_pc.c` | 374 | zostaje | programowy bufor obu ekranów – potrzebny do `--ppm` i testów rysowania |
| `pc/astcheck.c/.h` | 450 | zostaje | test zakresów AST (potrzebny edytorowi i grafowi A) |
| `pc/fgcheck.c/.h` | 182 | decyzja | porównuje graf B w C z wersją JS; zbędny, gdy generator będzie tylko w C |

## Przeglądarka (`web/`) – zostaje, z mniejszym dublowaniem

Rdzeń (parser, kompilator, VM, biblioteka) to ten sam kod C skompilowany do WebAssembly (Zig, `wasm32-wasi`).

| Plik | Linie | Co robi | Werdykt | Uwagi |
|---|---:|---|---|---|
| `main_web.c` | 197 | host: `platform.h` przez ok. 20 funkcji JS (canvas 2D, dźwięk, obrazki, dotyk) | zostaje | brakuje `Gfx.NewImage` (rysowanie do obrazka) i 3D – dopisać albo zgłaszać brak |
| `app.js` | 1401 | strona: edytor kodu (HTML), projekty, uruchamianie, ekrany, przyciski, udostępnianie linkiem | poprawić | rozdzielić: host gry (ekrany, wejście, dźwięk) / edytor / pliki i udostępnianie |
| `complete.js` | 364 | podpowiedzi w edytorze | poprawić | **dubluje `codeindex.c`** – zastąpić wywołaniami C przez wasm (jedna implementacja) |
| `graph.js`, `grapheditor.js` | 1226 | edytor grafu B w JS (model + generator kodu + SVG) | decyzja | **dubluje `fgraph.c`** (stąd padający test `fgraph`). Model i generator tylko w C, w JS sam widok |
| `index.html`, `style.css`, `przewodnik.html` | 862 | strona, wygląd, samouczek | zostaje | |
| `build.sh`, `make_examples.py`, `sjlj.s` | 51 | budowanie (Zig z pip), przykłady | zostaje | lista plików C z konfiguracji modułów zamiast ręcznej |
| publikacja (GitHub Actions → Pages) | | | decyzja | nowe repo na GitHubie czy publikacja dalej ze starego – do ustalenia przy etapie web |

## SDK, narzędzia, przykłady, testy

| Co | Werdykt | Uwagi |
|---|---|---|
| `sdk/Help.txt` | zostaje | tematy języka; usunąć wzmianki o ToEE |
| `sdk/MiniSharp.Api.cs` | poprawić | opis API dla podpowiedzi i VS Code; podzielić wg modułów (każdy moduł ma swój plik) |
| `MiniSharp.Games.csproj` | zostaje | projekt dla VS Code (IntelliSense); sprawdzić, czy nie wskazuje na gry |
| `tools/gen_help.py`, `gen_samples.py` | zostaje | czytają API wszystkich włączonych modułów |
| `tools/make_atlas.py` | zostaje | atlasy sprite'ów |
| `tools/ms.ps1` | zostaje | sprawdzanie kodu z PC |
| `tools/make_demo_assets.py` | zostaje | razem z przykładami |
| `examples/` | zostaje | ogólne: hello, bounce, snake, shapes, sprites, particles, coroutines, loch, kurs, Kosmos |
| `games/` | wyrzucić z rdzenia | Fallout, ToEE, Iso, Proc3D, Miasto… → repozytoria gier (Proc3D/IsoBench jako przykłady modułów) |
| `tests/` (język, projekty, błędy – ok. 50 grup) | zostaje | najcenniejsze, przenosić razem z kodem |
| `tests/toee`, `project_items` | moduł | test modułu ToEE |
| testy grafów (`graph_*`, `fgraph`) | decyzja | 3 z nich padają od dawna (`graph_edit`, `graph_edit_run` na MinGW; `fgraph` – nieaktualne dane z przeglądarki) |
| `minisharp-apk-files.zip` (w gicie) | wyrzucić | śmieć w repozytorium |

## Edytory grafów – porównanie (do decyzji)

| | **A: graf jako widok kodu** | **B: „mini Bolt”** |
|---|---|---|
| Źródło prawdy | plik `.cs`; graf rysuje się z AST | graf w JSON (ostatnia linia pliku `// @graph {...}`); kod generowany |
| Pliki | `graph.c` 576, `graphedit.c` 512, `gview.c` 216, `grapheditor.c` 307, `printer.c` 445 (~2050) | `fgraph.c` 1455, `bolt.c` 1024, `json.c` 325 (~2800) + wersja JS w `web/` |
| Działa dla | **każdego** programu MiniSharp (każda metoda ma graf) | tylko plików zrobionych w grafie; kod takiego pliku jest w edytorze tylko do czytania |
| Edycja | każda zmiana w grafie to łatka tekstu kodu (zakresy AST) – kod i graf zawsze zgodne | węzły z katalogu, kable, wartości; generator pisze kod od nowa |
| Wygląd | widok metody (instrukcje jako węzły) | przepływ od lewej do prawej, duże porty pod palec (jak Bolt) |
| Główna zaleta | jeden program, dwa widoki; nie ma „plików grafowych” | przyjazny dla początkujących, wygodny dotykiem |
| Słabość | graf instrukcji bywa gęsty, mniej „wizualny” | dwa światy (graf/kod), duży katalog węzłów do utrzymania; dziś dwie implementacje (C i JS) |
| Testy | `graph_kinds*`, `graph_ship`, `spans`, `graph_edit*` (2 padają na MinGW) | `fgraph` (pada – dane z przeglądarki nieaktualne) |

Oba są w C, więc każdy może działać na 3DS i w przeglądarce (przez wasm) – web nie przesądza wyboru.
Moja rekomendacja: **A jako podstawa** (kod jest jedynym źródłem, działa dla wszystkiego, jeden świat), a z B przenieść
to, co było w nim dobre dla dotyku: wygląd węzłów i portów, gesty, dodawanie węzła z katalogu jako wstawianie gotowego
fragmentu kodu. W przeglądarce ten sam model z C, widok w SVG. Decyzja należy do użytkownika.

## Znane problemy do naprawy przy przenoszeniu

0. Logika edytora w przeglądarce napisana drugi raz w JS (`complete.js`, `graph.js`) – ma pochodzić z C (wasm).

1. Funkcje wbudowane w jednej tabeli, moduły gier w rdzeniu (`natives.c`, `gfx3d.c`) – patrz architektura.
2. 3 testy padają od dawna (`graph_edit`, `graph_edit_run`, `fgraph`).
3. Wzmianki o ToEE w Help i komentarzach rdzenia (`project.c`, `codeindex.c`, `sdk/`).
4. `compiler.c` (3090 linii) i `natives.c` za duże na jeden plik.
5. Budowanie na PC wymaga ręcznego ustawienia `TMP` (gcc z MSYS2 w Git Bash nie widzi katalogu tymczasowego) –
   skrypt budowania powinien to załatwiać sam.
6. Śmieci w repo (`minisharp-apk-files.zip`), pliki wynikowe w katalogu głównym (`.3dsx`, `.elf` – w `.gitignore`,
   ale zaśmiecają folder) – buildy do `build/`.
