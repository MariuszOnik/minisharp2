using System.Collections.Generic;
class P { static void Main() { var l = new List<int> { 1 };
  l.ForEach(x => l.Add(x)); } }
