struct S { public int x; } class A { public S Pos { get; set; } } class P { static void Main() { var a = new A(); a.Pos.x = 1; } }
