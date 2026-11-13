class Program {
    static int Szukaj(int[] t) {
        int i = 0;
        while (true) {
            if (i == t.Length) break;
            if (t[i] > 3) return i;
            i++;
        }
    }
    static void Main() { Console.WriteLine(Szukaj(new int[] { 1 })); }
}
