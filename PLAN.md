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
- [x] Rozdział 7 „Tablice” (`{ … }`, `new T[n]`, `Length`, `for`/`foreach`, suma i maksimum, tablica w metodzie, `char[][]`).
- [x] Rozdział 8 „Napisy” (`+`, `$""`, znaki specjalne, `Length`/`s[i]`, metody, `Split`, `IsNullOrEmpty`).
- [x] Rozdział 9 „Rozmowa z użytkownikiem” (`Console.ReadLine`, `int.Parse`/`TryParse`, zgadywanka, linie do pustej, terminal).
- [x] Rozdział 10 „Klasy i obiekty” (pola, metody, konstruktor, `this`, obiekty w listach, referencje i `null`, `static`, właściwości, `private`).
- [x] Rozdział 11 „Dziedziczenie” (`: base(...)`, `virtual`/`override`, lista różnych obiektów, `base.`, `ToString`,
      `protected`, `abstract`, `is`/`as`, interfejsy).
- [x] Rozdział 12 „List i Dictionary” (dodawanie, szukanie, `Sort`, usuwanie od końca, słownik, `TryGetValue`,
      `foreach` po parach, liczenie słów, listy obiektów).
- [x] Rozdział 13 „Lambdy” (`Func`/`Action`, metody listy z lambdą, `Sort` z porównaniem, pamiętane zmienne,
      metoda jako wartość, zdarzenia `+=`/`-=`, ściąga typów).
- [x] Rozdział 14 „enum i struct” (enum w `switch`, liczby i rzutowania, enum jako klucz; struct kopiowany przy
      przypisaniu i w metodzie, struct czy klasa). Właściwości są już w rozdziale 10.
- [x] Rozdział 15 „Przestrzenie nazw” (`namespace`, pełne nazwy, `using`, ta sama nazwa w dwóch przestrzeniach,
      przestrzenie zagnieżdżone, kolejność szukania).
- [ ] Kolejne rozdziały (propozycja kolejności):
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
      - ~~przecinek w `for`~~ (zrobione 2026-11-13: `E_SEQ` w parserze tylko dla części 1 i 3 `for`, printer, test `forcomma`);
      - ~~`char` to jeden bajt~~ – **decyzja użytkownika 2026-11-13: `char` = punkt kodowy Unicode** (zrobione 2026-11-13:
        `lang/text.c`, napisy dalej w UTF-8, `ObjString.ulen` liczone leniwie + pamięć ostatniej pozycji; `Length`, `s[i]`,
        `Substring`, `IndexOf`, `Split`, `ToUpper/ToLower` z polskimi literami, grekiem i cyrylicą; testy `unicode`,
        `rt_strindex`). Zostało: sortowanie napisów po polsku (dziś `ą` po `z`, kolejność kodów).
      - ~~brak `string.Join(sep, lista)`~~ (zrobione 2026-11-14: `call_join` w `comp_call.c` kompiluje pętlę – lista,
        `T[]` i wersja `Join(sep, a, b, …)`, separator `string`/`char`, `ToString` obiektów, `null` = ""; test `join`,
        błędy `join_*`). Wpis w tabeli `lib/strings.c` tylko dla podpowiedzi i sygnatury (nigdy nie wołany).
      - ~~usuwanie z listy w trakcie `foreach` po niej przechodzi bez błędu~~ (zrobione 2026-11-14, użytkownik zostawił
        decyzję mnie: `ObjList.version` rośnie przy każdej zmianie – jak `_version` w .NET, także `l[i] = x` i `Sort`;
        `foreach` po `List` sprawdza go przed każdym krokiem (`L_VERSION`/`L_CHECKVER`), `ForEach` po każdym wywołaniu;
        testy `foreach_change`, `rt_foreach_*`, `rt_listforeach_add`). `Dictionary` też (2026-11-14): `ObjDict.version`
        rośnie tylko przy nowym kluczu, jak w .NET Core 3.0+ (`Remove`, `Clear`, nadpisanie wartości dozwolone); testy
        `dict_change`, `rt_dictforeach_add`. Różnica, która zostaje: `foreach` po `d.Keys`/`d.Values` idzie po kopii,
        więc dodanie klucza tam nie daje błędu (w .NET daje).
