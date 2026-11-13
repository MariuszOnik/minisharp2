# Plan: nowy MiniSharp (rdzeń niezależny od gier)

**Ten plik jest źródłem prawdy o stanie prac – czytaj go na początku każdej sesji i aktualizuj na końcu.**
Opis docelowy: [docs/ARCHITEKTURA.md](docs/ARCHITEKTURA.md). Ocena starego kodu: [docs/PRZEGLAD.md](docs/PRZEGLAD.md).

## Zasady pracy

1. Nowa sesja = nowy czat. Na start przeczytaj ten plik, weź **jeden** punkt z „Następny krok”.
2. Na końcu sesji: odhacz `[x]`, dopisz 1–3 zdania do „Dziennika”, commit.
3. Stare repo (`D:\Programowanie\FalloutDS\minisharp`) to **archiwum tylko do odczytu** – kod przenosimy stąd
   plik po pliku, z przeglądem. Nie wprowadzamy w nim zmian.
4. Kod przenosimy **razem z testami**; po każdym kroku wszystkie przeniesione testy przechodzą.
5. W rdzeniu żadnych nazw ani ścieżek gier. Gry i ich zasoby to moduły w repozytoriach gier.
6. Platformy: 3DS, przeglądarka (web) i Android (APK = wersja web w WebView). PC tylko do testów. Logika (podpowiedzi, graf) tylko w C – web woła ją
   przez wasm. Pliki `.cs` bez BOM.

## Gdzie co leży

| Co | Gdzie |
|---|---|
| Nowe repo | `E:\MiniSharp2`, GitHub: https://github.com/MariuszOnik/minisharp2 (każdy commit od razu `git push`; Actions: testy PC, `.3dsx`, `.apk`, wydania `3ds-latest`/`android-latest`) |
| Stare repo (archiwum) | `D:\Programowanie\FalloutDS\minisharp`; baza `931ad08` (gałąź `jezyk-v4`), późniejsze zmiany z `toee-3d` (`fa01cec`) |
| Projekty na MiniSharpie (wstrzymane) | `E:\minsharpProject\PLAN.md` (framework D&D na zasobach ToEE), `toee-models` |
| Konsola | FTP `192.168.1.90:5000`; testowe buildy jako `/3ds/MiniSharp-*.3dsx` (nie nadpisywać `/3ds/MiniSharp.3dsx` bez pytania) |
| Budowanie 3DS | devkitPro: `C:\devkitPro\msys2\usr\bin\bash.exe -lc "...; export DEVKITPRO=/opt/devkitpro DEVKITARM=/opt/devkitpro/devkitARM; make"` |
| Budowanie PC | gcc z `C:\msys64\mingw64\bin`; przez PowerShell z `TMP` ustawionym na katalog użytkownika |

## Etapy

### Etap 0 – przegląd i plan
- [x] Przegląd starego kodu z werdyktami: `docs/PRZEGLAD.md` (2026-11-12).
- [x] Architektura warstw i modułów: `docs/ARCHITEKTURA.md`.
- [x] **Edytor grafu: B – „Bolt jak w Unity”** (decyzja użytkownika 2026-11-13). Graf jest źródłem (JSON w pliku),
      kod C# generowany z niego. Zostają `fgraph.c`, `json.c`, `bolt.c`; graf A (`graph.c`, `graphedit.c`, `gview.c`,
      `grapheditor.c`, `graphview.c`) odpada.
- [x] **Terminal (REPL) na 3DS zostaje** (`repl.c`, `terminal.c`) – decyzja 2026-11-13.
- [x] **`printer.c` zostaje** jako jedyny sposób zamiany AST na kod: generator grafu buduje AST, `printer.c` drukuje
      kod (zawsze poprawny i sformatowany); w edytorze „Formatuj kod”. Testy wydruku (`print/*`) przechodzą razem z nim.
- [x] **Baza przenoszenia: commit `931ad08`** (gałąź `jezyk-v4`, stan sprzed Fallouta i ToEE) zamiast `toee-3d` –
      decyzja 2026-11-13. Późniejsze ogólne zmiany przenosimy osobno (lista z commitami: `docs/PRZEGLAD.md`,
      „Baza przenoszenia”). Uwagi:
      - przegląd był robiony na `toee-3d` – przy każdym pliku porównać z wersją z `931ad08`;
      - różnice `vm.c`/`compiler.c` z commita Gfx3D (`a43971f`) przejrzeć ręcznie (mogą zawierać ogólne poprawki);
      - przestrzenie nazw (`a350c5e`) przenieść już w etapie 1 razem z kompilatorem (`using`/`uses.txt` z nich korzystają).
