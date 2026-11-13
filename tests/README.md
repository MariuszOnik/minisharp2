# tests/

- `run_tests.sh` – uruchamiany przez `make test` (Windows: `tools/test.ps1`).
- `<nazwa>.cs` + `expected/<nazwa>.txt` – program i jego oczekiwane wyjście.
- `errors/<nazwa>.cs` + `errors/<nazwa>.expected` – oczekiwany komunikat błędu (stderr).

Foldery `platform/3ds` i `platform/web` powstają dopiero w etapach 3 i 5b – ich istnienie włącza buildy w Actions.
