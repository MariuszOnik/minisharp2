// a shared library file: used by tests/project_uses through its uses.txt
class Greeter {
    public static string Hello(string who) { return "Hello, " + who + "!"; }
    public static int Twice(int x) { return x * 2; }
}
