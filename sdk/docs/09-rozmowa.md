# Rozmowa z użytkownikiem

Do tej pory program tylko mówił: wypisywał, co policzył. Teraz nauczy się **słuchać**. `Console.ReadLine()` czeka,
aż użytkownik wpisze linię tekstu i zatwierdzi ją Enterem, a potem oddaje ją programowi jako napis.

Na 3DS, gdy program czeka na odpowiedź, na dolnym ekranie pojawia się pole i klawiatura: wpisz tekst i naciśnij
Enter (albo A). START kończy wejście – `ReadLine` oddaje wtedy `null`.

## Pytanie i odpowiedź

```cs
class Program
{
    static void Main()
    {
        Console.Write("Jak masz na imię? ");
        string imie = Console.ReadLine();
        Console.WriteLine($"Cześć, {imie}!");
    }
}
```

```wejscie
Ola
```

```wynik
Jak masz na imię? Ola
Cześć, Ola!
```

- `Console.Write` (bez `Line`) zostawia kursor w tej samej linii, więc odpowiedź stoi zaraz za pytaniem.
- `Console.ReadLine()` zwraca to, co wpisano, **bez** znaku Enter na końcu.
- Program stoi w miejscu, dopóki użytkownik nie odpowie. Nic się w tym czasie nie liczy.

## Liczby

`ReadLine` zawsze daje napis – nawet gdy ktoś wpisze `12`. Żeby liczyć, trzeba go zamienić na liczbę:
`int.Parse` (zob. rozdział 8).

```cs
class Program
{
    static void Main()
    {
        Console.Write("Pierwsza liczba: ");
        int a = int.Parse(Console.ReadLine());
        Console.Write("Druga liczba: ");
        int b = int.Parse(Console.ReadLine());
        Console.WriteLine($"{a} + {b} = {a + b}");
        Console.WriteLine($"{a} * {b} = {a * b}");
    }
}
```

```wejscie
12
30
```

```wynik
Pierwsza liczba: 12
Druga liczba: 30
12 + 30 = 42
12 * 30 = 360
```

Co, jeśli ktoś wpisze `abc`? `int.Parse` zatrzyma program błędem. Bezpieczniej jest użyć `int.TryParse`: zwraca
`true`, gdy się udało, a liczbę oddaje przez `out` (zob. rozdział 6). Wtedy można grzecznie zapytać jeszcze raz:

```cs
class Program
{
    static int PodajLiczbe(string pytanie)
    {
        int liczba;
        Console.Write(pytanie);
        while (!int.TryParse(Console.ReadLine(), out liczba))
        {
            Console.WriteLine("To nie jest liczba, spróbuj jeszcze raz.");
            Console.Write(pytanie);
        }
        return liczba;
    }

    static void Main()
    {
        int wiek = PodajLiczbe("Ile masz lat? ");
        Console.WriteLine($"Za 10 lat będziesz mieć {wiek + 10}.");
    }
}
```

```wejscie
dużo
12
```

```wynik
Ile masz lat? dużo
To nie jest liczba, spróbuj jeszcze raz.
Ile masz lat? 12
Za 10 lat będziesz mieć 22.
```

Pytanie w osobnej metodzie przydaje się, gdy pytasz o liczby wiele razy – pętlę sprawdzania piszesz tylko raz.

## Gra: zgadnij liczbę

Komputer losuje liczbę, a gracz zgaduje. Po każdej próbie słyszy „więcej” albo „mniej”. Tu liczba jest stała (7),
żeby przykład dawał zawsze ten sam wynik; w prawdziwej grze weź `Random.Range(1, 101)` (zob. rozdział 3).

```cs
class Program
{
    static void Main()
    {
        int sekret = 7;
        int proby = 0;
        Console.WriteLine("Myślę o liczbie od 1 do 100.");
        while (true)
        {
            Console.Write("Twój strzał: ");
            int strzal = int.Parse(Console.ReadLine());
            proby++;
            if (strzal < sekret) Console.WriteLine("Więcej!");
            else if (strzal > sekret) Console.WriteLine("Mniej!");
            else
            {
                Console.WriteLine($"Zgadłeś w {proby} próbach!");
                break;
            }
        }
    }
}
```

```wejscie
50
5
7
```

```wynik
Myślę o liczbie od 1 do 100.
Twój strzał: 50
Mniej!
Twój strzał: 5
Więcej!
Twój strzał: 7
Zgadłeś w 3 próbach!
```