- [x] **Braki względem C# znalezione przy rozdziale 15:** aliasy `using G = Gra;` / `using V = Gra.Wektor;` (zrobione
      2026-11-14, użytkownik zostawił wybór mnie: `NsName.alias` w `usings` klasy, `find_class` zamienia pierwszy człon
      nazwy na cel i szuka od przestrzeni globalnej, printer je drukuje, `project.c` bierze bibliotekę z celu; test
      `alias`, błąd `alias_generic`). Zostało: alias typu z `<…>`; podpowiedzi po `G.` (codeindex nie zna aliasów).
- [ ] **Braki względem C# znalezione przy rozdziale 6:**
      - ~~parametry z wartością domyślną (`int b = 10`)~~ (zrobione 2026-11-13);
      - ~~`ref` / `out` we własnych metodach i `int.TryParse(s, out int n)`~~ (zrobione 2026-11-13; zostało: pola,
        elementy tablic i właściwości przez `ref`, błąd przy czytaniu `out` przed przypisaniem);
      - skrócony inicjalizator tablicy `int[] t = { 1, 2 };` – dziś `unexpected '{' in expression` (działa `new int[] { … }`);
      - ~~brak `return` na jakiejś drodze to błąd w czasie działania, w C# – błąd kompilacji~~ (zrobione 2026-11-13).
- [ ] **Braki względem C# znalezione przy rozdziale 7 (tablice):**
      - ~~przypisanie do zmiennej pętli `foreach` przechodzi, w C# to błąd CS1656~~ (zrobione 2026-11-13: `Local.readOnly`,
        `check_writable_local` w `lvalue` i przy `ref`/`out`; błędy `foreach_*`; zostało: pole struktury przez taką zmienną, CS1654);
      - ~~`int[,]`~~ – **decyzja użytkownika 2026-11-13: dodać tablice prostokątne** (zrobione 2026-11-13: typ `T_ARRAY`
        z `rank = 2`, `ObjArray.rows/cols`, w VM `NEWARR2`, `IDX2` – dwa indeksy w jeden, potem zwykłe `GETELEM`/`SETELEM`,
        więc `=`, `+=`, `++` bez zmian – i `ARRDIM` dla `GetLength`; `new T[r, c]`, `{ {..}, {..} }`, printer, podpowiedzi;
        testy `rect`, `rt_rect*`, `ci_rect`, błędy `rect_*`). Tylko dwa wymiary i tylko `T[,]` (nie `T[,][]`, `T[][,]`).
