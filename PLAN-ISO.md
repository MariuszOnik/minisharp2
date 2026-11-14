# Plan: silnik izometryczny i edytor map (moduł `iso`)

**Osobny plan projektu iso** (decyzja użytkownika 2026-11-14). Główny plan i zasady pracy: [PLAN.md](PLAN.md) –
obowiązują tu tak samo (jeden punkt na sesję, testy na PC, DEVLOG, rozdział dokumentacji, commit + push).
Ten plik jest źródłem prawdy o stanie prac nad iso; PLAN.md tylko do niego odsyła.

## Cel

Moduł `modules/iso` (C) + aplikacja w menu hosta: **edytor map izometrycznych** z **narzędziem do prefabów**,
**hierarchią sceny** jak w Unity i **skryptami MiniSharp przypisanymi do obiektów** (inspektor). Wzorzec: Isometric
2.5D Toolset (unity-iso-tools, Matvey Cherevko, MIT) – bierzemy pomysły i algorytm sortowania (z podaniem autora),
nie kod edytora Unity.

## Zakres (decyzje użytkownika 2026-11-14)

- **Tak:** sortowanie 2D (obiekty wielokafelkowe, własny kąt / ratio / wysokość kafelka), przeliczenia ekran ↔ iso,
  dotyk/mysz → kafelek, scena z hierarchią, prefaby, skrypty `IsoBehaviour`, **refleksja w języku**, **fizyka lite**
  (proste kolizje prostopadłościanów + prosta grawitacja), wyzwalacze, raycast, edytor map, narzędzie do prefabów.
- **Nie (na razie):** modele 3D na mapie iso, pełna fizyka (masa, odbicia, stosy brył, kule, kapsuły), dowolne obroty
  (tylko 4 kierunki / odbicie), przenikające się obiekty (jak u Matta: niedozwolone).

## Materiały

| Co | Gdzie |
|---|---|
| unity-iso-tools (wzorzec, MIT) | `IsometricLib/unity-iso-tools/Assets/IsoTools` (`Scripts/IsoWorld.cs`, `Internal/IsoScreenSolver.cs`, `IsoSortingSolver.cs`, `Editor/IsoObjectEditor.cs`, `IsoEditorUtils.cs`, `Addons/Physics`) |
| Prototyp C# (stres test, szybkie sortowanie) | `IsometricLib/Gra.cs`, `IsometricLib/IsoSortowanieSzybkie.cs` |
| Stary moduł C | stare repo, gałąź `toee-3d`: `src/iso.c` + `iso.h` (373 linie, sortowanie z kubełkami, zależności nieruchomych liczone raz), `games/IsoBench`, `games/IsoBenchC` |
| Sprajty testowe | `IsometricLib/kenney_isometric-buildings.zip` (CC0) |

## Model (decyzje użytkownika 2026-11-14 + szkic)

- **Każda mapa to scena, każda scena jest niezależna** (własny plik, własny IsoWorld; nic nie przechodzi między
  scenami po cichu – zob. „Wspólne dane” niżej).
- **Współrzędne** w kafelkach (float), jak u Matta: `x`, `y` po podłodze, `z` w górę. Ekran = „piksele bazowe”
  (y w dół), kamera dokłada przesunięcie i zoom.
- **IsoWorld** – korzeń mapy: `TileSize`, `TileRatio`, `TileAngle`, `TileHeight`, `Origin` (przesunięcie całej mapy).
- **Węzeł sceny** – nazwa, rodzic, dzieci, pozycja **względem rodzica**. Węzeł bez bryły to grupa (folder).
- **IsoObject ma stały zestaw sekcji wbudowanych w C**, nie dowolne komponenty: Transform (pozycja, rozmiar bryły),
  Wygląd (warstwy sprajtów: obraz, przesunięcie, odbicie), Kolizja (bryła albo własny prostopadłościan, wyzwalacz,
  warstwa), Flagi, **Skrypty** (lista klas `IsoBehaviour` z wartościami pól). Otwarte na rozszerzanie są tylko skrypty.
