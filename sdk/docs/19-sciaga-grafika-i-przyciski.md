# Ściąga: grafika i przyciski

Wszystko, czym gra rysuje i czyta przyciski, w jednym miejscu. Jak tego używać krok po kroku, opisują rozdział 16
(Update i Draw) i rozdział 17 (obrazki). Tutaj jest pełna lista do szybkiego sprawdzenia.

Liczby z przecinkiem (`float`) można podawać także jako zwykłe liczby całkowite: `Gfx.Rect(10, 20, 30, 40, Color.Red)`.
Kolor to zwykły `int` (0xRRGGBB), więc można go trzymać w polu albo zmiennej.

## Rysowanie kształtów i tekstu

```text
Gfx.Clear(kolor)                               cały wybrany ekran jednym kolorem
Gfx.Rect(x, y, szer, wys, kolor)               wypełniony prostokąt, x, y = lewy górny róg
Gfx.RectLine(x, y, szer, wys, kolor)           ramka prostokąta, grubość 1
Gfx.RectLine(x, y, szer, wys, kolor, grubość)  ramka o wybranej grubości
Gfx.Circle(x, y, promień, kolor)               wypełnione koło, x, y = środek
Gfx.Line(x1, y1, x2, y2, kolor)                linia, grubość 1
Gfx.Line(x1, y1, x2, y2, kolor, grubość)       linia o wybranej grubości
Gfx.Pixel(x, y, kolor)                         jeden piksel
Gfx.Text(x, y, "napis", kolor)                 tekst, wielkość 14
Gfx.Text(x, y, "napis", kolor, wielkość)       tekst o wybranej wielkości
Gfx.TextWidth("napis")                         szerokość napisu w pikselach (float), wielkość 14
Gfx.TextWidth("napis", wielkość)               szerokość napisu o wybranej wielkości
```

`Gfx.TextWidth` przydaje się do wyśrodkowania: `Gfx.Text((Gfx.Width() - Gfx.TextWidth(s)) / 2, 100, s, Color.White)`.

## Kolory

```text
Color.Black  Color.White  Color.Red  Color.Green  Color.Blue  Color.Yellow
Color.Cyan  Color.Magenta  Color.Orange  Color.Purple  Color.Brown  Color.Pink
Color.Gray  Color.LightGray  Color.DarkGray  Color.DarkBlue  Color.DarkGreen  Color.DarkRed

Gfx.Rgb(r, g, b)               własny kolor, składowe 0..255 (większe i mniejsze są przycinane)
Gfx.Lerp(kolor1, kolor2, t)    kolor pomiędzy: t = 0 pierwszy, t = 1 drugi, 0.5 – w połowie
Gfx.Alpha(a)                   przezroczystość następnych rysunków: 1 – pełne, 0.5 – w połowie, 0 – niewidoczne
```

```cs
class Program
{
    static void Main()
    {
        Console.WriteLine(Gfx.Rgb(255, 0, 0) == Color.Red);
        Console.WriteLine(Gfx.Rgb(300, -5, 0) == Color.Red);
        Console.WriteLine(Gfx.Lerp(Color.Black, Color.White, 0.5f) == Gfx.Rgb(128, 128, 128));
        Console.WriteLine(Gfx.Lerp(Color.Red, Color.Blue, 0) == Color.Red);
    }
}
```

```wynik
True
True
True
True
```

`Gfx.Lerp` dobrze robi pasek życia: `Gfx.Lerp(Color.Red, Color.Green, zycie / 100f)` jest czerwony przy zerze
i zielony przy pełnym życiu. Przezroczystość wraca do 1 na początku każdej klatki.

## Obrazki

```text
Gfx.LoadImage("plik.png")                              wczytuje obrazek, oddaje jego numer (int); 0 = brak
Gfx.Image(obr, x, y)                                   rysuje, x, y = lewy górny róg
Gfx.Image(obr, x, y, skala)                            powiększony; ujemna skala odbija w poziomie
Gfx.ImagePart(obr, sx, sy, szer, wys, x, y)            kawałek obrazka (klatka z arkusza)
Gfx.ImagePart(obr, sx, sy, szer, wys, x, y, skala)     kawałek, powiększony
Gfx.ImageRot(obr, x, y, kąt, skala)                    obrócony; x, y = środek, kąt w stopniach
Gfx.ImageWidth(obr)                                    szerokość obrazka w pikselach
Gfx.ImageHeight(obr)                                   wysokość obrazka w pikselach
```

