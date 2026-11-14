# Dziedziczenie

W grze jest wiele rzeczy podobnych do siebie: wilk, smok i szczur to potwory. Każdy ma imię i życie, każdy dostaje
obrażenia – ale smok zieje ogniem, a szczur tylko gryzie. Zamiast pisać trzy prawie takie same klasy, piszesz jedną
ogólną (`Potwor`) i trzy, które z niej **dziedziczą**: dostają wszystko, co ma `Potwor`, i dokładają swoje.

## Klasa bazowa i pochodna

```cs
class Zwierze
{
    public string Imie;

    public void Przedstaw()
    {
        Console.WriteLine($"Jestem {Imie}");
    }
}

class Pies : Zwierze
{
    public void Szczekaj()
    {
        Console.WriteLine($"{Imie}: hau hau!");
    }
}

class Program
{
    static void Main()
    {
        Pies p = new Pies();
        p.Imie = "Burek";
        p.Przedstaw();
        p.Szczekaj();
    }
}
```

```wynik
Jestem Burek
Burek: hau hau!
```

- `class Pies : Zwierze` czytaj „pies to zwierzę”. `Zwierze` to **klasa bazowa**, `Pies` – **klasa pochodna**.
- `Pies` nie ma własnego pola `Imie` ani metody `Przedstaw`, a mimo to może ich używać – odziedziczył je.
- W drugą stronę to nie działa: zwykłe `Zwierze` nie umie `Szczekaj()`.

## Konstruktor i base(...)

Gdy klasa bazowa ma konstruktor z parametrami, klasa pochodna musi go zawołać. Robi to `: base(...)` zaraz za
nagłówkiem swojego konstruktora:

```cs
class Potwor
{
    public string Nazwa;
    public int Zycie;

    public Potwor(string nazwa, int zycie)
    {
        Nazwa = nazwa;
        Zycie = zycie;
        Console.WriteLine($"Powstaje potwór {nazwa}");
    }
}

class Smok : Potwor
{
    public int Ogien;

    public Smok(string nazwa) : base(nazwa, 100)
    {
        Ogien = 30;
        Console.WriteLine($"{Nazwa} ma {Zycie} życia i {Ogien} ognia");
    }
}

class Program
{
    static void Main()
    {
        Smok s = new Smok("Wawel");
    }
}
```

```wynik
Powstaje potwór Wawel
Wawel ma 100 życia i 30 ognia
```

Najpierw działa konstruktor bazowy (ustawia `Nazwa` i `Zycie`), dopiero potem reszta konstruktora `Smok`.

## virtual i override – inaczej w każdej klasie

Każdy potwór atakuje, ale każdy inaczej. Metoda oznaczona w klasie bazowej `virtual` („można ją zmienić”) może
dostać w klasie pochodnej nową wersję oznaczoną `override` („zmieniam”):

```cs
class Potwor
{
    public string Nazwa;

    public Potwor(string nazwa) { Nazwa = nazwa; }

    public virtual int Atak()
    {
        return 1;
    }
}

class Szczur : Potwor
{
    public Szczur() : base("szczur") { }
}

class Smok : Potwor
{
    public Smok() : base("smok") { }

    public override int Atak()
    {
        return 20;
    }
}

class Program
{
    static void Main()
    {
        Potwor a = new Szczur();
        Potwor b = new Smok();
        Console.WriteLine($"{a.Nazwa} zadaje {a.Atak()}");
        Console.WriteLine($"{b.Nazwa} zadaje {b.Atak()}");
    }
}
```

```wynik
szczur zadaje 1
smok zadaje 20
```

Zwróć uwagę: obie zmienne mają typ `Potwor`, a mimo to `b.Atak()` daje 20. Liczy się to, **jakim obiektem** coś
naprawdę jest (`new Smok()`), a nie typ zmiennej. Smok wpisany do zmiennej `Potwor` dalej jest smokiem.

## Lista różnych potworów

To jest główna zaleta dziedziczenia: jedna lista, różne obiekty, jedna pętla.

