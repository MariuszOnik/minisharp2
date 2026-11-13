# Plan: nowy MiniSharp (rdzeń niezależny od gier)

**Ten plik jest źródłem prawdy o stanie prac – czytaj go na początku każdej sesji i aktualizuj na końcu.**
Opis docelowy: [docs/ARCHITEKTURA.md](docs/ARCHITEKTURA.md). Dev log: [docs/DEVLOG.md](docs/DEVLOG.md). Ocena starego kodu: [docs/PRZEGLAD.md](docs/PRZEGLAD.md).

## Zasady pracy

1. Nowa sesja = nowy czat. Na start przeczytaj ten plik, weź **jeden** punkt z „Następny krok”.
2. Na końcu sesji: odhacz `[x]`, dopisz 1–3 zdania do dev logu ([docs/DEVLOG.md](docs/DEVLOG.md)), commit.
3. Stare repo (`D:\Programowanie\FalloutDS\minisharp`) to **archiwum tylko do odczytu** – kod przenosimy stąd
   plik po pliku, z przeglądem. Nie wprowadzamy w nim zmian.
4. Kod przenosimy **razem z testami**; po każdym kroku wszystkie przeniesione testy przechodzą.
5. W rdzeniu żadnych nazw ani ścieżek gier. Gry i ich zasoby to moduły w repozytoriach gier.
6. Platformy: 3DS, przeglądarka (web) i Android (APK = wersja web w WebView). PC tylko do testów. Logika (podpowiedzi, graf) tylko w C – web woła ją
   przez wasm. Pliki `.cs` bez BOM.
7. **Dokumentacja języka** (`sdk/docs/`, prośba użytkownika 2026-11-13): solidna, pełna przykładów – jak działa język,
   jak go używać, poradniki. **Każdy commit dopisuje albo poprawia jeden rozdział**, chyba że nie ma czego opisać.
   Pisana prosto, dla początkujących, z dopiskami „Dla zaawansowanych”. Format mały (podzbiór Markdown, opis w
   `sdk/docs/README.md`), żeby edytor na każdej platformie (3DS, web, Switch) mógł ją dołączyć jako Help; przykłady
   ` ```cs ` + ` ```wynik ` sprawdza `make test`, więc dokumentacja zawsze zgadza się z językiem. Po każdej zmianie
   języka przejrzeć też opisy w rozdziałach (testy sprawdzają tylko przykłady).

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

### Dokumentacja języka (przez cały czas, rozdział na commit)
- [x] Format i sprawdzanie przykładów (`sdk/docs/README.md`, `doc/*` w `make test`); rozdział 1 „Pierwszy program”.
- [x] Rozdział 2 „Zmienne i typy”.
- [x] Rozdział 3 „Rachunki” (działania, `+=`/`++`, `Math`/`Mathf`, `Random`).
- [x] Rozdział 4 „Warunki: if i else” (porównania, `&& || !`, `?:`, `switch`).
- [x] Rozdział 5 „Pętle” (`for`, `while`, `do … while`, `foreach`, `break`/`continue`, pętle w terminalu).
- [x] Rozdział 6 „Metody” (parametry, `return`, `=>`, przeciążanie, rekurencja, zasięg zmiennych).
- [ ] Kolejne rozdziały (propozycja kolejności): tablice, string,
      klasy i obiekty, dziedziczenie, List i Dictionary, lambdy, enum/struct/właściwości, przestrzenie nazw,
      korutyny, projekty (`uses.txt`), gra: Update/Draw, Gfx, Input, dźwięk; poradniki krok po kroku; ściąga API.
      Dla gotowego już kodu najpierw rozdziały języka, rozdziały o grafice i projektach – gdy powstaną te części.

### Etap 2 – biblioteka i projekty
- [x] `lib/`: Console, Math, Mathf, Random, string, char, int/float (z `natives.c`); List i Dictionary są w języku (`vm.c`).
- [x] `lang/project.c` (projekt = folder, `uses.txt`, `using X;` dołącza `../x/lib`), host PC przyjmuje folder; 12 testów
      projektów w `tests/projects/` (2026-11-13).
- [x] **Modyfikatory dostępu, wersja łagodna** (decyzja użytkownika 2026-11-13, zrobione 2026-11-13; test `access.cs`, 24 testy
      `errors/acc_*`, `errors/static_*`; też `private set;`, prywatny konstruktor, przeciążenia wybierane tylko spośród dostępnych). Dziś `public`/`private`/`protected`/`internal`
      i `static class` są przyjmowane i pomijane. Zasady: brak słówka = dostęp z każdej klasy (jak dziś, nic się nie psuje);
      jawne `private` – tylko w swojej klasie (błąd w rodzaju `'ile' is private in Licznik`); `protected` – w klasie i pochodnych;
      `internal` = brak słówka; `static class` – zakaz `new` i wszystkie składniki `static`. Każdy program poprawny w C# jest
      poprawny i u nas. Sprawdzenie przy wyszukiwaniu pola/metody/właściwości (`comp_types.c`, `comp_expr.c`, `comp_call.c`,
      też w lambdach), testy w `tests/errors/`, sekcja w rozdziale „Klasy i obiekty”.
