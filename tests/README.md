# tests/

- `run_tests.sh` – uruchamiany przez `make test` (Windows: `tools/test.ps1`).
- `<nazwa>.cs` + `expected/<nazwa>.txt` – program i jego oczekiwane wyjście.
- `errors/<nazwa>.cs` + `errors/<nazwa>.expected` – oczekiwany komunikat błędu (stderr).
- `rt_<nazwa>.cs` – błąd wykonania: porównywany jest tylko stderr.
- `opcje.txt` – dodatkowe argumenty hosta dla testu (np. `coroutines --frames 40`).
- `czeka.txt` – testy przeniesione, którym brakuje jeszcze części biblioteki/platformy; ich porażka nie psuje wyniku,
  a gdy zaczną przechodzić, skrypt każe je stamtąd usunąć.
- `projects/<nazwa>/` + `expected/<nazwa>.txt` – projekt (folder); foldery bez `expected` to biblioteki dla `uses.txt` /
  `using X;` (`shared_lib`, `shared_ns`, `autolib/lib`…).
- `unit/*.c` – testy jednostkowe w C (bez hosta).

Źródło: stare repo, `931ad08` (testy przestrzeni nazw `namespaces.cs`, `errors/ns_*` z `a350c5e`). Projekty z `toee-3d` (`project_ns` bez ToEE: wbudowana `Math` zamiast `Item`; bez `project_items` – moduł ToEE).
Jeszcze nieprzeniesione: indeks kodu (`ci*`), terminal (`repl`), wydruk/spany (`print/*`, `spans`) – etap 2; grafika/dźwięk (`media`,
`rt_img*`, `rt_sndbad`, `hello`, `loch_bot`, przykłady) – etap 3; grafy (`graph_*`, `fgraph`) – etap 5.

Foldery `platform/3ds` i `platform/web` powstają dopiero w etapach 3 i 5b – ich istnienie włącza buildy w Actions.
