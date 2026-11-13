class P
{
    static void Inc(ref int a) { a++; }
    static void Main()
    {
        int[] t = { 1 };
        foreach (int x in t) Inc(ref x);
    }
}
