class Program {
    static int Znak(int x) {
        if (x > 0) return 1;
        else if (x < 0) return -1;
    }
    static void Main() { Console.WriteLine(Znak(0)); }
}
