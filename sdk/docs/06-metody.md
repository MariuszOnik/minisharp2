# Metody

Gdy ten sam kawałek kodu jest potrzebny w kilku miejscach, nie kopiuj go. Nadaj mu nazwę i wołaj po nazwie. Taki
nazwany kawałek kodu to **metoda**. Znasz już jedną: `Main`, od której zaczyna się każdy program. W tym rozdziale
napiszesz własne. Poznasz parametry, `return`, metody o tej samej nazwie i metodę, która woła samą siebie.

## Pierwsza własna metoda

```cs
class Program
{
    static void Przywitaj()
    {
        Console.WriteLine("Witaj, wędrowcze!");
        Console.WriteLine("Rozgość się przy ognisku.");
    }

    static void Main()
    {
        Przywitaj();
        Console.WriteLine("...");
        Przywitaj();
    }
}
```

```wynik
Witaj, wędrowcze!
Rozgość się przy ognisku.
...
Witaj, wędrowcze!
Rozgość się przy ognisku.
```

Metodę piszemy w klasie, obok `Main`, a nie w środku `Main`. Kolejność nie ma znaczenia: `Przywitaj` mogłaby stać
pod `Main`. Rozbierzmy pierwszą linię:

- `static` – na razie piszemy go przed każdą metodą. Co znaczy, dowiesz się w rozdziale o klasach.
- `void` – metoda niczego nie oddaje (o oddawaniu wyniku za chwilę).
- `Przywitaj` – nazwa. Zwyczaj: zaczyna się wielką literą i mówi, co metoda robi.
- `()` – tu będą parametry. Ta metoda ich nie ma.

`Przywitaj();` w `Main` to **wywołanie**: program skacze do metody, wykonuje ją całą i wraca do następnej linii.

## Parametry – dane dla metody

Metoda może dostać wartości z zewnątrz. W nawiasie piszesz typ i nazwę każdego parametru, rozdzielone przecinkami.

```cs
class Program
{
    static void Pasek(string nazwa, int ile)
    {
        Console.Write(nazwa + ": ");
        for (int i = 0; i < ile; i++)
        {
            Console.Write("#");
        }
        Console.WriteLine(" " + ile);
    }

    static void Main()
    {
        Pasek("Życie", 8);
        Pasek("Mana", 3);
        int zloto = 5;
        Pasek("Złoto", zloto * 2);
    }
}
```

```wynik
Życie: ######## 8
Mana: ### 3
Złoto: ########## 10
```

Przy wywołaniu podajesz **argumenty** w tej samej kolejności i tych samych typach co parametry. Argumentem może być
liczba, zmienna albo całe wyrażenie (`zloto * 2`). Zostanie policzone, zanim metoda ruszy.

Wewnątrz metody parametr to zwykła zmienna. Gdy metoda zmieni swój parametr, wołający tego nie zobaczy, bo metoda
dostała **kopię** wartości:

```cs
class Program
{
    static void Ulecz(int hp)
    {
        hp += 50;
        Console.WriteLine("W metodzie: " + hp);
    }

    static void Main()
    {
        int hp = 10;
        Ulecz(hp);
        Console.WriteLine("W Main: " + hp);
    }
}
```

```wynik
W metodzie: 60
W Main: 10
```

To, że obie zmienne nazywają się `hp`, nic nie zmienia. To dwie różne zmienne. Żeby wynik wrócił do `Main`, metoda
musi go **oddać**.

## return – metoda oddaje wynik

Zamiast `void` piszesz typ wyniku, a w środku `return` z wartością:

```cs
class Program
{
    static int Ulecz(int hp, int ile)
    {
        int nowe = hp + ile;
        if (nowe > 100)
        {
            nowe = 100;
        }
        return nowe;
    }

    static void Main()
    {
        int hp = 70;
        hp = Ulecz(hp, 50);
        Console.WriteLine("Życie: " + hp);
        Console.WriteLine("Po drugiej miksturze: " + Ulecz(hp, 50));
    }
}
```

```wynik
Życie: 100
Po drugiej miksturze: 100
```

Wywołanie metody, która coś oddaje, jest **wartością**. Można je przypisać (`hp = Ulecz(...)`), dodać do napisu albo
podać jako argument innej metody.

`return` od razu kończy metodę. Kod pod nim już się nie wykona. Dzięki temu można wyjść wcześniej:

```cs
class Program
{
    static string Ocena(int punkty)
    {
        if (punkty >= 90) return "celująco";
        if (punkty >= 50) return "dobrze";
        return "spróbuj jeszcze raz";
    }

    static void Main()
    {
        Console.WriteLine(Ocena(95));
        Console.WriteLine(Ocena(60));
        Console.WriteLine(Ocena(10));
    }
}
```

```wynik
celująco
dobrze
spróbuj jeszcze raz
```

Metoda z typem wyniku musi oddać wartość na każdej drodze. Bez ostatniego `return` program zatrzyma się przy małej
liczbie punktów z błędem `method ended without returning a value`, bo nie byłoby czego oddać. W metodzie `void` można napisać samo `return;`, żeby wyjść wcześniej.

