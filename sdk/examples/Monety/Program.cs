// Monety – gra z rozdziału 17 („Obrazki i sprite'y”): zbierz kręcące się monety; obrazki gracz.png i moneta.png są obok.
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
