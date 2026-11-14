# Pierwsza gra: Update i Draw

Do tej pory program robił swoje w `Main` i się kończył. Gra działa inaczej: **kręci się w kółko**, około 60 razy na
sekundę. W każdym obrocie (to jest **klatka**) sprawdza przyciski, przesuwa rzeczy i rysuje ekran od nowa. Wystarczą
do tego dwie metody o specjalnych nazwach:

- `static void Update()` – logika: przyciski, ruch, zderzenia, punkty;
- `static void Draw()` – rysowanie: co widać na ekranie w tej klatce.

Aplikacja woła je sama, klatka po klatce, aż program się zakończy (`App.Quit()`) albo naciśniesz START+SELECT.
`Main` dalej działa – raz, na samym początku, więc to dobre miejsce na przygotowanie gry.

## Kulka, którą sterujesz

```cs
class Gra
{
    static float x = 200, y = 120;

    static void Update()
    {
        if (Input.Held(Btn.Left)) x -= 3;
        if (Input.Held(Btn.Right)) x += 3;
        if (Input.Held(Btn.Up)) y -= 3;
        if (Input.Held(Btn.Down)) y += 3;
    }

    static void Draw()
    {
        Gfx.Clear(Color.DarkBlue);
        Gfx.Circle(x, y, 12, Color.Yellow);
        Gfx.Text(5, 5, "Steruj krzyżakiem", Color.White);
    }
}
```

- `x` i `y` to **pola** klasy, nie zmienne w metodzie – muszą przetrwać od klatki do klatki.
- `Input.Held(Btn.Left)` jest prawdą, dopóki trzymasz przycisk. Kulka jedzie 3 piksele na klatkę, czyli 180 na sekundę.
- `Gfx.Clear` maluje cały ekran jednym kolorem. Bez niego zostawałyby ślady poprzednich klatek.
- Górny ekran ma 400 × 240 pikseli, punkt (0, 0) to **lewy górny** róg; `y` rośnie w dół.

## Rysowanie

Wszystko, czym rysujesz, ma na końcu kolor:

```text
Gfx.Clear(kolor)                        cały ekran
Gfx.Rect(x, y, szer, wys, kolor)        prostokąt (x, y = lewy górny róg)
Gfx.RectLine(x, y, szer, wys, kolor)    sama ramka (opcjonalnie grubość na końcu)
Gfx.Circle(x, y, promień, kolor)        koło (x, y = środek)
Gfx.Line(x1, y1, x2, y2, kolor)         linia (opcjonalnie grubość na końcu)
Gfx.Pixel(x, y, kolor)                  jeden piksel
Gfx.Text(x, y, "napis", kolor)          tekst (opcjonalnie wielkość na końcu)
```

Kolory gotowe to `Color.Red`, `Color.Green`, `Color.Blue`, `Color.Yellow`, `Color.White`, `Color.Black`, `Color.Gray`,
`Color.Orange`, `Color.Purple`, `Color.Pink`, `Color.Brown` i inne. Własny kolor zrobisz z trzech składowych 0–255:
`Gfx.Rgb(255, 128, 0)`. `Gfx.Alpha(0.5f)` sprawia, że następne rysunki są półprzezroczyste (`Gfx.Alpha(1)` wraca).

Konsola ma dwa ekrany. `Gfx.Screen(Screen.Bottom)` przełącza rysowanie na dolny (320 × 240, dotykowy),
`Gfx.Screen(Screen.Top)` z powrotem na górny. Każda klatka zaczyna się na górnym ekranie, bez przezroczystości. `Gfx.Width()` i `Gfx.Height()` mówią, jak duży jest wybrany ekran.

## Przyciski i dotyk

- `Input.Held(Btn.A)` – przycisk jest trzymany (ruch, strzelanie seriami);
- `Input.Pressed(Btn.A)` – naciśnięty **w tej klatce** (skok, wybór w menu – raz na naciśnięcie);
- `Input.Released(Btn.A)` – puszczony w tej klatce.

Przyciski: `Btn.A`, `Btn.B`, `Btn.X`, `Btn.Y`, `Btn.L`, `Btn.R`, `Btn.Start`, `Btn.Select`, `Btn.Up`, `Btn.Down`,
`Btn.Left`, `Btn.Right`. Dolny ekran: `Input.Touching()` (palec / rysik na ekranie), `Input.Touched()` (dotknięcie w tej
klatce), `Input.TouchX()` i `Input.TouchY()` (gdzie).

## Czas

