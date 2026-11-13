// Edge cases of lib/ that a plain C cast or strtol would give differently on PC (x86) and 3DS/Switch (ARM)
class Program
{
    static void Main()
    {
        Console.WriteLine(Mathf.FloorToInt(1e10f) + " " + Mathf.CeilToInt(-1e10f) + " " + Mathf.RoundToInt(Mathf.Sqrt(-1f)));
        Console.WriteLine(Mathf.FloorToInt(-2.5f) + " " + Mathf.RoundToInt(2.5f) + " " + Mathf.RoundToInt(3.5f));
        Console.WriteLine(int.Parse("2147483647") + " " + int.Parse("-2147483648") + " " + int.Parse(" 42 "));
        Console.WriteLine("a,b,,c".Split(',').Length + " " + "x".Split(',')[0]);
    }
}
