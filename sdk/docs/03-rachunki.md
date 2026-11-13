# Rachunki

Gry ciągle liczą: punkty, życie, pozycję na ekranie, czas. Ten rozdział pokazuje działania na liczbach,
skróty w rodzaju `+=` i `++` oraz gotowe funkcje z `Math`, `Mathf` i `Random`.

## Cztery działania i reszta

```cs
class Program
{
    static void Main()
    {
        int a = 17;
        int b = 5;
        Console.WriteLine(a + b);
        Console.WriteLine(a - b);
        Console.WriteLine(a * b);
        Console.WriteLine(a / b);
        Console.WriteLine(a % b);
        Console.WriteLine(-a);
    }
}
```

```wynik
22
12
85
3
2
-17
```

- `+ - *` działają jak na lekcji matematyki.
- `/` na dwóch `int` daje wynik bez ułamka: `17 / 5` to `3` (zob. rozdział 2).
- `%` to **reszta z dzielenia**: `17 % 5` to `2`. Przydaje się, gdy coś ma się powtarzać: `licznik % 2 == 0`
  znaczy „liczba parzysta”.

## Kolejność działań i nawiasy

Mnożenie i dzielenie idą przed dodawaniem i odejmowaniem, jak w matematyce. Nawiasy zmieniają kolejność.

```cs
class Program
{
    static void Main()
    {
        Console.WriteLine(2 + 3 * 4);
        Console.WriteLine((2 + 3) * 4);
        Console.WriteLine(10 - 4 - 3);
        Console.WriteLine(100 / 10 / 5);
    }
}
```

```wynik
14
20
3
2
```

Działania tej samej rangi liczą się od lewej: `10 - 4 - 3` to `(10 - 4) - 3`. Gdy nie masz pewności, dodaj
nawiasy – nic nie kosztują, a kod łatwiej czytać.

## Skróty: += i ++

Zmiana zmiennej o jakąś wartość zdarza się tak często, że ma krótszy zapis.

```cs
class Program
{
    static void Main()
    {
        int zloto = 10;
        zloto += 5;         // to samo co zloto = zloto + 5
        Console.WriteLine(zloto);
        zloto -= 3;
        Console.WriteLine(zloto);
        zloto *= 2;
        Console.WriteLine(zloto);
        zloto /= 4;
        Console.WriteLine(zloto);

        int tura = 1;
        tura++;             // o jeden więcej
        tura++;
        Console.WriteLine(tura);
        tura--;             // o jeden mniej
        Console.WriteLine(tura);
    }
}
```

```wynik
15
12
24
6
3
2
```

Są też `%=` i skróty dla tekstu: `napis += "!"` dokleja wykrzyknik na końcu.

> **Dla zaawansowanych:** `x++` zwraca starą wartość, a `++x` nową. `int a = 5; int b = a++;` daje `b = 5`,
> `a = 6`, a `int c = ++a;` daje `c = 7`. Najczytelniej używać `++` jako osobnego polecenia.

## Rachunki na ułamkach

Gdy choć jedna liczba jest `float`, wynik też jest `float`.

```cs
class Program
{
    static void Main()
    {
        float predkosc = 2.5f;
        float czas = 4;
        Console.WriteLine(predkosc * czas);
        Console.WriteLine(10 / 4f);
        Console.WriteLine(1 / 3f);

        int punkty = 7;
        int gracze = 2;
        float srednia = (float)punkty / gracze;
        Console.WriteLine(srednia);
    }
}
```

```wynik
10
2.5
0.3333333
3.5
```

`(float)punkty / gracze` – najpierw zamieniamy `punkty` na `float`, dopiero potem dzielimy, więc ułamek nie przepada.
Bez `(float)` wyszłoby `3`.

## Math i Mathf – gotowe funkcje

```cs
class Program
{
    static void Main()
    {
        Console.WriteLine(Math.Abs(-8));
        Console.WriteLine(Math.Max(3, 9));
        Console.WriteLine(Math.Min(3, 9));
        Console.WriteLine(Math.Clamp(150, 0, 100));
        Console.WriteLine(Math.Sqrt(16));
        Console.WriteLine(Math.Pow(2, 10));
        Console.WriteLine(Mathf.RoundToInt(2.6f));
        Console.WriteLine(Mathf.FloorToInt(2.6f));
        Console.WriteLine(Mathf.Lerp(0, 10, 0.25f));
    }
}
```

```wynik
8
9
3
100
4
1024
3
2
2.5
```

- `Abs` – wartość bez minusa. `Max`, `Min` – większa, mniejsza z dwóch.
- `Clamp(x, od, do)` – przycina liczbę do przedziału: życie nie spadnie poniżej 0 i nie przekroczy maksimum.
- `Sqrt` – pierwiastek, `Pow(a, b)` – potęga.
- `Mathf` to wersje dla `float` znane z Unity: `RoundToInt` zaokrągla, `FloorToInt` obcina w dół,
  `Lerp(a, b, t)` daje punkt między `a` i `b` (`t = 0` to `a`, `t = 1` to `b`) – przydatne do płynnego ruchu.
- Są też `Math.PI`, `Sin`, `Cos`, `Atan` i inne – pełna lista w ściądze API.

## Losowanie

```cs
class Program
{
    static void Main()
    {
        int kostka = Random.Range(1, 7);    // 1, 2, 3, 4, 5 albo 6
        Console.WriteLine(kostka >= 1 && kostka <= 6);

        float szansa = Random.Float();      // od 0 do prawie 1
        Console.WriteLine(szansa >= 0 && szansa < 1);
    }
}
```

```wynik
True
True
```

- `Random.Range(od, do)` losuje liczbę całkowitą od `od` do `do - 1`. Druga liczba **nie** wchodzi do losowania,
  dlatego kostka to `Random.Range(1, 7)`.
- `Random.Float()` losuje ułamek od `0` do prawie `1`. Szansa 30%: `if (Random.Float() < 0.3f)`.
- `Random.Next(n)` to skrót od `Random.Range(0, n)`.

> **Dla zaawansowanych:** losowanie startuje od zegara, więc każde uruchomienie daje inne liczby.
> `Random.Seed(123)` ustawia start na stałe – ten sam seed daje zawsze ten sam ciąg liczb, na każdej platformie.
> Przydaje się w testach i przy generowaniu poziomów z „kodu mapy”.
