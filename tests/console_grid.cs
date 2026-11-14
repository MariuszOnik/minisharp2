// Console on a character grid (the host runs it with --grid 20x6, see opcje.txt): Clear, SetCursorPosition,
// ResetColor, wrapping of a long line, scrolling
class Program
{
    static void Main()
    {
        Console.WriteLine("this text is cleared");
        Console.Clear();
        Console.WriteLine("+------+");
        Console.WriteLine("|      |");
        Console.WriteLine("+------+");
        Console.SetCursorPosition(2, 1);
        Console.Write("@");
        Console.SetCursorPosition(10, 1);
        Console.Write("x=" + 5);
        Console.SetCursorPosition(0, 3);
        Console.ResetColor();
        Console.WriteLine("abcdefghijklmnopqrstuvwxyz");
        Console.Write("end");
    }
}
