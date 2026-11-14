// Gfx.Layout: the logical screens (OneScreen 200x120 drawn twice as big on the top screen, Fill, back to TwoScreens),
// Gfx.Width/Height of the logical screen, the touch counted on the logical screen; Gfx.FrameRate(30) draws every
// second step while Update stays at 60 (opcje.txt: --frames 8 --touch 2@40,60 --touch 5@40,60 --hash;
// the first touch is on the bottom screen while only the top one is used, so it does not count)
class Game
{
    static int updates = 0, draws = 0;

    static void Main()
    {
        Console.WriteLine($"two: {Gfx.Width()}x{Gfx.Height()}");
        Gfx.Layout(Layout.Fill);
        Console.WriteLine($"fill: {Gfx.Width()}x{Gfx.Height()}");
        Gfx.Layout(Layout.OneScreen, 200, 120);
        Console.WriteLine($"one: {Gfx.Width()}x{Gfx.Height()}");
        Gfx.FrameRate(30);
    }

    static void Update()
    {
        updates++;
        if (Input.Touched()) Console.WriteLine($"touched screen {Input.TouchScreen()} at {Input.TouchX()},{Input.TouchY()} in frame {Time.Frame()}");
        if (Time.Frame() == 3)
        {
            Gfx.Layout(Layout.TwoScreens);
            Console.WriteLine($"two again: {Gfx.Width()}x{Gfx.Height()}");
        }
    }

    static void Draw()
    {
        draws++;
        if (Time.Frame() < 4)
        {
            Gfx.Clear(Color.DarkBlue);
            Gfx.Rect(0, 0, 10, 10, Color.Red);           // 20 x 20 real pixels at the top left
            Gfx.Circle(100, 60, 20, Color.Yellow);       // the middle of the 200 x 120 screen
        }
        else
        {
            Gfx.Screen(Screen.Bottom);
            Gfx.Clear(Color.Gray);
        }
        Console.WriteLine($"draw {draws} after {updates} updates");
    }
}
