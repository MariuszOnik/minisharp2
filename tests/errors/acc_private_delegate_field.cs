class Przycisk {
    private Action klik = () => Console.WriteLine("klik");
}
class Program {
    static void Main() {
        new Przycisk().klik();
    }
}
