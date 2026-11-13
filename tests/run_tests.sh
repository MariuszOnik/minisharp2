#!/bin/sh
# Uruchamia testy i porównuje wynik z zapisanym oczekiwanym (tests/expected/*.txt, tests/errors/*.expected).
# Wołany przez `make test` (zmienna BIN = ścieżka binarki testowej). Ostatnia linia: "passed: N, failed: M".
cd "$(dirname "$0")/.."
BIN=${BIN:-build/minisharp}
[ -f "$BIN" ] || [ -f "$BIN.exe" ] || BIN=
pass=0; fail=0

check() { # nazwa, wynik, plik z oczekiwanym wynikiem
    if [ "$2" = "$(cat "$3")" ]; then pass=$((pass+1)); else fail=$((fail+1)); echo "FAIL: $1"; fi
}

# Testy jednostkowe w C (zmienna UNIT = lista binarek z build/unit): każdy wypisuje swoje FAIL-e, kod 0 = ok.
for u in $UNIT; do
    if r=$("$u" 2>&1); then pass=$((pass+1)); else fail=$((fail+1)); echo "FAIL: unit/$(basename "$u")"; echo "$r"; fi
done

if [ -z "$BIN" ]; then
    echo "Brak binarki testowej (nie ma jeszcze hosta platform/pc z main) - 0 testow."
else
    # Programy z tests/*.cs, które mają plik tests/expected/<nazwa>.txt (bez dodatkowych opcji).
    for f in tests/*.cs; do
        [ -f "$f" ] || continue
        n=$(basename "$f" .cs)
        [ -f "tests/expected/$n.txt" ] || continue
        check "$n" "$("$BIN" "$f" 2>&1)" "tests/expected/$n.txt"
    done
    # Błędy kompilacji: tests/errors/<nazwa>.cs -> komunikat na stderr = tests/errors/<nazwa>.expected
    for f in tests/errors/*.cs; do
        [ -f "$f" ] || continue
        n=$(basename "$f" .cs)
        check "error/$n" "$("$BIN" "$f" 2>&1 >/dev/null)" "tests/errors/$n.expected"
    done
fi

echo "passed: $pass, failed: $fail"
[ $fail -eq 0 ]
