using System.Collections.Generic;

// foreach over a Dictionary: Remove, Clear and overwriting values are allowed (as in .NET Core 3.0+);
// adding a key is an error (rt_dictforeach_add.cs)
class Program
{
    static void Main()
    {
        var d = new Dictionary<string, int> { { "a", 1 }, { "b", 2 }, { "c", 3 }, { "d", 4 } };
        foreach (var kv in d) d[kv.Key] = kv.Value * 10;          // overwrite
        foreach (var kv in d) Console.Write(kv.Key + "=" + kv.Value + " ");
        Console.WriteLine();

        foreach (var kv in d) if (kv.Value >= 20 && kv.Value <= 30) d.Remove(kv.Key);   // remove while looping
        foreach (var kv in d) Console.Write(kv.Key + "=" + kv.Value + " ");
        Console.WriteLine();

        int seen = 0;
        foreach (var kv in d) { seen++; d.Clear(); }              // clear: the loop ends
        Console.WriteLine(seen + " " + d.Count);

        var e = new Dictionary<int, int> { { 1, 1 } };
        var later = new List<int>();
        foreach (var kv in e) later.Add(kv.Key + 1);              // the new keys after the loop
        foreach (int k in later) e[k] = 0;
        Console.WriteLine(e.Count);
    }
}
