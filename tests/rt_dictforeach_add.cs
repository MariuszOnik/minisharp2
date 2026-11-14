using System.Collections.Generic;
class P { static void Main() { var d = new Dictionary<int, int> { { 1, 1 } };
  foreach (var kv in d) d[kv.Key + 1] = 0; } }
