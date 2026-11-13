# Pierwszy program

MiniSharp to język programowania podobny do C# (tego z Unity). Piszesz w nim programy i gry, które działają
na Nintendo 3DS i w przeglądarce. Ten rozdział pokazuje najmniejszy program i to, jak go uruchomić.

## Cześć, świecie

```cs
class Program
{
    static void Main()
    {
        Console.WriteLine("Cześć, świecie!");
    }
}
```

```wynik
Cześć, świecie!
```

Co tu jest:

- `class Program` – każdy kod w MiniSharpie mieszka w jakiejś **klasie**. Nazwa może być dowolna.
- `static void Main()` – **metoda** `Main`. Program zaczyna się właśnie od niej.
- `Console.WriteLine(...)` – wypisuje tekst i przechodzi do nowej linii.
- Nawiasy klamrowe `{ }` pokazują, co jest w środku czego: metoda w klasie, polecenia w metodzie.
- Każde polecenie kończy się średnikiem `;`.

Uruchomienie: w edytorze naciśnij **Uruchom**. Tekst pojawi się na ekranie.

## Kilka linii i Write

`WriteLine` kończy linię, a `Write` nie – następny tekst pojawi się zaraz obok.

```cs
class Program
{
    static void Main()
    {
        Console.Write("Raz, ");
        Console.Write("dwa, ");
        Console.WriteLine("trzy!");
        Console.WriteLine();
        Console.WriteLine(2 + 3);
    }
}
```

```wynik
Raz, dwa, trzy!

5
```

`Console.WriteLine()` bez niczego w nawiasie daje pustą linię. Zamiast tekstu można wypisać liczbę albo wynik
rachunku: `2 + 3` wypisze `5`.

## Komentarze

Komentarz to notatka dla ludzi – program go pomija.

```cs
class Program
{
    // to jest komentarz do końca linii
    static void Main()
    {
        /* a to komentarz,
           który może mieć kilka linii */
        Console.WriteLine("Komentarze nic nie robią");
    }
}
```

```wynik
Komentarze nic nie robią
```

## Gdy coś jest źle

Jeśli w programie jest błąd, MiniSharp nie uruchomi go, tylko pokaże **numer linii** i opis. Na przykład gdy
zapomnisz średnika albo napiszesz `Console.WriteLinee`, zobaczysz coś w rodzaju:

```text
compile error: line 5: 'Console' has no method 'WriteLinee'
```

Edytor przeniesie kursor do tej linii. Popraw ją i uruchom jeszcze raz.

> **Dla zaawansowanych:** program jest najpierw w całości kompilowany do kodu bajtowego (tak jak C# do IL), dopiero
> potem uruchamiany przez maszynę wirtualną. Dlatego błąd w dowolnym miejscu pliku zatrzymuje cały program, zanim
> cokolwiek się wypisze. Pola `static` z wartością początkową są ustawiane przed `Main`.
