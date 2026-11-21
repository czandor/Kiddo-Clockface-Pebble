# Kiddo (Pebble Time, Time Round, Time 2, Round 2)

- Rögzíts minden módosítást a `docs/VALTOZASOK.md` fájlban. Az elrendezési vagy működési döntéseket és az új fejlesztési lépéseket vezesd át a `docs/FEJLESZTES.md` fájlba.
- A `docs/VALIDATION.md` külön jelezze, mit teszteltél az emulátoron és mit fizikai órán. Ha egy ellenőrzés nem volt elvégezhető, írd le a korlátot.
- A Fitbit referencia az `original/` mappában van (nincs verziókezelve). Az eredeti `C:\andor\Fitbit.7z` archívum csak olvasható referencia.
- A glifeket a `tools/make-glyphs.py` generálja az `original/` képeiből; a `pebble/resources/images/kiddo_*.png` és a `pebble/src/c/glyphs.h` generált fájl, kézzel ne szerkeszd.
- A build, a beállítások generálása és az emulátoros próba leírását a `docs/FEJLESZTES.md` tartalmazza.
- GitHub-repó: `czandor/Kiddo-Clockface-Pebble` (nyilvános). Pusholni csak Andor kérésére szabad.
- Minden kiadásnál a lefordított PBW egy verziózott GitHub Release-re is kerüljön fel (`Kiddo vX.Y.Z`, tag `vX.Y.Z`, `Kiddo-X.Y.Z.pbw` + `SHA256SUMS.txt`), és a letöltött fájl egyezzen a `dist/` buildjével.