- [x] `repl.c` (terminal) z testem `repl` (2026-11-13): `lang/repl.c`, host `--repl`; nowość – wejście w kilku liniach
      (`repl_incomplete`: otwarta `{`/`(`/`[` albo nagłówek `for/while/if/foreach (...)`, `else`, `do` bez treści).
      Ograniczenie: `else` musi być w linii z `}` (samo `}` kończy wejście).
- [ ] **Braki względem C# znalezione przy rozdziale 5** (zasada: program poprawny w C# działa i u nas):
      - przecinek w `for`: `for (int a = 0, b = 10; a < b; a += 3, b--)` – dziś błąd `expected ')', found ','`;
      - `char` to jeden bajt: `foreach (char c in "pętla")` daje dla `ę` dwa znaki (napisy w UTF-8). Do decyzji: `char` jako
        punkt kodowy Unicode (indeksowanie i `Length` po znakach) czy zostawić bajty i opisać.
- [ ] **Braki względem C# znalezione przy rozdziale 6:**
      - parametry z wartością domyślną (`int b = 10`) – dziś czytelny błąd „not supported yet”;
      - `ref` / `out` we własnych metodach (`out` jest tylko w `TryGetValue`; `int.TryParse(s, out int n)` też nie działa);
        komunikat dla `ref` jest niejasny: `expected name, found 'int'`;
      - skrócony inicjalizator tablicy `int[] t = { 1, 2 };` – dziś `unexpected '{' in expression` (działa `new int[] { … }`);
      - ~~brak `return` na jakiejś drodze to błąd w czasie działania, w C# – błąd kompilacji~~ (zrobione 2026-11-13).
- [ ] **`Console.ReadLine()` – odpowiedź użytkownika** (pomysł użytkownika 2026-11-13, jak `cin` w C++). Jak w C#: zwraca
      `string` (liczba przez `int.Parse`). VM się **wstrzymuje** zamiast blokować: `vm_call` zwraca stan „czeka na wejście”,
      host pokazuje pole wpisywania i wznawia VM z wpisanym tekstem – działa tak samo na 3DS, w przeglądarce (wasm nie może
      stanąć w połowie) i na PC (stdin, testy z plikiem odpowiedzi). `ReadLine` w lambdzie wołanej z funkcji wbudowanej
      (np. `Sort`) = czytelny błąd. Rozdział dokumentacji „Rozmowa z użytkownikiem” (np. zgadywanka liczby).
- [x] `printer.c` (AST → kod) z testami `print/*` (2026-11-13): `lang/printer.c` + `ms_format` („Formatuj kod”),
      host `--print`, `tests/unit/test_printer.c` (każdy plik testów, projektów i przykład z dokumentacji: wydruk daje
      to samo drzewo i ten sam tekst przy drugim wydruku). Nowe: dostęp, `static class`, `const`/`readonly`
      (`readOnly = 2` dla `const`), `using`/`namespace`. Wydruk gubi komentarze (nie ma ich w AST) – przed „Formatuj
      kod” w edytorze trzeba je zachować (np. komentarze w AST albo formatowanie tylko wcięć). Test `spans` nie
      przeniesiony: był dla grafu A (łatki po zakresach), wróci, jeśli przyda się edytorowi.
- [ ] `codeindex.c` z testami `ci`, `ci_inherit`, `ci_ns` (projekty z `/*?*/`, opcja hosta `--ci`).
- [x] **Rdzeń bez pętli gry** (uwaga użytkownika 2026-11-13, zrobione 2026-11-13). Kompilator zna tylko `Main`; warstwy wyżej
      rejestrują hooki (`ms_register_hook("Update")`, `ms_hook(p, "Update")`) i swoje klasy C# (`ms_register_source`, np. `Node`
      grafu – `graph/` zarejestruje go w etapie 5). Brak punktu wejścia zgłasza host. Test jednostkowy `tests/unit/test_hooks.c`.

