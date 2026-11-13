using System;
using System.Collections.Generic;

// string interpolation $"...": any type in the holes, braces, escapes, nested strings; same output as .NET
enum Color2 { Red, Green }

class Point
{
    public int x, y;
    public Point(int x, int y) { this.x = x; this.y = y; }
    public override string ToString() => $"({x}, {y})";
}

class Program
{
    static string Name(int i) => i == 0 ? "zero" : "many";

    static void Main()
    {
        int score = 42;
        float t = 1.5f;
        bool ok = true;
        char c = 'Z';
        string s = "text";
        string nothing = null;
        Console.WriteLine($"score: {score}");
        Console.WriteLine($"{score}");
        Console.WriteLine($"");
        Console.WriteLine($"no holes");
        Console.WriteLine($"{t} {ok} {c} {s} [{nothing}]");
        Console.WriteLine($"{score + 8} {score * 2 - 1} {score / 5} {score % 5}");
        Console.WriteLine($"{(score > 40 ? "high" : "low")} {Name(0)} {Name(3)}");
        Console.WriteLine($"{{literal braces}} {{{score}}}");
        Console.WriteLine($"tab\tnew\\line \"quoted\"");
        Console.WriteLine($"{new Point(1, 2)} and {new Point(3, -4)}");
        Console.WriteLine($"{Color2.Green} {(int)Color2.Green}");
        var list = new List<int> { 1, 2, 3 };
        Console.WriteLine($"count={list.Count} first={list[0]} last={list[list.Count - 1]}");
        Console.WriteLine($"{"inner"} {$"nested {score}"}");
        Console.WriteLine($"{s.ToUpper()}{s.Length}{s[0]}");
        Func<int, string> f = n => $"<{n}>";
        Console.WriteLine(f(7) + $"{f(8)}");
        string built = "";
        for (int i = 0; i < 3; i++) built += $"{i},";
        Console.WriteLine(built);
        Console.WriteLine($"a" + $"b{1}" + 2);
        Console.WriteLine($"{nothing}".Length);
    }
}