- [x] **`Console.ReadLine()` – odpowiedź użytkownika** (pomysł użytkownika 2026-11-13, jak `cin` w C++). Jak w C#: zwraca
      `string` (liczba przez `int.Parse`). VM się **wstrzymuje** zamiast blokować: `vm_call` zwraca stan „czeka na wejście”,
      host pokazuje pole wpisywania i wznawia VM z wpisanym tekstem – działa tak samo na 3DS, w przeglądarce (wasm nie może
      stanąć w połowie) i na PC (stdin, testy z plikiem odpowiedzi). `ReadLine` w lambdzie wołanej z funkcji wbudowanej
      (np. `Sort`) = czytelny błąd. Rozdział dokumentacji „Rozmowa z użytkownikiem” (np. zgadywanka liczby).
      Zrobione 2026-11-13: `vm_call` zwraca `MS_WAIT_INPUT`, `vm_resume_input(vm, linia)` (NULL = koniec wejścia → `null`),
      `ms_vm_wait_input` dla funkcji wbudowanych, `vm->depth` pilnuje zagnieżdżenia; host PC czyta stdin (`--echo`),
      testy `readline` (+ `tests/input/readline.txt`), `rt_readline_lambda`, `repl`; terminal: następna linia to
      odpowiedź (`repl_answer`); dokumentacja: blok ` ```wejscie `, rozdział 9. Na 3DS zwykły program pyta przez
      `app/conscreen.c` (2026-11-14; wcześniej błąd „tylko w terminalu”);
      testy na 3DS biorą odpowiedzi z `tests/input/`.
- [x] `printer.c` (AST → kod) z testami `print/*` (2026-11-13): `lang/printer.c` + `ms_format` („Formatuj kod”),
      host `--print`, `tests/unit/test_printer.c` (każdy plik testów, projektów i przykład z dokumentacji: wydruk daje
      to samo drzewo i ten sam tekst przy drugim wydruku). Nowe: dostęp, `static class`, `const`/`readonly`
      (`readOnly = 2` dla `const`), `using`/`namespace`. Wydruk gubi komentarze (nie ma ich w AST) – przed „Formatuj
      kod” w edytorze trzeba je zachować (np. komentarze w AST albo formatowanie tylko wcięć). Test `spans` nie
      przeniesiony: był dla grafu A (łatki po zakresach), wróci, jeśli przyda się edytorowi.
- [x] `codeindex.c` z testami `ci`, `ci_inherit`, `ci_ns` (projekty z `/*?*/`, opcja hosta `--ci`) – zrobione 2026-11-13:
      `lang/codeindex.c` (czyta tekst, nie AST – działa na kodzie z błędami), testy w `tests/ci/<nazwa>/`, nowy `ci_access`
      (podpowiedzi bez `private`/`protected` spoza klasy, ta sama zasada co `can_access`). Zakresy klas w strukturze
      zamiast tablicy globalnej, bez zależności od `repl.h` (`CI_WORD`).
- [x] **Rdzeń bez pętli gry** (uwaga użytkownika 2026-11-13, zrobione 2026-11-13). Kompilator zna tylko `Main`; warstwy wyżej
      rejestrują hooki (`ms_register_hook("Update")`, `ms_hook(p, "Update")`) i swoje klasy C# (`ms_register_source`, np. `Node`
      grafu – `graph/` zarejestruje go w etapie 5). Brak punktu wejścia zgłasza host. Test jednostkowy `tests/unit/test_hooks.c`.

### Etap 3 – platforma 3DS i host testów PC
- [x] **Grafika 2D, Input, Time, App – wspólne API** (decyzja użytkownika 2026-11-14: etap 3, na razie tylko 2D):
      `lib/gfx.c` – `Gfx.Screen/Clear/Rect/RectLine/Circle/Line/Pixel/Text/TextWidth/Alpha/Rgb/Lerp/Width/Height`,
      `Color.*`, `Screen.Top/Bottom`, `Input.Held/Pressed/Released/Touching/Touched/TouchX/TouchY`, `Btn.*`,
      `Time.Millis/Delta/Frame`, `App.Quit`. Platforma daje tylko `GfxBackend` (prostokąt, koło, linia, tekst) i co klatkę
      `lib_input_frame` / `lib_time_frame` – zamiast osobnego `platform.h`. Testy czekające od etapu 1 przechodzą
      (`czeka.txt` pusty).
- [x] `platform/pc`: ekrany w pamięci (`swgfx.c`, 400×240 + 320×240, czarne na początku klatki), `--press A@10`,
      `--press Left@5-20`, `--touch 3@100,50`, `--hash` (skrót obrazu do testów), `--ppm plik` (zrzut 400×480);
      testy `gfx_draw`, `input_frames`. Tekst na PC to prostokąt na znak (czcionka to sprawa platformy).
- [ ] `platform/3ds`: ekran, obrazki, dźwięk, dotyk, rysowanie do obrazka, teren; `Gfx`, `Input`, `Audio`, `Time`, `App`, `File`.
- [ ] **Podział `Console` na warstwy** (decyzja użytkownika 2026-11-13): `Console` to biblioteka, nie część języka ani platformy.
      (1) API jak w C# (`Write`, `WriteLine`, `ReadLine`, `Clear`, kolory, `SetCursorPosition`) w `lib/`, w C; wygodne dodatki
      (przeciążenia, formatowanie) mogą być w MiniSharpie przez `ms_register_source`. (2) Logika: siatka znaków, kursor, kolory,
      przewijanie, zawijanie – jedna w `lib/`, testowana na PC. **Zrobione 2026-11-14:** `lib/textgrid.c` (komórki ze
      znakiem i kolorami ConsoleColor, kursor, zawijanie bez pustej linii po pełnym wierszu, przewijanie, `\r \t \b`,
      `tg_version` do odświeżania, paleta), test `test_textgrid`.
      Podłączone do `Console` (2026-11-14): `lib_console_set_grid` – `Write` trafia też do siatki, działają `Console.Clear()`,
      `SetCursorPosition(x, y)` (poza siatką błąd jak wyjątek C#), `ResetColor()`; bez siatki te trzy nic nie robią.
      Host PC: `--grid 50x30` (na końcu wypisuje wiersze siatki); testy `console_grid`, `rt_setcursor`.
      Kolory (2026-11-14): `enum ConsoleColor` i właściwości `Console.ForegroundColor` / `BackgroundColor` napisane w
      MiniSharpie w `lib/console.c` (`ms_register_source`) nad funkcjami `Console.__Get/SetForeground`; nowa zasada
      kompilatora: **klasa ze źródła o nazwie klasy wbudowanej ją rozszerza** (`builtin_fallback`: czego klasa nie ma,
      to bierze z tabel). Błąd w kodzie takiej klasy pokazuje linię programu, który ją wywołał (`Func.fromSource`).
      Testy `console_colors`, `rt_consolecolor`; rozdział 9. Zostało: podpowiedzi po `Console.` nie pokazują jeszcze
      `ForegroundColor` (codeindex nie czyta źródeł zarejestrowanych), `CursorVisible`, `Console.ReadKey`. (3) Umowa z platformą – kilka funkcji: narysuj siatkę, daj
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
- [x] **Przycisk „Stop” zamiast limitu czasu** (zrobione 2026-11-13: `ms_set_interrupt` w `vm.c` przy sprawdzaniu pętli, test
      `tests/unit/test_interrupt.c`; 3DS: START+SELECT w `main_3ds.c`, PC: Ctrl+C; web – gdy powstanie `platform/web`) (pytanie użytkownika 2026-11-13: pętla gry w roguelike'u to `while (true)`).
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
- [x] **Nazwa builda na konsoli** (prośba użytkownika 2026-11-13): każdy build musi dać się odróżnić na karcie. Jest:
      tytuł w Homebrew Launcherze `MiniSharp2 RRRR-MM-DD GG:MM`, w opisie commit, w menu commit i data, plik na karcie
      `/3ds/MiniSharp-RRRRMMDD-GGMM.3dsx` (`tools/3ds_send.sh`). Build z Actions (2026-11-14): plik `MiniSharp-<nr>.3dsx`
      w wydaniu `3ds-latest`, tytuł w launcherze `MiniSharp2 nr <nr> <data>`, w menu `build <nr> / <commit>, <data>`
      (`BUILD_NUM` w `platform/3ds/Makefile`). Data z Actions jest w UTC.
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

## Do decyzji

- (rozstrzygnięte 2026-11-13: `char` = punkt kodowy Unicode; `int[,]` – dodać do języka.)

## Następny krok

Etap 2, w tej kolejności (każdy następny korzysta z poprzedniego). Punkty 1–4 to braki względem C# z rozdziału 6
(decyzja użytkownika 2026-11-13: przed `codeindex.c`); każdy z testami i poprawką rozdziału 6 („Dla zaawansowanych”):
1. [x] Skrócony inicjalizator tablicy `int[] t = { 1, 2 };` (zmienna, pole, właściwość) – zrobione 2026-11-13:
   `parse_init` w parserze robi `E_NEWARR` z `prefix = 2`, printer drukuje krótko; test `arrayinit`, błędy `arrinit_*`.
2. [x] Brak `return` na jakiejś drodze = błąd kompilacji – zrobione 2026-11-13: `stmt_can_end` w `comp_stmt.c`
   (osiągalność końca: `if`/`else`, `while (true)`/`for (;;)` bez `break`, `do`, `switch` z `default`, stałe
   `true`/`false`) dla metod, `get` właściwości i lambd; `OP_NORET` zostaje; test `returns`, błędy `noret_*`.
3. [x] Parametry z wartością domyślną (`int b = 10`) – zrobione 2026-11-13: `Param.def`, w `compiler.c`
   `resolve_defaults` zamienia je na literały (liczba, znak, napis, bool, null, `const`, człon enuma), `args_for_m`
   uzupełnia brakujące argumenty (metody, konstruktory, `base(...)`/`this(...)`, niejawne `base()`); przy remisie
   przeciążeń wygrywa wersja bez uzupełniania; test `defaultparams`, błędy `defparam_*`.
4. [x] `ref` / `out` we własnych metodach – zrobione 2026-11-13: `ref` to słowo kluczowe, `Param.mode`,
   `CMethod.refs/outs`; zmienne przekazane przez `ref`/`out` trafiają do komórek (skan jak dla lambd), metoda dostaje
   komórkę (`Local.refMode`, w lambdzie błąd); `check_out_params` w `comp_stmt.c` sprawdza przypisanie `out` na każdej
   drodze; przeciążenia z `ref` i bez; `int.TryParse` / `float.TryParse` przez ukryte natywne `#int` / `#float`;
   test `refout`, błędy `refout_*`, `tryparse_*`. Na razie tylko zmienne lokalne i parametry (nie pola, elementy).
