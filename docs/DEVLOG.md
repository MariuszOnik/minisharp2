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
- 2026-11-13: Skrócony inicjalizator tablicy `int[] t = { 1, 2 };` dla zmiennej, pola i właściwości (parser → `new int[] { … }`,
  printer zachowuje krótki zapis); `var t = { … }` i `{ … }` dla nie-tablicy to czytelne błędy. Rozdział 6: przykład
  tablicy przekazanej do metody.
- 2026-11-13: Brak `return` na jakiejś drodze metody, `get` albo lambdy z wynikiem to teraz błąd kompilacji (jak CS0161
  w C#): analiza osiągalności końca instrukcji w `comp_stmt.c`, stałymi warunkami są tylko literały `true`/`false`.
  `OP_NORET` w VM zostaje jako zabezpieczenie. Rozdział 6: opis, które drogi kompilator sprawdza.
- 2026-11-13: Parametry z wartością domyślną (`int b = 10`): wartość musi być stałą (także `const` i człon enuma), jest
  zamieniana na literał przy rejestracji metod, więc wołający kompiluje ją bez zależności od swoich zmiennych. Działa
  w metodach, konstruktorach i `base(...)`; printer drukuje `= wartość`. Rozdział 6: sekcja „Parametr z wartością domyślną”.
- 2026-11-13: `ref` i `out` we własnych metodach: zmienna przekazana tak żyje w komórce (ten sam mechanizm co zmienne
  z lambd), więc `ref` działa też razem z lambdami i przy przekazaniu parametru dalej. `out` musi dostać wartość na każdej
  drodze (prosta analiza jak CS0177). `int.TryParse` / `float.TryParse` bez zmian w VM: dwie ukryte funkcje natywne.
  Rozdział 6: sekcja „ref i out”.
- 2026-11-13: `codeindex.c` w `lang/` (podpowiedzi edytora po kropce, typ nazwy z deklaracji, dziedziczenie, przestrzenie
  nazw) z testami `tests/ci/*` przez host `--ci`. Nowość: nie podpowiada `private`/`protected` spoza klasy (`ci_access`).
  Ustalona szybsza ścieżka do pisania na konsoli: własna klawiatura + terminal, potem edytor, na przemian z rdzeniem.
- 2026-11-13: `app/ui.h` – rysowanie i wejście aplikacji przez tabelę funkcji platformy (`UiBackend`), pierwsza
  implementacja `platform/3ds/ui_3ds.c` na citro2d. Klawiatura dotykowa przeniesiona do `app/keyboard.c` bez libctru,
  ze skalowalnym obszarem i testem na PC (`test_keyboard`); na 3DS Y w menu otwiera test klawiatury. Rozdział 7 „Tablice”
  (przy nim wyszły dwa braki względem C#: przypisanie do zmiennej `foreach`, `int[,]`).
- 2026-11-13: Przycisk Stop: `ms_set_interrupt(check)` – VM pyta hosta co ~65 tys. skoków wstecz (to samo miejsce, co
  limit czasu terminala). 3DS: przytrzymane START+SELECT (funkcja sama woła `hidScanInput`, bo w `Main` z `while (true)`
  nikt inny nie czyta przycisków), PC: Ctrl+C. Rozdział 5: opis Stop przy nieskończonej pętli.
- 2026-11-13: Terminal w `app/terminal.c` na `ui.h` (bez libctru): logika linii, historii, podpowiedzi i przewijanego
  wyjścia testowana na PC (`test_terminal`, też układ na jeden ekran). Na 3DS Y w menu otwiera terminal; host z przyciskiem
  Stop wyłącza limit 5 s (`repl_set_time_limit`). Rozdział 1: sekcja „Terminal na 3DS”.
- 2026-11-13: Usługa języka `lang/service.c`: kolory linii, podpowiedzi w miejscu kursora i sygnatura wywołania
  z numerem argumentu – wszystko na tekście (`codeindex` + tabele wbudowanych), więc działa na kodzie z błędami.
  Uzupełnianie słów terminala przeniesione z `app/repl.c` do usługi: jedna logika dla terminala i edytora.
- 2026-11-13: Bufor tekstu edytora `app/textbuf.c` (bez rysowania i bez wiedzy o języku): kursor po znakach UTF-8,
  zaznaczanie, automatyczne wcięcia, cofanie/ponawianie (pisanie i kasowanie z rzędu = jeden krok), znacznik
  „zmieniony” zgodny z cofaniem do zapisanego stanu. Test `test_textbuf`.
- 2026-11-13: Edytor kodu `app/editor.c` na `ui.h`: tekst w `textbuf`, kolory, podpowiedzi i sygnatury z usługi języka,
  uruchamianie przez funkcję hosta i skok do linii błędu. Na 3DS przycisk B w menu. Testy z konsoli po poprzednich
  zmianach: 207 ok, 0 błędów. Rozdział 1: sekcja „Edytor na 3DS”.
- 2026-11-13: Przypisanie do zmiennej pętli `foreach` (`=`, `+=`, `++`, `ref`/`out`, też w lambdzie i dla słownika) to
  teraz błąd kompilacji jak CS1656 w C#. Rozdział 7 pokazuje komunikat.
- 2026-11-13: Przecinek w `for` (`for (i = 0, j = 5; i < j; i++, j--)`): nowy węzeł `E_SEQ` tylko w pierwszej
  i ostatniej części, kompilowany jak kolejne instrukcje; inne funkcje przechodzące po drzewie nie wymagały zmian.
  Test `forcomma`, rozdział 5: dopisek „Dla zaawansowanych”.
- 2026-11-13: Rozdział 8 „Napisy” – 6 sprawdzanych przykładów (sklejanie, `$"..."`, znaki, metody, `Split` z `int.Parse`).
- 2026-11-13: `char` to punkt kodowy Unicode (decyzja użytkownika): napisy zostają w UTF-8, a `Length`, `s[i]`,
  `Substring`, `IndexOf` liczą znaki jak C#; liczba znaków liczona raz, przy pierwszej potrzebie, a pętla po `s[i]`
  korzysta z zapamiętanej pozycji. Wielkie/małe litery i `char.IsLetter` także dla polskich liter, greki i cyrylicy.
  Przy okazji wyszła awaria przy wielu kompilacjach z błędem w jednym procesie – zapisana w „Do decyzji” do naprawy.
- 2026-11-13: Naprawiona losowa awaria (0xC0000005) przy błędzie kompilacji na 64-bitowym Windows: `setjmp` z MinGW
  zapamiętuje ramkę dla rozwijania stosu SEH i `longjmp` czasem się na tym wywracał (ok. 3% procesów, zależnie od
  układu pamięci – stąd niestabilny `test_hooks`). `lang/msjmp.h` włącza zwykły skok (`__USE_MINGW_SETJMP_NON_SEH`).
  Sprawdzone: 0 awarii na 200 procesów po 200 kompilacji z błędem i na 150 przebiegów `test_hooks`. 3DS i Linux bez zmian.
- 2026-11-13: Tablice prostokątne `T[,]` (decyzja użytkownika): elementy wiersz po wierszu w zwykłym `ObjArray`
  z liczbą wierszy i kolumn, a `t[i, j]` kompiluje się do `IDX2` (oba indeksy sprawdzone osobno) i zwykłego
  `GETELEM`/`SETELEM` – przypisania, `+=`, `++`, `Length` i `foreach` działają bez osobnego kodu. Rozdział 7: plansza
  jako `char[,]`, tablica tablic jako dopisek.
- 2026-11-13: Lista programów i projektów przed edytorem (`app/files.c`, test `test_files`): w folderze głównym
  projekty (foldery z `.cs`) i pojedyncze programy, w projekcie jego `.cs` i `uses.txt`; nowy plik i nowy projekt z nazwą
  wpisaną na klawiaturze (plik dostaje klasę z `Main` albo pustą klasę w projekcie). Na 3DS B w menu → lista → edytor,
  START w edytorze wraca do listy, a Run pliku z projektu uruchamia cały folder.
- 2026-11-13: `Console.ReadLine()` przez wstrzymanie VM: funkcja wbudowana prosi o linię, `vm_call` wraca z
  `MS_WAIT_INPUT` ze stosem i ramkami nietkniętymi, a `vm_resume_input` kładzie odpowiedź na stos i jedzie dalej – bez
  blokowania, więc nada się też do wasm. Nie wolno tego w wywołaniach zagnieżdżonych (lambda w `Sort`, `ToString`,
  korutyna) – czytelny błąd. PC czyta stdin, terminal bierze odpowiedź z następnej linii, rozdział 9 z blokiem `wejscie`.
- 2026-11-13: Rozdział 10 „Klasy i obiekty” – 8 sprawdzanych przykładów (pola, konstruktor, `this`, lista obiektów,
  dwie zmienne na jeden obiekt, `static`, właściwości, `private` z dokładnym komunikatem błędu).
- 2026-11-14: Rozdział 11 „Dziedziczenie” – 10 sprawdzanych przykładów (klasa bazowa, `base(...)`, `virtual`/`override`,
  lista potworów, `base.Metoda()`, `ToString`, `protected`, `abstract`, `is`/`as`, interfejsy). Sesja z telefonu,
  praca w chmurze na Linuksie; ostrzeżenie z `compiler.c:330` już nie występuje w gcc na Linuksie.
- 2026-11-14: Edytor zna cały projekt: `ed_set_project` wczytuje pozostałe pliki folderu (i bibliotek z `uses.txt`)
  do usługi języka, więc `s.` podpowiada metody klasy z innego pliku, a sygnatura wywołania też ją zna. Edytowany
  plik dalej idzie z bufora (tekst 0), inne z karty – karty plików przyjdą osobno.
- 2026-11-14: Usuwanie na liście plików: SELECT pyta („SELECT again: delete …”), drugi SELECT na tej samej pozycji
  usuwa, każdy inny przycisk lub zmiana wyboru odwołuje. Projekt (folder) znika tylko pusty, więc jedno złe naciśnięcie
  nie skasuje całej pracy.
- 2026-11-14: Rozdział 12 „List i Dictionary” (11 przykładów) i przy okazji `string.Join`: kompilator zamienia go na
  pętlę z ukrytymi zmiennymi, więc każdy element zamienia się w tekst tak jak w `"" + x` (także przez `ToString`
  obiektu), bez nowej instrukcji VM. Wyszło, że usuwanie z listy w `foreach` nie daje błędu jak w C# – w „Braki”.
- 2026-11-14: Zaznaczanie i schowek w edytorze: X przełącza tryb zaznaczania, w którym D-pad zaznacza, a pasek
  narzędzi zmienia się w Copy / Cut / Paste / All. Schowek jest wspólny dla plików, więc można przenieść kod między
  plikami projektu.
- 2026-11-14: Rozdział 13 „Lambdy” – 8 sprawdzanych przykładów: od `x => x * 2` przez `Find`/`RemoveAll`/`Sort`
  na listach po licznik pamiętający zmienną i zdarzenia z `+=`/`-=`. Wszystko działało bez zmian w języku.
- 2026-11-14: Rozdział 14 „enum i struct” – 7 przykładów; najważniejsze porównanie: struktura kopiowana przy
  przypisaniu obok klasy dzielonej przez dwie zmienne.
- 2026-11-14: Lista plików ma czwarty przycisk „Delete” dla rysika – działa jak SELECT: pierwsze dotknięcie pyta
  („Delete?” na czerwono), drugie usuwa.
- 2026-11-14: Help na konsoli: `app/help.c` czyta rozdziały `sdk/docs` w tym samym małym Markdownie, zawija akapity
  do szerokości ekranu, pokazuje kod i wynik na ciemnym tle, a dopiski „Dla zaawansowanych” z paskiem. Rozdziały jadą
  w `.3dsx` jako romfs, więc Help działa bez kopiowania plików na kartę (R w menu).
- 2026-11-14: Help łączy rozdziały z karty i wbudowane (pytanie użytkownika): wcześniej jeden rozdział na karcie
  chował wszystkie wbudowane. Teraz plik z karty zastępuje wbudowany o tej samej nazwie, a nowe pliki się dokładają.
- 2026-11-14: Zmiana listy w jej własnym `foreach` daje błąd jak w C#: lista ma licznik zmian, pętla zapamiętuje go
  na starcie i porównuje przed każdym krokiem (jedno porównanie na obrót). Wyjście z pętli zaraz po zmianie (`break`)
  i pętla po kopii dalej działają. Rozdział 12 opisuje błąd i trzy wyjścia z sytuacji.
- 2026-11-14: Help otwiera się też z edytora – piąty przycisk na pasku narzędzi; edytor nie zna Help, woła tylko
  `EditorHost.help`, a host 3DS uruchamia tę samą pętlę Help co z menu, bez przełączania ekranów.
- 2026-11-14: Rozdział 15 „Przestrzenie nazw” (5 przykładów). Przy okazji: ostrzeżenie gcc ARM w `compiler.c` (kopiowanie
  nazw hooków) usunięte – build 3DS jest bez ostrzeżeń; wyszło też, że alias `using G = Gra;` jest po cichu pomijany.
- 2026-11-14: Aliasy `using G = Gra;` (wcześniej po cichu pomijane): alias zastępuje pierwszy człon nazwy przy szukaniu
  klasy, więc działa w typach, `new`, wywołaniach statycznych i enumach bez osobnego kodu w każdym miejscu.
- 2026-11-14: Karty w edytorze: wszystkie pliki projektu są otwarte naraz (L + X / L + Y), podpowiedzi widzą
  niezapisane zmiany w innych kartach, Run zapisuje wszystko, a błąd kompilacji w innym pliku przełącza na jego kartę
  i linię. Pasek tytułu pokazuje karty z gwiazdką przy zmienionych.
- 2026-11-14: Przycisk „Copy” w Help: przykład widoczny na ekranie trafia do schowka edytora, więc można go wkleić
  (X, Paste) i od razu uruchomić. Help nie zna edytora – dostaje tylko funkcję kopiowania od hosta.
- 2026-11-14: `foreach` po słowniku: błąd tylko przy dodaniu nowego klucza – sprawdzone w źródłach .NET, że od
  .NET Core 3.0 `Remove`, `Clear` i nadpisanie wartości w trakcie pętli są dozwolone, więc u nas też.
- 2026-11-14: Siatka znaków pod `Console` (`lib/textgrid.c`): sama logika, bez rysowania – platforma czyta komórki.
  Pełny wiersz zawija się dopiero przy następnym znaku, więc 50 znaków i `\n` na ekranie 50×30 to jedna linia, nie dwie.
- 2026-11-14: `Console.Clear`, `SetCursorPosition`, `ResetColor` na siatce znaków; host PC dostał `--grid 20x6`, który na
  końcu wypisuje zawartość siatki – tak testujemy programy rysujące tekstem (ramki, plansze roguelike'a).
- 2026-11-14: Kolory konsoli jak w C# (`Console.ForegroundColor = ConsoleColor.Red`). Zamiast dokładać do kompilatora
  „właściwości funkcji wbudowanych”, klasa ze źródła o nazwie wbudowanej klasy ją rozszerza – właściwości i enum są
  napisane w MiniSharpie, a błąd w nich wskazuje linię użytkownika, nie linię ukrytego źródła.
- 2026-11-14: Program uruchomiony na 3DS (A w menu, Run w edytorze) pisze do siatki `Console` na górnym ekranie,
  z kolorami, a `Console.ReadLine` wreszcie działa poza terminalem: pole i klawiatura na dolnym ekranie. Ekran odświeża
  się co 50 ms podczas pisania i co klatkę w pętli `Update`; konsola tekstowa libctru zostaje tylko dla testów (X).
- 2026-11-14: Buildy z Actions da się rozróżnić: plik w wydaniu nazywa się `MiniSharp-<nr przebiegu>.3dsx`, a ten sam
  numer widać w Homebrew Launcherze i w menu.
- 2026-11-14: Etap 3, grafika 2D: API `Gfx`/`Input`/`Time`/`App` napisane raz w `lib/gfx.c`, platforma daje tylko kilka
  funkcji rysujących. Host PC rysuje w pamięci i umie „nacisnąć” przyciski w wybranych klatkach, więc gry da się
  testować w CI po skrócie obrazu. Pięć testów czekających od początku na Gfx/Input/App wreszcie przechodzi.
- 2026-11-14: Obrazki: `Gfx.LoadImage` + `Image`/`ImagePart`/`ImageRot` z odbiciem, skalą i przezroczystością; PNG dekoduje
  `stb_image`. Na 3DS tekstury układam ręcznie w kafelki 8×8 (porządek Mortona, wiersze od dołu) – do sprawdzenia na
  konsoli. Przy okazji wyszło, że program z samym `Draw()` nie dostawał klatek, a test grafiki porównywał czarny ekran.
- 2026-11-14: Rozdział 16 „Pierwsza gra”. Sprawdzając grę z rozdziału zrzutem ekranu z hosta PC wyszło, że po
  `Gfx.Screen(Screen.Bottom)` następna klatka rysowała dalej na dolnym ekranie – teraz każda klatka zaczyna od górnego.
- 2026-11-14: Rozdział 17 „Obrazki i sprite'y”; gra z monetami z rozdziału sprawdzona na hoście PC z wygenerowanymi
  obrazkami (zrzut ekranu: kręcące się monety, odbity gracz, licznik).
- 2026-11-14: Dwie gry przykładowe (Gwiazdki, Monety) w aplikacji – kopiują się na kartę przy pierwszym starcie, więc
  grafikę i obrazki można sprawdzić na konsoli od razu: B w menu, folder projektu, Run.
- 2026-11-14: Ekrany logiczne (`Gfx.Layout`) i stałe tempo gry: `Update` 60 razy na sekundę na każdym urządzeniu,
  `Draw` co odświeżenie albo świadomie rzadziej (`Gfx.FrameRate(30)`). Rytm klatek przyciąga się do vsync, żeby
  drgania zegara nie dawały na przemian 0 i 2 kroków.
- 2026-11-14: Edytor: wielkość kodu 9–24 px (A-/A+, L + A / L + B) i schematy kolorów z plików `themes/*.txt` (Col
  przełącza; trzy wbudowane). Ustawienia zostają w `editor.txt`. Nowy rozdział 18 opisuje aplikację i format schematu.
- 2026-11-14: `Time.Millis()` to teraz prawdziwy czas (zegar urządzenia), także bez klatek – benchmark w `Main` na 3DS
  pokazywał same zera, bo liczył zegar gry. Aplikacja na 3DS ma własny folder `sdmc:/minisharp2` (stary przemianowany
  na `oldMinisharp`), a każde uruchomienie pisze osobny log `logs/<program>_<czas>.txt` z pamięcią przed / po.
- 2026-11-14: Graficzne menu (`app/menu.c`): kafelki na dotyk, ostatnie pliki z edytora, opis i linia stanu na górnym
  ekranie. Host 3DS nie przełącza już ekranów między konsolą tekstową a citro2d (poza testami X).
- 2026-11-14: Dwa błędy obrazków na konsoli: tekstura 3DS była wgrywana z odwróconymi wierszami (każdy obrazek do góry
  nogami), a ujemna skala (`Gfx.Image(img, x, y, -3)`) odbijała też w pionie. Teraz minus odbija tylko w poziomie,
  jak opisuje rozdział 17; test `gfx_image` ma nowy hash.
- 2026-11-14: Odbity obrazek przeskakiwał o swoją szerokość: biblioteka przesuwała go w prawo, a backendy i tak rysują
  ujemną szerokość od `x`. Teraz `x` to zawsze lewy brzeg, odbity czy nie (sprawdzone na pikselach z `--ppm`).
- 2026-11-14: Osobny plan silnika izometrycznego (PLAN-ISO.md): moduł iso na bazie unity-iso-tools i starego `iso.c`,
  scena z hierarchią, prefaby, skrypty `IsoBehaviour` z inspektorem, fizyka lite i edytor map w menu hosta. Do języka
  dojdzie refleksja (podzbiór `System.Reflection`); modele 3D i pełna fizyka odłożone.
- 2026-11-14: PLAN-ISO dopracowany: każda mapa to niezależna scena, obiekty z prefabu albo jednorazowe, flagi
  Statyczny/Ruchomy, Widoczny, Aktywny, lista skryptów w inspektorze. Tabela „co złe w Unity” (wg Hipple'a): typowane
  odwołania zamiast GetComponent, sygnały zamiast singletonów, stała kolejność Update, testy bez okna. Podłoga sortowana, w prototypie sprawdzimy scalanie kafelków.
- 2026-11-14: Refleksja po stronie C (PLAN-ISO I2a): kompilator zostawia w programie opis klas – pola z typami,
  konstruktor bez parametrów, metody – a host może z nazwy klasy zrobić obiekt, ustawić mu pola z kontrolą typu, zawołać
  jego `Update` i trzymać go poza stosem bez obawy o GC. Na tym stanie loader sceny i inspektor; test `test_reflect`.
- 2026-11-14: Moduły w C (PLAN-ISO I0): moduł rejestruje funkcje, stałe i klasy C# pod jedną nazwą, a program włącza go
  zdaniem `using Iso;`. Bez tego klas modułu nie ma, a kompilator podpowiada, czego brakuje. Folder `modules/` wchodzi do
  buildów PC i 3DS sam; na razie bez żadnego modułu – pierwszy będzie iso.
