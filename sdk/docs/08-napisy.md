# Napisy

Imię gracza, komunikat, linia z pliku – to wszystko **napisy**, typ `string`. Znasz je od pierwszego rozdziału
(`"Witaj!"`). Tu nauczysz się je sklejać, wstawiać w nie wartości, przeglądać znak po znaku, szukać w nich
i ciąć na kawałki.

## Sklejanie i wstawianie wartości

```cs
class Program
{
    static void Main()
    {
        string imie = "Ala";
        int poziom = 7;
        Console.WriteLine("Gracz " + imie + " ma poziom " + poziom);
        Console.WriteLine($"Gracz {imie} ma poziom {poziom}");
        Console.WriteLine($"Za dwa poziomy: {poziom + 2}");

        string linia = "";
        for (int i = 0; i < 5; i++)
        {
            linia += "*";
        }
        Console.WriteLine(linia);
    }
}
```

```wynik
Gracz Ala ma poziom 7
Gracz Ala ma poziom 7
Za dwa poziomy: 9
*****
```

- `+` skleja napisy. Gdy jedna strona jest napisem, liczba też zamienia się w tekst.
- `$"..."` to napis z dziurami: to, co w `{ }`, zostaje policzone i wstawione. Zwykle czyta się go łatwiej niż
  długi łańcuch plusów.
- `+=` dokleja na końcu – przydaje się w pętli.

## Znaki specjalne

Niektórych znaków nie da się wpisać wprost w cudzysłów. Zapisuje się je z ukośnikiem `\`:

```cs
class Program
{
    static void Main()
    {
        Console.WriteLine("Pierwsza linia\nDruga linia");
        Console.WriteLine("Imię:\tAla");
        Console.WriteLine("Powiedział: \"Cześć!\"");
        Console.WriteLine("Ścieżka: C:\\gry");
    }
}
```

```wynik
Pierwsza linia
Druga linia
Imię:	Ala
Powiedział: "Cześć!"
Ścieżka: C:\gry
```

`\n` – nowa linia, `\t` – tabulator, `\"` – cudzysłów w środku napisu, `\\` – sam ukośnik.

## Długość i pojedyncze znaki

Napis to ciąg **znaków** (`char`). Jak w tablicy (zob. rozdział 7): `Length` mówi, ile ich jest, a `s[i]` daje
znak o numerze `i`, licząc od zera. Pojedynczy znak zapisuje się w apostrofach: `'a'`.

```cs
class Program
{
    static void Main()
    {
        string slowo = "kajak";
        Console.WriteLine(slowo.Length);
        Console.WriteLine(slowo[0]);
        Console.WriteLine(slowo[slowo.Length - 1]);

        string wspak = "";
        for (int i = slowo.Length - 1; i >= 0; i--)
        {
            wspak += slowo[i];
        }
        Console.WriteLine(wspak);
        Console.WriteLine(wspak == slowo ? "palindrom" : "zwykłe słowo");

        int samogloski = 0;
        foreach (char c in "programowanie")
        {
            if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' || c == 'y') samogloski++;
        }
        Console.WriteLine("Samogłoski: " + samogloski);
    }
}
```

```wynik
5
k
k
kajak
palindrom
Samogłoski: 6
```

Napisów nie zmienia się w miejscu: `slowo[0] = 'K';` to błąd (`strings cannot be changed; build a new string
instead`). Zamiast tego buduje się nowy napis (jak `wspak`
wyżej) albo używa metod z następnej części.

Polskie litery to zwykłe znaki: `"żółw".Length` to 4, `"żółw"[1]` to `'ó'`, a `"zażółć".ToUpper()` daje
`"ZAŻÓŁĆ"`.

> **Dla zaawansowanych:** w pamięci napis jest zapisany w UTF-8 (polska litera zajmuje 2 bajty), ale `Length`,
> `s[i]`, `Substring` i `IndexOf` liczą znaki, nie bajty – tak jak w C#. `(int)'ę'` to 281, numer znaku w Unicode.
> Napis z samych znaków ASCII jest indeksowany od razu; w napisie z polskimi literami `s[i]` musi odliczyć znaki,
> ale pętla po kolejnych `i` pamięta ostatnie miejsce, więc też jest szybka.

## Przydatne metody

