class Gracz {
    private int Zycie { get; set; }
}
class Program {
    static void Main() {
        Gracz g = new Gracz();
        Console.WriteLine(g.Zycie);
    }
}
