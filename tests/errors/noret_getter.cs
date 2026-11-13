class Postac {
    int hp;
    public int Hp {
        get { if (hp > 0) return hp; }
    }
}
class Program {
    static void Main() { Console.WriteLine(new Postac().Hp); }
}
