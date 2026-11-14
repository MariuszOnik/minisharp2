/* Unit test: reflection for layers above the language (lang/reflect.c, ms.h) – no platform host.
 * What a scene loader and an inspector need: find a class by name, list its fields with their types, make an object
 * of it, set fields by name with the type checked, find and call the methods a script overrides, keep objects
 * alive across calls. Prints "FAIL: ..." lines; exit code 0 = all passed. */
#include "ms.h"
#include <stdio.h>
#include <string.h>

static int fails;
static void check(int cond, const char* what) { if (!cond) { fails++; printf("FAIL: %s\n", what); } }

static const char* SRC =
    "using System.Collections.Generic;\n"
    "enum Kolor { Czerwony, Zielony = 5, Niebieski }\n"
    "struct Punkt { public int X; public int Y; }\n"
    "abstract class Zachowanie {\n"
    "    public string Nazwa = \"bez nazwy\";\n"
    "    protected int licznik;\n"
    "    public virtual void Start() { }\n"
    "    public virtual void Update() { licznik++; }\n"
    "    public abstract int Punkty();\n"
    "    public int Licznik() { return licznik; }\n"
    "}\n"
    "class Drzwi : Zachowanie {\n"
    "    public bool Zamkniete = true;\n"
    "    public float Szybkosc = 1.5f;\n"
    "    public Kolor Barwa = Kolor.Zielony;\n"
    "    public Punkt Miejsce;\n"
    "    public Drzwi Para;\n"
    "    public Zachowanie Cel;\n"
    "    public List<string> Klucze = new List<string>();\n"
    "    public int[,] Siatka;\n"
    "    public Dictionary<string, int> Licz;\n"
    "    public readonly int Stala = 7;\n"
    "    int ukryte;\n"
    "    private char znak;\n"
    "    public static int Wszystkie;\n"
    "    public Drzwi() { Wszystkie++; Nazwa = \"drzwi\"; }\n"
    "    public override void Update() { base.Update(); licznik += 10; }\n"
    "    public override int Punkty() { return licznik * 2; }\n"
    "    public void Ustaw(int a, int b) { licznik = a + b; }\n"
    "    public void Ustaw(int a) { licznik = a; }\n"
    "    public static void Statyczna() { }\n"
    "    public void Psuj() { Drzwi d = null; d.Zamkniete = false; }\n"
    "    public void Smieci() { for (int i = 0; i < 60000; i++) { string s = \"x\" + i; } }\n"
    "}\n"
    "class Skrzynia : Zachowanie { public int Zloto; public override int Punkty() { return Zloto; } }\n"
    "class TylkoZParametrem { public TylkoZParametrem(int x) { } }\n"
    "static class Narzedzia { public static int Dwa() { return 2; } }\n"
    "interface IUzywalny { void Uzyj(); }\n"
    "namespace Gra { class Drzwi { public int W; } class Lampa { public int Moc = 3; } }\n";

static Value iv(int i) { Value v; v.t = V_INT; v.as.i = i; return v; }
static Value fv(float f) { Value v; v.t = V_FLOAT; v.as.f = f; return v; }
static Value bv(int b) { Value v; v.t = V_BOOL; v.as.i = b; return v; }
static Value nullv(void) { Value v; v.t = V_NULL; v.as.o = NULL; return v; }

static int field(Program* p, int cls, const char* name) {
    int f = ms_field_find(p, cls, name);
    if (f < 0) { fails++; printf("FAIL: no field %s\n", name); }
    return f;
}

