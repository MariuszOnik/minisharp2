class Zwierze {
    private int wiek = 3;
}
class Pies : Zwierze {
    public void Pokaz() { Console.WriteLine(wiek); }
}
class Program {
    static void Main() { new Pies().Pokaz(); }
}