```cs
using System.Collections.Generic;

class Potwor
{
    public string Nazwa;
    public Potwor(string nazwa) { Nazwa = nazwa; }
    public virtual string Odglos() { return "..."; }
}

class Wilk : Potwor
{
    public Wilk() : base("wilk") { }
    public override string Odglos() { return "auuu"; }
}

class Smok : Potwor
{
    public Smok() : base("smok") { }
    public override string Odglos() { return "ROAR"; }
}

class Program
{
    static void Main()
    {
        List<Potwor> potwory = new List<Potwor>();
        potwory.Add(new Wilk());
        potwory.Add(new Smok());
        potwory.Add(new Potwor("cień"));

        foreach (Potwor p in potwory)
            Console.WriteLine($"{p.Nazwa}: {p.Odglos()}");
    }
}
```

```wynik
wilk: auuu
smok: ROAR
cień: ...
```

Gdy dojdzie nowy potwór, piszesz tylko nową klasę – pętla zostaje bez zmian.

## base.Metoda() – dołóż, zamiast zastępować

Czasem nowa wersja metody ma zrobić to samo co stara **i coś jeszcze**. Starą wersję wołasz przez `base.`:

```cs
class Postac
{
    public int Zycie = 10;

    public virtual void Obrazenia(int ile)
    {
        Zycie -= ile;
        Console.WriteLine($"traci {ile}, zostaje {Zycie}");
    }
}

class Rycerz : Postac
{
    public override void Obrazenia(int ile)
    {
        Console.WriteLine("tarcza zatrzymuje połowę");
        base.Obrazenia(ile / 2);
    }
}

class Program
{
    static void Main()
    {
        Postac r = new Rycerz();
        r.Obrazenia(6);
    }
}
```

```wynik
tarcza zatrzymuje połowę
traci 3, zostaje 7
```

## ToString – jak obiekt wygląda w napisie

Każda klasa ma ukrytą metodę `ToString()`, której używa `Console.WriteLine` i `$"..."`. Bez zmian wypisuje tylko
nazwę klasy. Gdy ją nadpiszesz, obiekt sam powie, jak go wypisać:

```cs
class Punkt
{
    public int X, Y;
    public Punkt(int x, int y) { X = x; Y = y; }

    public override string ToString()
    {
        return $"({X}, {Y})";
    }
}

class Program
{
    static void Main()
    {
        Punkt p = new Punkt(2, 5);
        Console.WriteLine(p);
        Console.WriteLine($"Gracz stoi w {p}");
    }
}
```

```wynik
(2, 5)
Gracz stoi w (2, 5)
```

## protected – tylko dla rodziny

Pole `private` widzi tylko jego klasa (zob. rozdział 10). Pole `protected` widzi jego klasa **i klasy pochodne**,
ale nie reszta programu:

```cs
class Konto
{
    protected int stan;

    public void Wplac(int ile) { stan += ile; }
    public int Stan() { return stan; }
}

class KontoZOdsetkami : Konto
{
    public void DodajOdsetki()
    {
        stan += stan / 10;
    }
}

class Program
{
    static void Main()
    {
        KontoZOdsetkami k = new KontoZOdsetkami();
        k.Wplac(200);
        k.DodajOdsetki();
        Console.WriteLine(k.Stan());
    }
}
```

```wynik
220
```

Gdyby `Main` spróbował napisać `k.stan = 1000000;`, kompilator zgłosi błąd – stan konta zmienia się tylko przez
metody.

## abstract – klasa, z której nie robi się obiektów

Czasem klasa bazowa to tylko pomysł: „figura” sama w sobie nie ma pola powierzchni, mają je dopiero koło i kwadrat.
Taką klasę oznacz `abstract`. Metoda `abstract` nie ma treści – **każda** klasa pochodna musi ją napisać:

