# Tablice

Pięć wyników gracza w pięciu zmiennych `w1`, `w2`… to dużo pisania, a pętla po nich nie przejdzie. Gdy masz wiele
wartości tego samego typu, trzymaj je w **tablicy**: jednej zmiennej z ponumerowanymi przegródkami. W tym rozdziale
utworzysz tablicę, przejdziesz po niej pętlą, policzysz sumę i największą wartość oraz zrobisz planszę z wierszami i kolumnami.

## Pierwsza tablica

```cs
class Program
{
    static void Main()
    {
        int[] wyniki = { 120, 85, 240, 60, 175 };
        Console.WriteLine("Pierwszy: " + wyniki[0]);
        Console.WriteLine("Trzeci: " + wyniki[2]);
        Console.WriteLine("Ile wyników: " + wyniki.Length);
        wyniki[1] = 90;
        Console.WriteLine("Drugi po poprawce: " + wyniki[1]);
    }
}
```

```wynik
Pierwszy: 120
Trzeci: 240
Ile wyników: 5
Drugi po poprawce: 90
```

- `int[]` to typ „tablica liczb `int`”. Nawiasy kwadratowe po typie mówią: wiele takich wartości.
- `{ 120, 85, … }` od razu wypełnia tablicę. Tablica ma tyle przegródek, ile podasz wartości.
- `wyniki[0]` to pierwsza przegródka. **Numeracja zaczyna się od 0**, więc ostatnia ma numer `Length - 1`.
  Numer przegródki nazywa się **indeksem**.
- `wyniki.Length` mówi, ile przegródek ma tablica. Tej liczby nie da się zmienić po utworzeniu tablicy.

## Pusta tablica o danej długości

Gdy wartości jeszcze nie znasz, utwórz tablicę przez `new` i podaj jej długość. Każda przegródka dostaje wartość
domyślną: `0` dla liczb, `false` dla `bool`, `null` dla `string` i obiektów.

```cs
class Program
{
    static void Main()
    {
        int[] kwadraty = new int[6];
        for (int i = 0; i < kwadraty.Length; i++)
        {
            kwadraty[i] = i * i;
        }
        for (int i = 0; i < kwadraty.Length; i++)
        {
            Console.Write(kwadraty[i] + " ");
        }
        Console.WriteLine();

        bool[] odwiedzone = new bool[3];
        Console.WriteLine(odwiedzone[0]);
    }
}
```

```wynik
0 1 4 9 16 25 
False
```

Pętla `for (int i = 0; i < t.Length; i++)` to najczęstszy sposób przejścia po tablicy: `i` przyjmuje kolejno
wszystkie indeksy od `0` do `Length - 1`. Warunek `i < t.Length` (a nie `<=`) pilnuje, żeby nie wyjść za koniec.

Długość może być zmienną, np. `new string[ilu]`. Wypełnić tablicę od razu można też dłuższym zapisem
`new int[] { 1, 2, 3 }` – przydaje się, gdy tablicę podajesz prosto do metody.

## foreach – każda wartość po kolei

Jeśli indeks nie jest potrzebny, a tylko wartości, wygodniej użyć `foreach` (zob. rozdział 5):

```cs
class Program
{
    static void Main()
    {
        string[] druzyna = { "Ala", "Bartek", "Celina" };
        foreach (string imie in druzyna)
        {
            Console.WriteLine("Witaj, " + imie + "!");
        }
    }
}
```

```wynik
Witaj, Ala!
Witaj, Bartek!
Witaj, Celina!
```

Przez zmienną pętli `foreach` nie zmienisz tablicy. Przypisanie `imie = "X";` (a także `+=`, `++`) w takiej pętli
to błąd, tak samo jak w C#:

```text
compile error: line 8: cannot assign to 'imie': it is the variable of a 'foreach' loop (to change the elements, use 'for' with an index)
```

Do zmian użyj `for` z indeksem.

## Suma, średnia, największa wartość

Trzy zadania, które przy tablicach wracają ciągle. Każde to jedna pętla i jedna zmienna na wynik.

```cs
class Program
{
    static void Main()
    {
        int[] punkty = { 7, 12, 3, 18, 9 };

        int suma = 0;
        foreach (int p in punkty)
        {
            suma += p;
        }

        int najwiecej = punkty[0];
        int ktory = 0;
        for (int i = 1; i < punkty.Length; i++)
        {
            if (punkty[i] > najwiecej)
            {
                najwiecej = punkty[i];
                ktory = i;
            }
        }

        Console.WriteLine("Suma: " + suma);
        Console.WriteLine("Średnia: " + (float)suma / punkty.Length);
        Console.WriteLine("Najwięcej: " + najwiecej + " (runda " + (ktory + 1) + ")");
    }
}
```

```wynik
Suma: 49
Średnia: 9.8
Najwięcej: 18 (runda 4)
```

Największą wartość zaczynamy od pierwszego elementu (`punkty[0]`), a nie od zera – gdyby wszystkie liczby były
ujemne, zero byłoby złym wynikiem. Średnia potrzebuje `(float)`, inaczej dzielenie dwóch `int` utnie ułamek
(zob. rozdział 3).

## Wyjście poza tablicę

Indeks spoza zakresu `0 … Length - 1` zatrzymuje program z błędem, który mówi, który indeks był zły i jak długa
jest tablica:

