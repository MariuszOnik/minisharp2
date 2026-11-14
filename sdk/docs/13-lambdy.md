# Lambdy – funkcje w zmiennych

Do tej pory zmienna trzymała liczbę, napis albo obiekt. Może też trzymać **kawałek kodu** – funkcję, którą wywołasz
później. Taka funkcja zapisana w miejscu użycia to **lambda**. Dzięki niej powiesz liście „znajdź pierwszego
potwora, który ma mniej niż 5 życia” jednym wierszem, a przyciskowi „gdy cię naciśnięto, zrób to”.

## Pierwsza lambda

```cs
class Program
{
    static void Main()
    {
        Func<int, int> podwoj = x => x * 2;
        Console.WriteLine(podwoj(5));
        Console.WriteLine(podwoj(21));

        Func<int, int, int> dodaj = (a, b) => a + b;
        Console.WriteLine(dodaj(3, 4));
    }
}
```

```wynik
10
42
7
```

- `x => x * 2` czytaj „x daje x razy 2”. Po lewej strzałki są parametry, po prawej – wynik.
- `Func<int, int>` to typ zmiennej: funkcja, która bierze `int` i oddaje `int`. **Ostatni** typ w nawiasach to
  zawsze typ wyniku: `Func<int, int, int>` bierze dwa `int` i oddaje `int`.
- Kilka parametrów piszesz w nawiasach: `(a, b) => a + b`.
- Zmienną z funkcją wywołujesz jak metodę: `podwoj(5)`.

## Action – funkcja bez wyniku

Gdy funkcja coś **robi**, a niczego nie oddaje, jej typ to `Action`. Dłuższą treść piszesz w klamrach:

```cs
class Program
{
    static void Main()
    {
        Action przywitaj = () => Console.WriteLine("Cześć!");
        Action<string, int> pokaz = (imie, zycie) =>
        {
            Console.WriteLine($"{imie} ma {zycie} życia");
            if (zycie < 3) Console.WriteLine("  uwaga, mało!");
        };

        przywitaj();
        pokaz("Ola", 10);
        pokaz("Kuba", 2);
    }
}
```

```wynik
Cześć!
Ola ma 10 życia
Kuba ma 2 życia
  uwaga, mało!
```

`()` to pusta lista parametrów. `Action<string, int>` bierze napis i liczbę, nic nie oddaje.

## Lambdy i listy

Tu lambdy błyszczą. Lista ma metody, które zamiast wartości biorą **pytanie** w postaci lambdy:

```cs
using System.Collections.Generic;

class Program
{
    static void Main()
    {
        List<int> zycie = new List<int> { 12, 3, 40, 0, 7, 0 };

        int pierwszySlaby = zycie.Find(z => z > 0 && z < 5);
        Console.WriteLine($"Pierwszy słaby: {pierwszySlaby}");

        List<int> silne = zycie.FindAll(z => z >= 10);
        Console.WriteLine($"Silne: {string.Join(", ", silne)}");

        Console.WriteLine($"Ktoś martwy? {zycie.Exists(z => z == 0)}");
        Console.WriteLine($"Wszyscy żywi? {zycie.TrueForAll(z => z > 0)}");

        int usuniete = zycie.RemoveAll(z => z == 0);
        Console.WriteLine($"Usunięto {usuniete}, zostało: {string.Join(", ", zycie)}");

        int suma = 0;
        zycie.ForEach(z => suma += z);
        Console.WriteLine($"Razem: {suma}");
    }
}
```

```wynik
Pierwszy słaby: 3
Silne: 12, 40
Ktoś martwy? True
Wszyscy żywi? False
Usunięto 2, zostało: 12, 3, 40, 7
Razem: 62
```

- `Find` – pierwszy pasujący element (gdy żaden nie pasuje: `0`, `null` albo `false` – wartość domyślna typu).
- `FindAll` – nowa lista wszystkich pasujących.
- `Exists` – czy choć jeden pasuje; `TrueForAll` – czy pasują wszystkie.
- `RemoveAll` – usuwa pasujące i mówi, ile ich było. To wygodniejsze niż pętla od końca z rozdziału 12.
- `ForEach` – robi coś z każdym elementem.

Są też `FindIndex` i `FindLast`, `FindLastIndex` – działają tak samo, tylko oddają miejsce albo szukają od końca.

## Sortowanie po swojemu

`Sort()` bez niczego ustawia liczby rosnąco, a napisy alfabetycznie. Lista obiektów nie wie sama, jak się ułożyć –
podaj jej lambdę, która porównuje dwa elementy. Lambda oddaje liczbę **ujemną**, gdy `a` ma być przed `b`,
**dodatnią**, gdy po `b`, i `0`, gdy są równe. Najprościej: odejmij.

