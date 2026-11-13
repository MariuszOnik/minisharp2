class Game {
    static void Main() {
        Console.WriteLine(Greeter.Hello("library"));
        var c = new Counter(); c.Add(); c.Add();
        Console.WriteLine("count " + c.Value() + ", " + Counter.Broken());
    }
}
