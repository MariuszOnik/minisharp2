class Licznik {
    private int ile;
    public void Dodaj() { ile++; }
}
class Program {
    static void Main() {
        Licznik l = new Licznik();
        l.Dodaj();
        Console.WriteLine(l.ile);
    }
}
