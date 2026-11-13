// Namespaces: full names, nested and relative names, 'using', a class of the scenario deriving from a library class,
// the same short name in several namespaces; same output as .NET
using System;
using System.Collections.Generic;
using Toee.Monsters;

namespace Toee
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
        public Item Hand = new Item("fist");        // Toee.Item: the namespace of the class comes first
        protected Creature(string n) { Name = n; }
        public virtual string Describe() { return Name + " (" + Sz + ", " + Hand.Name + ")"; }
    }

    static class Rules
    {
        public static int Mod(int score) { return score / 2 - 5; }
    }
}

namespace Toee.Monsters
{
    class Troll : Creature                          // Creature from the namespace around: Toee
    {
        public Troll() : base("Troll") { Sz = Size.Large; Hand = new Toee.Item("club"); }
        public override string Describe() { return "big " + base.Describe() + " mod " + Rules.Mod(23); }
    }

    class Item                                      // Toee.Monsters.Item: here it hides Toee.Item
    {
        public string Tag = "monster item";
    }

    static class Spawn
    {
        public static string What() { return new Item().Tag + " / " + new Toee.Item("axe").Name + " / " + Monsters.Spawn.Relative(); }
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
    using Toee;

    class Hero : Creature                           // a class of the scenario derived from the library
    {
        public Hero(string n) : base(n) { Sz = Size.Small; }
        public override string Describe() { return "hero " + base.Describe(); }
    }
}

class BigTroll : Toee.Monsters.Troll
{
    public override string Describe() { return "scenario " + base.Describe(); }
}

class Program
{
    static void Main()
    {
        Troll t = new Troll();                      // Toee.Monsters.Troll through 'using Toee.Monsters'
        Console.WriteLine(t.Describe());
        Console.WriteLine(new Other.Troll().Describe());
        Console.WriteLine(new BigTroll().Describe());
        Console.WriteLine(new Scenario.Hero("Lew").Describe());
        Toee.Creature c = t;
        Console.WriteLine(c.Hand.Name + " " + Toee.Item.Count() + " " + Toee.Rules.Mod(14));
        Console.WriteLine(Spawn.What());
        Toee.Size s = Toee.Size.Small;
        Console.WriteLine(s + " " + (int)Toee.Size.Large + " " + (s == Toee.Size.Small));
        Console.WriteLine(new Item().Tag);          // Toee.Monsters.Item through 'using'
        Console.WriteLine(Spawn.Pack(3).Count + " " + (c is Troll) + " " + (c is Toee.Monsters.Troll));
        var list = new List<Toee.Item>();
        list.Add(new Toee.Item("ring"));
        Toee.Item[] arr = new Toee.Item[] { list[0] };
        Console.WriteLine(arr[0].Name + " " + ((Toee.Creature)new BigTroll()).Name);
    }
}
