#!/bin/sh
# Uruchamia testy i porównuje wynik z zapisanym oczekiwanym (tests/expected/*.txt, tests/errors/*.expected).
# Wołany przez `make test` (zmienna BIN = ścieżka binarki testowej). Ostatnia linia: "passed: N, failed: M, waiting: K" (K = testy z tests/czeka.txt).
cd "$(dirname "$0")/.."
BIN=${BIN:-build/minisharp}
[ -f "$BIN" ] || [ -f "$BIN.exe" ] || BIN=
pass=0; fail=0

wait=0
# tests/czeka.txt: testy czekające na brakującą część biblioteki – ich porażka nie psuje wyniku
waiting() { grep -q "^$1[[:space:]]" tests/czeka.txt; }
check() { # nazwa, wynik, plik z oczekiwanym wynikiem
    if [ "$2" = "$(cat "$3")" ]; then
        pass=$((pass+1))
        if waiting "$1"; then fail=$((fail+1)); echo "FAIL: $1 juz przechodzi - usun go z tests/czeka.txt"; fi
    elif waiting "$1"; then wait=$((wait+1))
    else fail=$((fail+1)); echo "FAIL: $1"; fi
}

# Testy jednostkowe w C (zmienna UNIT = lista binarek z build/unit): każdy wypisuje swoje FAIL-e, kod 0 = ok.
for u in $UNIT; do
    if r=$("$u" 2>&1); then pass=$((pass+1)); else code=$?; fail=$((fail+1)); echo "FAIL: unit/$(basename "$u") (exit code $code)"; echo "$r"; fi
done

if [ -z "$BIN" ]; then
    echo "Brak binarki testowej (nie ma jeszcze hosta platform/pc z main) - 0 testow."
else
    # Programy z tests/*.cs, które mają plik tests/expected/<nazwa>.txt; argumenty z tests/opcje.txt, rt_* = tylko stderr.
    for f in tests/*.cs; do
        [ -f "$f" ] || continue
        n=$(basename "$f" .cs)
        [ -f "tests/expected/$n.txt" ] || continue
        args=$(sed -n "s/^$n //p" tests/opcje.txt)    # np. --frames 40
        in=/dev/null; [ -f "tests/input/$n.txt" ] && in="tests/input/$n.txt"   # odpowiedzi dla Console.ReadLine
        case $n in
            rt_*) check "$n" "$("$BIN" "$f" $args <"$in" 2>&1 >/dev/null)" "tests/expected/$n.txt" ;;   # błąd wykonania: tylko stderr
            *)    check "$n" "$("$BIN" "$f" $args <"$in" 2>&1)" "tests/expected/$n.txt" ;;
        esac
    done
    # Projekty: tests/projects/<nazwa>/ (folder = wszystkie jego .cs + biblioteki z uses.txt / 'using X;') z plikiem
    # tests/expected/<nazwa>.txt; stdout i stderr razem. Foldery bez expected to biblioteki używane przez projekty.
    for d in tests/projects/*/; do
        n=$(basename "$d")
        [ -f "tests/expected/$n.txt" ] || continue
        args=$(sed -n "s/^$n //p" tests/opcje.txt)
        check "$n" "$("$BIN" "tests/projects/$n" $args </dev/null 2>&1)" "tests/expected/$n.txt"
    done
    # Podpowiedzi edytora (codeindex): tests/ci/<nazwa>/ to projekt ze znacznikami /*?*/; --ci wypisuje, co edytor
    # podpowie w każdym znaczniku (typ przed kropką i pierwsze 8 nazw) = tests/expected/<nazwa>.txt
    for d in tests/ci/*/; do
        n=$(basename "$d")
        check "$n" "$("$BIN" --ci "tests/ci/$n" 2>&1)" "tests/expected/$n.txt"
    done
    # Printer AST: program wydrukowany przez --print działa tak samo jak oryginał (print/<nazwa>; bez rt_*,
    # bo ich komunikaty mają numery linii, które po wydruku są inne)
    rm -rf build/print; mkdir -p build/print
    [ -d tests/assets ] && cp -r tests/assets build/print/    # obrazki testów (Gfx.LoadImage szuka obok programu)
    for f in tests/*.cs; do
        n=$(basename "$f" .cs)
        [ -f "tests/expected/$n.txt" ] || continue
        case $n in rt_*) continue ;; esac
        args=$(sed -n "s/^$n //p" tests/opcje.txt)
        "$BIN" --print "$f" > "build/print/$n.cs" 2>&1
        in=/dev/null; [ -f "tests/input/$n.txt" ] && in="tests/input/$n.txt"
        check "print/$n" "$("$BIN" "build/print/$n.cs" $args <"$in" 2>&1)" "tests/expected/$n.txt"
    done
    # Terminal: tests/repl.txt podane na stdin hosta --repl (echo wejścia po "> ", wyniki, błędy, #complete)
    check repl "$("$BIN" --repl < tests/repl.txt 2>&1)" tests/expected/repl.txt
    # Przykłady z dokumentacji (sdk/docs/*.md): blok ```cs = cały program; po nim ```wynik = oczekiwane wyjście.
    # Przykład bez bloku wynik musi się przynajmniej skompilować. Blok ```wejscie (między cs a wynik) = linie wpisane
    # przez użytkownika dla Console.ReadLine; host z --echo wypisuje je w wyniku jak konsola.
    rm -rf build/doc; mkdir -p build/doc
    for md in sdk/docs/[0-9]*.md; do
        [ -f "$md" ] || continue
        awk -v out="build/doc/$(basename "$md" .md)" '
            /^```cs[[:space:]]*$/    { n++; f = out "_" n ".cs";  printf "" > f; inb = 1; next }
            /^```wejscie[[:space:]]*$/ { f = out "_" n ".in"; printf "" > f; inb = 1; next }
            /^```wynik[[:space:]]*$/ { f = out "_" n ".txt"; printf "" > f; inb = 1; next }
            /^```/ && inb            { inb = 0; close(f); next }
            inb                      { sub(/$/, ""); print > f }' "$md"
    done
    for f in build/doc/*.cs; do
        [ -f "$f" ] || continue
        n=${f%.cs}
        if [ -f "$n.in" ]; then check "doc/$(basename "$n")" "$("$BIN" "$f" --echo <"$n.in" 2>&1)" "$n.txt"
        elif [ -f "$n.txt" ]; then check "doc/$(basename "$n")" "$("$BIN" "$f" </dev/null 2>&1)" "$n.txt"
        elif r=$("$BIN" "$f" --check 2>&1); then pass=$((pass+1)); else fail=$((fail+1)); echo "FAIL: doc/$(basename "$n"): $r"; fi
    done
    # Błędy kompilacji: tests/errors/<nazwa>.cs -> komunikat na stderr = tests/errors/<nazwa>.expected
    for f in tests/errors/*.cs; do
        [ -f "$f" ] || continue
        n=$(basename "$f" .cs)
        check "error/$n" "$("$BIN" "$f" 2>&1 >/dev/null)" "tests/errors/$n.expected"
    done
fi

echo "passed: $pass, failed: $fail, waiting: $wait"
[ $fail -eq 0 ]
