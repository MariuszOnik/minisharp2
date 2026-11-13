// Namespaces: full names, nested and relative names, 'using', a class of the scenario deriving from a library class,
// the same short name in several namespaces; same output as .NET
using System;
using System.Collections.Generic;
using Lib.Monsters;

namespace Lib
{
    enum Size { Small, Medium, Large }

    class Item
    {
        public string Name;
        public Item(string n) { Name = n; }
        public static int Count() { return 99; }
    }

    abstract class Creature
    {
        public string Name;
        public Size Sz = Size.Medium;
        public Item Hand = new Item("fist");        // Lib.Item: the namespace of the class comes first
        protected Creature(string n) { Name = n; }
        public virtual string Describe() { return Name + " (" + Sz + ", " + Hand.Name + ")"; }
    }

    static class Rules
    {
        public static int Mod(int score) { return score / 2 - 5; }
    }
}

namespace Lib.Monsters
{
    class Troll : Creature                          // Creature from the namespace around: Lib
    {
        public Troll() : base("Troll") { Sz = Size.Large; Hand = new Lib.Item("club"); }
        public override string Describe() { return "big " + base.Describe() + " mod " + Rules.Mod(23); }
    }

    class Item                                      // Lib.Monsters.Item: here it hides Lib.Item
    {
        public string Tag = "monster item";
    }

    static class Spawn
    {
        public static string What() { return new Item().Tag + " / " + new Lib.Item("axe").Name + " / " + Monsters.Spawn.Relative(); }
        public static string Relative() { return "relative name"; }
        public static List<Creature> Pack(int n)
        {
            var l = new List<Creature>();
            for (int i = 0; i < n; i++) l.Add(new Troll());
            return l;
        }
    }
}

namespace Other
{
    class Troll { public string Describe() { return "other troll"; } }
}

namespace Scenario
{
    using Lib;

    class Hero : Creature                           // a class of the scenario derived from the library
    {
        public Hero(string n) : base(n) { Sz = Size.Small; }
        public override string Describe() { return "hero " + base.Describe(); }
    }
}

class BigTroll : Lib.Monsters.Troll
{
    public override string Describe() { return "scenario " + base.Describe(); }
}

class Program
{
    static void Main()
    {
        Troll t = new Troll();                      // Lib.Monsters.Troll through 'using Lib.Monsters'
        Console.WriteLine(t.Describe());
        Console.WriteLine(new Other.Troll().Describe());
        Console.WriteLine(new BigTroll().Describe());
        Console.WriteLine(new Scenario.Hero("Lew").Describe());
        Lib.Creature c = t;
        Console.WriteLine(c.Hand.Name + " " + Lib.Item.Count() + " " + Lib.Rules.Mod(14));
        Console.WriteLine(Spawn.What());
        Lib.Size s = Lib.Size.Small;
        Console.WriteLine(s + " " + (int)Lib.Size.Large + " " + (s == Lib.Size.Small));
        Console.WriteLine(new Item().Tag);          // Lib.Monsters.Item through 'using'
        Console.WriteLine(Spawn.Pack(3).Count + " " + (c is Troll) + " " + (c is Lib.Monsters.Troll));
        var list = new List<Lib.Item>();
        list.Add(new Lib.Item("ring"));
        Lib.Item[] arr = new Lib.Item[] { list[0] };
        Console.WriteLine(arr[0].Name + " " + ((Lib.Creature)new BigTroll()).Name);
    }
}