### Etap 3 – platforma 3DS i host testów PC
- [ ] `platform/platform.h`, `platform/pc` (konsola, bufor ekranów, `--frames/--press/--ppm`).
- [ ] `platform/3ds`: ekran, obrazki, dźwięk, dotyk, rysowanie do obrazka, teren; `Gfx`, `Input`, `Audio`, `Time`, `App`, `File`.
- [ ] **Podział `Console` na warstwy** (decyzja użytkownika 2026-11-13): `Console` to biblioteka, nie część języka ani platformy.
      (1) API jak w C# (`Write`, `WriteLine`, `ReadLine`, `Clear`, kolory, `SetCursorPosition`) w `lib/`, w C; wygodne dodatki
      (przeciążenia, formatowanie) mogą być w MiniSharpie przez `ms_register_source`. (2) Logika: siatka znaków, kursor, kolory,
      przewijanie, zawijanie – jedna w `lib/`, testowana na PC. (3) Umowa z platformą – kilka funkcji: narysuj siatkę, daj
      wpisaną linię, czy naciśnięto Stop (`platform/3ds`, `web`, `pc`). Host decyduje, gdzie siatka jest widoczna (cały ekran,
      okienko w menu/edytorze, tekst w testach). Kompilator i VM widzą `Console` jak każdą zarejestrowaną klasę.
