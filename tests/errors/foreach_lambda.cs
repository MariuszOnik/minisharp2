class P
{
    static void Main()
    {
        foreach (char ch in "ab")
        {
            Action a = () => { ch = (char)65; };
        }
    }
}
