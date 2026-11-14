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

## Model (szkic)

- **Współrzędne** w kafelkach (float), jak u Matta: `x`, `y` po podłodze, `z` w górę. Ekran = „piksele bazowe”
  (y w dół), kamera dokłada przesunięcie i zoom.
- **IsoWorld** – korzeń mapy: `TileSize`, `TileRatio`, `TileAngle`, `TileHeight`, `Origin` (przesunięcie całej mapy).
- **Węzeł sceny** – nazwa, rodzic, dzieci, pozycja **względem rodzica**, aktywny. Komponenty: `IsoObject` (rozmiar,
  ruchomy), `Sprite` (warstwy: obraz, przesunięcie, odbicie), `Collider` (bryła albo własny prostopadłościan,
  wyzwalacz, maska), `Script` (klasa + wartości pól).
- **Sortowanie zawsze na płaskiej liście** pozycji światowych – hierarchia służy edycji i grupowaniu. Dziecko
  IsoObjectu to albo warstwa wizualna rodzica (piętro, cień, efekt), albo osobny obiekt sortowany niezależnie.
- **Skrypty:**
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
- [ ] Podłoga jako **warstwa kafelków** (tablica, bez sortowania, rysowana pierwsza) – zob. „Do decyzji”.
- [ ] Rozdział dokumentacji „Świat izometryczny”.

### I2 – refleksja w języku (rdzeń, decyzja użytkownika 2026-11-14: dodajemy)
Nazwy jak w C# (`System.Reflection`, podzbiór), żeby program poprawny w C# działał i u nas.
- [ ] `obj.GetType()`, `typeof(T)`, `Type.Name`, `Type.FullName`, `Type.BaseType`, `IsSubclassOf`, `Type.GetType("Nazwa")`.
- [ ] `Activator.CreateInstance(type)` (konstruktor bez parametrów).
- [ ] `type.GetFields()` / `GetField("x")` → `FieldInfo` (`Name`, `FieldType`, `IsPublic`, `GetValue`, `SetValue`);
      potem `GetProperties` / `GetMethods` w razie potrzeby.
- [ ] To samo od strony C (dla sceny i inspektora): utwórz obiekt klasy po nazwie, ustaw/odczytaj pole po nazwie,
      lista klas dziedziczących po danej, lista publicznych pól z typami.
- [ ] Testy (`tests/`), błędy (nieznana klasa, zły typ w `SetValue`), rozdział dokumentacji „Refleksja”.

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
- **Podłoga**: osobna warstwa kafelków (tablica, bez sortowania – oszczędność pamięci i czasu na 3DS; mapa 48×48 to
  2304 obiekty mniej) czy zwykłe IsoObjecty jak w `Gra.cs`. Propozycja: warstwa kafelków + obiekty sortowane.
- **Format `.scene` / `.prefab`**: JSON (wspólny `json.c` z edytorem grafu, PLAN.md Etap 5) czy prosty tekst wierszowy.
  Propozycja: JSON.
- **Edytor map a pętla gry**: czy „Graj” uruchamia scenę w tym samym procesie co edytor (szybko), czy jak zwykły program
  z menu (czystsza pamięć).

## Następny krok

1. I2 (refleksja) – niezależne od reszty, rdzeń języka, można zacząć od razu.
2. I0: minimalne API modułów, potem I1 (przeniesienie `iso.c` z testem).
3. I3 → I4 → I5 / I6 (edytory po warstwie abstrakcji UI z PLAN.md Etap 4).
