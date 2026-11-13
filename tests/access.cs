// Access modifiers, the gentle version: no word = every class may use it; private = own class only;
// protected = own class and derived ones; internal = like no word; static class = only static members.
using System;

public static class Narzedzia {
    public const int Dwa = 2;
    private static int wywolania;
    public static int Podwoj(int x) { wywolania++; return x * Dwa; }
    public static int Wywolania => wywolania;
}

public class Licznik {
    private int ile;
    protected int krok = 1;
    int bezSlowa = 10;                          // no word: open to everyone (in C# it would be private)
    internal int wewnetrzne = 20;
    protected internal int obaSlowa = 30;
    private protected int tylkoPochodne = 40;
    public int Ile => ile;
    public int Punkty { get; private set; }
    public string Nazwa { get; protected set; } = "licznik";

    public void Dodaj() { ile += krok; Punkty += 10; }
    private void Zeruj() { ile = 0; }
    public void Reset() { Action a = Zeruj; a(); Func<int> f = () => ile + tylkoPochodne; Console.WriteLine(f()); }

    // a private overload is skipped from outside; the public one is picked
    private void Ustaw(int x) { ile = x; }
    public void Ustaw(float x) { Ustaw((int)x); }

    // a private member of another object of the same class is fine (as in C#)
    public bool Rowne(Licznik inny) { return inny.ile == ile; }
}

public class Podwojny : Licznik {
    public Podwojny() { krok = 2; Nazwa = "podwojny"; }
    public int Suma() { return krok + obaSlowa + tylkoPochodne; }
}

public class Gra {
    private static Gra jedyna;
    private Gra() { }
    public static Gra Jedyna() { if (jedyna == null) jedyna = new Gra(); return jedyna; }
    public int Poziom = 1;
}

public interface IGlos { string Glos(); }
public class Kot : IGlos { public string Glos() { return "miau"; } }

class Program {
    private static void Main() {
        Console.WriteLine(Narzedzia.Podwoj(21));
        Console.WriteLine(Narzedzia.Podwoj(Narzedzia.Dwa) + " " + Narzedzia.Wywolania);

        Licznik l = new Licznik();
        l.Dodaj(); l.Dodaj();
        Console.WriteLine(l.Ile + " " + l.Punkty + " " + l.Nazwa);
        Console.WriteLine(l.bezSlowa + l.wewnetrzne + l.obaSlowa);
        l.Ustaw(7.9f);
        Console.WriteLine(l.Ile);
        l.Reset();
        Console.WriteLine(l.Ile + " " + l.Rowne(new Licznik()));

        Podwojny p = new Podwojny();
        p.Dodaj();
        Console.WriteLine(p.Ile + " " + p.Suma() + " " + p.Nazwa);

        Gra g = Gra.Jedyna();
        g.Poziom = 3;
        Console.WriteLine(Gra.Jedyna().Poziom);

        IGlos k = new Kot();
        Console.WriteLine(k.Glos());
    }
}
