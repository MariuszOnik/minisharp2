# MiniSharp2 – instrukcja dla Claude

Użytkownik pisze po polsku; odpowiadaj po polsku. Pracuje w krótkich sesjach (nowy czat na sesję).

## Na start każdej sesji

1. Przeczytaj [PLAN.md](PLAN.md) – jedyne źródło prawdy o stanie prac. Weź **jeden** punkt z „Następny krok”.
2. Przy przenoszeniu kodu zajrzyj do [docs/PRZEGLAD.md](docs/PRZEGLAD.md) (werdykt dla każdego starego pliku)
   i [docs/ARCHITEKTURA.md](docs/ARCHITEKTURA.md) (warstwy, moduły, układ folderów).
3. Na koniec: odhacz punkt w PLAN.md, dopisz 1–3 zdania do „Dziennika”, commit.
4. **Każdy commit od razu wypychaj** (`git push`) na `origin` = https://github.com/MariuszOnik/minisharp2.git.
   GitHub Actions (`.github/workflows/build.yml`) buduje testy PC, `.3dsx` i `.apk`; po pushu sprawdź, czy przeszło.

## Czym jest ten projekt

Przebudowa języka MiniSharp (C#-podobny, kompilator + VM w C, działa na Nintendo 3DS i w przeglądarce przez wasm)
w **rdzeń niezależny od gier**. Stare repo to archiwum tylko do odczytu: kod przenosimy z niego plik po pliku,
z przeglądem i razem z testami.

Decyzje użytkownika (szczegóły w PLAN.md): platformy 3DS i web, PC tylko do testów; edytor kodu zostaje (jako czysty
edytor tekstu + „usługa języka”); **edytor grafu „Bolt jak w Unity”** (graf jest źródłem, kod generowany:
graf → AST → `printer.c`); terminal (REPL) zostaje; logika (podpowiedzi, graf) tylko w C – web woła ją przez wasm;
gry (ToEE, Fallout) to moduły w swoich repozytoriach, **nic z gier w rdzeniu**.

## Gdzie co leży

| Co | Gdzie |
|---|---|
| Stare repo MiniSharp (archiwum, nie zmieniać) | `D:\Programowanie\FalloutDS\minisharp` (gałęzie `toee-3d`, `jezyk-v4`, `main`; baza przenoszenia `931ad08`) |
| Stara kopia v1 (bez wartości) | `E:\minsharpProject\minisharp\minisharp` |
| Projekty gier na MiniSharpie (wstrzymane) | `E:\minsharpProject` (plan D&D w `PLAN.md`, eksport ToEE w `toee-models`) |
| Konsola 3DS | FTP `192.168.1.90:5000` (ftpd, anonimowo, karta jako `/`); programy w `/minisharp/`, log `/minisharp/log.txt` |
| Buildy testowe na konsolę | `/3ds/MiniSharp-*.3dsx` – **nie nadpisywać `/3ds/MiniSharp.3dsx` bez pytania** |
| Emulator | Azahar: `D:\Programowanie\azahar-windows-mxe-2127.0-alpha3\azahar-windows-mxe-2127.0-alpha3` |

## Budowanie (Windows)

- **3DS:** `C:\devkitPro\msys2\usr\bin\bash.exe -lc "cd <repo>; export DEVKITPRO=/opt/devkitpro DEVKITARM=/opt/devkitpro/devkitARM; make"`
- **PC (testy):** `powershell -File tools	est.ps1` (= `make test`, pełny log w `build	est.log`). Ręcznie: gcc z `C:\msys64\mingw64\bin`, uruchamiać z **PowerShell** z `$env:TMP = "$env:USERPROFILE\AppData\Local\Temp"`
  (w Git Bash gcc nie widzi katalogu tymczasowego i pada na `C:\windows\`).
- **Web:** Zig przez pip (`python -m ziglang`), `wasm32-wasi` – zob. stare `web/build.sh`.
- **Android:** APK = wersja web w WebView; folder `android/` + `android/prepare-assets.sh` (wzór: stare
  `minisharp-apk-files.zip` i `.github/workflows/apk-z-zip.yml`), budowane tylko w Actions.
- **Umowa z CI** (workflow pomija część, której jeszcze nie ma): `make test` – testy PC (etap 1);
  `make 3ds` → `build/MiniSharp.3dsx` (gdy jest `platform/3ds`); APK gdy są `android/` i `platform/web`.

## Pułapki

- Pliki `.cs` dla MiniSharp zapisuj **bez BOM** (kompilator go nie przyjmuje; PowerShell `Set-Content -Encoding utf8` dodaje BOM).
- Polskie znaki w skryptach Pythona podawanych heredokiem przez Bash potrafią się zepsuć – dłuższe skrypty zapisuj do
  pliku (narzędziem Write) i uruchamiaj `python -X utf8 plik.py`.
- Wyniki testów zapisuj do plików, w czacie tylko podsumowanie.
