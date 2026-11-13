class Program {
    static string Nazwa(int d) {
        switch (d) {
            case 1: return "jeden";
            default: break;
        }
    }
    static void Main() { Console.WriteLine(Nazwa(1)); }
}
