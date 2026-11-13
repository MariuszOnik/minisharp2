# Pętle

Komputer nie nudzi się powtarzaniem. Pętla każe mu wykonać te same polecenia wiele razy: wypisać dziesięć linii,
policzyć sumę, czekać, aż gracz zgadnie liczbę. W tym rozdziale poznasz `for`, `while`, `do … while`, `foreach`
oraz `break` i `continue`.

## for – policz od…do

```cs
class Program
{
    static void Main()
    {
        for (int i = 1; i <= 5; i++)
        {
            Console.WriteLine("Okrążenie " + i);
        }
    }
}
```

```wynik
Okrążenie 1
Okrążenie 2
Okrążenie 3
Okrążenie 4
Okrążenie 5
```

W nawiasie `for` są trzy części rozdzielone średnikami:

- `int i = 1` – **start**: tworzy licznik, robi się raz, na początku.
- `i <= 5` – **warunek**: sprawdzany przed każdym okrążeniem; gdy jest fałszywy, pętla się kończy.
- `i++` – **krok**: robi się po każdym okrążeniu (zob. rozdział 3: `++` dodaje 1).

Licznik może iść w dół albo skakać co kilka:

```cs
class Program
{
    static void Main()
    {
        for (int i = 10; i > 0; i -= 3)
        {
            Console.Write(i + " ");
        }
        Console.WriteLine("Start!");
    }
}
```

```wynik
10 7 4 1 Start!
```

## Suma w pętli

Częsty wzór: zmienna przed pętlą zbiera wynik, pętla dokłada do niej po kawałku.

```cs
class Program
{
    static void Main()
    {
        int suma = 0;
        for (int i = 1; i <= 100; i++)
        {
            suma += i;
        }
        Console.WriteLine("1 + 2 + ... + 100 = " + suma);
    }
}
```

```wynik
1 + 2 + ... + 100 = 5050
```

Zmienna `i` istnieje tylko wewnątrz pętli. `suma` jest zadeklarowana przed pętlą, więc po pętli nadal ją widać.

## while – dopóki warunek jest prawdziwy

`while` nie ma licznika. Powtarza, dopóki warunek jest prawdziwy, i dobrze pasuje tam, gdzie nie wiadomo z góry,
ile będzie okrążeń.

```cs
class Program
{
    static void Main()
    {
        int zloto = 1;
        int dni = 0;
        while (zloto < 1000)
        {
            zloto *= 2;
            dni++;
        }
        Console.WriteLine("Po " + dni + " dniach masz " + zloto + " sztuk złota");
    }
}
```

```wynik
Po 10 dniach masz 1024 sztuk złota
```

Uważaj: jeśli warunek nigdy nie stanie się fałszywy, pętla kręci się bez końca. Zwykle w środku zapomniano coś
zmienić (tu: `zloto *= 2`). W terminalu taka pętla zostanie przerwana po kilku sekundach komunikatem
`stopped: still running after 5 seconds (endless loop?)`.
Zwykły program (nie w terminalu) nie ma tego limitu – pętla gry `while (true)` jest w nim w porządku.

## do … while – co najmniej raz

`do … while` sprawdza warunek **po** okrążeniu, więc polecenia wykonają się przynajmniej raz.

```cs
class Program
{
    static void Main()
    {
        int liczba = 1234;
        int cyfry = 0;
        do
        {
            liczba /= 10;
            cyfry++;
        } while (liczba > 0);
        Console.WriteLine("Cyfr: " + cyfry);
    }
}
```

```wynik
Cyfr: 4
```

Dla `liczba = 0` wynik też jest dobry (1 cyfra), bo pierwsze okrążenie odbywa się zawsze. Po `while (...)` na końcu
jest średnik.

## foreach – po kolei każdy element

`foreach` przechodzi po wszystkich elementach czegoś, co ma ich wiele: po znakach napisu, po liście, po tablicy
(listy i tablice poznasz w dalszych rozdziałach).

```cs
class Program
{
    static void Main()
    {
        string slowo = "smok";
        foreach (char znak in slowo)
        {
            Console.Write("[" + znak + "]");
        }
        Console.WriteLine();
    }
}
```

```wynik
[s][m][o][k]
```

Czytaj to tak: „dla każdego znaku `znak` w `slowo` zrób…”. Nie ma licznika i nie da się pomylić o jeden.

> **Dla zaawansowanych:** na razie `char` to jeden bajt, a polska litera (np. `ę`) zajmuje w napisie dwa bajty, więc
> `foreach` po słowie z polskimi literami poda je jako dwa dziwne znaki. Wypisywanie całych napisów działa normalnie.

## break i continue

- `break` – natychmiast kończy pętlę.
- `continue` – pomija resztę tego okrążenia i przechodzi do następnego.

```cs
class Program
{
    static void Main()
    {
        for (int i = 1; i <= 10; i++)
        {
            if (i % 2 == 0) continue;   // parzyste pomijamy
            if (i > 7) break;           // po 7 koniec
            Console.Write(i + " ");
        }
        Console.WriteLine();
    }
}
```

```wynik
1 3 5 7 
```

## Pętla w pętli

Pętle można zagnieżdżać. Wewnętrzna wykonuje się cała przy każdym okrążeniu zewnętrznej.

```cs
class Program
{
    static void Main()
    {
        for (int y = 1; y <= 3; y++)
        {
            for (int x = 1; x <= 4; x++)
            {
                Console.Write(x * y + "\t");
            }
            Console.WriteLine();
        }
    }
}
```

```wynik
1	2	3	4	
2	4	6	8	
3	6	9	12	
```

`break` w pętli wewnętrznej kończy tylko ją; zewnętrzna jedzie dalej.

## Pętle w terminalu

W terminalu (REPL) pętlę można wpisać w kilku liniach. Dopóki jakaś klamra `{` albo nawias `(` są otwarte albo
linia kończy się nagłówkiem typu `for (...)` bez treści, terminal pokazuje `.` zamiast `>` i czeka na resztę:

```text
> for (int i = 1; i <= 3; i++)
. {
.     Console.WriteLine("i = " + i);
. }
i = 1
i = 2
i = 3
```

Zmienne utworzone w terminalu (`int suma = 0;`) zostają na kolejne linie, więc wynik pętli można potem obejrzeć,
wpisując samą nazwę: `suma`.

> **Dla zaawansowanych:** w terminalu `else` wpisuj w tej samej linii co klamrę zamykającą: `} else {`. Linia
> zawierająca samo `}` kończy polecenie i terminal od razu je wykonuje, więc `else` w następnej linii nie miałoby się
> do czego dołączyć.