5. [x] `codeindex.c` (nie podpowiada `private`/`protected` spoza klasy – `can_access`) – zrobione 2026-11-13.

**Szybsza ścieżka do pisania na konsoli** (decyzja użytkownika 2026-11-13): po `codeindex.c` edytor i terminal na 3DS
idą wcześniej, **na przemian z rdzeniem** (sesja edytor/terminal, sesja rdzeń: `Console.ReadLine`, Stop, rozdziały
dokumentacji). Bez systemowej klawiatury `swkbd` – **od razu własna**, rysowana kodem (wzór: stare `keyboard.c`).
6. [x] Minimalny `app/ui.h` + `platform/3ds/ui_3ds.c` (citro2d) + klawiatura `app/keyboard.c` – zrobione 2026-11-13:
   backend to tabela funkcji (`UiBackend`, `ui_set_backend`), wejście podawane w `UiInput` (przyciski, dotyk, czas);
   klawiatura skalowalna (`kbd_set_area`), test `tests/unit/test_keyboard.c` (warstwy, Shift, powtarzanie, trafienia,
   rysowanie). Na 3DS: Y w menu = test klawiatury (ekrany na chwilę przechodzą z konsoli tekstowej na citro2d).
   **Sprawdzić na konsoli**: czy test klawiatury działa i wraca do menu; czy rysowanie co klatkę (bez tekstury
   z klawiszami jak w starym kodzie) jest dość szybkie.
