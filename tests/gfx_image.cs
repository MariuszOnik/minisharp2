// Pictures: Gfx.LoadImage (next to the program, the same file once), ImageWidth/Height, Image with scale and mirror,
// (a negative scale mirrors left-right only, in the same place: x stays the left edge, the blue dot stays at the top), ImagePart (a sprite sheet: two 8x8 frames), ImageRot, transparency of the picture and Gfx.Alpha (--hash, opcje.txt)
class Game
{
    static int sheet;

    static void Main()
    {
        sheet = Gfx.LoadImage("assets/sheet.png");
        Console.WriteLine(sheet + " " + Gfx.ImageWidth(sheet) + "x" + Gfx.ImageHeight(sheet));
        Console.WriteLine(Gfx.LoadImage("assets/sheet.png") == sheet);
    }

    static void Draw()
    {
        Gfx.Clear(Color.Gray);
        Gfx.Image(sheet, 10, 10);
        Gfx.Image(sheet, 10, 30, 4);
        Gfx.Image(sheet, 100, 30, -4);
        Gfx.ImagePart(sheet, 8, 0, 8, 8, 200, 10);
        Gfx.ImagePart(sheet, 0, 0, 8, 8, 200, 30, 5);
        Gfx.ImageRot(sheet, 300, 150, 45, 3);
        Gfx.Alpha(0.5f);
        Gfx.Image(sheet, 10, 100, 6);
        Gfx.Alpha(1);
    }
}
