// Parametry z wartością domyślną (jak w C#): stałe, const, enum, przeciążenia, konstruktory, base(...).
using System;

enum Kolor { Czerwony, Zielony, Niebieski }

class Ustawienia {
    public const int MAKS = 99;
    public const string IMIE = "anonim";
}

class Bazowa {
    public int hp;
    public Bazowa(int hp = 10) { this.hp = hp; }
}

class Pochodna : Bazowa {
    public Pochodna() { }                    // niejawne base() z wartością domyślną
}

class Druga : Bazowa {
    public Druga() : base(7) { }
}

class Punkt {
    public float x, y;
    public Punkt(float x = 1, float y = 2.5f) { this.x = x; this.y = y; }
}

class Program {
    static int Dodaj(int a, int b = 10, int c = 100) => a + b + c;
    static string Powitaj(string imie = Ustawienia.IMIE, char znak = '!') => "Cześć, " + imie + znak;
    static string Opis(Kolor k = Kolor.Niebieski, bool duzy = false) => (duzy ? "DUŻY " : "") + k;
    static int Limit(int n = Ustawienia.MAKS) => n;
    static string Tekst(string s = null) => s ?? "(null)";
    static float Pol(float x = 3) => x / 2;
    static int Ujemna(int x = -5) => x;

    // przeciążenia: wygrywa wersja bez użycia wartości domyślnych
    static string Rys(int a) => "Rys(int)";
    static string Rys(int a, int b = 2) => "Rys(int, int = 2)";

    int licznik = 1;
    int Zwieksz(int o = 1) { licznik += o; return licznik; }

    static void Main() {
        Console.WriteLine(Dodaj(1));
        Console.WriteLine(Dodaj(1, 2));
        Console.WriteLine(Dodaj(1, 2, 3));
        Console.WriteLine(Powitaj());
        Console.WriteLine(Powitaj("Ola"));
        Console.WriteLine(Powitaj("Ola", '?'));
        Console.WriteLine(Opis());
        Console.WriteLine(Opis(Kolor.Zielony, true));
        Console.WriteLine(Limit());
        Console.WriteLine(Tekst());
        Console.WriteLine(Pol());
        Console.WriteLine(Ujemna());
        Console.WriteLine(Rys(1));
        Console.WriteLine(Rys(1, 5));
        Program p = new Program();
        p.Zwieksz();
        Console.WriteLine(p.Zwieksz(5));
        Console.WriteLine(new Bazowa().hp);
        Console.WriteLine(new Pochodna().hp);
        Console.WriteLine(new Druga().hp);
        Punkt pt = new Punkt();
        Console.WriteLine(pt.x + " " + pt.y);
        pt = new Punkt(4);
        Console.WriteLine(pt.x + " " + pt.y);
    }
}
