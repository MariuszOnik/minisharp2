class Zwierze {
    protected int wiek = 3;
}
class Pies : Zwierze { }
class Program {
    static void Main() {
        Pies p = new Pies();
        Console.WriteLine(p.wiek);
    }
}
