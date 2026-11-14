// using with an alias: a namespace (G.Kostka), a nested namespace, a class (V), inside a namespace block;
// ("using G = Gra;" stands for Gra, it does not open it as "using Gra;" does); same output as .NET
using G = Gra;
using P = Gra.Potwory;
using V = Gra.Wektor;
using System.Collections.Generic;

namespace Gra
{
    class Kostka { public static int Rzut() { return 4; } }
    class Wektor
    {
        public int X, Y;
        public Wektor(int x, int y) { X = x; Y = y; }
        public override string ToString() { return "(" + X + "," + Y + ")"; }
    }
}

namespace Gra.Potwory
{
    class Goblin { public int Atak() { return Kostka.Rzut() + 1; } }
    enum Rodzaj { Maly, Duzy }
}

namespace Inne
{
    using K = Gra.Kostka;
    class Test { public static int Ile() { return K.Rzut() * 10; } }
}

class Program
{
    static V Suma(V a, V b) { return new V(a.X + b.X, a.Y + b.Y); }

    static void Main()
    {
        Console.WriteLine(G.Kostka.Rzut());
        P.Goblin g = new P.Goblin();
        Console.WriteLine(g.Atak());
        V w = Suma(new V(1, 2), new Gra.Wektor(3, 4));
        Console.WriteLine(w);
        List<V> l = new List<V> { w };
        Console.WriteLine(l[0].X);
        Console.WriteLine(P.Rodzaj.Duzy);
        Console.WriteLine(Inne.Test.Ile());
    }
}