- [x] **Przenieść `repl.c` z `lang/` do `app/`** (zrobione 2026-11-13: `app/repl.c`, `app` w obu Makefile'ach) (decyzja użytkownika 2026-11-13): terminal to narzędzie na języku (jak edytor),
      klient `Console`, nie rdzeń. Test `repl` i host `--repl` zostają. Pomysł na później: `Script.Run(kod)` (kompilacja
      i uruchomienie tekstu w trakcie działania, zmienne żyją między wywołaniami) – wtedy terminal może być zwykłym
      programem w MiniSharpie (`Console.ReadLine` + `Script.Run`), który każdy przerobi.
- [ ] **Terminal i `Console` w stylu DOS** (pomysł użytkownika 2026-11-13): ekran jako siatka znaków (górny ekran 3DS
      przy czcionce 8×8 = 50×30), 16 kolorów, migający kursor-blok, znak zachęty w stylu `C:\>`. API jak w C#:
      `Console.ForegroundColor`/`BackgroundColor` (`ConsoleColor.Yellow`…), `Console.Clear()`, `Console.SetCursorPosition(x, y)`,
      `Console.CursorVisible` – na tym gry tekstowe (roguelike). Logika siatki w `lib/` (testy na PC: zrzut siatki jako tekst),
      rysowanie w `platform/*`; czcionka własna albo na wolnej licencji (nie kopiujemy czcionki ROM z kart VGA).
- [ ] **Przycisk „Stop” zamiast limitu czasu** (pytanie użytkownika 2026-11-13: pętla gry w roguelike'u to `while (true)`).
      Dziś limit 5 s działa **tylko w terminalu** (`repl_eval` ustawia `deadline`); zwykły program kręci się, ile chce.
      Docelowo: VM co ~65 tys. skoków wstecz woła funkcję hosta `ms_set_interrupt(int (*check)(void))`; host sprawdza
      przycisk przerwania (3DS: START+SELECT, PC: Ctrl+C, web: flaga z wątku strony – wasm w Workerze) i VM kończy z
      komunikatem „stopped by the user”. W terminalu limit czasu zostaje jako zapas tylko tam, gdzie host nie ma przycisku.
      `Console.ReadLine` (wstrzymanie VM) nie może liczyć się do limitu – czas mierzy się od wznowienia.
- [ ] **Wygląd terminala** (rozmowa 2026-11-13, po terminalu w stylu DOS): zielony „fosfor”, migający kursor, na 3DS
      przyciemniona co druga linia (scanlines – tanie), bez poświaty (za droga). Na 3DS terminal na **cały górny ekran**
      (ramka monitora zjadłaby 20–30% z 400×240), na dolnym **klawiatura rysowana kodem** (prostokąty + czcionka, jeden układ
      dla 3DS i web, skalowalna; zapas: systemowa `swkbd`). Na web/APK opcjonalna ramka monitora CRT (obrazek albo CSS/SVG)
      w `platform/web`, w widoku dokładnie z przodu, ekran jako prostokąt o znanych współrzędnych; treść ekranu zawsze
      rysuje kod. Klawiatury z wygenerowanego obrazka nie robimy (krzywe klawisze, zmyślone litery, ręczne hitboxy).
- [x] **Minimalny build na 3DS** (decyzja użytkownika 2026-11-13, zrobione 2026-11-13, przed resztą etapu 2):
      `platform/3ds/main_3ds.c` (konsola tekstowa libctru, A = `sdmc:/minisharp/main.cs`, X = testy z `/minisharp/tests/`
      do `tests/out/`, START = wyjście, log w `log.txt`), `make 3ds`, `tools/3ds_send.sh` (FTP), `tools/3ds_tests.sh`
      (prepare/compare). Emulator Azahar: 177 ok, 6 czeka (Gfx/Input/App).
- [ ] **Nazwa builda na konsoli** (prośba użytkownika 2026-11-13): każdy build musi dać się odróżnić na karcie. Jest:
      tytuł w Homebrew Launcherze `MiniSharp2 RRRR-MM-DD GG:MM`, w opisie commit, w menu commit i data, plik na karcie
      `/3ds/MiniSharp-RRRRMMDD-GGMM.3dsx` (`tools/3ds_send.sh`). Do zrobienia: to samo dla builda z Actions (wydanie
      `3ds-latest` – nazwa pliku z datą albo numerem przebiegu `github.run_number`) i data/numer na ekranie startowym
      pełnej wersji (etap 3).
- [x] Testy na prawdziwej konsoli (2026-11-13: 177 ok, 6 czeka; ułamki zgodne z PC): `sh tools/3ds_send.sh tests` (FTP konsoli: `192.168.1.90:5000`), X w menu,
      `sh tools/3ds_tests.sh fetch <katalog>` i `sh tools/3ds_tests.sh compare <katalog>` (punkt „Przenośność” niżej).
- [ ] Menu na 3DS (oddzielone od pętli gry), uruchamianie programów i projektów, log na kartę – rozbudowa minimalnego hosta.
- [ ] **Przenośność – sprawdzić na konsoli** (uwagi 2026-11-13; to, co zależy od procesora, VM już pilnuje: zawijanie `int`,
      przesunięcia `& 31`, `int.MinValue / -1`, nasycanie `(int)` – `tests/lib_edges.cs`):
      - ostatnia cyfra `float`: `sinf`, `powf` i wypisywanie `%.7g` pochodzą z różnych bibliotek C (3DS: newlib, Switch, PC) –
        uruchomić na konsoli testy wypisujące ułamki (`math2`, `basics`…) i porównać z `expected/`;
      - flush-to-zero na 3DS: jeśli FPU zeruje bardzo małe liczby, `Mathf.Epsilon` (1.4e-45) wyjdzie 0 – sprawdzić;
      - pamięć: `Value` ma 8 bajtów na 3DS, 16 na Switch/PC (na 3DS mieści się więcej obiektów); głęboka rekurencja
        parsera i kompilatora ma na 3DS mniej stosu – sprawdzić głęboko zagnieżdżony program (limit i czytelny błąd zamiast awarii).

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
- [ ] **Zakładka „Opcje” w edytorze** (prośba użytkownika 2026-11-13), zapisywana w pliku ustawień na karcie / w przeglądarce:
      - **własne klawisze**: użytkownik przypisuje akcje do przycisków (np. kasowanie pod inny przycisk niż domyślne A = spacja,
        B = backspace); domyślny układ jako przycisk „Przywróć”. Akcje edytora mają nazwy (tabela akcja → klawisz), żeby
        przypisanie działało tak samo na 3DS, Switch i w przeglądarce;
      - **schematy kolorowania** wczytywane z pliku (np. `themes/*.txt`: kolor dla słów kluczowych, typów, napisów, komentarzy,
        tła, zaznaczenia), kilka wbudowanych do wyboru;
      - **czcionki** do wyboru (inne pliki czcionek) i **powiększanie / zmniejszanie** czcionki w edytorze (też skrótem);
      - **zaznacz, kopiuj, wytnij, wklej** (schowek edytora; w przeglądarce także systemowy).
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

Etap 2, w tej kolejności (każdy następny korzysta z poprzedniego). Punkty 1–4 to braki względem C# z rozdziału 6
(decyzja użytkownika 2026-11-13: przed `codeindex.c`); każdy z testami i poprawką rozdziału 6 („Dla zaawansowanych”):
1. [x] Skrócony inicjalizator tablicy `int[] t = { 1, 2 };` (zmienna, pole, właściwość) – zrobione 2026-11-13:
   `parse_init` w parserze robi `E_NEWARR` z `prefix = 2`, printer drukuje krótko; test `arrayinit`, błędy `arrinit_*`.
2. [x] Brak `return` na jakiejś drodze = błąd kompilacji – zrobione 2026-11-13: `stmt_can_end` w `comp_stmt.c`
   (osiągalność końca: `if`/`else`, `while (true)`/`for (;;)` bez `break`, `do`, `switch` z `default`, stałe
   `true`/`false`) dla metod, `get` właściwości i lambd; `OP_NORET` zostaje; test `returns`, błędy `noret_*`.
3. Parametry z wartością domyślną (`int b = 10`, tylko stałe jak w C#): wartość w `Param`, uzupełnianie argumentów
   przy wywołaniu, wybór przeciążenia, printer.
4. `ref` / `out` we własnych metodach (na początek zmienne lokalne): przekazanie komórki jak przy zmiennych
   z lambd; `out` musi być przypisane przed wyjściem; printer; potem `int.TryParse(s, out int n)` jak `TryGetValue`.
5. `codeindex.c` (nie podpowiada `private`/`protected` spoza klasy – `can_access`).