int main(void) {
    char err[256];
    Program* p = ms_compile(SRC, err, sizeof err);
    if (!p) { printf("FAIL: compile: %s\n", err); return 1; }

    /* ---------- classes ---------- */
    int drzwi = ms_class_find(p, "Drzwi");
    int gdrzwi = ms_class_find(p, "Gra.Drzwi");
    int lampa = ms_class_find(p, "Lampa");
    int zach = ms_class_find(p, "Zachowanie");
    int skrz = ms_class_find(p, "Skrzynia");
    int kolor = ms_class_find(p, "Kolor");
    int punkt = ms_class_find(p, "Punkt");
    check(drzwi >= 0 && gdrzwi >= 0 && drzwi != gdrzwi, "Drzwi (global) and Gra.Drzwi are two classes");
    check(lampa >= 0 && !strcmp(ms_class_full_name(p, lampa), "Gra.Lampa") && !strcmp(ms_class_name(p, lampa), "Lampa"),
          "Lampa found by its name alone; full name Gra.Lampa");
    check(ms_class_find(p, "Nie.Ma") < 0 && ms_class_find(p, "NieMa") < 0, "unknown class is -1");
    check(!strcmp(ms_class_full_name(p, drzwi), "Drzwi"), "global class: full name = name");
    check(ms_class_kind(p, drzwi) == MS_CK_CLASS && ms_class_kind(p, kolor) == MS_CK_ENUM &&
          ms_class_kind(p, punkt) == MS_CK_STRUCT && ms_class_kind(p, ms_class_find(p, "IUzywalny")) == MS_CK_INTERFACE, "class kinds");
    check(ms_class_base(p, drzwi) == zach && ms_class_base(p, zach) == -1, "base classes");
    check(ms_class_is(p, drzwi, zach) && ms_class_is(p, drzwi, drzwi) && !ms_class_is(p, zach, drzwi) && !ms_class_is(p, skrz, drzwi),
          "class_is");
    check(ms_class_is_abstract(p, zach) && !ms_class_is_abstract(p, drzwi), "abstract");
    check(ms_class_can_create(p, drzwi) && ms_class_can_create(p, punkt) && !ms_class_can_create(p, zach) &&
          !ms_class_can_create(p, kolor) && !ms_class_can_create(p, ms_class_find(p, "Narzedzia")) &&
          !ms_class_can_create(p, ms_class_find(p, "TylkoZParametrem")) && !ms_class_can_create(p, ms_class_find(p, "IUzywalny")),
          "can_create: classes with a constructor without parameters and structs only");
    check(!ms_class_from_source(p, drzwi), "user's class is not from a registered source");

    /* an inspector: the classes derived from Zachowanie that can be put on an object */
    char list[256] = "";
    for (int i = 0; i < ms_class_count(p); i++)
        if (i != zach && ms_class_is(p, i, zach) && ms_class_can_create(p, i)) { strcat(list, ms_class_name(p, i)); strcat(list, " "); }
    check(!strcmp(list, "Drzwi Skrzynia "), "classes derived from Zachowanie");

    /* ---------- fields ---------- */
    MsField f;
    check(ms_field_count(p, zach) == 2 && ms_field_count(p, drzwi) == 2 + 12, "field counts (inherited first, no statics)");
    check(ms_field_info(p, drzwi, 0, &f) && !strcmp(f.name, "Nazwa") && f.kind == T_STRING && f.owner == zach &&
          f.access == MS_ACC_PUBLIC, "field 0: inherited public string Nazwa");
    check(ms_field_info(p, drzwi, 1, &f) && !strcmp(f.name, "licznik") && f.access == MS_ACC_PROTECTED, "field 1: protected licznik");
    check(ms_field_info(p, drzwi, field(p, drzwi, "Zamkniete"), &f) && f.kind == T_BOOL && f.owner == drzwi, "bool field");
    check(ms_field_info(p, drzwi, field(p, drzwi, "Szybkosc"), &f) && f.kind == T_FLOAT, "float field");
    check(ms_field_info(p, drzwi, field(p, drzwi, "Barwa"), &f) && f.kind == T_ENUM && f.cls == kolor, "enum field");
    check(ms_field_info(p, drzwi, field(p, drzwi, "Miejsce"), &f) && f.kind == T_STRUCT && f.cls == punkt, "struct field");
    check(ms_field_info(p, drzwi, field(p, drzwi, "Para"), &f) && f.kind == T_CLASS && f.cls == drzwi, "class field");
    check(ms_field_info(p, drzwi, field(p, drzwi, "Klucze"), &f) && f.kind == T_LIST && f.elemKind == T_STRING, "List<string> field");
    check(ms_field_info(p, drzwi, field(p, drzwi, "Siatka"), &f) && f.kind == T_ARRAY && f.elemKind == T_INT && f.rank == 2, "int[,] field");
    check(ms_field_info(p, drzwi, field(p, drzwi, "Licz"), &f) && f.kind == T_DICT && f.keyKind == T_STRING && f.elemKind == T_INT,
          "Dictionary<string, int> field");
    check(ms_field_info(p, drzwi, field(p, drzwi, "Stala"), &f) && f.readOnly == 1, "readonly field");
    check(ms_field_info(p, drzwi, field(p, drzwi, "ukryte"), &f) && f.access == MS_ACC_NONE, "field without access modifier");
    check(ms_field_info(p, drzwi, field(p, drzwi, "znak"), &f) && f.access == MS_ACC_PRIVATE && f.kind == T_CHAR, "private char field");
    check(ms_field_find(p, drzwi, "Wszystkie") < 0, "static field is not an instance field");
    check(!ms_field_info(p, drzwi, 99, &f) && !ms_field_info(p, 999, 0, &f), "bad numbers");

    /* ---------- enums ---------- */
    check(ms_enum_count(p, kolor) == 3 && !strcmp(ms_enum_name(p, kolor, 1), "Zielony") && ms_enum_value(p, kolor, 1) == 5 &&
          ms_enum_value(p, kolor, 2) == 6 && ms_enum_count(p, drzwi) == 0, "enum members");

    /* ---------- methods ---------- */
    int owner = -1;
    int upd = ms_method_find(p, drzwi, "Update", 0, &owner);
    check(upd >= 0 && owner == drzwi, "Drzwi overrides Update");
    owner = -1;
    check(ms_method_find(p, skrz, "Update", 0, &owner) >= 0 && owner == zach, "Skrzynia inherits Update of Zachowanie");
    check(ms_method_find(p, skrz, "Start", 0, &owner) >= 0 && owner == zach, "Start of the base class");
    check(ms_method_find(p, zach, "Punkty", 0, NULL) < 0, "abstract method: nothing to run");
    check(ms_method_find(p, drzwi, "Statyczna", 0, NULL) < 0, "static methods are not found");
    check(ms_method_find(p, drzwi, "Nie", 0, NULL) < 0, "unknown method");
    int ustaw1 = ms_method_find(p, drzwi, "Ustaw", 1, NULL), ustaw2 = ms_method_find(p, drzwi, "Ustaw", 2, NULL);
    check(ustaw1 >= 0 && ustaw2 >= 0 && ustaw1 != ustaw2, "overloads by the number of parameters");

    /* ---------- objects ---------- */
    VM* vm = vm_new(p);
    int si = ms_entry(p, MS_ENTRY_STATIC_INIT);
    check(si < 0 || vm_call(vm, si) == 1, "static init");

    Value d;
    check(vm_new_object(vm, drzwi, &d) == 1 && ms_class_of(d) == drzwi, "new Drzwi by number");
    int hd = ms_vm_keep(vm, d);
    check(hd >= 0, "keep the object");
    char buf[128];
    ms_value_to_text(vm, ms_get_field(d, field(p, drzwi, "Nazwa")), buf, sizeof buf);
    check(!strcmp(buf, "drzwi"), "constructor ran (Nazwa = drzwi)");
    check(ms_get_field(d, field(p, drzwi, "Zamkniete")).as.i == 1 && ms_get_field(d, field(p, drzwi, "Stala")).as.i == 7,
          "field initializers ran");
    check(ms_get_field(d, field(p, drzwi, "Klucze")).t == V_OBJ, "List made by its initializer");
    check(ms_class_of(ms_get_field(d, field(p, drzwi, "Miejsce"))) == punkt, "struct field holds a zeroed struct");
    Value tmp;
    check(vm_new_object(vm, zach, &tmp) == 0 && strstr(ms_vm_error(vm), "Zachowanie") != NULL, "abstract class cannot be created");
    check(vm_new_object(vm, 999, &tmp) == 0, "bad class number");

    /* set fields with the type checked */
    check(ms_set_field(vm, d, field(p, drzwi, "Zamkniete"), bv(0)) && ms_get_field(d, field(p, drzwi, "Zamkniete")).as.i == 0, "set bool");
    check(ms_set_field(vm, d, field(p, drzwi, "Szybkosc"), iv(3)) && ms_get_field(d, field(p, drzwi, "Szybkosc")).t == V_FLOAT &&
          ms_get_field(d, field(p, drzwi, "Szybkosc")).as.f == 3.0f, "int into a float field");
    check(ms_set_field(vm, d, field(p, drzwi, "Szybkosc"), fv(0.25f)), "set float");
    check(ms_set_field(vm, d, field(p, drzwi, "Barwa"), iv(6)), "set enum (its number)");
    Value s = vm_make_string(vm, "piwnica", 7);
    check(ms_set_field(vm, d, 0, s), "set string");
    check(!ms_set_field(vm, d, field(p, drzwi, "Zamkniete"), iv(1)) && strstr(ms_vm_error(vm), "'Zamkniete' of 'Drzwi' is bool, not an int"),
          "int into bool refused");
    check(!ms_set_field(vm, d, field(p, drzwi, "Szybkosc"), s) && strstr(ms_vm_error(vm), "float, not a string"), "string into float refused");
    check(!ms_set_field(vm, d, field(p, drzwi, "Barwa"), fv(1)), "float into enum refused");
    check(ms_set_field(vm, d, 0, nullv()), "null into a string field");

    Value d2, sk, gd;
    check(vm_new_object(vm, drzwi, &d2) == 1, "second Drzwi");
    int hd2 = ms_vm_keep(vm, d2);
    check(ms_set_field(vm, d, field(p, drzwi, "Para"), d2), "object into its own class field");
    check(vm_new_object(vm, skrz, &sk) == 1, "Skrzynia");
    int hsk = ms_vm_keep(vm, sk);
    check(ms_set_field(vm, d, field(p, drzwi, "Cel"), sk), "derived object into a base class field");
    check(!ms_set_field(vm, d, field(p, drzwi, "Para"), sk) && strstr(ms_vm_error(vm), "is Drzwi, not an object"), "wrong class refused");
    check(vm_new_object(vm, gdrzwi, &gd) == 1, "Gra.Drzwi");
    check(!ms_set_field(vm, d, field(p, drzwi, "Para"), gd), "Gra.Drzwi is not Drzwi");
    check(ms_set_field(vm, d, field(p, drzwi, "Para"), nullv()), "null into a class field");
    check(!ms_set_field(vm, d, field(p, drzwi, "Siatka"), vm_make_array(vm, 3)), "int[] into int[,] refused");
    check(!ms_set_field(vm, iv(1), 0, iv(1)) && !ms_set_field(vm, d, 99, iv(1)), "not an object / no such field");

    /* a struct is copied when it is stored */
    Value pt;
    check(vm_new_object(vm, punkt, &pt) == 1, "new Punkt");
    ms_set_field(vm, pt, 0, iv(4));
    check(ms_set_field(vm, d, field(p, drzwi, "Miejsce"), pt), "set struct");
    ms_set_field(vm, pt, 0, iv(9));
    Value stored = ms_get_field(d, field(p, drzwi, "Miejsce"));
    check(ms_get_field(stored, 0).as.i == 4 && stored.as.o != pt.as.o, "struct stored as a copy");
    check(!ms_set_field(vm, d, field(p, drzwi, "Miejsce"), nullv()), "null into a struct field refused");

    /* call what a script overrides */
    check(vm_call_args(vm, upd, &d, 1, NULL) == 1 && vm_call_args(vm, upd, &d, 1, NULL) == 1, "Update twice");
    Value r = nullv();
    int pk = ms_method_find(p, drzwi, "Punkty", 0, NULL);
    check(vm_call_args(vm, pk, &d, 1, &r) == 1 && r.t == V_INT && r.as.i == 44, "Punkty() = (1 + 10) * 2 * 2 after two Updates");
    Value a2[3] = { d, iv(5), iv(6) };
    check(vm_call_args(vm, ustaw2, a2, 3, NULL) == 1 && vm_call_args(vm, pk, &d, 1, &r) == 1 && r.as.i == 22, "Ustaw(5, 6) with arguments");
    check(vm_call_args(vm, ustaw2, a2, 2, NULL) == 0 && strstr(ms_vm_error(vm), "takes 3 arguments"), "wrong number of arguments");
    int supd = ms_method_find(p, skrz, "Update", 0, NULL);
    check(vm_call_args(vm, supd, &sk, 1, NULL) == 1 && ms_get_field(sk, 1).as.i == 1, "Skrzynia runs the base Update");

    /* a runtime error in a script: the call fails with the message and the VM can go on */
    int psuj = ms_method_find(p, drzwi, "Psuj", 0, NULL);
    check(vm_call_args(vm, psuj, &d, 1, NULL) == 0 && strstr(ms_vm_error(vm), "null"), "runtime error reported");
    check(vm_call_args(vm, pk, &d, 1, &r) == 1 && r.as.i == 22, "VM works after the error");

    /* kept objects survive the GC; others are freed */
    int smieci = ms_method_find(p, drzwi, "Smieci", 0, NULL);
    for (int i = 0; i < 4; i++) check(vm_call_args(vm, smieci, &d, 1, NULL) == 1, "making garbage");
    Value kd = ms_vm_kept(vm, hd);
    check(kd.as.o == d.as.o && ms_get_field(kd, field(p, drzwi, "Barwa")).as.i == 6, "kept object survives the GC");
    check(ms_class_of(ms_vm_kept(vm, hd2)) == drzwi && ms_class_of(ms_vm_kept(vm, hsk)) == skrz, "other kept objects too");
    ms_vm_release(vm, hd2);
    ms_vm_release(vm, hd2);                         /* twice: nothing happens */
    check(ms_vm_kept(vm, hd2).t == V_NULL, "released handle gives null");
    int h3 = ms_vm_keep(vm, sk);
    check(h3 == hd2, "released handle is reused");
    int h4 = ms_vm_keep(vm, sk);
    check(h4 >= 0 && h4 != h3 && h4 != hd, "next handle is new");
    for (int i = 0; i < 2; i++) check(vm_call_args(vm, smieci, &d, 1, NULL) == 1, "more garbage");
    check(ms_class_of(ms_vm_kept(vm, h3)) == skrz, "re-kept object survives");

    vm_free(vm);
    ms_free_program(p);
    if (!fails) printf("test_reflect: ok\n");
    return fails ? 1 : 0;
}
