using System;
class P { static void F(ref int x) { Action a = () => x++; } static void Main() { int a = 1; F(ref a); } }
