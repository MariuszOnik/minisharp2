# Przestrzenie nazw

Gdy program rośnie, klas robi się dużo, a nazwy zaczynają się powtarzać: `Item` może znaczyć przedmiot w plecaku,
ale też pozycję w menu. **Przestrzeń nazw** (`namespace`) to szuflada na klasy. Dwie klasy o tej samej nazwie mogą
leżeć w dwóch różnych szufladach i nie przeszkadzają sobie.

## Pierwsza przestrzeń nazw

```cs
namespace Gra
{
    class Gracz
    {
        public string Imie = "Ola";
    }
}

class Program
{
    static void Main()
    {
        Gra.Gracz g = new Gra.Gracz();
        Console.WriteLine(g.Imie);
    }
}
```

```wynik
Ola
```

- `namespace Gra { … }` wkłada klasę `Gracz` do szuflady `Gra`.
- Spoza szuflady klasę wołasz **pełną nazwą**: `Gra.Gracz`.
- Klasa `Program` nie jest w żadnej przestrzeni nazw – leży „na wierzchu”, tak jak wszystkie klasy z poprzednich
  rozdziałów.

## using – skrót do szuflady

Pisanie `Gra.` przed każdą nazwą męczy. `using Gra;` na początku pliku mówi: „szukaj klas także w szufladzie Gra”.

```cs
using Gra;

namespace Gra
{
    class Gracz
    {
        public string Imie;
        public Gracz(string imie) { Imie = imie; }
    }

    static class Zasady
    {
        public static int MaxZycie = 10;
    }
}

class Program
{
    static void Main()
    {
        Gracz g = new Gracz("Kuba");
        Console.WriteLine($"{g.Imie} ma maksymalnie {Zasady.MaxZycie} życia");
    }
}
```

```wynik
Kuba ma maksymalnie 10 życia
```

Linia `using System.Collections.Generic;`, którą piszesz od rozdziału 12, robi dokładnie to samo: otwiera szufladę,
w której leżą `List` i `Dictionary`.

## Ta sama nazwa w dwóch szufladach

```cs
namespace Plecak
{
    class Item
    {
        public string Opis() { return "przedmiot w plecaku"; }
    }
}

namespace Menu
{
    class Item
    {
        public string Opis() { return "pozycja w menu"; }
    }
}

class Program
{
    static void Main()
    {
        Plecak.Item a = new Plecak.Item();
        Menu.Item b = new Menu.Item();
        Console.WriteLine(a.Opis());
        Console.WriteLine(b.Opis());
    }
}
```

```wynik
przedmiot w plecaku
pozycja w menu
```

Gdy obie szuflady są otwarte przez `using`, samo `Item` byłoby niejasne – wtedy pisz pełną nazwę.

## Szuflady w szufladach

Nazwa przestrzeni może mieć kropki: `Gra.Potwory`, `Gra.Mapa`. Klasa z `Gra.Potwory` widzi bez pisania przedrostka
klasy z szuflady wokół niej, czyli z `Gra`:

```cs
namespace Gra
{
    class Kostka
    {
        public static int Rzut() { return 4; }
    }
}

namespace Gra.Potwory
{
    class Goblin
    {
        public int Atak() { return Kostka.Rzut() + 1; }   // Kostka z przestrzeni Gra
    }
}

class Program
{
    static void Main()
    {
        Gra.Potwory.Goblin g = new Gra.Potwory.Goblin();
        Console.WriteLine($"Goblin atakuje za {g.Atak()}");
    }
}
```

```wynik
Goblin atakuje za 5
```

## Krótsza nazwa: using G = …

Gdy nazwa szuflady jest długa, a nie chcesz jej otwierać w całości (bo ma klasę o tej samej nazwie co inna), nadaj jej
**krótki przydomek** (alias):

```cs
using M = Gra.Potwory.Mapa;

namespace Gra.Potwory.Mapa
{
    class Pole
    {
        public int X, Y;
        public Pole(int x, int y) { X = x; Y = y; }
    }
}

class Program
{
    static void Main()
    {
        M.Pole p = new M.Pole(3, 7);
        Console.WriteLine($"pole ({p.X}, {p.Y})");
    }
}
```

```wynik
pole (3, 7)
```

`M.Pole` znaczy to samo co `Gra.Potwory.Mapa.Pole`. Przydomek może też oznaczać jedną klasę: po
`using P = Gra.Potwory.Mapa.Pole;` wystarczy pisać `P`.

## Po co to w małym programie?

W jednym pliku – rzadko. Przestrzenie nazw przydają się, gdy:

- program ma wiele plików (projekt): każda część gry dostaje swoją szufladę, np. `Gra.Mapa`, `Gra.Walka`;
- używasz cudzej biblioteki: jej klasy są w jej przestrzeni nazw i nie zderzą się z Twoimi;
- piszesz własną bibliotekę dla kilku gier – `using MojaBiblioteka;` w każdej z nich.

O projektach z wieloma plikami i wspólnych bibliotekach (`uses.txt`) opowie osobny rozdział.

> **Dla zaawansowanych:** klasę szuka się najpierw w przestrzeni, w której jest kod, potem w przestrzeniach wokół
> niej (`Gra.Potwory`, potem `Gra`), na końcu w otwartych przez `using` i „na wierzchu”. Dlatego klasa
> `Gra.Potwory.Item` w kodzie z `Gra.Potwory` zasłania `Gra.Item` – do tej drugiej trzeba wtedy pełnej nazwy.
> Względna nazwa też działa: w `Gra` można napisać `Potwory.Goblin`. Przydomek (`using G = Gra;`) nie otwiera
> szuflady jak `using Gra;` – zastępuje tylko nazwę. Przydomka dla typu z `<…>` (`using L = List<int>;`) nie ma.