```cs
abstract class Figura
{
    public abstract int Pole();

    public void Pokaz()
    {
        Console.WriteLine($"pole = {Pole()}");
    }
}

class Kwadrat : Figura
{
    int bok;
    public Kwadrat(int bok) { this.bok = bok; }
    public override int Pole() { return bok * bok; }
}

class Prostokat : Figura
{
    int a, b;
    public Prostokat(int a, int b) { this.a = a; this.b = b; }
    public override int Pole() { return a * b; }
}

class Program
{
    static void Main()
    {
        Figura[] figury = { new Kwadrat(3), new Prostokat(2, 5) };
        foreach (Figura f in figury) f.Pokaz();
    }
}
```

```wynik
pole = 9
pole = 10
```

`new Figura()` to błąd kompilacji – z klasy abstrakcyjnej nie zrobisz obiektu. Zapomniane `Pole()` w nowej klasie
pochodnej też jest błędem, więc nie da się o nim zapomnieć.

## is i as – czym naprawdę jest obiekt

Na liście potworów może być smok, a chcesz, żeby tylko smok ział ogniem. `is` sprawdza typ obiektu, a wersja
`is Smok s` od razu daje zmienną `s` typu `Smok`:

```cs
using System.Collections.Generic;

class Potwor { }

class Smok : Potwor
{
    public void Ziej() { Console.WriteLine("smok zieje ogniem!"); }
}

class Wilk : Potwor { }

class Program
{
    static void Main()
    {
        List<Potwor> lista = new List<Potwor> { new Wilk(), new Smok(), new Wilk() };
        int wilki = 0;
        foreach (Potwor p in lista)
        {
            if (p is Smok s) s.Ziej();
            if (p is Wilk) wilki++;
        }
        Console.WriteLine($"wilków: {wilki}");

        Smok x = lista[0] as Smok;
        Console.WriteLine(x == null ? "lista[0] to nie smok" : "lista[0] to smok");
    }
}
```

```wynik
smok zieje ogniem!
wilków: 2
lista[0] to nie smok
```

`as` próbuje zamienić typ; gdy się nie da, daje `null` zamiast błędu. `is` jest też prawdziwe dla klas dalej
w rodzinie: smok `is Potwor`.

## Interfejsy – umiejętność zamiast rodziny

Klasa ma **jedną** klasę bazową. Gdy różne, niespokrewnione rzeczy mają coś umieć – beczkę i potwora da się
zniszczyć – opisz tę umiejętność **interfejsem**. Interfejs to lista metod bez treści; klasa, która go wymienia
po dwukropku, obiecuje je napisać.

```cs
using System.Collections.Generic;

interface IZniszczalne
{
    void Uderz(int sila);
    bool Zniszczone();
}

class Beczka : IZniszczalne
{
    int wytrzymalosc = 3;
    public void Uderz(int sila) { wytrzymalosc -= sila; }
    public bool Zniszczone() { return wytrzymalosc <= 0; }
}

class Goblin : IZniszczalne
{
    int zycie = 5;
    public void Uderz(int sila) { zycie -= sila; Console.WriteLine($"goblin: au! ({zycie})"); }
    public bool Zniszczone() { return zycie <= 0; }
}

class Program
{
    static void Main()
    {
        List<IZniszczalne> rzeczy = new List<IZniszczalne> { new Beczka(), new Goblin() };
        foreach (IZniszczalne r in rzeczy)
        {
            r.Uderz(4);
            Console.WriteLine(r.Zniszczone() ? "zniszczone" : "całe");
        }
    }
}
```

```wynik
zniszczone
goblin: au! (1)
całe
```

Nazwy interfejsów zwyczajowo zaczynają się od `I`. Klasa może mieć jedną klasę bazową i dowolnie wiele interfejsów:
`class Smok : Potwor, IZniszczalne, ILatajace`.

> **Dla zaawansowanych:** wywołanie metody `virtual` kosztuje w VM tyle co zwykłe – metoda jest wybierana z tablicy
> klasy obiektu, bez szukania po nazwie. Metody bez `virtual` nie da się nadpisać; nowa metoda o tej samej nazwie
> w klasie pochodnej jest błędem zamiast cichego „ukrycia” (`new` z C# nie jest obsługiwane). Interfejs może
> dziedziczyć z innych interfejsów (`interface ILeczone : IZniszczalne`), a `is`/`as` działają też z interfejsami.
