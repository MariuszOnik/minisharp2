// ref i out we własnych metodach (jak w C#): zmienne lokalne i parametry, out var, przeciążenia, lambdy, struktury.
using System;
using System.Collections.Generic;

struct Punkt { public int x, y; }

class Licznik {
    public int ile;
    public Licznik(ref int start) { ile = start; start = 0; }
    public void Dodaj(ref int suma) { suma += ile; }
}

class Program {
    static void Zamien(ref int a, ref int b) { int t = a; a = b; b = t; }
    static void Podwoj(ref string s) => s = s + s;
    static bool Dziel(int a, int b, out int wynik, out int reszta) {
        if (b == 0) { wynik = 0; reszta = 0; return false; }
        wynik = a / b;
        reszta = a % b;
        return true;
    }
    static void MinMax(int[] t, out int min, out int max) {
        min = t[0]; max = t[0];
        foreach (int x in t) {
            if (x < min) min = x;
            if (x > max) max = x;
        }
    }
    // przekazanie dalej: parametr ref/out idzie do innej metody
    static void Zwieksz(ref int x) { x++; }
    static void ZwiekszDwaRazy(ref int x) { Zwieksz(ref x); Zwieksz(ref x); }
    static void Ustaw(out int x) { Ustaw7(out x); }
    static void Ustaw7(out int x) { x = 7; }
    // out przypisany na każdej drodze switch / if
    static void Nazwa(int n, out string s) {
        switch (n) {
            case 1: s = "jeden"; break;
            case 2: s = "dwa"; break;
            default: s = "dużo"; break;
        }
    }
    static void Petla(out int n) {
        while (true) {
            n = 3;
            break;
        }
    }
    static void Przesun(ref Punkt p) { p.x += 10; }
    static void Nowy(out Punkt p) { p = new Punkt(); p.y = 5; }
    static void Lista(ref List<int> l) { l = new List<int> { 9 }; }

    // przeciążenia: z ref i bez
    static string Co(int a) => "int";
    static string Co(ref int a) => "ref int";

    static void Main() {
        int a = 1, b = 2;
        Zamien(ref a, ref b);
        Console.WriteLine(a + " " + b);

        string s = "ab";
        Podwoj(ref s);
        Console.WriteLine(s);

        if (Dziel(17, 5, out int w, out var r)) Console.WriteLine(w + " r " + r);
        int w2, r2;
        Console.WriteLine(Dziel(1, 0, out w2, out r2) + " " + w2);

        MinMax(new int[] { 4, -2, 9, 3 }, out int mn, out int mx);
        Console.WriteLine(mn + ".." + mx);

        int z = 5;
        ZwiekszDwaRazy(ref z);
        Console.WriteLine(z);
        Ustaw(out int u);
        Console.WriteLine(u);

        Nazwa(2, out string n2);
        Nazwa(5, out string n5);
        Console.WriteLine(n2 + " " + n5);
        Petla(out int p3);
        Console.WriteLine(p3);

        // zmienna przekazana przez ref i używana w lambdzie: jedna komórka
        int licz = 0;
        Action pokaz = () => Console.WriteLine("lambda widzi " + licz);
        Zwieksz(ref licz);
        pokaz();
        Action inc = () => Zwieksz(ref licz);
        inc(); inc();
        Console.WriteLine(licz);

        Punkt pt = new Punkt();
        Przesun(ref pt);
        Console.WriteLine(pt.x);
        Nowy(out Punkt q);
        Console.WriteLine(q.x + " " + q.y);

        List<int> l = null;
        Lista(ref l);
        Console.WriteLine(l[0]);

        int c = 1;
        Console.WriteLine(Co(c) + ", " + Co(ref c));

        int start = 4;
        Licznik k = new Licznik(ref start);
        int suma = 100;
        k.Dodaj(ref suma);
        Console.WriteLine(start + " " + suma);

        // TryGetValue do zmiennej, która jest w komórce
        Dictionary<string, int> d = new Dictionary<string, int> { { "a", 42 } };
        int v = 0;
        Zwieksz(ref v);
        d.TryGetValue("a", out v);
        Console.WriteLine(v);

        // int.TryParse / float.TryParse
        if (int.TryParse("123", out int liczba)) Console.WriteLine(liczba + 1);
        Console.WriteLine(int.TryParse("12x", out var zla) + " " + zla);
        int istn = 5;
        Console.WriteLine(int.TryParse(" -7 ", out istn) + " " + istn);
        Console.WriteLine(int.TryParse(null, out istn) + " " + istn);
        Console.WriteLine(float.TryParse("2.5", out float f) + " " + f);
    }
}
