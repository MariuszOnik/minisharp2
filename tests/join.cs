using System.Collections.Generic;

enum Kolor { Czerwony, Zielony }

class Punkt
{
    public int X, Y;
    public Punkt(int x, int y) { X = x; Y = y; }
    public override string ToString() { return "(" + X + "," + Y + ")"; }
}

class Pusty { }

class Program
{
    static List<int> Liczby() { Console.WriteLine("Liczby() raz"); return new List<int> { 1, 2, 3 }; }

    static void Main()
    {
        Console.WriteLine(string.Join(", ", new List<string> { "a", "b", "c" }));
        Console.WriteLine(string.Join("-", new int[] { 1, 2, 3 }));
        Console.WriteLine(string.Join(" ", new List<float> { 1.5f, -2, 0.25f }));
        Console.WriteLine(string.Join("", new List<bool> { true, false }));
        Console.WriteLine(string.Join('|', new char[] { 'x', 'ż', 'z' }));
        Console.WriteLine(string.Join("; ", new List<Punkt> { new Punkt(1, 2), new Punkt(3, 4) }));
        Console.WriteLine(string.Join(",", new List<Kolor> { Kolor.Zielony, Kolor.Czerwony }));
        Console.WriteLine(string.Join(",", new Pusty[] { new Pusty() }));
        Console.WriteLine("[" + string.Join(", ", new List<string>()) + "]");
        Console.WriteLine("[" + string.Join(", ", new List<string> { "a", null, "c" }) + "]");
        Console.WriteLine(string.Join(null, new List<int> { 4, 5 }));
        Console.WriteLine(string.Join(" + ", 1, "dwa", 3.5f, new Punkt(0, 0)));
        Console.WriteLine(string.Join(", ", "tylko jeden"));
        Console.WriteLine(string.Join("/", Liczby()));
        string[] slowa = "ala ma kota".Split(' ');
        Console.WriteLine(string.Join("_", slowa).ToUpper());
        int suma = 0;
        foreach (int x in new List<int> { 1, 2 }) { string s = string.Join(",", new List<int> { x, x }); suma += s.Length; }
        Console.WriteLine(suma);
    }
}
