# Fejlesztői átadás — Kiddo (Pebble Time, Pebble Time Round, Pebble Time 2 & Pebble Round 2)

## Mi hol van

| Fájl | Feladat |
| --- | --- |
| `pebble/src/c/main.c` | Natív C óralap: platformonkénti elrendezés, palettás sprite-lapos glifrajzolás, véletlen színek, egészségtábla, csuklórázás (második tábla, igény szerinti HR-mérés), Néma idő harangja, beállításkezelés. |
| `pebble/src/c/glyphs.h` | **Generált** (`tools/make-glyphs.py`): platformonként a sprite-lap erőforrás-azonosítója, a dobozméretek és minden glif helye, eltolása és előretolása (1/16 px). |
| `pebble/resources/images/kiddo_<platform>.png` | **Generált** sprite-lapok: 4 szín (átlátszó, fekete körvonal/árnyék, fehér kitöltés, világosszürke akkuvég). |
| `pebble/src/pkjs/companion.js` | Telefonos Clay beállítások (platformfüggő oldal), `localStorage` mentés, színkonverzió, AppMessage küldés. |
| `tools/make-glyphs.py` | Glif- és ikongenerátor az `original/source/resources/images` Fitbit-képeiből; az alvás-, harang- és áthúzottharang-ikont ugyanilyen stílusban maga rajzolja. |
| `tools/prepare-project.cjs` | A `pebble/package.json` és a `pebble/src/pkjs/config.json` forrása. Az AppMessage kulcsok sorrendje adja az azonosítókat, ezért új kulcs mindig a lista végére kerül. |
| `tools/build.ps1`, `tools/build.sh` | Windowsról a WSL `NineTimeSDK` buildet futtatja, a PBW-t `dist/Kiddo-<verzió>.pbw` néven menti, `SHA256SUMS.txt`-vel. |
| `tools/test.cjs` | Manifest-, glifkészlet- és `companion.js`-tesztek (mockolt Pebble/Clay környezetben). |
| `tools/emulator-shots.sh` | Dokumentációs képernyőképek mind a négy emulátoron (24 h, második tábla, 12 h PM és AM). |
| `tools/emulator-pulse.sh` | Pulzusminta-sorozat az Emery emulátorra (a Blokkból átvéve). |

Az eredeti Fitbit Kiddo forrás, a képek és a bolti képernyőképek az `original/` mappában vannak (nincs verziókezelve). Forrásuk a `C:\andor\Fitbit.7z` archívum `Fitbit/Kiddo/` mappája, ami csak olvasható referencia. Kibontása 7-Zip nélkül, a Windows saját `tar.exe`-jével:

```powershell
& "$env:SystemRoot\System32\tar.exe" -xf C:\andor\Fitbit.7z -C <célmappa> "Fitbit/Kiddo/*"
```

A Kiddo a Fitbit **FunColor** átnevezett, újabb (2022-es, Versa 3/Sense) változata, ugyanazzal az UUID-del. A port alapja a `Fitbit/Kiddo/source/source` (azonos a `Kiddo-export.zip`-pel).

## Fordítás és próba

A projekt gyökerében Windows alatt:

```powershell
python -P tools/make-glyphs.py          # csak ha a glifek / méretek változtak
powershell -ExecutionPolicy Bypass -File tools/build.ps1
node tools/test.cjs
```

A `make-glyphs.py` a Pillow csomagot használja, és `--preview <mappa>` kapcsolóval 3×-os nagyítású sprite-lap előnézetet is ír.

Emulátoros telepítés (`NineTimeSDK` WSL-ben; `--emulator basalt`, `chalk`, `emery` vagy `gabbro`):

```bash
export PATH=/opt/ninetime-cli/bin:/usr/local/bin:/usr/bin:/bin
cd /mnt/c/andor/Pebble/Kiddo/pebble
pebble install --emulator emery ../dist/Kiddo-1.0.0.pbw
pebble emu-set-time --emulator emery 20:08:00     # vagy Unix UTC másodperc, pl. 1790798880 = 2026-09-30 22:08 (CEST)
pebble emu-time-format --emulator emery --format 12h
pebble emu-tap --emulator emery                    # csuklórázás: második tábla
pebble emu-steps --emulator emery 12345
pebble emu-heart-rate --emulator emery --quality excellent 78
pebble screenshot --no-open --emulator emery ../docs/emery.png
```

