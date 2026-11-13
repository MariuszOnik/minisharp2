class Program {
    static string Nazwa(int d) {
        switch (d) {
            case 1: return "jeden";
            case 2: return "dwa";
        }
    }
    static void Main() { Console.WriteLine(Nazwa(1)); }
}
