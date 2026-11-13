class P { static void F(bool b, out int x) { if (b) x = 1; } static void Main() { F(true, out int y); } }
