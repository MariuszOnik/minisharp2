using Autolib2;

// found by 'using Autolib;' in project_autolib: tests/autolib/lib
namespace Autolib
{
    static class Dice
    {
        public static int Roll() { return 4 + Bonus.Value(); }
    }
}
