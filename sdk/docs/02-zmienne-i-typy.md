# Zmienne i typy

Zmienna to pudełko z nazwą, w którym program trzyma wartość: liczbę punktów, imię gracza, czy drzwi są otwarte.
Każda zmienna ma **typ** – mówi on, co wolno do niej włożyć.

## Pierwsza zmienna

```cs
class Program
{
    static void Main()
    {
        int punkty = 10;
        Console.WriteLine(punkty);
        punkty = punkty + 5;
        Console.WriteLine("Masz " + punkty + " punktów");
    }
}
```

```wynik
10
Masz 15 punktów
```

- `int punkty = 10;` – tworzy zmienną typu `int` (liczba całkowita) o nazwie `punkty` i wkłada do niej `10`.
- `punkty = punkty + 5;` – bierze starą wartość, dodaje 5 i wkłada wynik z powrotem. Znak `=` znaczy „włóż”,
  a nie „równa się”.
- `"Masz " + punkty` – tekst sklejony z liczbą daje tekst.

Nazwa zmiennej zaczyna się od litery albo `_`, dalej mogą być litery, cyfry i `_`. Wielkość liter ma znaczenie:
`punkty` i `Punkty` to dwie różne zmienne.

## Podstawowe typy

```cs
class Program
{
    static void Main()
    {
        int zycie = 3;              // liczba całkowita
        float predkosc = 2.5f;      // liczba z ułamkiem (f na końcu)
        bool otwarte = true;        // prawda albo fałsz
        char litera = 'A';          // jeden znak, w apostrofach
        string imie = "Ala";        // tekst, w cudzysłowie

        Console.WriteLine(zycie);
        Console.WriteLine(predkosc);
        Console.WriteLine(otwarte);
        Console.WriteLine(litera);
        Console.WriteLine(imie);
    }
}
```

```wynik
3
2.5
True
A
Ala
```

- `int` – liczby całkowite: `-7`, `0`, `1000`.
- `float` – liczby z ułamkiem. Ułamek piszemy z kropką, a na końcu dodajemy `f`: `0.5f`, `3f`.
- `bool` – tylko `true` (prawda) albo `false` (fałsz). Przyda się w `if` (zob. rozdział 4).
- `char` – jeden znak w apostrofach: `'x'`.
- `string` – tekst w cudzysłowie: `"Cześć"`. Może być pusty: `""`.

## var – niech kompilator zgadnie

Gdy od razu dajesz wartość, zamiast typu możesz napisać `var`. Typ wynika z wartości.

```cs
class Program
{
    static void Main()
    {
        var x = 5;          // int
        var y = 1.5f;       // float
        var napis = "hej";  // string
        Console.WriteLine(x + y);
        Console.WriteLine(napis + "!");
    }
}
```

```wynik
6.5
hej!
```

Zmienna z `var` nadal ma jeden typ na zawsze – do `x` nie włożysz potem tekstu.

## Typ pilnuje porządku

Do zmiennej `int` nie włożysz tekstu. Taki program się nie skompiluje:

```text
int zycie = "dużo";
compile error: line 5: variable initializer: cannot convert string to int
```

Dzięki temu wiele pomyłek widać od razu, zanim program ruszy.

## Liczby całkowite i ułamki

Dzielenie dwóch liczb `int` daje `int` – ułamek jest obcinany. Żeby dostać ułamek, choć jedna liczba musi być `float`.

```cs
class Program
{
    static void Main()
    {
        Console.WriteLine(7 / 2);
        Console.WriteLine(7 % 2);
        Console.WriteLine(7f / 2);

        float f = 3.9f;
        int i = (int)f;         // obcina ułamek
        float g = i;            // int do float: bez problemu
        Console.WriteLine(i);
        Console.WriteLine(g);
    }
}
```

```wynik
3
1
3.5
3
3
```

- `7 % 2` – reszta z dzielenia (tu `1`).
- `int` można włożyć do `float` bez niczego. W drugą stronę trzeba napisać `(int)` – to **rzutowanie**. Ułamek
  przepada (`3.9f` daje `3`), nie ma zaokrąglania.

## Zamiana tekstu na liczbę i z powrotem

```cs
class Program
{
    static void Main()
    {
        string s = "42";
        int n = int.Parse(s);
        Console.WriteLine(n + 1);

        float f = float.Parse("1.25");
        Console.WriteLine(f * 2);

        string t = n.ToString();
        Console.WriteLine(t + "!");
        Console.WriteLine(t.Length);
    }
}
```

```wynik
43
2.5
42!
2
```

`int.Parse` i `float.Parse` czytają liczbę z tekstu, `ToString()` robi z liczby tekst. `"42" + 1` dałoby `"421"`
(sklejanie tekstu), a `int.Parse("42") + 1` daje `43`.

## Stałe i wartości graniczne

```cs
class Program
{
    const int MaxZycie = 5;

    static void Main()
    {
        Console.WriteLine(MaxZycie);
        Console.WriteLine(int.MaxValue);
        Console.WriteLine(int.MinValue);
    }
}
```

```wynik
5
2147483647
-2147483648
```

`const` to wartość, której nie wolno zmienić – dobra na ustawienia gry. `int` mieści liczby od `int.MinValue`
do `int.MaxValue`.

> **Dla zaawansowanych:** `int` ma zawsze 32 bity, a przepełnienie zawija się jak w C#
> (`int.MaxValue + 1` daje `int.MinValue`) – tak samo na 3DS, w przeglądarce i na PC. `float` to 32-bitowa liczba
> zmiennoprzecinkowa (około 7 cyfr dokładności), więc długie rachunki na ułamkach mogą minimalnie odbiegać od wyniku „na kartce” –
> ułamków nie porównuj przez `==`, tylko sprawdzaj, czy różnica jest bardzo mała.
> Rzutowanie `(int)` za dużego `float` daje `int.MaxValue` albo `int.MinValue` zamiast przypadkowej liczby.