## Kilka linii, aż do pustej

Czasem nie wiadomo, ile odpowiedzi przyjdzie – np. lista zakupów. Czytamy linie, aż użytkownik wciśnie Enter
bez wpisywania niczego:

```cs
using System.Collections.Generic;

class Program
{
    static void Main()
    {
        var zakupy = new List<string>();
        Console.WriteLine("Co kupić? (pusta linia kończy)");
        string linia;
        while ((linia = Console.ReadLine()) != "")
        {
            zakupy.Add(linia);
        }
        Console.WriteLine($"Na liście jest {zakupy.Count} rzeczy:");
        foreach (string z in zakupy)
        {
            Console.WriteLine("- " + z);
        }
    }
}
```

```wejscie
chleb
mleko
jabłka

```

```wynik
Co kupić? (pusta linia kończy)
chleb
mleko
jabłka

Na liście jest 3 rzeczy:
- chleb
- mleko
- jabłka
```

`(linia = Console.ReadLine()) != ""` robi dwie rzeczy naraz: zapisuje odpowiedź w `linia` i sprawdza, czy nie jest
pusta.

## Kolory, czyszczenie ekranu i pozycja kursora

Tekst może mieć kolor. `Console.ForegroundColor` to kolor liter, `Console.BackgroundColor` – tła; wartości bierzesz
z `ConsoleColor` (16 kolorów: `Black`, `Blue`, `Green`, `Red`, `Yellow`, `White`, `Gray`, `DarkBlue`…).
Kolor działa na wszystko, co wypiszesz **potem**. `Console.ResetColor()` wraca do szarego na czarnym.

```cs
class Program
{
    static void Main()
    {
        Console.ForegroundColor = ConsoleColor.Red;
        Console.WriteLine("Uwaga, smok!");
        Console.ForegroundColor = ConsoleColor.Green;
        Console.WriteLine("Mikstura leczy 5 życia.");
        Console.ResetColor();
        Console.WriteLine($"Teraz kolor to {Console.ForegroundColor}.");
    }
}
```

```wynik
Uwaga, smok!
Mikstura leczy 5 życia.
Teraz kolor to Gray.
```

Na ekranie konsoli pierwsza linia jest czerwona, druga zielona (w tekście wyniku powyżej kolorów nie widać).

Dwie metody przydają się w grach tekstowych, które rysują planszę:

- `Console.Clear()` czyści cały ekran i ustawia kursor w lewym górnym rogu;
- `Console.SetCursorPosition(x, y)` przenosi kursor do kolumny `x` i wiersza `y` (liczone od 0) – następny `Write`
  pisze właśnie tam, np. gwiazdkę gracza w środku planszy.

```text
Console.Clear();
Console.SetCursorPosition(10, 5);
Console.Write("@");
```

> **Dla zaawansowanych:** gdy program pisze do pliku albo do zwykłego okna tekstowego (np. testy na PC bez
> `--grid`), `Clear` i `SetCursorPosition` nic nie robią, a kolory są tylko zapamiętywane. Pozycja spoza ekranu to
> błąd, jak wyjątek w C#. Na ekranie 3DS konsola ma 40 kolumn i 20 wierszy.

## W terminalu

W terminalu `ReadLine` też działa: następna linia, którą wpiszesz, jest odpowiedzią, a nie nowym poleceniem.

```text
> Console.Write("Imię? "); var imie = Console.ReadLine();
Imię? Ola
> Console.WriteLine("Cześć " + imie);
Cześć Ola
```

> **Dla zaawansowanych:** gdy wejście się skończy (np. program na PC czyta plik podany na wejście i plik się
> skończył), `ReadLine` zwraca `null`, jak w C#. Pętla `while ((linia = Console.ReadLine()) != null)` czyta wtedy
> wszystko do końca. Program czekający na odpowiedź jest naprawdę **wstrzymany**: maszyna wirtualna oddaje sterowanie
> aplikacji i wraca do pracy z wpisanym tekstem – dlatego to samo działa na konsoli i w przeglądarce, gdzie program
> nie może po prostu stać w miejscu. Z tego powodu `ReadLine` nie wolno wołać w lambdzie, którą wywołuje funkcja
> wbudowana (np. porównanie w `Sort`), w `ToString()` ani w korutynie – to kończy się czytelnym błędem.