```text
int[] t = new int[3];
t[3] = 1;
```

```text
runtime error: line 2 (Program.Main): index 3 is outside the array (length 3)
```

To najczęstszy błąd przy tablicach. Zwykle winna jest pętla z `<=` zamiast `<` albo liczenie od 1 zamiast od 0.

## Tablica w metodzie

Tablicę można przekazać do metody i z niej oddać. Metoda dostaje **tę samą** tablicę, nie kopię – jeśli zmieni
przegródkę, zmiana jest widoczna u wołającego.

```cs
class Program
{
    static void Podwoj(int[] t)
    {
        for (int i = 0; i < t.Length; i++)
        {
            t[i] *= 2;
        }
    }

    static int[] Odliczanie(int od)
    {
        int[] wynik = new int[od];
        for (int i = 0; i < od; i++)
        {
            wynik[i] = od - i;
        }
        return wynik;
    }

    static void Main()
    {
        int[] liczby = { 1, 2, 3 };
        Podwoj(liczby);
        Console.WriteLine(liczby[0] + " " + liczby[1] + " " + liczby[2]);

        foreach (int x in Odliczanie(4))
        {
            Console.Write(x + "... ");
        }
        Console.WriteLine("start!");
    }
}
```

```wynik
2 4 6
4... 3... 2... 1... start!
```

To samo dzieje się przy zwykłym przypisaniu: po `int[] b = a;` obie nazwy wskazują jedną tablicę. Zmiana `b[0]`
zmienia też `a[0]`.

> **Dla zaawansowanych:** tablica jest typem referencyjnym, jak obiekty klas. Zmienna przechowuje odnośnik do
> tablicy, a nie same wartości. Dlatego `Podwoj` widzi zmiany, choć nie ma `ref` (zob. rozdział 6), a `int[] b = a;`
> nie kopiuje elementów. Kopię zrobisz pętlą do nowej tablicy (`new int[a.Length]`).

## Plansza: wiersze i kolumny

Plansza gry, mapa lochu albo tabela ma wiersze i kolumny. Do tego służy **tablica dwuwymiarowa** `T[,]`: przy
tworzeniu podajesz dwie długości, a przy czytaniu dwa indeksy – najpierw wiersz, potem kolumnę.

```cs
class Program
{
    static void Main()
    {
        char[,] plansza = new char[3, 5];
        for (int y = 0; y < plansza.GetLength(0); y++)
        {
            for (int x = 0; x < plansza.GetLength(1); x++)
            {
                plansza[y, x] = '.';
            }
        }
        plansza[1, 2] = '@';
        plansza[0, 4] = '#';

        for (int y = 0; y < 3; y++)
        {
            for (int x = 0; x < 5; x++)
            {
                Console.Write(plansza[y, x]);
            }
            Console.WriteLine();
        }
        Console.WriteLine("Pól: " + plansza.Length);
    }
}
```

```wynik
....#
..@..
.....
Pól: 15
```

- `new char[3, 5]` – 3 wiersze po 5 kolumn, wszystkie z wartością domyślną.
- `plansza[y, x]` – wiersz `y`, kolumna `x`, oba liczone od zera.
- `GetLength(0)` to liczba wierszy, `GetLength(1)` – kolumn. `Length` to liczba wszystkich pól (tu 15).

Małą tablicę można od razu wypełnić, wiersz po wierszu:

```cs
class Program
{
    static void Main()
    {
        int[,] mapa =
        {
            { 1, 1, 1, 1 },
            { 1, 0, 2, 1 },
            { 1, 1, 1, 1 },
        };
        int sciany = 0;
        foreach (int pole in mapa)
        {
            if (pole == 1) sciany++;
        }
        Console.WriteLine("Ścian: " + sciany + ", skarb w wierszu 1, kolumnie " + 2 + ": " + mapa[1, 2]);
    }
}
```

```wynik
Ścian: 10, skarb w wierszu 1, kolumnie 2: 2
```

Każdy wiersz musi mieć tyle samo wartości. `foreach` przechodzi po wszystkich polach, wiersz po wierszu.
Indeks spoza planszy daje błąd z oboma numerami, np. `index [1, 5] is outside the array [3, 5]`.

> **Dla zaawansowanych:** jest też **tablica tablic** `char[][]` – tablica, której elementami są osobne tablice
> (wiersze). Pisze się wtedy `t[y][x]`, a każdy wiersz trzeba utworzyć osobno (`t[y] = new char[5];`), za to wiersze
> mogą mieć różne długości. Gdy liczba elementów ma się zmieniać w trakcie programu, zamiast tablicy użyj `List`
> (osobny rozdział).

## Podsumowanie

- `int[] t = { 1, 2, 3 };` – tablica z wartościami; `new int[n]` – `n` przegródek z wartością domyślną.
- `t[i]` czyta i zapisuje przegródkę; indeksy od `0` do `t.Length - 1`.
- `for` z indeksem do zmian i do pracy z numerami, `foreach` do samego czytania.
- Tablica przekazana do metody to ta sama tablica, nie kopia.
- Plansza: `char[,] p = new char[wiersze, kolumny];`, pole `p[y, x]`, rozmiary `GetLength(0)` i `GetLength(1)`.
