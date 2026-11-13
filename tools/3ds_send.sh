#!/bin/sh
# Wysyła build/MiniSharp.3dsx na konsolę przez FTP (ftpd na 3DS: 192.168.1.90:5000, anonimowo, karta jako /)
# jako /3ds/MiniSharp-RRRRMMDD-GGMM.3dsx – data i godzina w nazwie odróżniają buildy na karcie
# (ta sama data jest w tytule aplikacji w Homebrew Launcherze i w menu). /3ds/MiniSharp.3dsx zostaje nietknięty.
#   sh tools/3ds_send.sh             tylko .3dsx
#   sh tools/3ds_send.sh tests       też testy do /minisharp/tests/ (X w menu uruchamia je na konsoli)
cd "$(dirname "$0")/.."
FTP=${FTP:-ftp://192.168.1.90:5000}
f=build/MiniSharp.3dsx
[ -f "$f" ] || { echo "brak $f - najpierw make 3ds"; exit 2; }
name="MiniSharp-$(date -r "$f" +%Y%m%d-%H%M).3dsx"
curl -sS --connect-timeout 5 -T "$f" "$FTP/3ds/$name" || exit 1
echo "wyslano /3ds/$name"
if [ "$1" = tests ]; then
    tmp=build/sd; rm -rf "$tmp"
    sh tools/3ds_tests.sh prepare "$tmp" > /dev/null && rm -f "$tmp/minisharp/tests/auto"
    (cd "$tmp" && find minisharp -type f) | while read -r p; do
        curl -sS --ftp-create-dirs -T "$tmp/$p" "$FTP/$p" || exit 1
    done && echo "wyslano testy do /minisharp/tests/"
fi
