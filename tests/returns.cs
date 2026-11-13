// Każda droga metody z wynikiem kończy się 'return' (jak w C#) – przypadki, które kompilator musi przyjąć.
using System;

class Postac {
    int hp = 5;
    public int Hp { get { if (hp > 0) return hp; else return 0; } }
    public string Stan => hp > 0 ? "żyje" : "padł";
}

class Program {
    static int Znak(int x) {
        if (x > 0) return 1;
        else if (x < 0) return -1;
        else return 0;
    }
    static int PierwszyParzysty(int[] t) {
        int i = 0;
        while (true) {
            if (i >= t.Length) return -1;
            if (t[i] % 2 == 0) return t[i];
            i++;
        }
    }
    static int Petla() {
        for (int i = 0; ; i++) {
            if (i * i > 50) return i;
        }
    }
    static int PetlaZeSwitchem(int n) {
        while (true) {
            switch (n) {
                case 0: break;          // wychodzi ze switch, nie z pętli
                default: n--; continue;
            }
            return 100;
        }
    }
    static string Nazwa(int d) {
        switch (d) {
            case 1: return "jeden";
            case 2: { return "dwa"; }
            default: return "dużo";
        }
    }
    static int Do(int n) {
        do { n++; } while (true);
    }
    static int Stala() {
        if (true) return 7;
    }
    static int Zagniezdzone(int n) {
        while (true) {
            while (n > 10) { n -= 10; break; }     // break wewnętrznej pętli nie kończy zewnętrznej
            if (n < 5) return n;
            n--;
        }
    }
    static void Main() {
        Console.WriteLine(Znak(-4) + " " + Znak(0) + " " + Znak(9));
        Console.WriteLine(PierwszyParzysty(new int[] { 3, 5, 8, 9 }) + " " + PierwszyParzysty(new int[] { 1 }));
        Console.WriteLine(Petla());
        Console.WriteLine(PetlaZeSwitchem(3));
        Console.WriteLine(Nazwa(1) + " " + Nazwa(2) + " " + Nazwa(5));
        Console.WriteLine(Stala() + " " + Zagniezdzone(27));
        Postac p = new Postac();
        Console.WriteLine(p.Hp + " " + p.Stan);
        Func<int, int> abs = x => { if (x < 0) return -x; return x; };
        Console.WriteLine(abs(-3));
        if (false) Console.WriteLine(Do(0));
    }
}
