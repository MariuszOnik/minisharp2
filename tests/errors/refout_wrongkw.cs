class P { static void F(out int x) { x = 1; } static void Main() { int a = 0; F(ref a); } }