- **Flagi w inspektorze:**
  - `Statyczny` / `Ruchomy` – statyczny: zależności sortowania liczone raz, kolizja jako przeszkoda; przesunięcie
    statycznego ze skryptu działa, ale przelicza jego zależności (ostrzeżenie w logu).
  - `Widoczny` on/off – niewidoczny nie jest rysowany ani sortowany, ale kolizja, wyzwalacz i skrypty działają
    (obiekty zdarzeń, strefy, punkty startu). W edytorze widać go jako półprzezroczysty szkielet bryły.
  - `Aktywny` on/off – nieaktywny nie istnieje dla gry (bez `Update`, bez kolizji, bez rysowania); dotyczy też dzieci.
  - (tylko edytor) `Zablokowany` – nie da się go zaznaczyć na mapie.
- **Prefab albo obiekt jednorazowy:** obiekt sceny może być w całości opisany w scenie (specjalny, jedyny w swoim
  rodzaju) albo być instancją prefabu z nadpisanymi polami. „Zapisz jako prefab” robi z jednorazowego prefab,
  „Odepnij od prefabu” robi z instancji obiekt jednorazowy. Na start **bez zagnieżdżonych prefabów i wariantów**.
- **Sortowanie zawsze na płaskiej liście** pozycji światowych – hierarchia służy edycji i grupowaniu. Dziecko
  IsoObjectu to albo warstwa wizualna rodzica (piętro, cień, efekt), albo osobny obiekt sortowany niezależnie.
- **Podłoga = zwykłe sortowane obiekty** (decyzja 2026-11-14: bez osobnej warstwy). Do sprawdzenia w prototypie:
  **scalanie** – grupę jednakowych, płaskich kafelków tworzących pełny prostokąt na tej samej wysokości sortujemy jako
  **jedną dużą bryłę** (rysowaną w środku od tyłu do przodu), np. 40 kafelków = 1 obiekt dla sortowania. Działa, gdy
  sprajty nie wystają poza bryłę i nic w nią nie wchodzi; to samo dla rzędu ściany. Flaga grupy `Scal` + licznik par
  przed / po w stres teście.
- **Skrypty** (na start tylko `.cs`, grafy później; kilka skryptów na obiekt, lista w inspektorze z „+ Dodaj skrypt”):
  ```cs
  using Iso;
  class Drzwi : IsoBehaviour {
      public bool Zamkniete = true;          // widoczne i edytowalne w inspektorze
      public override void Start() { }
      public override void Update() { }      // 60 razy na sekundę (pętla hosta)
      public override void OnTap() { }
      public override void OnTriggerEnter(IsoObject kto) { }
  }
  ```

  Pola do inspektora: publiczne pola typów prostych (`int`, `float`, `bool`, `string`, enum), `Image`, prefab oraz
  **odwołania do innych obiektów sceny / ich skryptów** (`public Drzwi cel;` – wybierane w inspektorze).

## Unity: co dobre, co złe, co robimy inaczej

Źródła: unity-iso-tools, wystąpienie Ryana Hipple'a „Game Architecture with Scriptable Objects” (Unite 2017),
doświadczenia z Unity. Zasada Hipple'a: system ma być **modułowy, edytowalny i debugowalny**, a każdą scenę da się
uruchomić i przetestować osobno.

**Dobre – bierzemy:** skrypt przypięty do obiektu, pola edytowalne w inspektorze, prefaby, hierarchia, scena jako
plik, przyciąganie do kratek i obiektów, szkielet bryły na sprajcie.

