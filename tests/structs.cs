using System;
using System.Collections.Generic;

// struct: copied when assigned, passed or stored; fields of struct type; arrays, lists and dictionaries of structs;
// methods and properties; same output as .NET
struct V2
{
    public float x, y;
    public V2(float x, float y) { this.x = x; this.y = y; }
    public float Len2 => x * x + y * y;
    public void Scale(float k) { x *= k; y *= k; }
    public V2 Plus(V2 o) => new V2(x + o.x, y + o.y);
    public static V2 Zero => new V2(0, 0);
    public override string ToString() => "(" + x + ", " + y + ")";
}

struct Rect
{
    public V2 min, max;
    public Rect(V2 min, V2 max) { this.min = min; this.max = max; }
    public V2 Size => new V2(max.x - min.x, max.y - min.y);
}

struct Cell { public int a; public string tag; }

class Body
{
    public V2 pos;
    public V2 vel = new V2(1, 2);
    public Rect box;
    public V2 Pos { get { return pos; } set { pos = value; } }
    public void Step() { pos = pos.Plus(vel); }
}

class Program
{
    static V2 shared;

    static void Bump(V2 v) { v.x += 100; Console.WriteLine("inside " + v); }
    static V2 Moved(V2 v) { v.y -= 1; return v; }

    static void Main()
    {
        V2 a = new V2(3, 4);
        V2 b = a;
        b.x = 100;
        Console.WriteLine(a + " " + b + " " + a.Len2);
        a.Scale(2);
        Console.WriteLine(a);
        Bump(a);
        Console.WriteLine("after " + a);
        V2 m = Moved(a);
        Console.WriteLine(a + " " + m);

        V2 z = new V2();
        Console.WriteLine(z + " " + V2.Zero + " " + shared);
        shared = a; a.x = 0;
        Console.WriteLine(shared + " " + a);

        var body = new Body();
        Console.WriteLine(body.pos + " " + body.vel + " " + body.box.min + " " + body.box.max);
        body.Step(); body.Step();
        body.pos.x += 0.5f;
        body.box.max.y = 7;
        Console.WriteLine(body.pos + " " + body.box.max + " " + body.box.Size);
        V2 copy = body.Pos;
        copy.x = -1;
        Console.WriteLine(body.Pos + " " + copy);
        body.Pos = copy;
        copy.y = -2;
        Console.WriteLine(body.Pos + " " + copy);

        Rect r = new Rect(new V2(1, 1), new V2(4, 5));
        Rect r2 = r;
        r2.min.x = 50;
        Console.WriteLine(r.min + " " + r2.min + " " + r.Size);

        var arr = new V2[3];
        Console.WriteLine(arr[2]);
        arr[1].x = 9;
        arr[1].Scale(2);
        V2 fromArr = arr[1];
        fromArr.y = 3;
        Console.WriteLine(arr[1] + " " + fromArr);
        V2[] init = new V2[] { new V2(1, 1), a };
        a.y = 77;
        Console.WriteLine(init[1] + " " + a);

        var list = new List<V2>();
        list.Add(a);
        a.x = -5;
        V2 item = list[0];
        item.x = 1000;
        Console.WriteLine(list[0] + " " + a + " " + item);
        Console.WriteLine(list.Contains(new V2(0, 77)) + " " + list.IndexOf(new V2(0, 77)) + " " + list.Contains(new V2(1, 2)));
        list.Add(new V2(5, 5));
        V2 found = list.Find(v => v.x > 1);
        found.x = 0;
        Console.WriteLine(found + " " + list[1]);
        foreach (var v in list) Console.Write(v.Len2 + " ");
        Console.WriteLine();

        var dict = new Dictionary<V2, string>();
        dict[new V2(1, 2)] = "one-two";
        dict[new V2(1, 2)] = "again";
        Console.WriteLine(dict.Count + " " + dict[new V2(1, 2)] + " " + dict.ContainsKey(new V2(2, 1)));
        var pos = new Dictionary<string, V2>();
        pos["hero"] = new V2(3, 3);
        V2 hero;
        pos.TryGetValue("hero", out hero);
        hero.x = 30;
        Console.WriteLine(pos["hero"] + " " + hero);

        Cell cell = new Cell();
        Console.WriteLine(cell.a + " [" + cell.tag + "]");
        Cell cell2 = cell;
        cell2.tag = "x";
        Console.WriteLine("[" + cell.tag + "] [" + cell2.tag + "]");

        Func<V2, V2> twice = v => v.Plus(v);
        Console.WriteLine(twice(new V2(1, 2)));
    }
}
