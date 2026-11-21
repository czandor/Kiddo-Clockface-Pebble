# Változások

## 1.0.0 (2026-11-21)

- Első Pebble-kiadás a Fitbit Kiddo óralapból (Design & Code: Andor Czafik): Pebble Time (`basalt`), Pebble Time Round (`chalk`), Pebble Time 2 (`emery`) és Pebble Round 2 (`gabbro`).
- Az eredeti Fitbit-glifekből generált, platformonkénti palettás sprite-lap (`tools/make-glyphs.py`), futásidőben átszínezett kitöltéssel: minden betű saját véletlen színt kaphat.
- Új, az eredeti stílusú ikonok: alvás (hold), harang és áthúzott harang; töltésjelző `+` glif.
- Háttérszín a Blokk szerint, napszak nélkül: fix (színválasztó), naponta, óránként, percenként (alapértelmezett) vagy rázásra véletlen.
- Betű-, tábla- és táblaszám-színek: szivárvány / egy véletlen / fix.
- Egészségtábla 6 választható hellyel; az eredeti emelet helyén alvás, az AZM helyén aktív percek.
- Néma idő jelzése alapból a fő tábla jobb oldalán: harang + `ON`, illetve áthúzott harang + `OFF`.
- Csuklórázás: második tábla 20 másodpercre, opcionális rezgés, `Random on wrist flick` háttérmód, és Pebble Time 2-n opcionális, igény szerinti HR-mérés villogó szívvel (a Blokk logikája).
- HR-szenzor nélküli órákon (`basalt`, `chalk`, `gabbro`) a pulzus helyén aktív percek, a beállítóoldal pedig nem kínál pulzust és rázásos mérést.
- Kerek kijelzőn 12 órás módban a nap és az `AM`/`PM` beljebb húzva.
- Töltés közben `+` jel a százalék mögött.
- Pebble Time 2-n a felső elemek lejjebb kerültek, a függőleges térközök egyenletesek.
- A tábla fekete kerete kicsinyítéskor elvékonyodott és szakadozott: most folytonos, Pebble Time-on és Time Roundon 1 px, Pebble Time 2-n és Round 2-n 2 px vastag.
- Színszabályok minden órán: a háttér és a tábla mindig a 29 élénk palettaszínből jön (pasztell, szürkés és sötét árnyalatok nélkül). A betűk, táblaszámok és táblaikonok mind az 50 Kiddo-színből jöhetnek, de csak abból, amelyik nagyon eltér az alattuk lévő felülettől (CIE76 ≥ 50). A táblaikonok is véletlen színűek, az akkuikon a töltöttség szerinti színű marad, és a háttér ettől is nagyon eltér.
