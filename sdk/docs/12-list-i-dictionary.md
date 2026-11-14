# List i Dictionary

Tablica ma stały rozmiar: `new int[5]` to zawsze pięć miejsc. W grze rzeczy się pojawiają i znikają – pociski,
potwory, przedmioty w plecaku. Do tego służy **lista** (`List`), która rośnie i maleje sama. Drugi pojemnik,
**słownik** (`Dictionary`), trzyma pary „klucz → wartość”, np. „nazwa przedmiotu → ile sztuk”.

Oba wymagają na początku pliku linii `using System.Collections.Generic;`.

## Pierwsza lista

```cs
using System.Collections.Generic;

class Program
{
    static void Main()
    {
        List<string> plecak = new List<string>();
        plecak.Add("miecz");
        plecak.Add("chleb");
        plecak.Add("lina");

        Console.WriteLine($"Przedmiotów: {plecak.Count}");
        Console.WriteLine($"Pierwszy: {plecak[0]}");

        foreach (string rzecz in plecak)
            Console.WriteLine($"- {rzecz}");
    }
}
```

```wynik
Przedmiotów: 3
Pierwszy: miecz
- miecz
- chleb
- lina
```

- `List<string>` czytaj „lista napisów”. W ostrych nawiasach stoi typ elementów: `List<int>`, `List<Potwor>`…
- `Add` dokłada na koniec. `Count` mówi, ile jest elementów (w tablicy było `Length`).
- `plecak[0]` to pierwszy element, tak jak w tablicy. Indeksy idą od 0 do `Count - 1`.

## Lista od razu z elementami

Elementy można podać w klamrach, tak jak w tablicy:

```cs
using System.Collections.Generic;

class Program
{
    static void Main()
    {
        List<int> punkty = new List<int> { 40, 15, 90, 15 };
        punkty.Add(60);

        int suma = 0;
        for (int i = 0; i < punkty.Count; i++)
            suma += punkty[i];
        Console.WriteLine($"Suma: {suma}");

        punkty[1] = 20;
        Console.WriteLine($"Teraz drugi to {punkty[1]}");
    }
}
```

```wynik
Suma: 220
Teraz drugi to 20
```

## Wyjmowanie i szukanie

```cs
using System.Collections.Generic;

class Program
{
    static void Pokaz(List<string> l)
    {
        Console.WriteLine(string.Join(", ", l));
    }

    static void Main()
    {
        List<string> l = new List<string> { "a", "b", "c", "b" };
        l.Remove("b");          // pierwsze "b"
        Pokaz(l);
        l.RemoveAt(0);          // element o indeksie 0
        Pokaz(l);
        l.Insert(1, "x");       // wstaw na miejsce 1
        Pokaz(l);

        Console.WriteLine(l.Contains("x"));
        Console.WriteLine(l.IndexOf("b"));
        Console.WriteLine(l.IndexOf("zzz"));

        l.Clear();
        Console.WriteLine($"Po Clear: {l.Count}");
    }
}
```

```wynik
a, c, b
c, b
c, x, b
True
2
-1
Po Clear: 0
```

- `Remove(x)` usuwa pierwszy element równy `x`; `RemoveAt(i)` – element o indeksie `i`.
- `Insert(i, x)` wstawia na miejsce `i`, a resztę przesuwa dalej.
- `Contains(x)` mówi, czy `x` jest na liście, `IndexOf(x)` – gdzie (albo `-1`, gdy go nie ma).
- `string.Join(", ", lista)` skleja elementy w jeden napis z przecinkami.

## Sortowanie i odwracanie

```cs
using System.Collections.Generic;

class Program
{
    static void Main()
    {
        List<int> wyniki = new List<int> { 30, 5, 120, 70 };
        wyniki.Sort();
        Console.WriteLine(string.Join(" ", wyniki));
        wyniki.Reverse();
        Console.WriteLine($"Najlepszy: {wyniki[0]}");

        List<string> imiona = new List<string> { "Ola", "Adam", "Kuba" };
        imiona.Sort();
        Console.WriteLine(string.Join(" ", imiona));
    }
}
```

