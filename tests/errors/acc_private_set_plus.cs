class Gracz {
    public int Punkty { get; private set; }
}
class Program {
    static void Main() {
        Gracz g = new Gracz();
        g.Punkty += 1;
    }
}
