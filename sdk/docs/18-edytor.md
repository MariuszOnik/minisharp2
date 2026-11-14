# Edytor na konsoli

Ten rozdział opisuje samą aplikację na 3DS: jak pisać, uruchamiać i ustawiać edytor pod siebie.

## Menu i lista plików

- **A** w menu uruchamia `main.cs`, **B** otwiera listę programów i projektów, **Y** terminal, **R** ten Help.
- Na liście: **A** albo dwa dotknięcia otwierają, **X** nowy plik, **Y** nowy projekt, **SELECT** dwa razy albo przycisk
  „Delete” usuwa (folder projektu tylko pusty).
- Projekt to folder: wszystkie jego pliki `.cs` kompilują się razem i otwierają się w edytorze jako **karty**.

## Przyciski w edytorze

- **Krzyżak** – kursor; **L + krzyżak** – początek i koniec linii, strona w górę i w dół.
- **B** – kasuje znak; **Y** – cofnij; **R** – wstaw pierwszą podpowiedź.
- **A** – uruchom (najpierw zapisuje wszystkie zmienione karty); **SELECT** – zapisz; **START** – wyjdź.
- **X** – tryb zaznaczania: krzyżak zaznacza, a przyciski u góry to Copy, Cut, Paste, All, Done.
- **L + X** / **L + Y** – następna / poprzednia karta projektu.
- **L + A** / **L + B** – większy / mniejszy kod.

Przyciski na dolnym ekranie: Save, Run, Undo, Redo, Help, a obok podpowiedzi małe **A-**, **A+** (wielkość kodu)
i **Col** (kolejny schemat kolorów).

## Wielkość kodu

Kod może mieć od 9 do 24 pikseli (na początku 13). Większy łatwiej czytać, mniejszy mieści więcej linii. Edytor pamięta
wybór w pliku `editor.txt` w folderze `minisharp` na karcie.

## Własne kolory

Schematy kolorów leżą w folderze `minisharp/themes` na karcie: na początek `ciemny`, `jasny` i `fosfor`. **Col** przełącza
je po kolei. Własny schemat to zwykły plik tekstowy, np. `themes/moj.txt`:

```text
# Mój schemat – linie zaczynające się od # to komentarze
background  202030
text        E0E0E0
keyword     FF9040
type        40C0FF
string      A0E060
number      FFD060
comment     708090
```

Każda linia to nazwa i kolor w zapisie `RRGGBB` (jak w programach graficznych, może być z `#` na początku). Czego
w pliku nie ma, zostaje w kolorze domyślnym. Wszystkie nazwy: `background`, `text`, `keyword` (słowa jak `if`, `class`),
`type` (`int`, nazwy klas), `string` (napisy), `number`, `comment`, `linenumber`, `linenumberactive` (numer linii
z kursorem), `currentline` (tło tej linii), `selection`, `cursor`, `errorline` (linia z błędem), `titlebar`.

Plik możesz przygotować na komputerze albo telefonie i wgrać na kartę (np. przez FTP), a potem wybrać go przyciskiem
**Col**.
