using System;
using System.Collections.Generic;

// enum: values, names, casts, operators, switch, [Flags], enums inside a class; same output as .NET
enum Dir { Up, Down, Left = 10, Right }

[Flags]
enum Mask { None = 0, A = 1, B = 2, C = 4, AB = A | B, All = A | B | C }

enum Level : int { Low = -1, Mid = Low + 2, High = Mid * 10 }

class Enemy
{
    public enum State { Idle, Chase, Attack, Dead }
    public State state = State.Idle;
    public Dir facing;

    public void Next()
    {
        if (state == State.Dead) return;
        state++;
    }
}

class Program
{
    static Dir Opposite(Dir d)
    {
        switch (d)
        {
            case Dir.Up: return Dir.Down;
            case Dir.Down: return Dir.Up;
            case Dir.Left: return Dir.Right;
            default: return Dir.Left;
        }
    }

    static string Describe(Enemy.State s)
    {
        switch (s)
        {
            case Enemy.State.Idle: case Enemy.State.Chase: return "calm-ish";
            case Enemy.State.Attack: return "angry";
        }
        return "gone";
    }

    static void Main()
    {
        Dir d = Dir.Right;
        Console.WriteLine(d);
        Console.WriteLine((int)d);
        Console.WriteLine((int)Dir.Up + " " + (int)Dir.Down + " " + (int)Dir.Left);
        Console.WriteLine((Dir)1);
        Console.WriteLine((Dir)5);
        Console.WriteLine("d = " + d + ", d + 1 = " + (d + 1) + ", d - 1 = " + (d - 1));
        Console.WriteLine(Dir.Right - Dir.Left);
        Console.WriteLine(d.ToString() + "!");
        Console.WriteLine(Opposite(Dir.Up) + " " + Opposite(Dir.Left) + " " + Opposite(Dir.Right));
        Console.WriteLine(d == Dir.Right);
        Console.WriteLine(d != Dir.Up);
        Console.WriteLine(Dir.Up < Dir.Left);
        Console.WriteLine(Dir.Right >= Dir.Left);

        Mask m = Mask.A | Mask.C;
        Console.WriteLine(m);
        Console.WriteLine(Mask.AB);
        Console.WriteLine(Mask.None);
        Console.WriteLine(Mask.All);
        Console.WriteLine((Mask)7);
        Console.WriteLine((Mask)6);
        Console.WriteLine((Mask)8);
        Console.WriteLine((m & Mask.C) == Mask.C);
        Console.WriteLine((m & Mask.B) == Mask.None);
        m |= Mask.B;
        Console.WriteLine(m);
        m &= ~Mask.A;
        Console.WriteLine(m);
        m ^= Mask.C;
        Console.WriteLine(m);

        Console.WriteLine(Level.Low + " " + (int)Level.Mid + " " + (int)Level.High);

        var e = new Enemy();
        Console.WriteLine(e.state + " " + e.facing);
        for (int i = 0; i < 5; i++) { Console.WriteLine(e.state + " -> " + Describe(e.state)); e.Next(); }
        e.facing = (Dir)11;
        Console.WriteLine(e.facing);
        Enemy.State s = Enemy.State.Attack;
        Console.WriteLine(s);

        var counts = new Dictionary<Dir, int>();
        Dir[] moves = new Dir[] { Dir.Up, Dir.Left, Dir.Up, Dir.Right, Dir.Up };
        foreach (var mv in moves)
        {
            if (counts.ContainsKey(mv)) counts[mv]++;
            else counts[mv] = 1;
        }
        foreach (var kv in counts) Console.WriteLine(kv.Key + ": " + kv.Value);

        var list = new List<Dir> { Dir.Right, Dir.Up, Dir.Left, Dir.Down };
        list.Sort();
        foreach (var x in list) Console.Write(x + " ");
        Console.WriteLine();
        Console.WriteLine(list.IndexOf(Dir.Left) + " " + list.Contains(Dir.Down));

        Dir[] all = new Dir[3];
        Console.WriteLine(all[0] + " " + all.Length);
        float f = (float)Dir.Left / 4;
        Console.WriteLine(f);
        Dir fromFloat = (Dir)2.9f;
        Console.WriteLine((int)fromFloat);
    }
}
