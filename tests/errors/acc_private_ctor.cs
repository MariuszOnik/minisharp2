class Gra {
    private Gra() { }
    public static Gra Nowa() { return new Gra(); }
}
class Program {
    static void Main() {
        Gra g = new Gra();
    }
}
