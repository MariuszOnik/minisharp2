// rectangular arrays T[,]: new T[rows, cols], { { .. }, { .. } }, t[i, j], GetLength, Length, foreach
class Cell { public int v; public Cell(int v) { this.v = v; } }

class P
{
    static int[,] board = new int[3, 4];

    static int Sum(int[,] t)
    {
        int s = 0;
        for (int y = 0; y < t.GetLength(0); y++)
            for (int x = 0; x < t.GetLength(1); x++)
                s += t[y, x];
        return s;
    }

    static void Print(char[,] map)
    {
        for (int y = 0; y < map.GetLength(0); y++)
        {
            string line = "";
            for (int x = 0; x < map.GetLength(1); x++) line += map[y, x];
            Console.WriteLine(line);
        }
    }

    static void Main()
    {
        Console.WriteLine(board.GetLength(0) + " x " + board.GetLength(1) + " = " + board.Length);
        board[1, 2] = 5;
        board[2, 3] += 7;
        board[0, 0]++;
        Console.WriteLine(board[1, 2] + " " + board[2, 3] + " " + board[0, 0] + " " + board[2, 0]);
        Console.WriteLine("sum " + Sum(board));

        int[,] m = { { 1, 2, 3 }, { 4, 5, 6 } };
        Console.WriteLine(m.GetLength(0) + " " + m.GetLength(1) + " " + m[1, 0]);
        string all = "";
        foreach (int v in m) all += v + " ";      // row after row, like C#
        Console.WriteLine(all);

        var n = new float[,] { { 0.5f, 1.5f }, { 2.5f, 3.5f } };
        Console.WriteLine(n[1, 1] - n[0, 0]);

        char[,] map = new char[3, 5];
        for (int y = 0; y < 3; y++)
            for (int x = 0; x < 5; x++)
                map[y, x] = (y == 0 || y == 2 || x == 0 || x == 4) ? '#' : '.';
        map[1, 2] = '@';
        Print(map);

        string[,] names = new string[2, 2];
        names[0, 1] = "Ala";
        Console.WriteLine(names[0, 1] + " " + (names[1, 1] == null));

        Cell[,] cells = new Cell[1, 2];
        cells[0, 1] = new Cell(9);
        Console.WriteLine(cells[0, 1].v + " " + (cells[0, 0] == null));

        int[,] empty = new int[4, 0];
        Console.WriteLine(empty.Length + " " + empty.GetLength(0) + " " + empty.GetLength(1));

        int[] flat = { 1, 2 };
        Console.WriteLine(flat.GetLength(0));
    }
}
