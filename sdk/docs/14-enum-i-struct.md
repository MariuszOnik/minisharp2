# enum i struct

Ten rozdział pokazuje dwa małe, ale bardzo przydatne rodzaje typów. **enum** to lista nazwanych możliwości:
kierunki, stany potwora, rodzaje przedmiotów. **struct** to mała paczka danych, która – inaczej niż obiekt klasy –
kopiuje się przy każdym przypisaniu, jak liczba.

## Pierwszy enum

```cs
enum Kierunek { Gora, Dol, Lewo, Prawo }

class Program
{
    static void Main()
    {
        Kierunek k = Kierunek.Lewo;
        Console.WriteLine(k);

        if (k == Kierunek.Lewo) Console.WriteLine("idziemy w lewo");

        k = Kierunek.Prawo;
        Console.WriteLine($"teraz: {k}");
    }
}
```

```wynik
Lewo
idziemy w lewo
teraz: Prawo
```

- `enum Kierunek { … }` wymienia wszystkie możliwe wartości. Zmienna typu `Kierunek` może mieć tylko jedną z nich.
- Wartość zapisujesz z nazwą typu: `Kierunek.Lewo`. Wypisana pokazuje swoją nazwę.

Po co to, skoro można by trzymać kierunek jako liczbę 0–3 albo napis `"lewo"`? Bo literówka `Kierunek.Lweo` to błąd
kompilacji, a literówka w napisie `"lweo"` przejdzie po cichu i zepsuje grę dopiero w trakcie.

## enum w switch

`switch` i enum świetnie do siebie pasują – na przykład stan potwora:

```cs
enum Stan { Spi, Goni, Atakuje, Martwy }

class Program
{
    static string Opis(Stan s)
    {
        switch (s)
        {
            case Stan.Spi: return "chrapie";
            case Stan.Goni: return "biegnie za tobą";
            case Stan.Atakuje: return "gryzie!";
            default: return "leży";
        }
    }

    static void Main()
    {
        Stan s = Stan.Spi;
        while (s != Stan.Martwy)
        {
            Console.WriteLine(Opis(s));
            s++;
        }
        Console.WriteLine(Opis(s));
    }
}
```

```wynik
chrapie
biegnie za tobą
gryzie!
leży
```

`s++` przechodzi do następnej wartości z listy.

## enum i liczby

Każda wartość enuma to w środku liczba: pierwsza ma 0, następna 1 i tak dalej. Możesz to zmienić i możesz
zamieniać w obie strony przez `(int)` i `(Kierunek)`:

```cs
enum Poziom { Latwy = 1, Sredni = 2, Trudny = 5 }

class Program
{
    static void Main()
    {
        Poziom p = Poziom.Trudny;
        int mnoznik = (int)p;
        Console.WriteLine($"{p}: obrażenia x{mnoznik}");

        Poziom q = (Poziom)2;
        Console.WriteLine(q);
        Console.WriteLine(Poziom.Latwy < Poziom.Trudny);
    }
}
```

```wynik
Trudny: obrażenia x5
Sredni
True
```

## enum w klasie i na liście

Enum może być polem obiektu, elementem listy albo kluczem słownika:

```cs
using System.Collections.Generic;

enum Rodzaj { Bron, Zbroja, Jedzenie }

class Przedmiot
{
    public string Nazwa;
    public Rodzaj Rodzaj;
    public Przedmiot(string nazwa, Rodzaj rodzaj) { Nazwa = nazwa; Rodzaj = rodzaj; }
}

class Program
{
    static void Main()
    {
        List<Przedmiot> plecak = new List<Przedmiot>
        {
            new Przedmiot("miecz", Rodzaj.Bron), new Przedmiot("chleb", Rodzaj.Jedzenie),
            new Przedmiot("hełm", Rodzaj.Zbroja), new Przedmiot("jabłko", Rodzaj.Jedzenie)
        };

        Dictionary<Rodzaj, int> ile = new Dictionary<Rodzaj, int>();
        foreach (Przedmiot p in plecak)
        {
            if (ile.ContainsKey(p.Rodzaj)) ile[p.Rodzaj]++;
            else ile[p.Rodzaj] = 1;
        }
        foreach (var para in ile) Console.WriteLine($"{para.Key}: {para.Value}");
    }
}
```

