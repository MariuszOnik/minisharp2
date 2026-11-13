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
6. Platformy: 3DS i przeglądarka (web). PC tylko do testów. Logika (podpowiedzi, graf) tylko w C – web woła ją
   przez wasm. Pliki `.cs` bez BOM.

## Gdzie co leży

| Co | Gdzie |
|---|---|
| Nowe repo | `E:\minsharpProject\minisharp-core` (git lokalny, czysta historia) |
| Stare repo (archiwum) | `D:\Programowanie\FalloutDS\minisharp`, gałąź `toee-3d` (`fa01cec`) |
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
- [ ] Pozostałe decyzje: terminal/REPL na 3DS (zostaje?), `printer.c` (graf B go nie potrzebuje – formatowanie kodu
      w edytorze albo wyrzucić), **baza przenoszenia**: commit `931ad08` (gałąź `jezyk-v4`, stan sprzed Fallouta i ToEE)
      zamiast `toee-3d` + lista późniejszych ogólnych zmian do przeniesienia osobno (przestrzenie nazw, `using`
      dołączające biblioteki, `File.WriteLines`, `Input.Text`, `Audio.PlayMusic`, rysowanie do obrazka, przewijanie menu).
- [ ] Przejrzeć architekturę z użytkownikiem i ją zatwierdzić (układ folderów, nazwy).

### Etap 1 – język (lang/) + testy na PC
- [ ] Szkielet repo: foldery, `Makefile` dla testów PC (sam ustawia `TMP`), skrypt `tools/test.ps1`, `.gitignore`.
- [ ] Przenieść `ast.h`, `parser.c`, `vm.c`, `disasm.c` i rozdzielić `ms.h` na publiczne API i wnętrze.
- [ ] Przenieść `compiler.c` podzielony na kilka plików, bez zmiany zachowania.
- [ ] Rejestracja funkcji wbudowanych tabelami (`ms_register`) zamiast jednej tabeli.
- [ ] Przenieść testy języka (`tests/*.cs`, `tests/errors`, `expected`) – wszystkie mają przejść.

### Etap 2 – biblioteka i projekty
- [ ] `lib/`: Console, Math, Mathf, Random, string, char, int/float, List, Dictionary (część rdzenia z `natives.c`).
- [ ] `project.c` (projekt = folder, `uses.txt`, `using X;`), `repl.c` jeśli terminal zostaje, `codeindex.c`.
- [ ] Testy projektów i indeksu kodu.

### Etap 3 – platforma 3DS i host testów PC
- [ ] `platform/platform.h`, `platform/pc` (konsola, bufor ekranów, `--frames/--press/--ppm`).
- [ ] `platform/3ds`: ekran, obrazki, dźwięk, dotyk, rysowanie do obrazka, teren; `Gfx`, `Input`, `Audio`, `Time`, `App`, `File`.
- [ ] Menu na 3DS (oddzielone od pętli gry), uruchamianie programów i projektów, log na kartę. Pierwszy build na konsolę.

### Etap 4 – edytor kodu
- [ ] Usługa języka (`lang/service.h`): kolory, podpowiedzi, sygnatury, błędy – na bazie `codeindex.c`; testy na PC.
- [ ] `editor.c` jako edytor tekstu bez wiedzy o języku (pyta usługę), klawiatura, UI, Help z `sdk/` (bez wzmianek o grach), przykłady.

### Etap 5 – edytor grafu: Bolt jak w Unity (B)
- [ ] Przegląd Bolta z Unity jako wzorca (bez kopiowania kodu i grafik – `docs/graphs-notes.md` w starym repo): co z niego
      mamy (graf przepływu, zdarzenia, węzły z pamięcią, zdarzenia klasy bazowej), czego brak (np. graf stanów, zmienne
      grafu/obiektu, superjednostki = podgrafy, wyszukiwarka węzłów) – lista i kolejność do ustalenia z użytkownikiem.
- [ ] `json.c`, `fgraph.c` (katalog węzłów + generator kodu) z testami na PC; katalog węzłów budowany z API
      zarejestrowanych funkcji (moduły dokładają swoje węzły same, bez ręcznego katalogu).
- [ ] `bolt.c` – edytor na 3DS; potem ten sam model i generator w przeglądarce przez wasm (widok SVG), bez `graph.js`.

### Etap 5b – przeglądarka
- [ ] `platform/web` (`main_web.c`) + budowanie wasm (Zig) z listy modułów; `Gfx.NewImage` w JS.
- [ ] Strona: edytor HTML/JS, podpowiedzi i graf przez wywołania C (bez `complete.js`/`graph.js` jako drugiej logiki).
- [ ] Publikacja (GitHub Pages) – nowe repo na GitHubie albo dalej stare: decyzja użytkownika.

### Etap 6 – moduły
- [ ] API modułów (folder, tabela funkcji, `api.cs`, testy, włączanie w buildzie).
- [ ] `modules/iso`, `modules/3d` (ogólne 3D: `Gfx3D`, `Cam`, prymitywy, siatki).
- [ ] Przeniesienie części ToEE do modułu w `toee-models` (osobny build `MiniSharp-ToEE.3dsx`).

## Następny krok

Etap 0: pozostałe decyzje (baza `931ad08`?, terminal, `printer.c`) i zatwierdzenie architektury. Potem etap 1 od
szkieletu repo.

## Dziennik
- 2026-11-12: decyzja użytkownika – MiniSharp ma być podstawą niezależną od gier; nowe repo z czystą historią,
  przenoszenie z przeglądem, tylko 3DS (PC do testów). Zrobiony przegląd starego kodu i szkic architektury.
- 2026-11-12: web jednak zostaje (rdzeń w C → wasm, różni się tylko platforma); zasada: logika edytora tylko w C.
- 2026-11-13: edytor grafu B („Bolt jak w Unity”). Historia języka przejrzana: proponowana baza `931ad08` (`jezyk-v4`).
