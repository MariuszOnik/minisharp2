# Dokumentacja MiniSharp – jak ją pisać

Rozdziały to pliki `NN-nazwa.md` (kolejność = numer). Każdy edytor (3DS, przeglądarka, później np. Switch) dołącza je
jako Help, więc format jest celowo **mały** – podzbiór Markdown, który łatwo pokazać na każdym ekranie:

- `# Tytuł` – tytuł rozdziału (jeden, w pierwszej linii); `## Podtytuł` – sekcja; głębiej nie schodzimy.
- Zwykłe akapity (pusta linia między nimi), listy z `- `, `kod w tekście` w odwróconych apostrofach, **pogrubienie**.
- Blok ` ```cs ` – **cały program**, który się kompiluje. Zaraz po nim może być blok ` ```wynik ` z tym, co program
  wypisze. `make test` sprawdza każdy taki przykład (`doc/<rozdział>_<numer>`), więc dokumentacja nie kłamie.
- Blok ` ```text ` – fragment, którego nie sprawdzamy (np. pół programu, komunikat błędu).
- Akapit zaczynający się od `> **Dla zaawansowanych:**` – dopisek, który edytor może zwinąć albo pokazać innym kolorem.
- Bez tabel, obrazków, HTML i linków zewnętrznych. Odwołanie do innego rozdziału: „zob. rozdział 3”.

Pisane po polsku, prosto, dla początkujących: najpierw przykład, potem wyjaśnienie. Pliki w UTF-8 bez BOM.
Bez nazw gier. Każdy commit dopisuje albo poprawia jeden rozdział (gdy jest co opisać).
