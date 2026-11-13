// a library in its own namespace: its Math does not hide the built-in Math of programs without 'using Lib;'
namespace Lib
{
    class Math
    {
        public string Name = "library math";
    }

    static class Weapon
    {
        public static string Info() { return "weapon from the library"; }
    }
}
