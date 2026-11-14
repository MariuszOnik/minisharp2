using System.Collections.Generic;

// foreach over a List: changes that C# allows go on; the error cases are rt_foreach_*.cs
class Program
{
    static void Main()
    {
        List<int> a = new List<int> { 1, 2, 3, 4 };
        List<int> b = new List<int>();

        foreach (int x in a) if (x % 2 == 0) b.Add(x);          // another list: fine
        Console.WriteLine(string.Join(",", b));

        foreach (int x in a) { if (x == 2) { a.Remove(x); break; } }   // change, then leave at once: fine
        Console.WriteLine(string.Join(",", a));

        foreach (int x in new List<int>(a)) if (x > 1) a.Remove(x);     // over a copy: fine
        Console.WriteLine(string.Join(",", a));

        List<int> c = new List<int> { 5, 6 };
        int s = 0;
        foreach (int x in c) { int i = c.IndexOf(x); s += i + (c.Contains(6) ? 1 : 0); }   // reading: fine
        Console.WriteLine(s);

        for (int i = c.Count - 1; i >= 0; i--) c.RemoveAt(i);          // 'for': fine
        Console.WriteLine(c.Count);

        foreach (int x in a) { }                                        // nested foreach over the same list
        foreach (int x in a) foreach (int y in a) s += x * y;
        Console.WriteLine(s);

        List<int> d = new List<int> { 1, 2, 3 };
        d.RemoveAll(x => x == 9);                                       // removed nothing: still the same list
        foreach (int x in d) { if (x == 1) d.RemoveAll(y => y == 9); }
        Console.WriteLine(d.Count);
    }
}
