using System;
using System.Collections.Generic;

// method overloading and several constructors (: this(...), : base(...)); same output as .NET
class Shape
{
    public string name;
    public float w, h;

    public Shape() : this("shape") { }
    public Shape(string name) : this(name, 1, 1) { }
    public Shape(string name, float side) : this(name, side, side) { }
    public Shape(string name, float w, float h)
    {
        this.name = name; this.w = w; this.h = h;
        Console.WriteLine("Shape(" + name + ", " + w + ", " + h + ")");
    }

    public virtual float Area() => w * h;
    public virtual float Area(float scale) => Area() * scale * scale;
    public override string ToString() => name + " " + w + "x" + h;
}

class Square : Shape
{
    public Square() : base("square", 2) { }
    public Square(float side) : base("square", side) { }
    public override float Area() => w * w;
}

class Printer
{
    public static string Show(int x) => "int " + x;
    public static string Show(float x) => "float " + x;
    public static string Show(string x) => "string " + x;
    public static string Show(bool x) => "bool " + x;
    public static string Show(Shape s) => "shape " + s;
    public static string Show(int a, int b) => "two ints " + (a + b);
    public static string Show(float a, int b) => "float+int " + (a + b);
    public static string Show(List<int> l) => "list of " + l.Count;
    public static string Show(int[] a) => "array of " + a.Length;

    public static int Apply(Func<int, int> f, int x) => f(x);
    public static int Apply(Func<int, int, int> f, int x) => f(x, x + 1);
    static int Twice(int v) => v * 2;
    static int Twice(int a, int b) => a * b;

    public int total;
    public void Add(int x) { total += x; }
    public void Add(int x, int y) { Add(x); Add(y); }
    public void Add(string s) { total += s.Length; }

    public static void Run()
    {
        Console.WriteLine(Show(1));
        Console.WriteLine(Show(1.5f));
        Console.WriteLine(Show("s"));
        Console.WriteLine(Show(true));
        Console.WriteLine(Show('A'));
        Console.WriteLine(Show(new Square()));
        Console.WriteLine(Show(2, 3));
        Console.WriteLine(Show(2.5f, 3));
        Console.WriteLine(Show(new List<int> { 1, 2 }));
        Console.WriteLine(Show(new int[] { 1, 2, 3 }));
        Console.WriteLine(Apply(x => x + 1, 5));
        Console.WriteLine(Apply((a, b) => a * b, 5));
        Func<int, int> one = Twice;
        Func<int, int, int> two = Twice;
        Console.WriteLine(one(10) + " " + two(3, 4));
        var p = new Printer();
        p.Add(1); p.Add(2, 3); p.Add("four");
        Console.WriteLine(p.total);
    }
}

class Program
{
    static void Main()
    {
        var a = new Shape();
        var b = new Shape("rect", 2, 3);
        var c = new Shape("sq", 4);
        var d = new Square();
        var e = new Square(5);
        Shape[] all = new Shape[] { a, b, c, d, e };
        foreach (var s in all) Console.WriteLine(s + " area " + s.Area() + " scaled " + s.Area(2));
        Printer.Run();
    }
}
