class P { static void F(ref int x) { x++; } static void Main() { int[] t = { 1 }; F(ref t[0]); } }
