using System;
class P { static void F(ref int x) { x++; } static void Main() { Action<int> a = F; } }
