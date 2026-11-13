class Program {
    static void Main() {
        Func<int, int> abs = x => {
            if (x < 0) return -x;
        };
        Console.WriteLine(abs(1));
    }
}