```cs
class Program
{
    static void Main()
    {
        string s = "  Miecz Ognia  ";
        string t = s.Trim();
        Console.WriteLine("[" + t + "]");
        Console.WriteLine(t.ToUpper());
        Console.WriteLine(t.ToLower());
        Console.WriteLine(t.Contains("Ogni"));
        Console.WriteLine(t.StartsWith("Miecz"));
        Console.WriteLine(t.EndsWith("Lodu"));
        Console.WriteLine(t.IndexOf("Ognia"));
        Console.WriteLine(t.IndexOf("Lodu"));
        Console.WriteLine(t.Substring(6));
        Console.WriteLine(t.Substring(0, 5));
        Console.WriteLine(t.Replace("Ognia", "Lodu"));
    }
}
```

```wynik
[Miecz Ognia]
MIECZ OGNIA
miecz ognia
True
True
False
6
-1
Ognia
Miecz
Miecz Lodu
```

- `Trim()` – bez spacji na początku i końcu.
- `ToUpper()`, `ToLower()` – wielkie / małe litery.
- `Contains(x)`, `StartsWith(x)`, `EndsWith(x)` – czy jest w środku / na początku / na końcu (`true`/`false`).
- `IndexOf(x)` – od którego znaku zaczyna się `x`; `-1`, gdy go nie ma.
- `Substring(od)` – kawałek od znaku `od` do końca; `Substring(od, ile)` – `ile` znaków od `od`.
- `Replace(co, na)` – nowy napis, w którym każde `co` zamieniono na `na`.

Żadna z tych metod nie zmienia `t` – każda oddaje **nowy** napis albo odpowiedź. Żeby zachować wynik, przypisz go:
`t = t.ToUpper();`.

## Cięcie na kawałki: Split

`Split` dzieli napis w miejscach podanego znaku i oddaje tablicę kawałków. Tak czyta się na przykład dane zapisane
w jednej linii:

```cs
class Program
{
    static void Main()
    {
        string linia = "Ala,12,łucznik";
        string[] pola = linia.Split(',');
        Console.WriteLine("Pól: " + pola.Length);
        string imie = pola[0];
        int poziom = int.Parse(pola[1]);
        Console.WriteLine($"{imie} (poziom {poziom + 1} po awansie), klasa: {pola[2]}");

        foreach (string slowo in "raz dwa trzy".Split(' '))
        {
            Console.WriteLine("- " + slowo);
        }
    }
}
```

```wynik
Pól: 3
Ala (poziom 13 po awansie), klasa: łucznik
- raz
- dwa
- trzy
```

`int.Parse` zamienia napis z cyframi na liczbę (zob. rozdział 2). Gdy napis może nie być liczbą, użyj
`int.TryParse` (zob. rozdział 6).

## Sklejanie kawałków: string.Join

`string.Join` robi odwrotność `Split`: skleja elementy tablicy (albo listy, zob. rozdział 12) w jeden napis,
wstawiając między nie podany separator. Liczby same zamieniają się na tekst:

```cs
class Program
{
    static void Main()
    {
        string[] slowa = "raz dwa trzy".Split(' ');
        Console.WriteLine(string.Join(" + ", slowa));

        int[] wyniki = { 10, 25, 7 };
        Console.WriteLine("Wyniki: " + string.Join(", ", wyniki));
        Console.WriteLine(string.Join("-", "2026", 11, 14));
    }
}
```

```wynik
raz + dwa + trzy
Wyniki: 10, 25, 7
2026-11-14
```

Separator stoi tylko **między** elementami – na końcu nie ma zbędnego przecinka. Zamiast tablicy można też podać
kilka wartości po przecinku, jak w ostatniej linii.

## Pusty napis i null

`""` to napis bez znaków (`Length` równe 0). `null` to brak napisu – na nim nie da się wywołać metody.
`string.IsNullOrEmpty(s)` sprawdza oba przypadki naraz:

```cs
class Program
{
    static void Main()
    {
        string a = "";
        string b = null;
        string c = "x";
        Console.WriteLine(string.IsNullOrEmpty(a));
        Console.WriteLine(string.IsNullOrEmpty(b));
        Console.WriteLine(string.IsNullOrEmpty(c));
    }
}
```

```wynik
True
True
False
```

## Podsumowanie

- `+` i `$"... {x} ..."` budują napisy; `\n`, `\t`, `\"`, `\\` to znaki specjalne.
- `s.Length` i `s[i]` – długość i znak, jak w tablicy; napisu nie zmienia się w miejscu.
- `Trim`, `ToUpper`, `Contains`, `IndexOf`, `Substring`, `Replace`, `Split` – każda oddaje nowy wynik.
- `string.Join(sep, tablica)` – skleja kawałki z separatorem.
- `string.IsNullOrEmpty(s)` – czy napis jest pusty albo go nie ma.
