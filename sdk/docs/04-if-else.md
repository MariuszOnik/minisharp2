# Warunki: if i else

Program nie zawsze robi to samo: gracz ma życie albo nie, klucz jest w plecaku albo go brak. Ten rozdział
pokazuje, jak kazać programowi wybierać: `if`, `else`, porównania, łączenie warunków i `switch`.

## Pierwsze if

```cs
class Program
{
    static void Main()
    {
        int zycie = 0;
        if (zycie <= 0)
        {
            Console.WriteLine("Koniec gry");
        }
        Console.WriteLine("To wypisze się zawsze");
    }
}
```

```wynik
Koniec gry
To wypisze się zawsze
```

- `if (warunek) { ... }` – polecenia w klamrach wykonają się tylko wtedy, gdy warunek jest prawdziwy (`true`).
- Warunek jest zawsze w nawiasach okrągłych i daje `bool` (zob. rozdział 2).
- Po klamrze program idzie dalej normalnie, niezależnie od tego, czy weszło do środka.

## else – w przeciwnym razie

```cs
class Program
{
    static void Main()
    {
        int punkty = 75;
        if (punkty >= 50)
        {
            Console.WriteLine("Zdane!");
        }
        else
        {
            Console.WriteLine("Spróbuj jeszcze raz");
        }
    }
}
```

```wynik
Zdane!
```

`else` to druga droga: wykona się wtedy, gdy warunek był fałszywy. Zawsze wykona się dokładnie jedna z dwóch części.

## else if – kilka możliwości

```cs
class Program
{
    static void Main()
    {
        int punkty = 75;
        if (punkty >= 90)
            Console.WriteLine("Ocena: 5");
        else if (punkty >= 70)
            Console.WriteLine("Ocena: 4");
        else if (punkty >= 50)
            Console.WriteLine("Ocena: 3");
        else
            Console.WriteLine("Ocena: 2");
    }
}
```

```wynik
Ocena: 4
```

- Warunki są sprawdzane **po kolei, od góry**. Wykona się tylko pierwsza pasująca część – `75` pasuje też
  do `>= 50`, ale tam program już nie dochodzi.
- Gdy w środku jest tylko jedno polecenie, klamry można pominąć. Przy dwóch i więcej są konieczne, więc w razie
  wątpliwości lepiej je pisać.

## Porównania

```cs
class Program
{
    static void Main()
    {
        int a = 3;
        int b = 5;
        Console.WriteLine(a == b);
        Console.WriteLine(a != b);
        Console.WriteLine(a < b);
        Console.WriteLine(a >= b);

        string haslo = "smok";
        Console.WriteLine(haslo == "smok");
    }
}
```

```wynik
False
True
True
False
True
```

- `==` równe, `!=` różne, `<` mniejsze, `>` większe, `<=` mniejsze lub równe, `>=` większe lub równe.
- Uwaga: **`=` przypisuje, `==` porównuje**. `if (a = 5)` to błąd – MiniSharp go nie przepuści.
- Napisy też porównuje się przez `==`; wielkość liter ma znaczenie (`"Smok" == "smok"` to `False`).

## I, lub, nie: && || !

```cs
class Program
{
    static void Main()
    {
        bool maKlucz = true;
        int zloto = 20;

        if (maKlucz && zloto >= 10)
            Console.WriteLine("Otwierasz skrzynię i płacisz strażnikowi");

        if (!maKlucz || zloto < 5)
            Console.WriteLine("Nie przejdziesz");
        else
            Console.WriteLine("Droga wolna");
    }
}
```

```wynik
Otwierasz skrzynię i płacisz strażnikowi
Droga wolna
```

- `a && b` („i”) – prawda, gdy **oba** warunki są prawdziwe.
- `a || b` („lub”) – prawda, gdy **choć jeden** jest prawdziwy.
- `!a` („nie”) – odwraca: `!true` to `false`.
- `&&` liczy się przed `||`, tak jak mnożenie przed dodawaniem. Gdy łączysz oba, dodaj nawiasy, żeby było
  widać, o co chodzi: `(a && b) || c`.

> **Dla zaawansowanych:** `&&` i `||` liczą skrótem: gdy lewa strona już rozstrzyga wynik, prawa nie jest
> sprawdzana. Dzięki temu `if (wrog != null && wrog.Zycie > 0)` jest bezpieczne – przy `null` program nie sięgnie
> do `wrog.Zycie`.

## Krótki wybór: ? :

```cs
class Program
{
    static void Main()
    {
        int zycie = 3;
        string stan = zycie > 0 ? "żyje" : "pokonany";
        Console.WriteLine("Bohater " + stan);
        Console.WriteLine(zycie == 1 ? "1 serce" : zycie + " serca");
    }
}
```

```wynik
Bohater żyje
3 serca
```

`warunek ? a : b` daje `a`, gdy warunek jest prawdziwy, a w przeciwnym razie `b`. To skrót od `if`/`else`, gdy
chodzi tylko o wybór **wartości**. Obie możliwości muszą być tego samego typu.

## switch – wybór po wartości

```cs
class Program
{
    static void Main()
    {
        int dzien = 6;
        switch (dzien)
        {
            case 1:
                Console.WriteLine("Poniedziałek");
                break;
            case 6:
            case 7:
                Console.WriteLine("Weekend!");
                break;
            default:
                Console.WriteLine("Zwykły dzień");
                break;
        }

        string polecenie = "idz";
        switch (polecenie)
        {
            case "idz": Console.WriteLine("Idziesz naprzód"); break;
            case "stop": Console.WriteLine("Stoisz"); break;
        }
    }
}
```

```wynik
Weekend!
Idziesz naprzód
```

- `switch (x)` porównuje `x` z kolejnymi `case`. Pasuje liczba, znak, napis albo wartość `enum`.
- Każda część kończy się `break;`. Kilka `case` jedno pod drugim prowadzi do tego samego kodu (`6` i `7`).
- `default:` wykona się, gdy nic nie pasuje. Można go pominąć – wtedy przy braku dopasowania nic się nie dzieje.
- `switch` przydaje się, gdy wybierasz spośród wielu konkretnych wartości; do zakresów (`>= 50`) lepsze jest `if`.

## Zmienne w klamrach

```cs
class Program
{
    static void Main()
    {
        int zloto = 30;
        if (zloto > 10)
        {
            int reszta = zloto - 10;
            Console.WriteLine("Po zakupie zostaje " + reszta);
        }
        // tu nie ma już zmiennej 'reszta'
        Console.WriteLine("Złoto: " + zloto);
    }
}
```

```wynik
Po zakupie zostaje 20
Złoto: 30
```

Zmienna utworzona w klamrach żyje tylko do ich końca. Gdy chcesz jej użyć później, utwórz ją wcześniej, przed `if`.