Plik szukany jest w folderze programu (albo projektu). Działają PNG, JPG, BMP, GIF i TGA; program może mieć
najwyżej 256 obrazków, każdy do 1024 × 1024 pikseli (zob. rozdział 17).

## Ekrany i tempo

```text
Gfx.Screen(Screen.Top)                   rysowanie na górnym ekranie (400 × 240) – tak zaczyna się każda klatka
Gfx.Screen(Screen.Bottom)                rysowanie na dolnym ekranie (320 × 240, dotykowy)
Gfx.Width()   Gfx.Height()               rozmiar wybranego ekranu
Gfx.Layout(Layout.TwoScreens)            dwa ekrany jak na 3DS (domyślnie)
Gfx.Layout(Layout.OneScreen, szer, wys)  jeden ekran o wybranym rozmiarze (16..4096), powiększony do wyświetlacza
Gfx.Layout(Layout.Fill)                  jeden ekran w rozmiarze wyświetlacza
Gfx.FrameRate(n)                         Draw 60, 30, 20 albo 15 razy na sekundę; Update zawsze 60
```

Przy układzie z jednym ekranem `Gfx.Screen(Screen.Bottom)` jest błędem – nie ma dolnego ekranu.

## Przyciski

```text
Input.Held(przycisk)        trzymany (prawda w każdej klatce, dopóki trzymasz)
Input.Pressed(przycisk)     naciśnięty w tej klatce (raz na naciśnięcie)
Input.Released(przycisk)    puszczony w tej klatce

Btn.A  Btn.B  Btn.X  Btn.Y  Btn.L  Btn.R  Btn.Start  Btn.Select
Btn.Up  Btn.Down  Btn.Left  Btn.Right
```

`Pressed` i `Released` porównują tę klatkę z poprzednią, więc sprawdzaj je w `Update()` – tam każda klatka jest
liczona dokładnie raz.

## Dotyk

```text
Input.Touching()       palec albo rysik jest na ekranie
Input.Touched()        dotknięcie zaczęło się w tej klatce
Input.TouchX()         gdzie – w pikselach ekranu logicznego
Input.TouchY()
Input.TouchScreen()    na którym ekranie: 0 = Screen.Top, 1 = Screen.Bottom
```

Na 3DS dotykowy jest tylko dolny ekran. Przy `Layout.OneScreen` i `Layout.Fill` dotyk jest przeliczany na piksele
tego jednego ekranu, więc `TouchX` i `TouchY` pasują do tego, co rysujesz.

## Czas i koniec programu

```text
Time.Frame()     numer kroku Update: 0, 1, 2, … (60 na sekundę)
Time.Delta()     czas jednego kroku w sekundach: 1/60
Time.Millis()    prawdziwe milisekundy od startu programu
App.Quit()       koniec programu (także START+SELECT)
```

## Przydatne przy grach

```text
Random.Range(od, do)          losowa liczba od..do-1 (int)
Random.Next(n)                losowa liczba 0..n-1
Random.Float()                losowa liczba 0..1 (float)
Random.Seed(n)                ten sam ciąg losowań przy każdym uruchomieniu
Mathf.Clamp(x, min, max)      przytnij do zakresu
Mathf.Lerp(a, b, t)           wartość pomiędzy (płynny ruch kamery, zanikanie)
Mathf.MoveTowards(a, b, krok) zbliż a do b najwyżej o krok
Mathf.Sin(r)  Mathf.Cos(r)    kąt w radianach; stopnie * Mathf.Deg2Rad
Mathf.Atan2(dy, dx)           kąt do celu, w radianach
```

> **Dla zaawansowanych:** funkcje `Gfx` wołane poza `Draw()` (np. w `Main` albo `Update`) nie psują programu, ale
> obraz jest czyszczony przed każdą klatką, więc widać tylko to, co narysował `Draw()`. `Gfx.LoadImage`,
> `Gfx.Layout` i `Gfx.FrameRate` najlepiej wołać raz, w `Main`. Pełną listę funkcji z typami parametrów pokazują
> podpowiedzi edytora po wpisaniu kropki, np. `Gfx.`.