Az összes dokumentációs kép egyben: `bash tools/emulator-shots.sh basalt chalk emery gabbro` (WSL-ben; Git Bashből `MSYS_NO_PATHCONV=1` kell a `/mnt/...` útvonal miatt).

Fizikai órára (telefonon bekapcsolt Developer Connection mellett):

```bash
pebble install --phone 192.168.130.135 ../dist/Kiddo-1.0.0.pbw
```

A telefonos JS induláskor elküldi a beállításokat, ezért nem alapértelmezett opciók (pl. rázásra HR-mérés) emulátoros kipróbálásához ideiglenesen a `companion.js` `defaults` objektumát kell átírni, majd a próba után visszaállítani. A Néma idő az emulátorban nem kapcsolható (a menüben csak a Watchfaces van, a Back nyomva tartása sem kapcsolja), ezért az `OFF` állapotot egy ideiglenes, `bool quiet = true;` build mutatja meg.

## Elrendezési és működési döntések (`1.0.0`)

- **Glifek a Fitbit-képekből, nem fontból:** az eredeti minden karaktere egy szürkeárnyalatos kitöltőmaszk (`x.png`) és egy fekete körvonal + jobbra lent 3D-árnyék réteg (`xb.png`). A `make-glyphs.py` ezeket platformméretre skálázza (LANCZOS), és 4 osztályba sorolja: kitöltés, ha a maszk ≥ 110; különben fekete, ha a réteg alfája ≥ 110 és sötét (< 150); világos rétegpixel a maszk szélén kitöltés, attól távol világosszürke (csak az akku pólusa); minden más átlátszó. A glifek a tartalmukra vannak vágva, és egy közös, polcos elrendezésű sprite-lapra kerülnek (Basalt/Chalk 256×109, Emery 256×190, Gabbro 320×176).
- **Futásidejű átszínezés:** a lap `2BitPalette` + `pbi` erőforrás. Betöltéskor a kód megkeresi a fehér palettabejegyzést (`s_fill_index`), és minden glif előtt ezt írja át, majd `gbitmap_set_bounds()` + `graphics_draw_bitmap_in_rect()` (`GCompOpSet`) rajzolja. Így minden betű más színű lehet, külön betűtípus és extra RAM nélkül (a lap 7–14 KB).
- **Szövegelrendezés:** a Fitbit `SIZES` táblája (bal eltolás, látható szélesség, `spacer`) skálázva, 1/16 px pontos előretolással; a bal/közép/jobb igazítás ugyanúgy működik, mint a Fitbit `printText()`-ben. Az órák nincsenek nullával kiegészítve (`9:05`), mint az eredetiben.
- **Méretek** (Fitbit 336 px-es dobozokhoz képest): nagy számjegy 0,48 / 0,50 / 0,66 / 0,74, kis szöveg és ikon 0,56 / 0,56 / 0,72 / 0,78 (Basalt / Chalk / Emery / Gabbro); egészségtábla 140×50, 136×48, 194×68, 202×70.
- **Felső sor:** 24 órás módban (vagy kikapcsolt `AM`/`PM` mellett) a nap középen; 12 órás módban a nap balra, az `AM`/`PM` jobbra igazítva `TOP_MARGIN` távolságra a széltől. A kerek kijelzőkön ez a távolság nagyobb (Chalk 50, Gabbro 72), így a két szó beljebb, a körön belül, nagyjából az időjegyek szélességében van.
- **Lekerekített sarkok:** téglalap kijelzőn a háttér `CORNER_R` sugarú lekerekített téglalap fekete alapon (Basalt 12, Emery 16), az eredeti `border.png` kártyás kinézetéhez; kerek kijelzőn nincs maszk.
- **Színek:**
  - A Kiddo színtáblája eleve a Pebble 64 színes palettája volt (a „normal” árnyalatok a Pebble Time kijelzőjét utánozták). Pebble Time-on és Time Roundon a véletlen színek a Fitbit-táblában engedélyezett 50 színből jönnek (fekete, fehér és a legsötétebbek nélkül).
  - **Élénk színkészlet Pebble Time 2-n és Round 2-n (`RANDOM_POOL`, `#if PBL_PLATFORM_EMERY || PBL_PLATFORM_GABBRO`):** Andor szerint ugyanaz a lap a régi Time Roundon szebb, élénkebb színű, mint a PT2-n. A kód mindkettőre ugyanazokat a `GColor8` értékeket küldi, a különbség a kijelzőből jön. A PT2 is 64 színű, tükröződő memory LCD, de egy optikailag ragasztott érintőréteg van fölötte, és tesztek és tulajdonosok egyaránt sötétebbnek, tompább színűnek írják le (a fórumon a fekete hátteret, fehér számokat ajánlják). A Round 2 is érintőképernyős. Tükröződő kijelzőnél a fény kétszer halad át a felső rétegeken, ezért minden plusz réteg sötétít és fakít, leginkább a pasztell és a köztes árnyalatoknál (ez saját következtetés, hivatalos magyarázat nincs). Ezért ezen a két órán a véletlen színek csak a 29 élénk palettaszínből jönnek: egy csatorna `FF`, egy másik `00` vagy `55` (17 teljesen telített + 12 élénk, kicsit világosabb). Kimarad 8 sötétebb telített (pl. Islamic Green, Cobalt Blue), 6 pasztell (pl. Melon, Celeste) és 7 szürkés (pl. Light Gray, Brass, Cadet Blue) szín. Források: [Android Authority](https://www.androidauthority.com/pebble-time-2-metal-design-display-3586414/), [marius.blog](https://marius.blog/pebble-time-2-review/), [T3](https://www.t3.com/active/fitness-trackers/pebble-time-2-review), [repebble fórum](https://forum.repebble.com/t/dark-screen-on-new-pebble-time-2/894), [Pebble súgó – háttérvilágítás](https://help.repebble.com/en/articles/15277496-backlight).
  - **Háttér a Blokk szerint, napszak nélkül:** `Choose one color` (Clay színválasztó), `Random every day`, `Random every hour`, `Random every minute` (alapértelmezett) és `Random on wrist flick`; a kulcsok értékei azonosak a Blokkéval (`0`, `4`, `2`, `1`, `3`).
  - Minden véletlen szín egy `seed`-ből, elemenként és betűnként determinisztikusan jön (`random_color()`), így újrarajzoláskor (pl. HR-villogás) nem változik; csak a háttér-opció szerinti időpontban kap a `seed` új értéket. Fix háttérnél a betűk percenként kapnak új színt. Óránkénti / napi / rázásos módban a `seed` tartósan mentődik (`COLOR_STATE_KEY`).
  - Ütközéskerülés, mint az eredetiben: a háttéren lévő szöveg nem lehet háttérszínű, és szivárványmódban két egymás utáni betű nem azonos; a tábla nem lehet háttérszínű; a táblán lévő szám nem lehet se tábla-, se ikonszínű.
  - Beállítható: betűk (szivárvány / szövegenként egy véletlen / fix), tábla (véletlen / fix), táblaszámok (véletlen / fix). A szögek világosszürkék (az eredeti alapérték). Az ikonok fix színűek az eredeti szerint (lépés, aktív perc, táv, kalória); az alvás az emelet sárgáját, a harang zöldet, az áthúzott harang pirosat kapott.
- **Egészségtábla:** 6 választható hely (3 a fő, 3 a második táblán): `Nothing`, `Heart rate`, `Steps`, `Active minutes`, `Distance`, `Calories`, `Sleep hours`, `Vibration`. Az elhelyezés az eredeti 300×100-as táblából arányosítva (oszlopok 20 / 50 / 80 %-nál; az oldalsókon ikon fent, érték lent, középen fordítva; 4 szög).
  - Alapértelmezés Pebble Time 2-n (`emery`): pulzus, lépés, **harang** | táv, kalória, **alvás**. HR-szenzor nélkül (`basalt`, `chalk`, `gabbro`) a pulzus helyén aktív percek vannak.
  - Az **alvás** az eredeti emelet (barométer) helyére került (hold ikon, órában, egy tizedesjeggyel).
  - A Fitbit Active Zone Minutes helyett a Pebble aktív percei (`HealthMetricActiveSeconds / 60`) szerepelnek, az eredeti AZM-ikonnal.
  - Távolság az eredeti formátumban: 10 alatt két, felette egy tizedesjegy; alapból az óra mértékegység-beállítása szerint (`health_service_get_measurement_system_for_display`).
  - Kalória: aktív + nyugalmi kcal (a Fitbit is a teljes napi értéket mutatja).
- **Néma idő jelzése (alapból látható):** a Blokk `VIB` mezőjéhez hasonlóan alapértelmezésként a fő tábla jobb oldalán, az eredeti AZM helyén: harang + `ON`, ha az óra rezeghet; áthúzott harang + `OFF`, ha a Néma idő aktív. A Néma időnek nincs eseménye: a percenkénti frissítés és a fókusz-visszatérés (`app_focus_service`) rajzolja újra.
- **Csuklórázás** (a Fitbit tábla-koppintás megfelelője):
  - A második táblát 20 másodpercre mutatja (újabb rázás azonnal visszavált); kikapcsolható.
  - `Random on wrist flick` háttérmódban új színeket sorsol.
  - **Igény szerinti HR-mérés** (`HR_SHAKE`, csak Pebble Time 2-n, alapból ki): a Blokk / ProgrammersWatch logikája. Rázásra `1 Hz`-es mintavételt kér legfeljebb 45 másodpercig, és két egymást követő, az induló értéktől eltérő, legfeljebb ±5 BPM-mel szóró nyers mintát vár. Mérés közben a szív 500 ms-onként villog (sötétszürke ↔ normál szín). Az eredmény 60 másodpercig érvényes, a mintavétel leálláskor / időtúllépéskor / bezáráskor visszaáll (`sample_period = 0`). Csak akkor indul, ha valamelyik helyen ki van választva a pulzus. A mérés a második tábla alatt is fut, így a fő táblára visszaváltva már a friss érték látszik.
  - Opcionális 60 ms-os rezgés, Néma időben alapból tiltva.
- **HR-szenzor nélküli órák:** a C kód fordítási időben (`HAS_HR_SENSOR = 0`) kizárja a HR API-kat, és egy esetleg beállított pulzushelyet üresen hagy. A telefonos beállítóoldal ezeken az órákon (`Pebble.getActiveWatchInfo().platform`) nem kínál pulzust és rázásos mérést, a korábban mentett pulzushelyet aktív percekre cseréli. Ehhez a Clay példány a `showConfiguration` eseménykor, platformra szűrt konfigurációval jön létre.
- **Akkujelzés:** az ikon színe tizedenként az eredeti szerint (piros → zöld), töltőn kék (`Very Light Blue`); a szám szivárványszínű. Töltőn (`is_charging || is_plugged`) a százalék mögött `+` áll, mint a Blokkban. A `+` nincs az eredeti Fitbit-készletben: a `make-glyphs.py` (`plus_pair()`) rajzolja a kis glifek stílusában, a számjegyek magasságához igazítva.
- **Függőleges elrendezés (`TOP_Y`, `TIME_Y`, `DATE_Y`, `BOARD_Y`, `BATT_Y`):** Andor visszajelzése alapján („PT2-n felül sűrűn vannak dolgok, alul meg szellős”) az Emery elrendezés lejjebb került (`9 / 31 / 90 / 119 / 198`), így a sorközök 8–13 px között egyenletesek, és az akku alatt is marad 8 px. Basalt-on a tábla 4 px-szel lejjebb van (`BOARD_Y = 83`), hogy a dátum és az akku felé egyforma legyen a hézag.
- **Frissítés:** `MINUTE_UNIT` tick, Health-, akku- és fókuszesemények. Képkocka-gyorsítótár nincs: a teljes rajzolás néhány tucat kis bitkép, nincs pixelenkénti menet.
- **Verem (Time Round):** a rajzolás bitképekkel megy (nincs `graphics_draw_text`), a szövegpuffer statikus, a rajzoló függvények `noinline`-ok. Chalk emulátoron hardfault nélkül fut.
- **Memória (1.0.0 build):** RAM-lábnyom Basalt-on ~7,7 KB (57,8 KB szabad heap), az erőforrások 11–21 KB platformonként.
- **Betűtípus:** az eredeti glifek az *Agent Orange* fontból (© Jakob Fischer, pizzadude.dk) készültek. A PBW-ben csak az előre renderelt bitképek vannak (mint a Fitbit-verzióban), a TTF nem.