```wynik
5 30 70 120
Najlepszy: 120
Adam Kuba Ola
```

## Usuwanie w pętli – od końca

Częsta sytuacja: przejdź po liście potworów i usuń martwe. W `foreach` się tego nie da – lista zmieniałaby się pod
nogami pętli, więc program zatrzyma się z błędem (tak samo jak w C#):

```text
runtime error: line 9 (Program.Main): the list was changed inside its own foreach (Add, Remove, Clear, l[i] = ...); ...
```

Użyj zwykłego `for` i idź **od końca**: usunięcie elementu nie przesuwa wtedy tych, których jeszcze nie
sprawdziłeś.

```cs
using System.Collections.Generic;

class Program
{
    static void Main()
    {
        List<int> zycie = new List<int> { 5, 0, 3, 0, 0, 8 };
        for (int i = zycie.Count - 1; i >= 0; i--)
        {
            if (zycie[i] == 0) zycie.RemoveAt(i);
        }
        Console.WriteLine(string.Join(" ", zycie));
    }
}
```

```wynik
5 3 8
```

Inne wyjścia: przejdź po kopii listy (`foreach (int z in new List<int>(zycie))`) albo użyj `RemoveAll` z rozdziału
o lambdach. Zmienianie **innej** listy w `foreach` jest w porządku.

## Indeks poza listą

Tak jak w tablicy, sięgnięcie poza listę zatrzymuje program z błędem, który mówi, co poszło nie tak:

```text
runtime error: line 9 (Program.Main): index 5 is outside the list (length 2)
```

## Pierwszy słownik

Słownik pamięta pary: **klucz** (np. nazwa) i **wartość** (np. liczba). Po kluczu od razu znajduje wartość –
nie trzeba szukać po kolei.

```cs
using System.Collections.Generic;

class Program
{
    static void Main()
    {
        Dictionary<string, int> ceny = new Dictionary<string, int>();
        ceny.Add("miecz", 100);
        ceny.Add("tarcza", 60);
        ceny["mikstura"] = 25;

        Console.WriteLine($"Miecz kosztuje {ceny["miecz"]}");
        ceny["miecz"] = 80;
        Console.WriteLine($"Po promocji: {ceny["miecz"]}");
        Console.WriteLine($"Towarów: {ceny.Count}");
    }
}
```

```wynik
Miecz kosztuje 100
Po promocji: 80
Towarów: 3
```

- `Dictionary<string, int>` – klucze to napisy, wartości to liczby.
- `ceny["miecz"]` czyta wartość dla klucza. `ceny["miecz"] = 80` zapisuje: zmienia starą albo dodaje nową parę.
- `Add` dodaje parę, ale klucz nie może się powtarzać – drugie `Add("miecz", …)` to błąd.

## Czy klucz jest w słowniku?

Czytanie klucza, którego nie ma, zatrzymuje program. Zapytaj najpierw przez `ContainsKey` albo użyj `TryGetValue`,
które sprawdza i czyta za jednym razem:

```cs
using System.Collections.Generic;

class Program
{
    static void Main()
    {
        Dictionary<string, int> ceny = new Dictionary<string, int> { { "miecz", 100 }, { "łuk", 70 } };

        if (ceny.ContainsKey("łuk")) Console.WriteLine($"Łuk: {ceny["łuk"]}");

        if (ceny.TryGetValue("topór", out int cena))
            Console.WriteLine($"Topór: {cena}");
        else
            Console.WriteLine("Topora nie ma w sklepie");

        ceny.Remove("miecz");
        Console.WriteLine(ceny.ContainsKey("miecz"));
    }
}
```

```wynik
Łuk: 70
Topora nie ma w sklepie
False
```

Bez sprawdzenia dostaniesz błąd z podpowiedzią:

```text
runtime error: line 7 (Program.Main): key 'topór' is not in the dictionary (use ContainsKey or TryGetValue)
```

## Przechodzenie po słowniku

`foreach` daje pary: `Key` to klucz, `Value` – wartość. Kolejność jest taka, w jakiej pary dodawano.
Same klucze to `Keys`, same wartości – `Values`.

```cs
using System.Collections.Generic;

class Program
{
    static void Main()
    {
        Dictionary<string, int> plecak = new Dictionary<string, int>();
        plecak["strzała"] = 20;
        plecak["chleb"] = 2;
        plecak["złoto"] = 150;

        foreach (var para in plecak)
            Console.WriteLine($"{para.Key}: {para.Value}");

        int razem = 0;
        foreach (int ile in plecak.Values) razem += ile;
        Console.WriteLine($"Razem sztuk: {razem}");
    }
}
```

```wynik
strzała: 20
chleb: 2
złoto: 150
Razem sztuk: 172
```

`var` zamiast typu pary oznacza „kompilator sam wie, jaki to typ”. Pełna nazwa to `KeyValuePair<string, int>`.

W pętli po słowniku wolno zmieniać wartości (`plecak[para.Key] = 0`) i usuwać pary (`Remove`), ale **nie wolno
dodawać nowych kluczy** – program zatrzyma się z błędem. Nowe klucze zbierz w liście i dodaj po pętli.

## Liczenie słów – przykład z życia

Słownik świetnie liczy, ile razy coś wystąpiło:

```cs
using System.Collections.Generic;

class Program
{
    static void Main()
    {
        string tekst = "kot pies kot mysz kot pies";
        Dictionary<string, int> ile = new Dictionary<string, int>();

        foreach (string slowo in tekst.Split(' '))
        {
            if (ile.ContainsKey(slowo)) ile[slowo]++;
            else ile[slowo] = 1;
        }

        foreach (var para in ile)
            Console.WriteLine($"{para.Key} x{para.Value}");
    }
}
```

```wynik
kot x3
pies x2
mysz x1
```

## Lista obiektów i słownik obiektów

W liście i słowniku mogą leżeć też Twoje obiekty (zob. rozdział 10):

```cs
using System.Collections.Generic;

class Przedmiot
{
    public string Nazwa;
    public int Waga;
    public Przedmiot(string nazwa, int waga) { Nazwa = nazwa; Waga = waga; }
}

class Program
{
    static void Main()
    {
        List<Przedmiot> plecak = new List<Przedmiot>
        {
            new Przedmiot("zbroja", 15),
            new Przedmiot("chleb", 1),
            new Przedmiot("kilof", 6)
        };

        int waga = 0;
        foreach (Przedmiot p in plecak) waga += p.Waga;
        Console.WriteLine($"Plecak waży {waga}");

        Dictionary<string, Przedmiot> poNazwie = new Dictionary<string, Przedmiot>();
        foreach (Przedmiot p in plecak) poNazwie[p.Nazwa] = p;
        Console.WriteLine($"Kilof waży {poNazwie["kilof"].Waga}");
    }
}
```

```wynik
Plecak waży 22
Kilof waży 6
```

> **Dla zaawansowanych:** lista to tablica, która sama się powiększa – `Add` na końcu jest szybkie, a `Insert(0, …)`
> i `RemoveAt(0)` przesuwają wszystkie elementy, więc na długiej liście są wolne. Słownik to tablica mieszająca:
> `d[klucz]`, `ContainsKey` i `Remove` nie zależą od liczby par, a `ContainsValue` przegląda wszystkie. Lista ma też
> metody przyjmujące funkcję, np. `Find`, `FindAll`, `RemoveAll`, `Exists`, `Sort` z własnym porównaniem – opisuje
> je rozdział o lambdach. Kopia listy: `new List<int>(stara)`.
