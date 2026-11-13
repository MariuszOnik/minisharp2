# Klasy i obiekty

Każdy dotychczasowy program miał jedną klasę, `Program`, i same metody `static`. Teraz zrobisz własne **klasy**:
przepisy na rzeczy z programu – gracza, potwora, konto w banku. Rzecz zrobiona według przepisu to **obiekt**.
Klasa mówi, co obiekt **wie** (pola) i co **umie** (metody).

## Pierwsza klasa

```cs
class Gracz
{
    public string Imie;
    public int Zycie;

    public void Pokaz()
    {
        Console.WriteLine($"{Imie}: {Zycie} życia");
    }
}

class Program
{
    static void Main()
    {
        Gracz a = new Gracz();
        a.Imie = "Ola";
        a.Zycie = 10;

        Gracz b = new Gracz();
        b.Imie = "Kuba";
        b.Zycie = 7;

        a.Pokaz();
        b.Pokaz();
    }
}
```

```wynik
Ola: 10 życia
Kuba: 7 życia
```

- `Imie` i `Zycie` to **pola** – zmienne, które ma każdy obiekt osobno.
- `new Gracz()` robi nowy obiekt. `a` i `b` to dwa różne obiekty, każdy z własnym imieniem i życiem.
- `a.Pokaz()` woła metodę na obiekcie `a`. W środku metody `Imie` znaczy „imię **tego** obiektu”.
- Metoda `Pokaz` **nie** ma `static` – należy do obiektu, nie do całej klasy.

## Konstruktor

Ustawianie każdego pola po kolei jest męczące i łatwo o czymś zapomnieć. **Konstruktor** to metoda o nazwie klasy,
bez typu wyniku, którą `new` woła od razu:

```cs
class Gracz
{
    public string Imie;
    public int Zycie;

    public Gracz(string imie, int zycie)
    {
        Imie = imie;
        Zycie = zycie;
    }

    public void Obrazenia(int ile)
    {
        Zycie -= ile;
        if (Zycie < 0) Zycie = 0;
        Console.WriteLine($"{Imie} traci {ile}, zostaje {Zycie}");
    }

    public bool Zyje()
    {
        return Zycie > 0;
    }
}

class Program
{
    static void Main()
    {
        Gracz g = new Gracz("Ola", 10);
        g.Obrazenia(4);
        g.Obrazenia(8);
        Console.WriteLine(g.Zyje() ? "jeszcze żyje" : "koniec gry");
    }
}
```

```wynik
Ola traci 4, zostaje 6
Ola traci 8, zostaje 0
koniec gry
```

Metoda obiektu może zmieniać jego pola (`Zycie -= ile`) – zmiana zostaje w obiekcie na później.

## this

Gdy parametr nazywa się tak samo jak pole, pole poznasz po `this.` – „ten obiekt”:

```cs
class Punkt
{
    public int x, y;

    public Punkt(int x, int y)
    {
        this.x = x;
        this.y = y;
    }

    public int OdlegloscOdZera()
    {
        return Math.Abs(x) + Math.Abs(y);
    }
}

class Program
{
    static void Main()
    {
        Punkt p = new Punkt(3, -4);
        Console.WriteLine($"({p.x}, {p.y}) jest {p.OdlegloscOdZera()} kroków od (0, 0)");
    }
}
```

```wynik
(3, -4) jest 7 kroków od (0, 0)
```

## Obiekty w tablicy i w liście

Obiekty trzyma się tak samo jak liczby: w zmiennych, tablicach i listach.

```cs
using System.Collections.Generic;

class Potwor
{
    public string Nazwa;
    public int Sila;

    public Potwor(string nazwa, int sila)
    {
        Nazwa = nazwa;
        Sila = sila;
    }
}

class Program
{
    static void Main()
    {
        var potwory = new List<Potwor>();
        potwory.Add(new Potwor("szczur", 2));
        potwory.Add(new Potwor("ork", 7));
        potwory.Add(new Potwor("smok", 20));

        Potwor najsilniejszy = potwory[0];
        foreach (Potwor p in potwory)
        {
            if (p.Sila > najsilniejszy.Sila) najsilniejszy = p;
        }
        Console.WriteLine($"Potworów: {potwory.Count}, najsilniejszy: {najsilniejszy.Nazwa}");
    }
}
```

```wynik
Potworów: 3, najsilniejszy: smok
```

## Dwie zmienne, jeden obiekt

Zmienna nie **jest** obiektem – tylko na niego **wskazuje**. Gdy przypiszesz ją do drugiej zmiennej, obie wskazują
ten sam obiekt:

```cs
class Licznik
{
    public int Ile;
}

class Program
{
    static void Dodaj(Licznik l)
    {
        l.Ile++;
    }

    static void Main()
    {
        Licznik a = new Licznik();
        Licznik b = a;
        b.Ile = 5;
        Console.WriteLine(a.Ile);
        Dodaj(a);
        Console.WriteLine(b.Ile);

        Licznik c = null;
        Console.WriteLine(c == null ? "c nie wskazuje na nic" : "c ma obiekt");
    }
}
```

```wynik
5
6
c nie wskazuje na nic
```

- `b = a` nie kopiuje obiektu. Zmiana przez `b` jest widoczna przez `a`.
- Metoda, która dostaje obiekt, może go zmienić – zmiana zostaje po powrocie (inaczej niż z liczbą).
- `null` znaczy „żaden obiekt”. Sięgnięcie do pola przez `null` (`c.Ile`) kończy program błędem
  `null reference`.

## Pola i metody static

`static` znaczy „należy do klasy, nie do obiektu”. Pole `static` jest jedno na cały program, wspólne dla
wszystkich obiektów:

```cs
class Moneta
{
    public static int Wybite = 0;
    public int Numer;

    public Moneta()
    {
        Wybite++;
        Numer = Wybite;
    }
}

class Program
{
    static void Main()
    {
        Moneta a = new Moneta();
        Moneta b = new Moneta();
        Moneta c = new Moneta();
        Console.WriteLine($"wybito {Moneta.Wybite}, ostatnia ma numer {c.Numer}");
    }
}
```

```wynik
wybito 3, ostatnia ma numer 3
```

Do pola `static` sięgasz przez nazwę klasy: `Moneta.Wybite`. Tak samo działa `Math.Abs` czy `Console.WriteLine` –
to metody `static` klas wbudowanych.

## Właściwości

**Właściwość** wygląda z zewnątrz jak pole, ale w środku może mieć kod. Najprostsza, `{ get; set; }`, to po prostu
pole. Z `get` można też policzyć wartość w locie:

```cs
class Prostokat
{
    public int Szer { get; set; }
    public int Wys { get; set; }
    public int Pole { get { return Szer * Wys; } }

    public Prostokat(int szer, int wys)
    {
        Szer = szer;
        Wys = wys;
    }
}

class Program
{
    static void Main()
    {
        var p = new Prostokat(3, 4);
        Console.WriteLine(p.Pole);
        p.Szer = 10;
        Console.WriteLine(p.Pole);
    }
}
```

```wynik
12
40
```

`Pole` nie ma `set`, więc `p.Pole = 5` to błąd kompilacji – pola prostokąta nie da się ustawić, tylko policzyć.

## private: tylko dla swojej klasy

Słowo `private` przed polem albo metodą znaczy: „tego używa tylko ta klasa”. Inne klasy go nie widzą. Dzięki temu
klasa pilnuje swoich danych – np. konto nie pozwoli zejść poniżej zera:

```cs
class Konto
{
    private int saldo;

    public void Wplac(int ile)
    {
        if (ile > 0) saldo += ile;
    }

    public bool Wyplac(int ile)
    {
        if (ile > saldo) return false;
        saldo -= ile;
        return true;
    }

    public int Saldo { get { return saldo; } }
}

class Program
{
    static void Main()
    {
        var k = new Konto();
        k.Wplac(100);
        Console.WriteLine(k.Wyplac(30) ? "wypłacono 30" : "brak środków");
        Console.WriteLine(k.Wyplac(500) ? "wypłacono 500" : "brak środków");
        Console.WriteLine($"saldo: {k.Saldo}");
    }
}
```

```wynik
wypłacono 30
brak środków
saldo: 70
```

Gdyby `Main` napisał `k.saldo = 1000000;`, kompilator odpowie błędem:

```text
compile error: line 33: 'saldo' is private in Konto; only code inside class Konto can use it
```

- `public` – dostęp z każdej klasy.
- `private` – tylko w tej klasie.
- `protected` – w tej klasie i w klasach, które po niej dziedziczą (o dziedziczeniu w następnym rozdziale).
- Bez słówka – w MiniSharpie dostęp z każdej klasy (jak `public`).

> **Dla zaawansowanych:** w C# brak słówka przy składniku klasy znaczy `private`. MiniSharp jest tu łagodniejszy,
> żeby krótkie programy działały bez pisania `public` wszędzie – ale każdy program poprawny w C# jest poprawny i tu.
> Jeśli chcesz pisać kod, który przeniesiesz do C#, pisz `public` jawnie. `internal` znaczy u nas to samo co brak
> słówka. `static class` (np. zbiór pomocniczych metod) nie pozwala na `new`, a wszystkie jej składniki muszą być
> `static`. Obiekty, na które nic już nie wskazuje, sprząta automatycznie odśmiecacz pamięci – nie trzeba ich usuwać.
