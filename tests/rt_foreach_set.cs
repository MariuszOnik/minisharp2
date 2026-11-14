using System.Collections.Generic;
class P { static void Main() { var l = new List<int> { 1, 2 };
  foreach (int x in l) l[0] = x; } }
