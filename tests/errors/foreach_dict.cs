class P
{
    static void Main()
    {
        var d = new Dictionary<string, int>();
        foreach (var kv in d) kv = kv;
    }
}
