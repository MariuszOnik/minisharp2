using System;
using System.Collections.Generic;

// properties: automatic, => and full accessors, static, read-only, virtual / abstract / override,
// interfaces, base.X, compound assignment, delegates; same output as .NET
interface IHealth
{
    int Hp { get; set; }
    bool Alive { get; }
}

abstract class Unit : IHealth
{
    static int made;
    public static int Made { get { return made; } }
    public int Hp { get; set; }
    public bool Alive => Hp > 0;
    public abstract string Kind { get; }
    public virtual int Speed { get { return 1; } }
    public string Name { get; }
    public int Id { get; } = 100;

    protected Unit(string name, int hp)
    {
        Name = name;
        Hp = hp;
        made++;
        Id += made;
    }

    public override string ToString() => Kind + " " + Name + " (" + Hp + ")";
}

class Orc : Unit
{
    public Orc(string name) : base(name, 20) { }
    public override string Kind => "orc";
    public override int Speed { get { return base.Speed + 2; } }
}

class Bat : Unit
{
    public Bat() : base("bat", 3) { }
    public override string Kind { get { return "bat"; } }
}

class Counter
{
    int n;
    public int N { get => n; set => n = value * 10; }
    public int Raw { get { return n; } }
    public static int Total { get; private set; }
    public Action<string> OnChange { get; set; }
    public List<int> History { get; } = new List<int>();

    int clamped;
    public int Clamped
    {
        get { return clamped; }
        set
        {
            if (value < 0) value = 0;
            if (value > 9) value = 9;
            clamped = value;
            History.Add(value);
            Total++;
            if (OnChange != null) OnChange("clamped " + value);
        }
    }
}

class Program
{
    static float Speed { get; set; } = 1.5f;
    static int Doubled => (int)(Speed * 2);

    static void Main()
    {
        var units = new List<Unit> { new Orc("Grom"), new Bat(), new Orc("Ulg") };
        foreach (var u in units)
            Console.WriteLine(u + " kind=" + u.Kind + " alive=" + u.Alive + " speed=" + u.Speed + " id=" + u.Id);
        Console.WriteLine("made " + Unit.Made);

        IHealth h = units[1];
        h.Hp -= 3;
        Console.WriteLine(h.Alive + " " + h.Hp + " " + units[1]);
        h.Hp++;
        Console.WriteLine(units[1].Alive);

        var c = new Counter();
        c.N = 3;
        Console.WriteLine(c.N + " " + c.Raw);
        c.N++;
        Console.WriteLine(c.N);
        c.N += 1;
        Console.WriteLine(c.N);
        int x = (c.N = 2) + 1;
        Console.WriteLine(x + " " + c.N);

        c.OnChange = s => Console.WriteLine("event: " + s);
        c.Clamped = 5;
        c.Clamped = -4;
        c.Clamped += 20;
        Console.WriteLine(c.Clamped + " " + Counter.Total + " " + c.History.Count);
        foreach (var v in c.History) Console.Write(v + " ");
        Console.WriteLine();
        c.OnChange("called directly");

        Action<int> set = v => c.N = v;
        set(7);
        Func<int> get = () => c.N;
        Console.WriteLine(get());

        Console.WriteLine(Speed + " " + Doubled);
        Speed *= 3;
        Console.WriteLine(Speed + " " + Doubled + " " + Program.Doubled);
    }
}
