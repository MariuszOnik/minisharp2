using System;

// no namespace and no 'using Lib;'
class Game
{
    static void Main()
    {
        Console.WriteLine("built-in Math: " + Math.Abs(-3));
        Console.WriteLine(Weapon.Info());              // unique name: code outside namespaces may use it
        Console.WriteLine(new Lib.Math().Name);
        Console.WriteLine(Scen.Use.Name());
        Console.WriteLine(Zeta.Count());
    }
}