## Krótka metoda ze strzałką

Gdy cała metoda to jedno wyrażenie, można pominąć klamry i `return` i napisać `=>`:

```cs
class Program
{
    static int Kwadrat(int x) => x * x;
    static bool Parzysta(int x) => x % 2 == 0;

    static void Main()
    {
        Console.WriteLine(Kwadrat(7));
        Console.WriteLine(Parzysta(7));
    }
}
```

```wynik
49
False
```

`static int Kwadrat(int x) => x * x;` znaczy dokładnie to samo co `static int Kwadrat(int x) { return x * x; }`.

## Ta sama nazwa, różne parametry

Kilka metod może mieć tę samą nazwę, jeśli różnią się parametrami (liczbą albo typami). Kompilator wybiera tę, która
pasuje do argumentów. To się nazywa **przeciążanie**.

```cs
class Program
{
    static int Wieksza(int a, int b) => a > b ? a : b;
    static int Wieksza(int a, int b, int c) => Wieksza(Wieksza(a, b), c);
    static float Wieksza(float a, float b) => a > b ? a : b;

    static void Main()
    {
        Console.WriteLine(Wieksza(3, 8));
        Console.WriteLine(Wieksza(3, 8, 5));
        Console.WriteLine(Wieksza(2.5f, 1.5f));
    }
}
```

```wynik
8
8
2.5
```

Znasz to już z `Console.WriteLine`. Przyjmuje napis, liczbę albo nic, bo ma kilka wersji.

## Metoda woła samą siebie

Metoda może wywołać samą siebie. Taka metoda jest **rekurencyjna**. Musi mieć warunek, przy którym przestaje
się wołać, inaczej wołałaby się bez końca.

```cs
class Program
{
    static int Silnia(int n)
    {
        if (n <= 1) return 1;          // tu rekurencja się kończy
        return n * Silnia(n - 1);
    }

    static void Odliczaj(int n)
    {
        if (n == 0)
        {
            Console.WriteLine("Start!");
            return;
        }
        Console.Write(n + " ");
        Odliczaj(n - 1);
    }

    static void Main()
    {
        Console.WriteLine("5! = " + Silnia(5));
        Odliczaj(3);
    }
}
```

```wynik
5! = 120
3 2 1 Start!
```

`Silnia(5)` to `5 * Silnia(4)`, czyli `5 * 4 * Silnia(3)` i tak dalej aż do `Silnia(1)`, która oddaje 1 bez
dalszego wołania. To samo da się zrobić pętlą (zob. rozdział 5). Rekurencja przydaje się przy danych, które
rozgałęziają się jak drzewo, na przykład przy przeszukiwaniu labiryntu.

## Zmienne metody są tylko jej

Zmienna utworzona w metodzie (także parametr) istnieje tylko podczas jednego wywołania tej metody. Inne metody jej nie
widzą, a przy następnym wywołaniu zaczyna od nowa. Dlatego dwie metody mogą mieć zmienne o tej samej nazwie i nie
przeszkadzają sobie. Żeby kilka metod dzieliło jedną wartość, potrzebne jest pole klasy. Pola poznasz w rozdziale
o klasach.

## Dobre metody

- Jedna metoda, jedno zadanie. Nazwa mówi, co robi: `Ulecz`, `Ocena`, `RysujMape`.
- Jeśli ten sam kod pojawia się drugi raz, to dobry kandydat na metodę.
- Długi `Main` podziel na kilka metod o czytelnych nazwach. Wtedy `Main` czyta się jak spis treści.

> **Dla zaawansowanych:** w MiniSharpie metody nie mają jeszcze parametrów z wartością domyślną
> (`int b = 10`), `ref` ani `out`, więc kompilator zgłosi błąd. `out` jest na razie
> tylko w `TryGetValue` słownika. Zamiast kilku wyników przez `out` oddaj jeden obiekt albo strukturę (zob. rozdział
> o klasach). Tablice i obiekty są przekazywane **przez referencję**: metoda dostaje kopię adresu, nie kopię danych,
> więc gdy zmieni element tablicy, wołający tę zmianę zobaczy:

```cs
class Program
{
    static void Podwoj(int[] t)
    {
        for (int i = 0; i < t.Length; i++) t[i] *= 2;
    }

    static void Main()
    {
        int[] hp = { 10, 25, 40 };
        Podwoj(hp);
        Console.WriteLine(hp[0] + " " + hp[1] + " " + hp[2]);
    }
}
```

```wynik
20 50 80
```

> `int[] hp = { 10, 25, 40 };` to skrót od `new int[] { 10, 25, 40 }`. Działa tylko w deklaracji zmiennej, pola
> albo właściwości z podanym typem tablicy; przy `var` i w zwykłym wyrażeniu (np. argument `Podwoj({ 1, 2 })`)
> trzeba napisać pełne `new int[] { 1, 2 }`, tak jak w C#.
