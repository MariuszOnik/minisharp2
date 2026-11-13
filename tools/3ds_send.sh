#!/bin/sh
# Wysyła build/MiniSharp.3dsx na konsolę przez FTP (ftpd na 3DS: 192.168.1.90:5000, anonimowo, karta jako /)
# jako /3ds/MiniSharp-RRRRMMDD-GGMM.3dsx – data i godzina w nazwie odróżniają buildy na karcie
# (ta sama data jest w tytule aplikacji w Homebrew Launcherze i w menu). /3ds/MiniSharp.3dsx zostaje nietknięty.
#   sh tools/3ds_send.sh             tylko .3dsx
#   sh tools/3ds_send.sh tests       też testy do /minisharp/tests/ (X w menu uruchamia je na konsoli), tylko nowe/zmienione
#   sh tools/3ds_send.sh tests all   wszystkie testy od nowa
cd "$(dirname "$0")/.."
FTP=${FTP:-ftp://192.168.1.90:5000}
f=build/MiniSharp.3dsx
[ -f "$f" ] || { echo "brak $f - najpierw make 3ds"; exit 2; }
name="MiniSharp-$(date -r "$f" +%Y%m%d-%H%M).3dsx"
curl -sS --connect-timeout 5 -T "$f" "$FTP/3ds/$name" || exit 1
echo "wyslano /3ds/$name"
if [ "$1" = tests ]; then
    # Wysyła tylko nowe i zmienione pliki: sumy wysłanych są w build/3ds_sent.md5.
    # "tests all" wysyła wszystko od nowa (np. po wymianie karty albo skasowaniu /minisharp/tests/).
    tmp=build/sd; sent=build/3ds_sent.md5
    [ "$2" = all ] && rm -f "$sent"
    touch "$sent"; rm -rf "$tmp"
    sh tools/3ds_tests.sh prepare "$tmp" > /dev/null && rm -f "$tmp/minisharp/tests/auto"
    (cd "$tmp" && find minisharp -type f | sort | xargs md5sum) > "$sent.new"
    grep -vxFf "$sent" "$sent.new" > "$sent.todo"
    while read -r sum p; do
        p=${p#\*}                    # md5sum na Windows pisze "*ścieżka" (tryb binarny)
        curl -sS --ftp-create-dirs -T "$tmp/$p" "$FTP/$p" || exit 1
        echo "$sum  $p" >> "$sent"   # po każdym udanym pliku – przerwana wysyłka nie zgubi postępu
    done < "$sent.todo"
    echo "testy: wyslano $(wc -l < "$sent.todo") nowych/zmienionych z $(wc -l < "$sent.new") (/minisharp/tests/)"
    mv "$sent.new" "$sent"; rm -f "$sent.todo"
fi
