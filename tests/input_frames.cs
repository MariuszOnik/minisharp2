// Input on the PC host: --press / --touch in opcje.txt hold buttons and touch the screen in given frames;
// Held / Pressed / Released, Touched / Touching, Time.Frame and Time.Delta, App.Quit
class Game
{
    static int x = 0;

    static void Update()
    {
        int f = Time.Frame();
        if (Input.Held(Btn.Right)) x++;
        if (Input.Pressed(Btn.A)) Console.WriteLine("A pressed in frame " + f);
        if (Input.Released(Btn.A)) Console.WriteLine("A released in frame " + f);
        if (Input.Held(Btn.A) && Input.Held(Btn.B)) Console.WriteLine("A+B in frame " + f);
        if (Input.Touched()) Console.WriteLine("touched at " + Input.TouchX() + "," + Input.TouchY() + " in frame " + f);
        if (Input.Touching() && !Input.Touched()) Console.WriteLine("still touching in frame " + f);
        if (f == 0) Console.WriteLine("delta " + (Time.Delta() > 0.016f && Time.Delta() < 0.017f));
        if (f == 12) { Console.WriteLine("x = " + x + ", quit"); App.Quit(); }
        if (f > 12) Console.WriteLine("never");
    }
}
