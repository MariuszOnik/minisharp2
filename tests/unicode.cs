// char is one character (a code point), strings count characters: Polish letters like in C#
class P
{
    static string Reverse(string s)
    {
        string r = "";
        for (int i = s.Length - 1; i >= 0; i--) r += s[i];
        return r;
    }

    static void Main()
    {
        string w = "żółw";
        Console.WriteLine(w.Length);
        Console.WriteLine(w[0] + " " + w[1] + " " + w[3]);
        Console.WriteLine(Reverse("zażółć gęślą jaźń"));

        // foreach and a char literal
        int n = 0;
        foreach (char c in "pętla ęę")
        {
            if (c == 'ę') n++;
        }
        Console.WriteLine("ę: " + n);

        // codes
        char e = 'ę';
        Console.WriteLine((int)e);
        Console.WriteLine((char)261);
        Console.WriteLine((char)(e + 1) == 'Ě' ? "next" : "not next");
        Console.WriteLine('ą' < 'ź');

        // char methods
        Console.WriteLine(char.ToUpper('ż') + "" + char.ToUpper('ł') + char.ToLower('Ś') + char.ToUpper('a'));
        Console.WriteLine(char.IsLetter('ń') + " " + char.IsLetter('1') + " " + char.IsLetter('Ж') + " " + char.IsWhiteSpace(' '));

        // string methods
        Console.WriteLine("Zażółć Gęślą".ToUpper());
        Console.WriteLine("ŹDŹBŁO".ToLower());
        Console.WriteLine("Привет".ToUpper() + " " + "ΑΒΓ".ToLower());
        Console.WriteLine("żółw".Substring(1, 2) + "|" + "żółw".Substring(3));
        Console.WriteLine("łąka i łódź".IndexOf("łódź") + " " + "łąka".IndexOf('k') + " " + "łąka".IndexOf("x"));
        string[] parts = "ałbłc".Split('ł');
        Console.WriteLine(parts.Length + ": " + parts[0] + parts[1] + parts[2]);
        Console.WriteLine("[" + "  źle  ".Trim() + "]");
        Console.WriteLine("gęś".Contains("ęś") + " " + "gęś".StartsWith("gę") + " " + "gęś".EndsWith("ś"));
        Console.WriteLine("gęsi gęś".Replace("gę", "GĘ"));

        // in an interpolated string, concatenation, a List and a Dictionary
        char z = 'ź';
        Console.WriteLine($"[{z}] " + z + 'ó');
        var letters = new List<char>();
        foreach (char c in "źdźbło") letters.Add(c);
        Console.WriteLine(letters.Count + " " + letters[1] + letters[2]);
        var count = new Dictionary<char, int>();
        foreach (char c in "źdźźło")
        {
            if (count.ContainsKey(c)) count[c]++;
            else count[c] = 1;
        }
        Console.WriteLine("ź x" + count['ź'] + ", ł x" + count['ł']);

        Console.WriteLine("ąę"[1]);
    }
}
