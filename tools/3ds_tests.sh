#!/bin/sh
# Testy języka na 3DS (konsola albo emulator Azahar), porównane z tests/expected/ tak jak w run_tests.sh.
#   sh tools/3ds_tests.sh prepare <sd>   kopiuje testy do <sd>/minisharp/tests/ (+ plik "auto": aplikacja sama je uruchomi)
#   sh tools/3ds_tests.sh fetch <dir>     pobiera wyniki z konsoli (FTP 192.168.1.90:5000) do <dir>/minisharp/tests/out/
#   sh tools/3ds_tests.sh compare <sd>   porównuje <sd>/minisharp/tests/out/*.txt z oczekiwanymi wynikami
# <sd> = katalog karty: emulator – "$APPDATA/Azahar/sdmc"; konsola – skopiuj przez FTP albo podaj zamontowaną kartę.
# Na 3DS: MiniSharp.3dsx z Homebrew Launchera, X = testy (albo plik auto – testy od razu przy starcie).
cd "$(dirname "$0")/.."
cmd=$1; sd=$2
[ -n "$sd" ] || { echo "usage: $0 prepare|compare <sd-root>"; exit 2; }
T="$sd/minisharp/tests"

if [ "$cmd" = prepare ]; then
    rm -rf "$T/out"; mkdir -p "$T/errors" "$T/projects"
    cp tests/*.cs tests/opcje.txt "$T/"
    cp tests/errors/*.cs "$T/errors/"
    cp -r tests/projects/* "$T/projects/"
    : > "$T/auto"
    echo "testy skopiowane do $T"
    exit 0
fi
if [ "$cmd" = fetch ]; then     # pobiera /minisharp/tests/out/ z konsoli przez FTP do <sd>/minisharp/tests/out/
    FTP=${FTP:-ftp://192.168.1.90:5000}
    rm -rf "$T/out"; mkdir -p "$T/out"
    curl -sS --connect-timeout 5 --list-only "$FTP/minisharp/tests/out/" | tr -d '\r' | while read -r p; do
        curl -sS -o "$T/out/$(basename "$p")" "$FTP/minisharp/tests/out/$(basename "$p")"
    done
    echo "pobrano $(ls "$T/out" | wc -l) wynikow do $T/out"
    exit 0
fi
[ "$cmd" = compare ] || { echo "unknown command $cmd"; exit 2; }

O="$T/out"
pass=0; fail=0; wait=0; miss=0
waiting() { grep -q "^$1[[:space:]]" tests/czeka.txt; }
check() { # nazwa, plik wyniku, plik oczekiwany, filtr (all|err)
    if [ ! -f "$2" ]; then miss=$((miss+1)); echo "BRAK: $1"; return; fi
    if [ "$4" = err ]; then got=$(grep -E '^(compile|runtime) error:|^cannot read' "$2" | tr -d '\r')
    else got=$(tr -d '\r' < "$2" | sed 's|sdmc:/minisharp/||g'); fi    # ścieżki na karcie jak na PC
    if [ "$got" = "$(cat "$3")" ]; then pass=$((pass+1))
    elif waiting "$1"; then wait=$((wait+1))
    else fail=$((fail+1)); echo "FAIL: $1"; diff "$3" "$2" | head -6; fi
}
for f in tests/*.cs; do
    n=$(basename "$f" .cs); [ -f "tests/expected/$n.txt" ] || continue
    case $n in rt_*) check "$n" "$O/$n.txt" "tests/expected/$n.txt" err ;;
               *)    check "$n" "$O/$n.txt" "tests/expected/$n.txt" all ;; esac
done
for d in tests/projects/*/; do
    n=$(basename "$d"); [ -f "tests/expected/$n.txt" ] || continue
    check "$n" "$O/$n.txt" "tests/expected/$n.txt" all
done
for f in tests/errors/*.cs; do
    n=$(basename "$f" .cs)
    check "error/$n" "$O/error_$n.txt" "tests/errors/$n.expected" err
done
echo "3DS: passed: $pass, failed: $fail, waiting: $wait, missing: $miss"
[ $fail -eq 0 ] && [ $miss -eq 0 ]