- [x] Architektura zatwierdzona (2026-11-13): układ folderów i nazwy z `docs/ARCHITEKTURA.md`, w tym nowy `graph/`
      (model grafu + generator bez UI, wspólny dla 3DS i wasm) i `android/` (WebView z wersją web).

### Etap 1 – język (lang/) + testy na PC
- [x] Szkielet repo: foldery, `Makefile` dla testów PC (sam ustawia `TMP`), skrypt `tools/test.ps1`, `.gitignore`.
- [x] Przenieść `ast.h`, `parser.c`, `vm.c`, `disasm.c` i rozdzielić `ms.h` na publiczne API i wnętrze.
- [x] Przenieść `compiler.c` podzielony na kilka plików, bez zmiany zachowania (z przestrzeniami nazw `a350c5e`).
- [x] Rejestracja funkcji wbudowanych tabelami (`ms_register`) zamiast jednej tabeli.
- [x] Przenieść testy języka (`tests/*.cs`, `tests/errors`, `expected`) – przechodzą poza czekającymi na bibliotekę (`tests/czeka.txt`).

### Etap 2 – biblioteka i projekty
- [x] `lib/`: Console, Math, Mathf, Random, string, char, int/float (z `natives.c`); List i Dictionary są w języku (`vm.c`).
- [ ] `project.c` (projekt = folder, `uses.txt`, `using X;`), `repl.c` (terminal), `printer.c` (AST → kod), `codeindex.c`.
- [ ] Testy projektów i indeksu kodu.

### Etap 3 – platforma 3DS i host testów PC
- [ ] `platform/platform.h`, `platform/pc` (konsola, bufor ekranów, `--frames/--press/--ppm`).
- [ ] `platform/3ds`: ekran, obrazki, dźwięk, dotyk, rysowanie do obrazka, teren; `Gfx`, `Input`, `Audio`, `Time`, `App`, `File`.
- [ ] Menu na 3DS (oddzielone od pętli gry), uruchamianie programów i projektów, log na kartę. Pierwszy build na konsolę.

### Etap 4 – edytor kodu
- [ ] **Warstwa abstrakcji edytora** (prośba użytkownika 2026-11-13): edytor, terminal, menu i edytor grafu (`app/`) nie wołają
      libctru ani citro2d, tylko interfejsu `app/ui.h` (rysuj prostokąt/tekst/obrazek, przycinanie, zdarzenia: klawisze, przyciski,
      dotyk/mysz, wpisany tekst, układ ekranów: jeden lub dwa, rozdzielczość). Logika edytora (bufor tekstu, kursor, zaznaczenie,
      cofanie, karty) osobno od rysowania i testowana na PC. Implementacja `ui.h` jest w `platform/<konsola>` – przeniesienie na
      inną platformę (np. Switch: `platform/switch` na libnx) = nowy folder platformy, bez zmian w `app/`.
      Uwagi do projektu `ui.h`:
      - 3DS ma dwa ekrany i rysik, Switch jeden duży ekran, dotyk tylko w trybie przenośnym i Joy-Cony – układ ekranów jest
        **parametrem**, nie stałą „góra i dół”. Edytor sam rozkłada okna: przy jednym ekranie np. kod obok podpowiedzi zamiast
        kodu na górze i klawiatury na dole;
      - wersja web (przeglądarka i APK) zostaje osobno z interfejsem w HTML/JS i nie używa `ui.h`; ze wspólnego kodu w C bierze
        tylko logikę: usługę języka i model grafu.
- [ ] Usługa języka (`lang/service.h`): kolory, podpowiedzi, sygnatury, błędy – na bazie `codeindex.c`; testy na PC.
- [ ] `editor.c` jako edytor tekstu bez wiedzy o języku (pyta usługę), klawiatura, UI, Help z `sdk/` (bez wzmianek o grach), przykłady.
- [ ] **„Idź do definicji / pokaż API”** (prośba użytkownika 2026-11-13): kursor na nazwie klasy + przycisk otwiera tę klasę,
      żeby podejrzeć jej metody i pola. Usługa języka zwraca, *skąd* jest klasa:
      - klasa z projektu (też z innego folderu, z `uses.txt`/`using X;`) – `codeindex.c` zna plik i linię deklaracji →
        otwieramy ten plik w nowej karcie na tej linii (edytowalny);
      - klasa wbudowana (rdzeń, `lib/`, moduł: `Console`, `Math`, `List`…) nie ma pliku `.cs` → otwieramy **podgląd API tylko
        do odczytu**: opis z `sdk/api/*.cs` / `api.cs` modułu, a gdy go brak – wygenerowany z zarejestrowanych tabel
        (`ms_native_at`: nazwy, parametry, typ wyniku). Ten sam podgląd posłuży Help i wyszukiwarce węzłów w grafie;
      - przestrzeń nazw w `using` (np. `using X;`) – lista jej klas do wyboru. Działa też na nazwie metody (`Console.Write|`).

