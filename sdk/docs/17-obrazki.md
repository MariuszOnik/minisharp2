# Obrazki i sprite'y

Kółka i prostokąty wystarczą na początek, ale gra wygląda jak gra dopiero z obrazkami: postacią, potworami,
kafelkami mapy. Obrazek rysujesz w programie graficznym (np. w darmowym Aseprite, LibreSprite albo Piskel), zapisujesz
jako **PNG** i kładziesz w folderze programu na karcie (dla projektu – w jego folderze, może być podfolder).

## Wczytanie i rysowanie

```cs
class Gra
{
    static int bohater;
    static float x = 100, y = 100;

    static void Main()
    {
        bohater = Gfx.LoadImage("bohater.png");
    }

    static void Update()
    {
        if (Input.Held(Btn.Left)) x -= 2;
        if (Input.Held(Btn.Right)) x += 2;
    }

    static void Draw()
    {
        Gfx.Clear(Color.DarkGreen);
        Gfx.Image(bohater, x, y);
    }
}
```

- `Gfx.LoadImage("bohater.png")` wczytuje plik **raz**, w `Main`, i oddaje jego numer. Ten numer trzymasz w polu
  (`int`) i podajesz przy rysowaniu. Wczytywanie w `Draw` też zadziała (ten sam plik daje ten sam numer), ale `Main`
  to porządniejsze miejsce.
- `Gfx.Image(obrazek, x, y)` rysuje go tak, że jego lewy górny róg jest w `x, y`.
- Przezroczyste miejsca obrazka (kanał alfa w PNG) są przezroczyste także w grze – postać nie ma prostokątnego tła.
- `Gfx.ImageWidth(obrazek)` i `Gfx.ImageHeight(obrazek)` mówią, jak duży jest obrazek.

Gdy pliku nie ma, program zatrzyma się z błędem, który podaje pełną ścieżkę – łatwo sprawdzić literówkę.

## Powiększanie i odbicie

Mały obrazek (np. 16 × 16) na ekranie 400 × 240 jest malutki. Czwarta liczba to **skala**:

```text
Gfx.Image(bohater, x, y, 3);     // trzy razy większy, piksele zostają ostre (pixel art)
Gfx.Image(bohater, x, y, -3);    // to samo, ale odbity w poziomie – postać patrzy w drugą stronę
```

Ujemna skala odbija obrazek, a jego miejsce na ekranie się nie zmienia, więc „obrót” postaci w lewo i w prawo to tylko
zmiana znaku:

```text
float skala = patrzyWLewo ? -3 : 3;
Gfx.Image(bohater, x, y, skala);
```

## Arkusz sprite'ów i animacja

Animację zapisuje się zwykle jako **arkusz**: wszystkie klatki obok siebie w jednym pliku. Jeśli klatka ma 16 × 16
pikseli, a w arkuszu są cztery, plik ma 64 × 16. `Gfx.ImagePart` rysuje tylko kawałek obrazka:

```text
Gfx.ImagePart(obrazek, sx, sy, szer, wys, x, y);          // kawałek od (sx, sy) o rozmiarze szer × wys
Gfx.ImagePart(obrazek, sx, sy, szer, wys, x, y, skala);
```

Animacja to wybór kawałka według czasu:

```cs
class Gra
{
    static int arkusz;

    static void Main()
    {
        arkusz = Gfx.LoadImage("chodzenie.png");     // 4 klatki po 16 x 16
    }

    static void Draw()
    {
        Gfx.Clear(Color.Black);
        int klatka = (Time.Frame() / 8) % 4;         // nowa klatka co 8 klatek gry, 0 1 2 3 0 1 ...
        Gfx.ImagePart(arkusz, klatka * 16, 0, 16, 16, 180, 100, 3);
    }
}
```

`Time.Frame() / 8` rośnie o jeden co 8 klatek gry, a `% 4` zawija liczenie do czterech klatek animacji. Mniejsza
liczba niż 8 – szybsza animacja.

## Obrót i przezroczystość

```text
Gfx.ImageRot(obrazek, x, y, kąt, skala);   // x, y = środek; kąt w stopniach, zgodnie z ruchem wskazówek zegara
Gfx.Alpha(0.5f);                           // następne rysunki (także obrazki) w połowie przezroczyste
Gfx.Image(duch, x, y);
Gfx.Alpha(1);
```

`ImageRot` liczy od **środka** obrazka, więc kręcący się obrazek zostaje w miejscu – dobre dla monet, pocisków,
kół pojazdów.

## Gra z obrazkami: kolekcja monet

```cs
using System.Collections.Generic;

class Moneta
{
    public float X, Y;
    public Moneta(float x, float y) { X = x; Y = y; }
}

class Gra
{
    static int gracz, moneta;
    static float x = 50, y = 120;
    static bool wLewo = false;
    static List<Moneta> monety = new List<Moneta>();
    static int punkty = 0;

    static void Main()
    {
        gracz = Gfx.LoadImage("gracz.png");          // 16 x 16
        moneta = Gfx.LoadImage("moneta.png");        // 8 x 8
        for (int i = 0; i < 8; i++) monety.Add(new Moneta(80 + i * 35, Random.Range(40, 200)));
    }

    static void Update()
    {
        if (Input.Held(Btn.Left)) { x -= 2; wLewo = true; }
        if (Input.Held(Btn.Right)) { x += 2; wLewo = false; }
        if (Input.Held(Btn.Up)) y -= 2;
        if (Input.Held(Btn.Down)) y += 2;
        monety.RemoveAll(m => Mathf.Abs(m.X - (x + 24)) < 24 && Mathf.Abs(m.Y - (y + 24)) < 24 && Zbierz());
    }

    static bool Zbierz() { punkty++; return true; }

    static void Draw()
    {
        Gfx.Clear(Gfx.Rgb(30, 60, 30));
        foreach (Moneta m in monety)
            Gfx.ImageRot(moneta, m.X, m.Y, Time.Frame() * 4, 2);     // kręcące się monety
        Gfx.Image(gracz, x, y, wLewo ? -3 : 3);
        Gfx.Text(6, 4, $"Monety: {punkty}", Color.Yellow);
        if (monety.Count == 0) Gfx.Text(150, 110, "Wszystkie!", Color.White, 20);
    }
}
```

Monety zbiera `RemoveAll` z lambdą (zob. rozdział 13): usuwa te, które są blisko gracza, a przy okazji liczy punkty.

> **Dla zaawansowanych:** obrazek może mieć najwyżej 1024 × 1024 piksele; na 3DS każdy trafia do pamięci grafiki
> w rozmiarze zaokrąglonym w górę do potęgi dwójki (obrazek 100 × 20 zajmuje tyle co 128 × 32), więc arkusz kilku
> klatek jest tańszy niż wiele osobnych plików. Rysowanie bierze najbliższy piksel (bez rozmywania), co pasuje do
> pixel artu. Oprócz PNG działają też JPG, BMP, GIF (pierwsza klatka) i TGA. Numer obrazka to zwykły `int`; `0`
> znaczy „brak obrazka”. Obrazki znikają z pamięci, gdy program się kończy.