7. [x] Terminal na 3DS – zrobione 2026-11-13: `app/terminal.c` (linia z kursorem, historia, wejście w kilku liniach,
   podpowiedzi `repl_complete`, przewijane wyjście zawijane po 56 znakach UTF-8, 200 linii, układ na 1 lub 2 ekrany),
   test `tests/unit/test_terminal.c`; Y w menu 3DS; Stop START+SELECT, wtedy bez limitu 5 s (`repl_set_time_limit`).
   Brak jeszcze płótna `Gfx` (etap 3). **Sprawdzić na konsoli**: wejście/wyjście z terminala, czytelność czcionki 13 px,
   czy wiersze po 56 znaków mieszczą się na 400 px (czcionka systemowa jest proporcjonalna), Stop w `while (true) { }`.
8. [x] (rdzeń) usługa języka `lang/service.h` – zrobione 2026-11-13: `svc_colors` (kolory leksykalne, stan komentarza
   między liniami, klasy projektu jako typy), `svc_complete` (po kropce `codeindex`, inaczej nazwy z pliku, klasy
   projektu, wbudowane, słowa kluczowe), `svc_signature` (sygnatura wywołania i numer argumentu: własne metody
   z `codeindex`, wbudowane z tabel z „(+N)” dla przeciążeń), `svc_complete_line` dla terminala (dawne `ms_complete`
   z `app/repl.c`); test `tests/unit/test_service.c`. Brak: sygnatury konstruktorów (`codeindex` ich nie zapisuje),
   błędy z liniami (dojdą z „Uruchom” w edytorze).
9. [x] Edytor kodu na 3DS – zrobione 2026-11-13: bufor `app/textbuf.c` (test `test_textbuf`) i `app/editor.c`
   (test `test_editor`): widok z numerami linii i kolorami z usługi języka, podpowiedzi i sygnatura wywołania,
   przyciski Save/Run/Undo/Redo, zapis bez BOM, Run przez host (`EditorHost.run`) i skok do linii błędu, ostrzeżenie
   o niezapisanych zmianach. Na 3DS: B w menu edytuje `main.cs`, Run wychodzi na chwilę do konsoli tekstowej.
   **Sprawdzić na konsoli**: czytelność, szybkość rysowania przy dłuższym pliku, Run i powrót.