| Problem w Unity | U nas |
|---|---|
| Ukryte zależności: `GetComponent<X>()`, `Find("Nazwa")`, `[RequireComponent]` – błąd wychodzi dopiero w grze | Odwołania jako **typowane pola ustawiane w inspektorze**; scena przy wczytaniu i edytor sprawdzają brakujące / złego typu i pokazują listę błędów. `Find` jest, ale nie jest głównym sposobem |
| Singletony i menedżery (`GameManager.Instance`) – scena nie działa sama, test wymaga całej gry | **Wspólne dane** jako pliki projektu (pomysł Hipple'a: „zmienne” i „kanały zdarzeń” jako zasoby): np. `Zmienna<int> ZdrowieGracza`, `Sygnał DrzwiOtwarte` – skrypt A wysyła, skrypt B słucha, połączenia w inspektorze. Bez statycznego stanu w skryptach gry |
| Dziesiątki drobnych komponentów zależnych od siebie (Collider wymaga Rigidbody wymaga IsoObject…) | Stały zestaw sekcji w C (Transform, Wygląd, Kolizja, Flagi) – zawsze są, nic nie trzeba dokładać ani szukać |
| Kolejność `Awake`/`Start`/`Update` między obiektami nieokreślona → błędy „raz działa, raz nie” | Kolejność **stała i opisana**: najpierw wszystkie `Start` w kolejności hierarchii, potem `Update` w tej samej kolejności |
| Każdy `Update` kosztuje, nawet pusty | Wołamy tylko metody, które skrypt naprawdę nadpisał (kompilator wie); statyczne obiekty bez skryptów nie są odwiedzane wcale |
| Testy wymagają edytora i trybu Play | Scena i skrypty działają **bez okna** na PC: test wczytuje scenę, robi N kroków po 1/60 s (zegar gry już jest powtarzalny) i sprawdza stan. Skrypt można też utworzyć bez sceny, na obiekcie testowym |
| Zmiana nazwy pola gubi dane w scenach po cichu | Nieznane pole przy wczytaniu = ostrzeżenie, wartość zostaje w pliku (nie ginie przy zapisie) |
| Zagnieżdżone prefaby i warianty – trudne do zrozumienia nadpisania | Jeden poziom: prefab + nadpisane pola; edytor pokazuje, które pola są nadpisane |
| Brak oddzielenia: kod gry zna edytor (`#if UNITY_EDITOR`) | Edytor tylko czyta i zapisuje plik sceny i woła refleksję; skrypty gry nic o nim nie wiedzą |

**Ryzyka u nas:** VM jest wolniejsza od C# w Unity – setki skryptów z `Update` na 3DS trzeba zmierzyć w prototypie;
refleksja to nowy fragment rdzenia języka (testy błędów!); pamięć 3DS przy dużych scenach (liczniki w logu).

## Prototyp (cel najbliższy, decyzja 2026-11-14: „zbudujmy i sprawdźmy”)

Najmniejsza rzecz, która sprawdza cały pomysł, zanim powstaną pełne edytory:
- **P1 – runtime** (I0 + I1 + I2 + najważniejsze z I3): scena z pliku pisanego ręcznie, IsoWorld z przesuwaniem,
  obiekty jednorazowe i z prefabu, flagi `Statyczny/Ruchomy`, `Widoczny`, `Aktywny`, skrypty `IsoBehaviour` z polami
  ustawianymi przez refleksję, `Sygnał` jako wspólne zdarzenie, test bez okna na PC. Stres test na 3DS: liczba
  obiektów, czas sortowania, czas skryptów, pamięć; podłoga zwykła i **scalona** – porównanie.
- **P2 – mini edytor**: kafelek w menu, mapa, paleta prefabów, stawianie / zaznaczanie / przesuwanie z przyciąganiem,
  inspektor (flagi, lista skryptów i ich pola), zapis sceny, „Graj”.
- Po prototypie: wnioski do tego pliku, potem dalsze etapy poniżej.

## Etapy

### I0 – przygotowanie
- [ ] Ustalić, gdzie leżą materiały (`IsometricLib/` jest nieśledzony; zob. „Do decyzji”).
- [ ] Minimalne API modułów z PLAN.md Etap 6 (folder `modules/<nazwa>`, moduł rejestruje swoją tabelę funkcji,
      `api.cs`, testy, włączanie w buildzie, `using Iso;`) – wystarczy tyle, ile potrzebuje iso.

### I1 – rdzeń modułu: sortowanie i przeliczenia
- [ ] Przenieść `iso.c` / `iso.h` z `toee-3d` do `modules/iso` z przeglądem (nagłówek z autorem unity-iso-tools zostaje).
- [ ] Test jednostkowy PC: kolejność z szybkiego sortowania = kolejność z testu wszystkich par (jak „Sprawdź” w `Gra.cs`),
      mapy 8–48 kratek, ruchome obiekty, obiekty wielokafelkowe, zmiana `TileAngle`/`TileRatio`.
- [ ] API MiniSharp: `IsoWorld` (parametry kafelka, `Origin`, `IsoToScreen`, `ScreenToIso(p, z)`, `TouchTile`),
      `IsoObject` (`Position`, `Size`, `TilePosition`, `Moving`), rysowanie posortowanych sprajtów, kamera (przesunięcie, zoom).
- [ ] Przykład / test runtime: stres test w stylu `Gra.cs` (budynki, ludziki, liczniki par i zależności).
- [ ] **Scalanie** grupy płaskich kafelków w jedną bryłę sortowania (sprawdzenie warunków: pełny prostokąt, jedna
      wysokość, sprajty w bryle) + test zgodności kolejności z wersją bez scalania.
- [ ] Rozdział dokumentacji „Świat izometryczny”.

### I2 – refleksja w języku (rdzeń, decyzja użytkownika 2026-11-14: dodajemy)
Po co: plik sceny zapisuje obiekty jako tekst (`"class": "Drzwi"`, `"Zamkniete": true`). Loader musi z nazwy zrobić
obiekt i ustawić pola po nazwie, a inspektor – wypisać klasy dziedziczące po `IsoBehaviour` i ich publiczne pola.

**I2a – po stronie C (potrzebne prototypowi):**
- [x] Zrobione 2026-11-14: kompilator zapisuje w `ClassRT` pełną nazwę (`Gra.Drzwi`), rodzaj, konstruktor bez
      parametrów, pola instancji wg slotów (typ, element / klucz, `T[,]`, dostęp, `readonly`, klasa deklarująca)
      i własne metody. API w `ms.h` (`lang/reflect.c` + `vm.c`): `ms_class_find/name/full_name/kind/base/is/can_create`,
      `ms_field_count/info/find`, `ms_enum_*`, `ms_method_find` (override wygrywa, `owner` mówi, czy skrypt nadpisał
      metodę), `vm_new_object` (inicjalizatory pól + konstruktor), `vm_call_args`, `ms_set_field` (typ sprawdzany jak
      przypisanie w C#: `int` → `float`, struktura kopiowana, klasa pochodna do pola bazowej), `ms_get_field`,
      `ms_vm_keep/kept/release` (obiekty trzymane przez hosta są korzeniami GC). Test `tests/unit/test_reflect.c`.

**I2b – API dla programisty MiniSharp (później, nazwy jak w C#, podzbiór `System.Reflection`):**
- [ ] `obj.GetType()`, `typeof(T)`, `Type.Name`, `Type.FullName`, `Type.BaseType`, `IsSubclassOf`, `Type.GetType("Nazwa")`.
- [ ] `Activator.CreateInstance(type)` (konstruktor bez parametrów).
- [ ] `type.GetFields()` / `GetField("x")` → `FieldInfo` (`Name`, `FieldType`, `IsPublic`, `GetValue`, `SetValue`);
      potem `GetProperties` / `GetMethods` w razie potrzeby.
- [ ] Testy, błędy (nieznana klasa, zły typ w `SetValue`), rozdział dokumentacji „Refleksja”.

### I3 – scena, hierarchia, prefaby, IsoBehaviour (bez edytora)
- [ ] Model sceny w C (węzły, komponenty, pozycje światowe, przesunięcie IsoWorld przesuwa całą mapę) + testy PC.
- [ ] Format pliku `.scene` i `.prefab` (tekst, zob. „Do decyzji”); instancja prefabu = odwołanie + nadpisane pola.
- [ ] Runtime: `Scene.Load`, `Scene.Find`, `Instantiate(prefab, pozycja)`, `Destroy`, aktywowanie / ukrywanie gałęzi.
- [ ] `IsoBehaviour`: `Start`, `Update`, `OnDestroy`, `OnTap`; pola ze sceny ustawiane przez refleksję (I2);
      kolejność wywołań opisana i stała (powtarzalne testy).
- [ ] Rozdział dokumentacji „Sceny i skrypty obiektów”.

### I4 – fizyka lite
- [ ] Kolizje prostopadłościanów w przestrzeni iso (te same kubełki co sortowanie): `Move(dx, dy, dz)` ze ślizganiem
      po ścianach (oś po osi), maski / warstwy kolizji.
- [ ] Prosta grawitacja w Z: spadanie, stanie na podłodze i na obiektach, wysokość stopnia (wchodzenie na schodki), skok.
- [ ] Wyzwalacze: `OnTriggerEnter` / `OnTriggerExit`; `OnCollision` przy zderzeniu ruchomego z przeszkodą.
- [ ] `Raycast` / `BoxCast` po prostopadłościanach i podłodze (np. klik w obiekt, linia wzroku).
- [ ] Siatka chodzenia z mapy + A* (czego Matt nie ma, a przyda się w grach w stylu Fallout / ToEE).
- [ ] Testy PC, rozdział dokumentacji.

### I5 – narzędzie do prefabów (aplikacja)
Wymaga warstwy abstrakcji UI z PLAN.md Etap 4.
- [ ] Kalibracja świata z kafelka podłogi (odczyt `TileSize` / `TileRatio` ze sprajta).
- [ ] Sprajt (albo warstwy: parter, piętra, dach) + nałożony szkielet bryły; zmiana rozmiaru X/Y/Z i punktu
      zaczepienia D-padem / rysikiem; przyciąganie do pół kafelka.
- [ ] Podgląd sortowania: bryła testowa chodzi wokół prefabu.
- [ ] Kolizja (= bryła albo osobny prostopadłościan), wyzwalacz, ruchomy, domyślny skrypt z polami → zapis `.prefab`.

### I6 – edytor map (aplikacja, kafelek w menu hosta)
- [ ] Moduł sam rejestruje kafelek w `app/menu.c` (build bez iso = brak kafelka).
- [ ] Układ 3DS: górny ekran – mapa; dolny – zakładki Hierarchia / Inspektor / Paleta.
- [ ] Widok: przesuwanie, zoom, siatka, bieżący poziom Z, podświetlenie kafelka pod rysikiem.
- [ ] Malowanie podłogi: pędzel, prostokąt, wypełnianie, gumka.
- [ ] Stawianie prefabów, zaznaczanie, przesuwanie z przyciąganiem do kratek i obiektów (jak `IsoEditorUtils`).
- [ ] Hierarchia: drzewo, zmiana rodzica, grupy, ukryj / zablokuj; przesunięcie IsoWorld lub grupy przesuwa wszystko pod nią.
- [ ] Inspektor: transform, sprajt, kolizja, **skrypt** (lista klas `IsoBehaviour` z usługi języka) i jego publiczne pola.
- [ ] Cofnij / ponów, zapis i odczyt `.scene`.
- [ ] **Graj** w edytorze: uruchamia scenę ze skryptami, Stop wraca do edycji w stanie sprzed uruchomienia.
- [ ] Wersja web: ta sama logika przez wasm (po PLAN.md Etap 5b).

### I7 – integracje
- [ ] Węzły modułu iso w edytorze grafu Bolt (z zarejestrowanych funkcji); graf jako skrypt obiektu zamiast `.cs`.
- [ ] Przykładowa mała gra na scenie z edytora (moduł przykładów, nie rdzeń).

## Do decyzji

- **Gdzie trzymać `IsometricLib/`** (projekt Unity + zip Kenneya, ~2 MB): propozycja – poza repo (np.
  `E:\minsharpProject\IsometricLib`) albo w `.gitignore`; do repo tylko sprajty potrzebne testom.
- (rozstrzygnięte 2026-11-14: podłoga to zwykłe sortowane obiekty, do sprawdzenia scalanie w prototypie.)
- **Wspólne dane między scenami** (np. ekwipunek gracza przy przejściu na inną mapę). Propozycja (rozmowa
  2026-11-14): obiekt **`Game`** należący do hosta, żyje przez całą grę – zna listę scen projektu
  (`Game.LoadScene("piwnica")`: stara scena znika, nowa się wczytuje) i trzyma dane przetrwające zmianę mapy jako
  nazwane `Zmienna`/`Sygnał` (podpinane w inspektorze, w teście podmieniane – żeby nie stał się singletonem „na
  wszystko”). Do potwierdzenia: czy to wystarczy, czy potrzebny jest też zapis stanu gry na kartę.
- **Format `.scene` / `.prefab`**: JSON (wspólny `json.c` z edytorem grafu, PLAN.md Etap 5) czy prosty tekst wierszowy.
  Propozycja: JSON.
- **Edytor map a pętla gry**: czy „Graj” uruchamia scenę w tym samym procesie co edytor (szybko), czy jak zwykły program
  z menu (czystsza pamięć).

## Następny krok

Prototyp P1, w tej kolejności:
1. [x] I2a (refleksja po stronie C) – zrobione 2026-11-14.
2. I0: minimalne API modułów, potem I1 (przeniesienie `iso.c` z testem, scalanie).
3. Scena z pliku + flagi + `IsoBehaviour` + `Sygnał` (część I3), test bez okna, stres test na 3DS.
Potem P2 (mini edytor) i dalsze etapy.