```cs
class Gra
{
    static int razy = 0;

    static void Update()
    {
        if (Time.Frame() % 20 == 0)                 // co 20 klatek, czyli trzy razy na sekundę
        {
            Console.WriteLine($"klatka {Time.Frame()}, minęło {Time.Frame() / 60f} s");
            razy++;
        }
        if (razy == 3) App.Quit();
    }
}
```

```wynik
klatka 0, minęło 0 s
klatka 20, minęło 0.3333333 s
klatka 40, minęło 0.6666667 s
```

- `Time.Frame()` – numer klatki (0, 1, 2, …); 60 klatek to sekunda.
- `Time.Delta()` – ile sekund trwała ostatnia klatka (zwykle 1/60). Ruch `x += 180 * Time.Delta()` jest „180 pikseli na
  sekundę” niezależnie od tego, czy gra zwolni.
- `Time.Millis()` – milisekundy od startu programu.
- `App.Quit()` – koniec programu.

`Console.WriteLine` działa także w grze: tekst trafia do dziennika (`log.txt`), a gdy program się skończy, widać go
na ekranie konsoli.

## Gra: zbierz gwiazdki

Wszystko razem: gracz, gwiazdki w losowych miejscach, zderzenie, punkty i koniec gry.

```cs
using System.Collections.Generic;

class Gwiazdka
{
    public float X, Y;
    public Gwiazdka() { Nowe(); }
    public void Nowe() { X = Random.Range(20, 380); Y = Random.Range(40, 220); }
}

class Gra
{
    static float x = 200, y = 120;
    static int punkty = 0;
    static int czas = 30 * 60;                       // 30 sekund w klatkach
    static List<Gwiazdka> gwiazdki = new List<Gwiazdka>();

    static void Main()
    {
        for (int i = 0; i < 5; i++) gwiazdki.Add(new Gwiazdka());
    }

    static void Update()
    {
        if (czas == 0)
        {
            if (Input.Pressed(Btn.Start)) App.Quit();
            return;
        }
        czas--;
        float v = Input.Held(Btn.B) ? 5 : 3;        // B = bieg
        if (Input.Held(Btn.Left)) x -= v;
        if (Input.Held(Btn.Right)) x += v;
        if (Input.Held(Btn.Up)) y -= v;
        if (Input.Held(Btn.Down)) y += v;
        x = Mathf.Clamp(x, 10, 390);
        y = Mathf.Clamp(y, 30, 230);

        foreach (Gwiazdka g in gwiazdki)
        {
            float dx = g.X - x, dy = g.Y - y;
            if (dx * dx + dy * dy < 18 * 18)          // odległość mniejsza niż 18 pikseli
            {
                punkty++;
                g.Nowe();
            }
        }
    }

    static void Draw()
    {
        Gfx.Clear(Color.Black);
        foreach (Gwiazdka g in gwiazdki) Gfx.Circle(g.X, g.Y, 6, Color.Yellow);
        Gfx.Circle(x, y, 10, Color.Cyan);
        Gfx.Rect(0, 0, 400, 22, Color.DarkBlue);
        Gfx.Text(6, 4, $"Punkty: {punkty}", Color.White);
        Gfx.Text(300, 4, $"Czas: {czas / 60}", Color.White);
        if (czas == 0) Gfx.Text(120, 110, "Koniec! START = wyjście", Color.Orange, 18);

        Gfx.Screen(Screen.Bottom);
        Gfx.Clear(Color.DarkGray);
        Gfx.Text(20, 100, "Zbieraj gwiazdki krzyżakiem, B = bieg", Color.White);
    }
}
```

Zderzenie liczymy po kwadracie odległości: `dx * dx + dy * dy < 18 * 18` znaczy to samo co „odległość < 18”, a nie
potrzebuje pierwiastka. Gwiazdka nie znika z listy – dostaje nowe miejsce (`g.Nowe()`), więc w `foreach` nic nie
usuwamy (zob. rozdział 12).

> **Dla zaawansowanych:** w każdej klatce aplikacja woła najpierw `Update()` wszystkich klas, które ją mają, potem
> korutyny, potem `Draw()`. Kolejność klas ustala atrybut `[Order(n)]`. Ekrany są czyszczone na czarno przed każdą
> klatką. Program bez `Draw()`, za to z `Update()`, też dostaje klatki – wtedy na ekranie widać jego konsolę (gry
> tekstowe). Na PC testy uruchamia się z `--frames 120 --press A@10 --press Left@5-20`, a `--ppm obraz.ppm` zapisuje
> zrzut obu ekranów.
