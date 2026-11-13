// Podpowiedzi pokazują tylko to, czego kompilator pozwoli tu użyć:
// private - tylko w swojej klasie, protected - w klasie i pochodnych, brak słówka - wszędzie.
class Konto
{
    private int saldo;
    protected int limit;
    public string Wlasciciel;
    int numer;
    private void Przelicz() { }
    protected void Zablokuj() { }
    public void Wplac(int ile) { this./*?*/ }
}

class Oszczednosciowe : Konto
{
    void Odsetki() { this./*?*/ }
}

class Bank
{
    static void Main()
    {
        var k = new Konto();
        k./*?*/
        Oszczednosciowe o = new Oszczednosciowe();
        o./*?*/
    }
}