```wynik
Bron: 1
Jedzenie: 2
Zbroja: 1
```

## Pierwszy struct

`struct` wygląda prawie jak klasa: pola, konstruktor, metody. Różnica jest jedna, ale ważna: **zmienna struktury
trzyma same dane, nie odnośnik do obiektu**. Przypisanie robi kopię.

```cs
struct Punkt
{
    public int X, Y;
    public Punkt(int x, int y) { X = x; Y = y; }
    public override string ToString() { return $"({X}, {Y})"; }
}

class PunktK
{
    public int X, Y;
    public PunktK(int x, int y) { X = x; Y = y; }
}

class Program
{
    static void Main()
    {
        Punkt a = new Punkt(1, 2);
        Punkt b = a;          // kopia
        b.X = 100;
        Console.WriteLine($"struct: a = {a}, b = {b}");

        PunktK c = new PunktK(1, 2);
        PunktK d = c;         // ten sam obiekt
        d.X = 100;
        Console.WriteLine($"klasa:  c.X = {c.X}, d.X = {d.X}");
    }
}
```

```wynik
struct: a = (1, 2), b = (100, 2)
klasa:  c.X = 100, d.X = 100
```

Przy klasie `c` i `d` to dwie nazwy **tego samego** obiektu (zob. rozdział 10). Przy strukturze `a` i `b` to dwa
osobne punkty.

## Kopia także w metodzie

Struktura przekazana do metody też jest kopiowana. Metoda może zmieniać swoją kopię, ale oryginał zostaje.
Gdy chcesz zmieniony wynik, oddaj go przez `return`:

```cs
struct Wektor
{
    public int X, Y;
    public Wektor(int x, int y) { X = x; Y = y; }
}

class Program
{
    static void Przesun(Wektor w) { w.X += 10; }

    static Wektor Przesuniety(Wektor w) { w.X += 10; return w; }

    static void Main()
    {
        Wektor poz = new Wektor(0, 0);
        Przesun(poz);
        Console.WriteLine(poz.X);
        poz = Przesuniety(poz);
        Console.WriteLine(poz.X);
    }
}
```

```wynik
0
10
```

## Kiedy struct, a kiedy klasa?

- **struct** – małe wartości, które traktujesz jak liczbę: punkt, wektor, kolor, rozmiar, pole planszy. Kopiowanie
  jest tu naturalne: przesunięcie jednego punktu nie powinno przesunąć innego.
- **klasa** – rzeczy z własną „tożsamością”, które żyją i się zmieniają: gracz, potwór, drzwi. Gdy potwór traci życie,
  chcesz, żeby widziały to wszystkie miejsca, które go trzymają.

Struktura nie może dziedziczyć z klasy (zob. rozdział 11). `new Punkt()` bez argumentów daje punkt z samymi zerami.

> **Dla zaawansowanych:** enum to w VM zwykły `int`, więc jest tak samo szybki jak liczba; nazwy wartości są
> w programie tylko do wypisywania. Enum może mieć jawne wartości z wyrażeń (`Duzy = Maly * 10`) i atrybut `[Flags]`
> do łączenia wartości przez `|`. Struktura w VM to obiekt kopiowany przy zapisie do zmiennej, pola, elementu tablicy
> czy listy – dlatego lista struktur oddaje kopię, a `lista[0].X = 5` jest błędem kompilacji (zmiana kopii nic by
> nie dała): wyjmij element do zmiennej, zmień go i wstaw z powrotem.
