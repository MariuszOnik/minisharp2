struct S { public int x; } class P { static void Main() { var l = new List<S>(); l.Add(new S()); l[0].x = 5; } }
