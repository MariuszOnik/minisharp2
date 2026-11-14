// Console colours: the ConsoleColor enum and the ForegroundColor / BackgroundColor properties (a source class that
// extends the built-in Console, lib/console.c); without a grid they are remembered and read back
class Program
{
    static void Main()
    {
        Console.ForegroundColor = ConsoleColor.Yellow;
        Console.BackgroundColor = ConsoleColor.DarkBlue;
        Console.WriteLine($"{Console.ForegroundColor} on {Console.BackgroundColor}");
        ConsoleColor c = Console.ForegroundColor;
        if (c == ConsoleColor.Yellow) Console.WriteLine("read back: yes");
        Console.WriteLine((int)ConsoleColor.White + " " + ConsoleColor.DarkGray);
        Console.ForegroundColor++;
        Console.WriteLine(Console.ForegroundColor);
        Console.ResetColor();
        Console.WriteLine(Console.ForegroundColor + " " + Console.BackgroundColor);
        Console.WriteLine("Write still works");
    }
}
