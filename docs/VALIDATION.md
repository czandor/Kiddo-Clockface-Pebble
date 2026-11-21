# Ellenőrzések

## 1.0.0 (2026-11-21)

### Build és tesztek
- **Build:** `tools/build.ps1` sikeres `basalt` + `chalk` + `emery` + `gabbro` célra, fordítási figyelmeztetés nélkül (csak a szokásos `RWX` linker-figyelmeztetés). Basalt RAM-lábnyom ~7,7 KB, 57,8 KB szabad heap.
- **Egységtesztek (`node tools/test.cjs`):** sikeres. Ellenőrzi: manifest és platformonkénti sprite-lap erőforrás, a Clay és a C kód minden AppMessage kulcsa szerepel a manifestben (és fordítva), minden kirajzolt karakterhez (napok, hónapok, `am`/`pm`, `on`/`off`, számok, `.`, `+`) van glif, az alapértékek (Emery: pulzus / lépés / harang | táv / kalória / alvás; HR nélkül aktív perc a pulzus helyén), a színkonverzió (`0xRRGGBB` → `GColor8`), a mentés, valamint hogy HR nélküli órán a beállítóoldal nem kínál pulzust és rázásos mérést.

### Emulátoron
- **Mind a 4 platform (`docs/<platform>-24h.png`, `-12h.png`, `-12h-am.png`, `-second-board.png`):** 24 órás (`20:08`) és 12 órás mód `PM` (`4:06`) és `AM` (`10:00`) felirattal, valamint rázásra (`emu-tap`) a második tábla (táv, kalória, alvás). Chalk-on hardfault nélkül fut (a szövegrajzolás bitképekkel megy, nincs `graphics_draw_text`).
- **Leghosszabb tartalom:** `WED`, `SEPTEMBER 30`, `10:08 PM` / `22:08`, 5 jegyű lépésszám (`12345`): mind a 4 kijelzőn belül marad; a kerek órákon 12 órás módban a nap és az `AM`/`PM` beljebb, a körön belül van.
- **Néma idő (`docs/*-quiet-off*.png`):** az emulátorban a Néma idő nem kapcsolható (a menüben csak a Watchfaces van, a Back nyomva tartása sem kapcsolja), ezért egy ideiglenes `bool quiet = true;` builddel ellenőrizve: mind a 4 platformon áthúzott piros harang + `OFF`. A tesztkód visszaállítva, a kiadott build a valódi `quiet_time_is_active()` értéket használja. (Az Emery Néma idős kép még a korábbi, sűrűbb felső elrendezéssel készült.)
- **Rázásos HR-mérés, Emery (`docs/emery-hr-measuring.png`, `docs/emery-hr-result.png`):** ideiglenes `companion.js` alapértékkel (`HR_SHAKE: 1`, utána visszaállítva). `emu-tap` → második tábla, újabb `emu-tap` → fő tábla villogó szívvel; `78` BPM-es `emu-heart-rate` minták után a napló: `Shake HR measurement started` → `received a fresh result: 78`, a szív piros, az érték `78`. (A képek a korábbi Emery elrendezéssel készültek.)
- **Élénk színkészlet (`docs/vivid-colors.png`):** Emery és Gabbro emulátoron 5 időpontban (`9:12`, `11:47`, `14:25`, `18:03`, `20:08`), azaz 5 különböző véletlen színsorsolással: csak élénk háttér-, tábla- és betűszínek, szürkés, pasztell vagy sötét árnyalat nincs. A `tools/test.cjs` ellenőrzi, hogy az élénk készlet pontosan a 29 megfelelő palettaszín, és hogy csak `emery`-n és `gabbro`-n érvényes.
- **Töltés (`docs/emery-charging.png`):** `emu-battery --percent 70 --charging` → kék akku ikon és `70+`; Basalt-on is ellenőrizve.

### Fizikai órán
- **Telepítés a telefonon keresztül (`192.168.130.135`):** az 1.0.0 build sikeresen felkerült (`App install succeeded`). Az első próbálkozás `Connection refused` volt (a Developer Connection nem fogadott), később sikerült.
- **Andor visszajelzése Pebble Time 2-n:** „felül sűrűn vannak dolgok, alul meg szellős”. Erre az Emery elrendezés lejjebb került, és kérésre bekerült a töltésjelző `+`. Az új buildet Andor a Pebble Time 2-n jóváhagyta („Jó így”).
- **Pebble Time Round (valódi óra, ugyanazon a telefonon, `docs/chalk-physical.png`):** az 1.0.0 build sikeresen telepítve, a `pebble screenshot --phone` képe szerint hiba nélkül fut (`17:32`, 24 órás mód, aktív perc / lépés / harang `ON`, akku `100`); a teljes rajzolás valódi chalk hardveren is hardfault nélkül lefut, az elrendezés a körön belül van.
- **Élénk színkészletes build:** az első telepítés nem sikerült, mert a telefon épp nem volt elérhető (WSL-ből `No route to host`, Windowsról sem ping, sem a `9000`-es port). Később sikerült, de a telefonhoz akkor még a Time Round volt csatlakoztatva (a `pebble screenshot --phone` kép 180×180-as), amelyen a színkészlet nem változott. A PT2-n az élénk színek hatását még Andornak kell megnéznie.
- **Észrevétel a Time Roundon:** a valódi órán egy akkuszámjegy (`9`) a zöld háttérhez nagyon hasonló színt kapott, alig látszott. Az eredeti Fitbit-logika csak a pontosan azonos színt zárja ki, a hasonlót nem.
- **Még ellenőrzendő fizikai órán:** a valódi Néma idő (`OFF`, áthúzott harang), a rázás érzékenysége és a második tábla, a rázásos HR-mérés, a színek olvashatósága valódi kijelzőn, a Clay beállítóoldal megnyitása és mentése a telefonon, valamint a Time Round 12 órás módja és a Round 2 elrendezés valódi órán.