10. [x] Wybór pliku / projektu z karty – zrobione 2026-11-13: `app/files.c` (test `test_files`): w folderze głównym
   projekty (foldery z `.cs`, bez `tests/`) i programy `.cs`, w projekcie jego `.cs` i `.txt`; A/dotyk otwiera, B wraca,
   X nowy plik (program z `Main` albo pusta klasa w projekcie), Y nowy projekt (`Program.cs`); nazwa z klawiatury
   ekranowej. Na 3DS: B w menu → lista → edytor, START w edytorze wraca do listy; Run pliku z projektu uruchamia
   folder (`files_project_of`). **Sprawdzić na konsoli**: lista, dotyk, nowy plik/projekt, Run projektu.
11. Edytor, dalej: ~~usługa języka dostaje wszystkie pliki projektu~~ (zrobione 2026-11-14: `ed_set_project` czyta
   pozostałe pliki projektu i folderów z `uses.txt` przez `project_load`, edytowany plik to tekst 0 usługi, reszta tylko
   do odczytu; 3DS woła to po `ed_open`, gdy plik leży w projekcie; test w `test_editor`). ~~Karty kilku plików
   projektu~~ (zrobione 2026-11-14: pliki projektu otwierają się jako karty w kolejności nazw, L + X / L + Y przełącza,
   usługa języka widzi niezapisany tekst wszystkich kart, Run zapisuje wszystkie zmienione, błąd „Plik.cs line N”
   otwiera kartę tego pliku, START ostrzega o zmianach w każdej karcie; najwyżej 12 kart, test w `test_editor`), ~~usuwanie pliku~~ (zrobione 2026-11-14: SELECT dwa razy
   na liście; folder projektu tylko pusty; test w `test_files`; też narzędzie „Delete” na dotyk, dwa dotknięcia),
   ~~zaznaczanie i schowek~~ (zrobione 2026-11-14: X włącza tryb zaznaczania – D-pad i L + D-pad powiększają
   zaznaczenie, cztery narzędzia to wtedy Copy / Cut / Paste / All; jeden schowek dla wszystkich plików; test w
   `test_editor`; schowek systemowy dojdzie w wersji web), ~~wyjście programu w oknie zamiast konsoli tekstowej~~ (zrobione 2026-11-14:
   `app/conscreen.c` – siatka `Console` 40 × 20 na górnym ekranie z kolorami, `Console.ReadLine` pyta przez pole i
   klawiaturę na dolnym (START = koniec wejścia), na końcu wynik albo błąd na czerwono do A / B; na 3DS tak działa A w
   menu i Run w edytorze; test `test_conscreen` uruchamia prawdziwy program z dwoma pytaniami; 50 × 30 czeka na
   czcionkę bitmapową 8 × 8 – systemowa jest proporcjonalna i przy 8 px nieczytelna), ~~Help~~ (zrobione 2026-11-14: `app/help.c` – lista rozdziałów
   `sdk/docs` i czytnik z zawijaniem, blokami kodu/wyniku, dopiskami, przewijaniem, Prev/Next; test `test_help`; na 3DS
   R w menu, rozdziały z `romfs:/docs` wbudowanych w `.3dsx` łączone z `sdmc:/minisharp/docs` – plik z karty zastępuje
   wbudowany o tej samej nazwie, nowe pliki dochodzą; `help_load_dirs`). Zostało:
   ~~Help z wnętrza edytora~~ (zrobione 2026-11-14: piąte narzędzie „Help”, `EditorHost.help`; w trybie zaznaczania
   piąte to „Done”) i ~~„skopiuj przykład do edytora”~~ (zrobione 2026-11-14: przycisk „Copy” w czytniku Help kładzie
   pierwszy przykład widoczny na ekranie do schowka edytora – `help_set_copy`, `ed_set_clipboard`).

**Stan 2026-11-14 (sesja autonomiczna z telefonu):** punkt 11 zrobiony prawie cały (karty, schowek, Help, ekran
konsoli z `ReadLine`). Zostały z drobnych: czcionka bitmapowa 8 × 8 z polskimi literami (konsola 50 × 30),
`Console.ReadKey` / `CursorVisible` (gry tekstowe – `ReadKey` wymaga wstrzymania VM jak `ReadLine`), podpowiedzi
`Console.ForegroundColor` i po aliasie. **Do decyzji użytkownika – który duży etap teraz:** (a) etap 3: `Gfx`, `Input`,
`Time`, host PC z `--frames/--press/--ppm` (gry graficzne na 3DS); (b) etap 5: edytor grafu „Bolt” (najpierw lista
funkcji Bolta do ustalenia); (c) etap 5b: wersja web + APK z nowego rdzenia.
