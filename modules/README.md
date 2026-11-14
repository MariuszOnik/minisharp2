# modules/ – moduły w C

Moduł to opcjonalna część napisana w C (np. silnik izometryczny), **wkompilowana** w aplikację (3DS nie ma bibliotek
ładowanych w locie: `.3dsx` to jeden plik; web: jeden `.wasm`). Program włącza moduł zdaniem **`using Nazwa;`**
w dowolnym swoim pliku – do tego czasu klas modułu nie ma, a ich nazwy są wolne. Bez `using` kompilator podpowiada:
`(it is in module Iso: add 'using Iso;')`.

## Układ

```
modules/
  modules.c / modules.h   lista modułów tej aplikacji: modules_register_all() woła rejestrację każdego
  <nazwa>/                folder modułu – Makefile PC i 3DS biorą go same (nazwy plików .c muszą być unikalne
                          w całym projekcie: obiekty 3DS lądują w jednym katalogu)
    <nazwa>.c / .h        kod modułu i funkcja rejestrująca, np. iso_register()
    api.cs                opis API dla Help / podglądu „Idź do definicji” (gdy powstanie)
```

## Rejestracja

```c
static const NativeDef isoNatives[] = { { "IsoWorld", "TileSize", 0, { 0 }, T_FLOAT, w_tile_size }, ... };
static const MsModule isoModule = { "Iso", isoNatives, N, isoConsts, M,
    "public abstract class IsoBehaviour { public virtual void Update() { } }" };   /* klasy C# modułu albo NULL */

void iso_register(void) { ms_register_module(&isoModule); }
```

- `ms_register_module` (lang/ms.h) – struktura i tabele nie są kopiowane (muszą żyć do końca programu).
- Funkcje modułu są w ogólnej tabeli (`ms_native_count`), więc Help i katalog węzłów grafu je widzą;
  kompilator pokazuje je programowi dopiero po `using`.
- Klasy C# modułu (`source`) dochodzą do programu tylko, gdy moduł jest włączony; program może mieć własną klasę
  o tej samej nazwie (wtedy wygrywa jego).
- Stan modułu zerujemy przy starcie każdego programu: `ms_on_vm_new(reset)`.
- `using` w jednym pliku włącza moduł dla całego programu (program poprawny w C# i tak ma `using` w każdym pliku,
  który go potrzebuje). Pełne nazwy bez `using` (`Iso.IsoWorld.X()`) – jeszcze nie.

Test mechanizmu: `tests/unit/test_modules.c`. Plan modułu iso: [PLAN-ISO.md](../PLAN-ISO.md).
