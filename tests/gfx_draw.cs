// 2D drawing on both screens (the PC host draws in memory; --hash prints a hash of the picture, opcje.txt):
// Clear, Rect, RectLine, Circle with Alpha, Line, Pixel, Text, Rgb, Lerp, Width/Height, TextWidth
class Game
{
    static void Main()
    {
        Console.WriteLine(Gfx.Rgb(255, 0, 128) + " " + Gfx.Rgb(300, -5, 16));
        Console.WriteLine(Gfx.Lerp(Color.Black, Color.White, 0.5f) + " " + Gfx.Lerp(0x000000, 0xFF0000, 2));
        Gfx.Screen(Screen.Bottom);
        Console.WriteLine(Gfx.Width() + "x" + Gfx.Height());
        Gfx.Screen(Screen.Top);
        Console.WriteLine(Gfx.Width() + "x" + Gfx.Height() + " " + Gfx.TextWidth("abc") + " " + Gfx.TextWidth("ab", 20));
    }

    static void Draw()
    {
        Gfx.Screen(Screen.Top);
        Gfx.Clear(Color.DarkBlue);
        Gfx.Rect(10, 10, 100, 50, Color.Red);
        Gfx.Alpha(0.5f);
        Gfx.Circle(110, 120, 30, Color.Yellow);
        Gfx.Alpha(1);
        Gfx.Line(0, 0, 399, 239, Color.White, 3);
        Gfx.Line(0, 239, 100, 200, Color.Cyan);
        Gfx.Pixel(399, 0, Color.Magenta);
        Gfx.Text(5, 200, "Hej", Color.Green);
        Gfx.Screen(Screen.Bottom);
        Gfx.Clear(Gfx.Rgb(30, 30, 30));
        Gfx.RectLine(20, 20, 280, 200, Color.Orange, 4);
        Gfx.RectLine(100, 100, 10, 10, Color.White);
    }
}
