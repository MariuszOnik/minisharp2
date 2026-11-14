using System.Collections.Generic;
class P { static void Main() { var l = new List<int> { 1, 0, 2 };
  foreach (int x in l) if (x == 0) l.Remove(x); } }
