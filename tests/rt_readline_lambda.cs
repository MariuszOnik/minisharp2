// Console.ReadLine in a lambda that a built-in function (Sort) calls cannot stop the VM: a clear error.
using System.Collections.Generic;

class Program
{
    static void Main()
    {
        var list = new List<int> { 3, 1, 2 };
        list.Sort((a, b) => { Console.ReadLine(); return a - b; });
        Console.WriteLine("not reached");
    }
}