### Etap 5 – edytor grafu: Bolt jak w Unity (B)
- [ ] Przegląd Bolta z Unity jako wzorca (bez kopiowania kodu i grafik – `docs/graphs-notes.md` w starym repo): co z niego
      mamy (graf przepływu, zdarzenia, węzły z pamięcią, zdarzenia klasy bazowej), czego brak (np. graf stanów, zmienne
      grafu/obiektu, superjednostki = podgrafy, wyszukiwarka węzłów) – lista i kolejność do ustalenia z użytkownikiem.
- [ ] `json.c`, `fgraph.c` (katalog węzłów + generator: graf → AST → `printer.c` → kod) z testami na PC; katalog węzłów budowany z API
      zarejestrowanych funkcji (moduły dokładają swoje węzły same, bez ręcznego katalogu).
- [ ] `bolt.c` – edytor na 3DS; potem ten sam model i generator w przeglądarce przez wasm (widok SVG), bez `graph.js`.

### Etap 5b – przeglądarka
- [ ] `platform/web` (`main_web.c`) + budowanie wasm (Zig) z listy modułów; `Gfx.NewImage` w JS.
- [ ] Strona: edytor HTML/JS, podpowiedzi i graf przez wywołania C (bez `complete.js`/`graph.js` jako drugiej logiki).
- [ ] Publikacja (GitHub Pages) w nowym repo `minisharp2`.
- [ ] Android: folder `android/` (WebView z wersją web, `prepare-assets.sh`) – wzór w starym `minisharp-apk-files.zip`,
      ale jako zwykłe pliki w repo, nie zip. Job `build-android` w Actions już czeka.

### Etap 6 – moduły
- [ ] API modułów (folder, tabela funkcji, `api.cs`, testy, włączanie w buildzie).
- [ ] `modules/iso`, `modules/3d` (ogólne 3D: `Gfx3D`, `Cam`, prymitywy, siatki).
- [ ] Przeniesienie części ToEE do modułu w `toee-models` (osobny build `MiniSharp-ToEE.3dsx`).

## Następny krok

Etap 2: `project.c` (projekt = folder, `uses.txt`, `using X;`) + testy projektów (`project*`, `ci*` z `931ad08`, `project_ns`
z `a350c5e`); host PC przyjmuje folder. Potem w osobnych krokach `repl.c`, `printer.c`, `codeindex.c`.
Uwaga: ostrzeżenie gcc `-Walloc-size-larger-than` w `lang/compiler.c:330` – przy okazji obejrzeć.

## Dziennik
- 2026-11-12: decyzja użytkownika – MiniSharp ma być podstawą niezależną od gier; nowe repo z czystą historią,
  przenoszenie z przeglądem, tylko 3DS (PC do testów). Zrobiony przegląd starego kodu i szkic architektury.
- 2026-11-12: web jednak zostaje (rdzeń w C → wasm, różni się tylko platforma); zasada: logika edytora tylko w C.
- 2026-11-13: edytor grafu B („Bolt jak w Unity”). Historia języka przejrzana: proponowana baza `931ad08` (`jezyk-v4`).
- 2026-11-13: terminal zostaje, `printer.c` zostaje (graf → AST → kod, „Formatuj kod”). Repo przeniesione do
  `E:\MiniSharp2` (nowa nazwa folderu na prośbę użytkownika), dodany `CLAUDE.md` dla nowych sesji.
- 2026-11-13: baza przenoszenia potwierdzona: `931ad08`. Różnica do `toee-3d` w rdzeniu języka jest mała
  (compiler +158, parser +87, vm +80 linii, 4 commity); większość późniejszych zmian to gry (`gfx3d.c`, `natives.c`).
- 2026-11-13: repo na GitHubie (`minisharp2`), każdy commit wypychany. Workflow `build.yml`: testy PC, 3DS, APK
  (Android dochodzi jako platforma: web w WebView); joby pomijają części, których jeszcze nie ma.
