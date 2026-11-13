// for with ',' in the first and the last part (C#: several initializers / iterators)
class P
{
    static void Main()
    {
        for (int a = 0, b = 10; a < b; a += 3, b--)
            Console.Write(a + "," + b + " ");
        Console.WriteLine();

        int i, j;
        for (i = 0, j = 5; i < j; i++, j--)
            Console.Write(i + "-" + j + " ");
        Console.WriteLine();
        Console.WriteLine("after: " + i + " " + j);

        // three steps, and a call among them
        int n = 0;
        for (int k = 0; k < 3; k++, n += 10, Console.Write("."))
        {
        }
        Console.WriteLine(" n = " + n);

        // the variables declared in 'for' and assigned in its steps
        string s = "";
        for (int lo = 0, hi = 4; lo <= hi; lo++, hi--)
        {
            if (lo == hi) s += "[" + lo + "]";
            else s += lo + "" + hi + " ";
        }
        Console.WriteLine(s);
    }
}
