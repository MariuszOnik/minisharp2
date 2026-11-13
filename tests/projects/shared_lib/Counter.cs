class Counter {
    int n;
    public void Add() { n++; }
    public int Value() { return n; }
    public static int Broken() { return Greeter.Twice(21); }
}
