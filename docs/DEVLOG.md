# Dev log MiniSharp2

Co sesję 1–3 zdania: co zrobione, co postanowione, na co uważać. Najnowsze na dole. Stan prac i kolejne kroki: [PLAN.md](../PLAN.md).

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
- 2026-11-13: poprawione ostrzeżenie gcc w `compiler.c` (rozmiar `calloc` z `int`). Dziennik przeniesiony z PLAN.md tutaj.
  Ruszyła dokumentacja języka `sdk/docs/` (format w `sdk/docs/README.md`, rozdział 1 „Pierwszy program”); przykłady
  z dokumentacji sprawdza `make test` (`doc/*`).
- 2026-11-13: `lang/project.c` z `toee-3d` (`uses.txt` + `using X;` → `../x/lib` lub `../lib/X`, pliki rozdzielone bajtem 0x1C,
  błędy mapowane na „Plik.cs line N”); host PC przyjmuje folder. 12 testów projektów w `tests/projects/` (162 ok z przykładami dokumentacji, 6 czeka);
  `project_ns` przepisany bez ToEE. Dokumentacja: rozdział 2 „Zmienne i typy”. W planie etapu 4 zakładka „Opcje” edytora.
- 2026-11-13: rdzeń bez pętli gry: kompilator zna tylko `Main`; `Update`/`Draw` rejestruje host (`ms_register_hook`, `ms_hook`),
  on też zgłasza brak punktu wejścia; dokładana na sztywno klasa `Node` zastąpiona ogólnym `ms_register_source`.
  Test `tests/unit/test_hooks.c`; 163 ok. W planie kolejność: modyfikatory dostępu → `repl.c` → `printer.c` → `codeindex.c`.
- 2026-11-13: dokumentacja: rozdział 3 „Rachunki” (6 przykładów sprawdzanych przez `make test`; 169 ok).
- 2026-11-13: modyfikatory dostępu (wersja łagodna): `protected`/`internal` mają własne tokeny, AST pamięta `access`
  (też `private set;` i `static class`); kompilator sprawdza je przy użyciu pola, właściwości, metody, delegata i konstruktora
  (`can_access`/`check_access`, przeciążenia tylko spośród dostępnych). Bez słówka – dostęp wszędzie jak dotąd. Test `access.cs`
  + 24 testy błędów; dokumentacja: rozdział 4 „Warunki: if i else”, uwaga o `public` w rozdziale 1 (202 ok).
- 2026-11-13: `lang/repl.c` (terminal) z `931ad08`, bez nagłówka platformy; nowe wejście w kilku liniach (`repl_incomplete`),
  host PC `--repl`, test `repl` (bez `Gfx`). Dokumentacja: rozdział 5 „Pętle”. Wyszły braki: przecinek w `for`, `char` jako bajt
  (polskie litery w `foreach`). W planie etapu 3 wygląd terminala (fosfor, scanlines, klawiatura rysowana kodem). 211 ok.
- 2026-11-13: pierwszy build na 3DS (wcześniej niż w planie): `platform/3ds` z konsolą tekstową libctru, `make 3ds`
  (wspólne warstwy `lang lib graph` kompilują się pod devkitARM bez zmian), tryb testów z wynikami na karcie
  i porównanie na PC (`tools/3ds_tests.sh`). Azahar: 177 ok, 6 czeka; build wysłany na konsolę jako
  `/3ds/MiniSharp-20261113-1610.3dsx`. Dokumentacja: „Uruchamianie na 3DS” w rozdziale 1.
- 2026-11-13: testy na prawdziwej konsoli (`tools/3ds_tests.sh fetch` przez FTP): 177 ok, 6 czeka – ułamki i stos bez
  różnic względem PC.
- 2026-11-13: `repl.c`/`repl.h` przeniesione z `lang/` do `app/` – terminal to narzędzie na języku, nie rdzeń.
  `app` dopisane do `SRCDIRS` (PC) i `SOURCES` (3DS); testy PC 211 ok, build 3DS przechodzi.
- 2026-11-13: `tools/3ds_send.sh tests` wysyła tylko nowe i zmienione testy (sumy md5 w `build/3ds_sent.md5`),
  `tests all` – wszystko od nowa. Wcześniej za każdym razem szło 206 plików.
- 2026-11-13: `printer.c` w `lang/` (AST → kod, `ms_format`, host `--print`) z dostępem, `static class`, `const`/`readonly`
  i `namespace`; test jednostkowy przepuszcza przez wydruk 202 pliki, a `print/*` sprawdza, że wydrukowany program
  działa tak samo. Pułapka MinGW: `return ms_parse(...)` jako wywołanie ogonowe zabiera ramkę `setjmp` i `longjmp`
  kończy się `0xC0000028`. Rozdział 6 „Metody”.