- 2026-11-13: architektura zatwierdzona; doszedł folder `graph/` (fgraph/json poza `app/` i `lang/`). Etap 0 zamknięty.
- 2026-11-13: szkielet repo: `Makefile` (`make test` buduje `build/minisharp` z `lang/ lib/ graph/ platform/pc`, TMP w `build/tmp`),
  `tests/run_tests.sh` (testy `tests/*.cs` + `expected/`, `errors/`), `tools/test.ps1` (log w `build/test.log`).
  `platform/3ds` i `platform/web` celowo jeszcze nie istnieją – ich foldery włączają buildy w Actions.
- 2026-11-13: `lang/`: `ast.h`, `parser.c`, `vm.c`, `disasm.c` z `931ad08`. `ms.h` = publiczne API (wartości, obiekty, `NativeDef`,
  nieprzezroczyste `Program`/`VM`, akcesory `ms_entry`, `ms_vm_error`, `ms_vm_fail`…), `vm.h` = wnętrze (bajtkod, `Program`, `VM`).
  `vm.c` nie zna już platformy: reset grafiki/Iso/FPS przez hooki `ms_on_vm_new`, zegar `ms_set_clock`; `ms_natives` to na razie
  pusty wskaźnik (wypełni `ms_register`). Parser sprawdzony lokalnie: 158 plików `.cs` starego repo OK, 18 błędów = `.expected`.
  Uwaga: testy `.cs` potrzebują `main` – do kroku „testy języka” trzeba minimalnego hosta PC (Console), nie czekać na etap 3.
- 2026-11-13: `compiler.c` z `931ad08` + przestrzenie nazw (`a350c5e`, też w `parser.c`/`ast.h`), podzielony na `compiler.h`,
  `comp_types.c`, `comp_expr.c`, `comp_call.c`, `comp_lambda.c`, `comp_stmt.c`, `compiler.c` (ogólne nazwy przemianowane:
  `op`→`emit_op`, `expr`→`compile_expr`, `call`→`compile_call`…). Sprawdzone: bajtkod i komunikaty błędów 213 programów
  (testy, przykłady, 13 projektów z `a350c5e`) identyczne przed i po podziale. Odłożone do swoich funkcji wbudowanych:
  `T_INTARR` (`File`, `ff4efc5`), `T_FLTARR` (Gfx3D) i `ms_require_entry` (API osadzania `6ab15e9`). Testy przestrzeni nazw
  (`namespaces.cs`, `project_ns`, `errors/ns_*`) brać z `a350c5e`, nie z `931ad08`.
- 2026-11-13: `lang/register.c`: `ms_register`/`ms_register_consts` (+ makra `MS_REGISTER(tabela)`) dopisują tabele do jednej
  rosnącej listy (kolejność = indeksy w bajtkodzie i kolejność przeciążeń; ta sama tabela drugi raz nic nie robi; wpis bez funkcji
  odrzucany), `ms_native_count`/`ms_native_at` dla Help i katalogu węzłów grafu. Doszły testy jednostkowe w C (`tests/unit/*.c`,
  linkowane bez hosta): `test_register.c` kompiluje i uruchamia program na zarejestrowanych tabelach.
- 2026-11-13: testy języka przeniesione: 32 programy `tests/*.cs` + 111 błędów `errors/` z `931ad08`, przestrzenie nazw z `a350c5e`.
  Minimalny host `platform/pc/main_pc.c` (plik `.cs`, `--frames`, zegar 60 kl./s) i `lib/console.c` (Console; wyjście ustawia
  host przez `lib_set_output`, `lib_register_all`). Wynik: 132 przechodzi, 13 czeka (`tests/czeka.txt`: Math, string… – etap 2;
  App, Gfx – etap 3); argumenty testów w `tests/opcje.txt`.
- 2026-11-13: `lib/`: `math.c` (Math, Mathf), `random.c`, `strings.c`, `convert.c` (int/float/char), każdy swoją tabelą;
  wspólne pomocnicze w `lib/libvalue.h`. W `ms.h` doszło `ms_vm_push_root`/`ms_vm_pop_root` (zamiast `vm->stack` w `Split`).
  Poprawki przenośności: `Mathf.FloorToInt` itp. nasycają jak `(int)` w VM (zwykłe rzutowanie C daje inny wynik na x86 i ARM),
  `int.Parse` przez `strtoll` z błędem poza zakresem (`long` ma 32 bity na 3DS, 64 na Switch). Testy: 141 ok, 6 czeka (etap 3).