```cs
using System.Collections.Generic;

class Gracz
{
    public string Imie;
    public int Punkty;
    public Gracz(string imie, int punkty) { Imie = imie; Punkty = punkty; }
}

class Program
{
    static void Main()
    {
        List<Gracz> ranking = new List<Gracz>
        {
            new Gracz("Ola", 120), new Gracz("Kuba", 300), new Gracz("Ela", 75)
        };

        ranking.Sort((a, b) => b.Punkty - a.Punkty);      // od największej liczby punktów
        foreach (Gracz g in ranking) Console.WriteLine($"{g.Imie}: {g.Punkty}");

        List<int> liczby = new List<int> { 5, 1, 4 };
        liczby.Sort((a, b) => b - a);                       // malejąco
        Console.WriteLine(string.Join(" ", liczby));
    }
}
```

```wynik
Kuba: 300
Ola: 120
Ela: 75
5 4 1
```

## Lambda pamięta zmienne

Lambda widzi zmienne z miejsca, w którym ją napisano – i może je zmieniać. Zmienna żyje tak długo jak lambda,
nawet gdy metoda, która ją utworzyła, dawno się skończyła:

```cs
class Program
{
    static Func<int> Licznik()
    {
        int n = 0;
        return () => { n++; return n; };
    }

    static void Main()
    {
        Func<int> a = Licznik();
        Func<int> b = Licznik();
        Console.WriteLine(a());
        Console.WriteLine(a());
        Console.WriteLine(a());
        Console.WriteLine(b());
    }
}
```

```wynik
1
2
3
1
```

Każde wywołanie `Licznik()` robi nową zmienną `n`, więc `a` i `b` liczą osobno.

## Metoda jako wartość

Zamiast lambdy możesz podać nazwę zwykłej metody o pasującej postaci:

```cs
using System.Collections.Generic;

class Program
{
    static bool Parzysta(int x) { return x % 2 == 0; }

    static int Zastosuj(Func<int, int> f, int x) { return f(x); }

    static int Kwadrat(int x) { return x * x; }

    static void Main()
    {
        List<int> l = new List<int> { 1, 2, 3, 4, 5, 6 };
        Console.WriteLine(string.Join(" ", l.FindAll(Parzysta)));
        Console.WriteLine(Zastosuj(Kwadrat, 9));
        Console.WriteLine(Zastosuj(x => x + 100, 9));
    }
}
```

```wynik
2 4 6
81
109
```

Metoda `Zastosuj` przyjmuje funkcję jako parametr – nie wie, co ona robi, po prostu ją woła.

## Zdarzenia: += i -=

Do zmiennej typu `Action` można dopisać **kilka** funkcji przez `+=`. Wywołanie zmiennej woła je wszystkie po kolei.
Tak robi się zdarzenia: „gdy potwór zginie, daj punkty, zagraj dźwięk i wypisz komunikat”.

```cs
class Potwor
{
    public string Nazwa;
    public Action<Potwor> PrzySmierci;

    public Potwor(string nazwa) { Nazwa = nazwa; }

    public void Zgin()
    {
        Console.WriteLine($"{Nazwa} pada");
        if (PrzySmierci != null) PrzySmierci(this);
    }
}

class Program
{
    static int punkty = 0;

    static void DajPunkty(Potwor p) { punkty += 10; }

    static void Main()
    {
        Potwor smok = new Potwor("smok");
        smok.PrzySmierci += DajPunkty;
        smok.PrzySmierci += p => Console.WriteLine($"Pokonano: {p.Nazwa}!");
        smok.Zgin();
        Console.WriteLine($"Punkty: {punkty}");

        smok.PrzySmierci -= DajPunkty;
        smok.Zgin();
        Console.WriteLine($"Punkty: {punkty}");
    }
}
```

```wynik
smok pada
Pokonano: smok!
Punkty: 10
smok pada
Pokonano: smok!
Punkty: 10
```

`-=` odpina funkcję. Sprawdzenie `!= null` jest potrzebne: gdy nic nie jest podpięte, zmienna jest pusta.

## Ściąga typów

- `Action` – bez parametrów, bez wyniku; `Action<T>` – jeden parametr; `Action<T1, T2>` – dwa…
- `Func<W>` – bez parametrów, wynik `W`; `Func<T, W>` – parametr `T`, wynik `W`…
- `Predicate<T>` – to samo co `Func<T, bool>`: pytanie „tak czy nie?” o element.
- `Comparison<T>` – to samo co `Func<T, T, int>`: porównanie dwóch elementów do sortowania.

> **Dla zaawansowanych:** zmienna lokalna, której używa lambda, trafia do osobnej „komórki” w pamięci, wspólnej dla
> metody i lambdy – dlatego obie widzą zmiany. W pętli `foreach` zmienna jest nowa w każdym obrocie (jak w C#),
> więc lambdy zapisane w pętli pamiętają każda swój element. Lambda w `Sort`, `Find` i innych metodach listy
> nie może wołać `Console.ReadLine()` – czytanie od użytkownika działa tylko w zwykłym kodzie (zob. rozdział 9).
