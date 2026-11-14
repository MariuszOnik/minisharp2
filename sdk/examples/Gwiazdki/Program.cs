// Gwiazdki – gra z rozdziału 16 („Pierwsza gra”): zbieraj gwiazdki krzyżakiem, B = bieg, 30 sekund.
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
