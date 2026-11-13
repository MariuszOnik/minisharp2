// Skrócony inicjalizator tablicy: int[] t = { 1, 2 }; (zmienna, pole, pole statyczne, właściwość)
class Plansza {
    int[] rzad = { 1, 2, 3 };
    static string[] imiona = { "Ala", "Ola" };
    public float[] Wagi { get; set; } = { 0.5f, 1.5f };
    public int Suma() { int s = 0; foreach (int x in rzad) s += x; return s; }
    public static string Pierwsze() { return imiona[0]; }
}

class Program {
    static void Main() {
        int[] t = { 10, 20, 30 };
        Console.WriteLine(t.Length + " " + t[2]);
        string[] puste = { };
        Console.WriteLine(puste.Length);
        int[] a = { 1, 2 }, b = { 3 };
        Console.WriteLine(a.Length + b.Length);
        int[][] siatka = { new int[] { 1 }, new int[] { 2, 3 } };
        Console.WriteLine(siatka[1][1]);
        char[] litery = { 'a', 'b', };
        Console.WriteLine(litery[1]);
        Plansza p = new Plansza();
        Console.WriteLine(p.Suma() + " " + Plansza.Pierwsze() + " " + p.Wagi[1]);
    }
}
