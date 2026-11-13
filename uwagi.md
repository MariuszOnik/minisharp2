Twoje pytanie: czy coś działa inaczej na 3DS, Switchu i PC?
MiniSharp jako język ma zachowywać się wszędzie tak samo. Program .cs nie zna procesora. Różnice mogą się brać tylko z kodu w C, który wykonuje bajtkod. Procesory są takie:

3DS: ARM11, 32-bitowy;
Switch: ARM Cortex-A57, 64-bitowy;
PC do testów: x86-64.
W C jest kilka operacji, których wynik standard zostawia procesorowi („undefined behavior”). VM jest na nie przygotowana, sprawdziłem w kodzie:

Operacja	Problem w C	Jak to robi VM
int się przepełnia (a + b, a * b)	niezdefiniowane, kompilator może „optymalizować”	liczy na uint32_t, wynik zawija się jak w C#
x << 40	x86 bierze z liczby przesunięć 5 bitów, ARM 8 bitów, więc wyniki są różne	& 31, jak w C#
int.MinValue / -1	na x86 program się wywraca	osobny przypadek
(int)1e10f, (int)NaN	x86 daje int.MinValue, ARM nasyca	nasyca jawnie, jak 3DS
char ze znakiem lub bez	na x86 char ma znak, na ARM nie ma	lekser używa (unsigned char)
Dwie takie pułapki znalazłem w bibliotece, którą właśnie przenosiłem. Obie poprawiłem:

Mathf.FloorToInt/CeilToInt/RoundToInt używały zwykłego rzutowania C. Mathf.FloorToInt(1e10f) dawało więc na PC -2147483648, a na 3DS 2147483647. Teraz wszędzie wynik jest jak na 3DS.
int.Parse używał strtol, a typ long ma 32 bity na 3DS i Windows, ale 64 bity na Switchu i Linuksie. int.Parse("3000000000") dawało na 3DS 2147483647, a na Switchu liczbę ujemną. Teraz wszędzie jest błąd „outside int”.
Testy lib_edges.cs i rt_intparse.cs to pilnują. CI działa na 64-bitowym Linuksie, a testy lokalne na Windows, więc każdy commit sprawdzamy na dwóch różnych platformach.
Na co jeszcze uważać (tego nie sprawdzałem, to ryzyka na przyszłość):

Ostatnia cyfra wyniku float: sinf, powf i wypisywanie przez %.7g pochodzą z bibliotek C, które są różne na 3DS (newlib), Switchu i PC. W rzadkich przypadkach ostatnia cyfra może się różnić. Testy z wypisywaniem ułamków warto kiedyś uruchomić na konsoli.
Bardzo małe liczby float na 3DS: jednostka zmiennoprzecinkowa 3DS może być ustawiona tak, że zeruje takie liczby („flush-to-zero”). Wtedy Mathf.Epsilon (1.4e-45) mógłby wyjść 0. Do sprawdzenia na konsoli w etapie 3.
Pamięć: wartość w VM zajmuje 8 bajtów na 3DS i 16 bajtów na Switchu i PC. Programy się przez to nie zmieniają, ale na 3DS mieści się w pamięci więcej obiektów. Głęboka rekurencja w parserze i kompilatorze ma też mniej stosu na 3DS.
Kolejność bajtów jest wszędzie taka sama (little-endian), więc tu nie ma problemu.
Ogólna zasada: wszystko, co zależy od maszyny, trzymamy w C pod kontrolą (jawne zawijanie, nasycanie, typy int32_t). Wtedy ten sam program MiniSharp daje ten sam wynik na każdej konsoli.

PLAN.md jest zaktualizowany. Następny krok to project.c (projekt w folderze, uses.txt) z testami projektów. Nadal czeka do obejrzenia ostrzeżenie gcc w lang/compiler.c:330.

dopisz do planu ze chemy miec solidna , pełną przykładów dokumentacje jezyka , jak dziala, jak go uzywac ,poradniki przyklady , co comit dopisujemy jeden rozdzial , chyba ze nie ma co dopisac. Dokumentacja ma byc tak napisana by edytor na dowolnej platformei mugl ja dolaczyc ok  ? pisana jezykiem jak dla poczatkujacych , ale moga byc dopiski  dla zaawansowanych ok ?, potem : Następny krok to project.c (projekt w folderze, uses.txt) z testami projektów

co i kiedy to naprawimy ?: Nadal czeka do obejrzenia ostrzeżenie gcc w lang/compiler.c:330.

prowadzimy tez dev log ok ?