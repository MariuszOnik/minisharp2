// Console.ReadLine: the VM stops, the host gives a line (tests/input/readline.txt) and the program goes on.
using System.Collections.Generic;

class Program
{
    static int count;

    static string Ask(string question)
    {
        Console.Write(question + " ");
        string answer = Console.ReadLine();
        count++;
        return answer;
    }

    static int AskNumber(string question)
    {
        int n;
        string s;
        while (!int.TryParse(s = Ask(question), out n))
        {
            if (s == null) return -1;           // end of input
            Console.WriteLine("that is not a number");
        }
        return n;
    }

    static void Main()
    {
        string name = Ask("Name?");
        Console.WriteLine("Hello, " + name + "!");
        int a = AskNumber("First number?");
        int b = AskNumber("Second number?");
        Console.WriteLine($"{a} + {b} = {a + b}");

        // a guessing game
        int secret = 7, tries = 0;
        while (true)
        {
            int g = AskNumber("Guess:");
            if (g < 0) break;
            tries++;
            if (g < secret) Console.WriteLine("more");
            else if (g > secret) Console.WriteLine("less");
            else { Console.WriteLine($"yes, in {tries} tries"); break; }
        }

        // lines into a list until an empty one
        var words = new List<string>();
        string w;
        while (!string.IsNullOrEmpty(w = Console.ReadLine()))
            words.Add(w.ToUpper());
        string all = "";
        foreach (string x in words) all += (all == "" ? "" : ", ") + x;
        Console.WriteLine(all);

        string polish = Console.ReadLine();
        if (polish != null) Console.WriteLine(polish.Length + " " + polish[0]);

        // the end of input: null
        string end = Console.ReadLine();
        Console.WriteLine(end == null ? "end of input" : "more: " + end);
        Console.WriteLine("asked " + count + " times");
    }
}
