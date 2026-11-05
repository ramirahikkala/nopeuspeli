# Arcade-kotelo v3.17

## Mitä muuttui (v3.16 → v3.17)
Myös ULN2803-moduulin piikkirima on käännetty: lyhyt pää nousee 1,4 mm ULN-PCB:n yli ja pitkä pää tulee 4,4 mm protolevyn alle. Paneelin ULN-tasku on **poistettu**, koska piirin välys paneeliin on ilman taskuakin 1,76 mm. Vain `panel` muuttui, muut osat ovat samat kuin v3.16:ssa.

ULN:n pitkien piikkien välykset: puristuslista L 7,0 (juotoskartiot 6,4), paneelin rivat/tornit 2,9, runko ja kaapelireitti 15,4, akku 22,1, pohjalevy 63,9.

## v3.15 → v3.16
T-Displayn piikkirima on käännetty niin kuin se on oikeasti koottu: lyhyt pää (3,0) menee T-Displayyn ja nousee 1,3 mm sen PCB:n yli. Se ei osu LCD:hen eikä runkoon, koska STEP-mallissa on rako piikkirivien kohdalla. Pitkä pää tulee 4,4 mm protolevyn alapinnan alle. **Tulostettavat osat ovat samat kuin v3.15:ssä**, koska kaikki välykset ovat vähintään 1 mm.

Pitkien piikkien välykset: puristuslista R 2,4 (juotoskartiot 1,8), paneelin rivat/tornit 3,3, runko ja kaapelireitti/korvakkeet 11,5, akku 22,1, pohjalevy 63.

## Aiemmat muutokset (v3.14 → v3.15)
Elektroniikan mitat on korjattu vastaamaan mittauksiasi. Kaikki on mitattu levyjen alapinnoista:
- protolevyn alapinta → T-Displayn PCB:n alapinta 4,3 mm
- T-Displayn PCB:n alapinta → lasi 6,5 mm, TD-PCB 1,55 mm
- protolevyn alapinta → lasi siis 10,8 mm, ja levyjen väli on 2,7 mm (vakio-piikkirima 2,5 + paljasta piikkiä 0,16)

Näyttö ja paneelin ikkuna ovat ennallaan. Protolevy siirtyi 1,8 mm lähemmäs paneelia. T-Displayn STEP-mallia on korotettu 0,12 mm (6,38 → 6,5).

## Tulostettavat osat
| Osa | Muutos |
|---|---|
| bottom_plate, button_hole_coupon | ei muutosta |
| clamp_bar_L | sama osa, vain paikka muuttui |
| clamp_bar_R | TD-kiskot matalammat (korkeus 9,0 → 7,1) |
| panel | stopparirivat, ohjainseinät ja tornit 1,8 lyhyemmät |
| frame | kaapelikanava 0,12 syvempi (pieni muutos) |
| td_key_x2, test_coupon | päivitetty uuteen korkeuteen |

## Tärkeät välykset
- ULN-piiri → paneeli 1,76 · ULN-piikit → paneeli 2,86
- T-Displayn piikit → paneeli 3,59
- Listat → paneeli 0,2 (esijännitys)
- Akku ja pohjalevyn lukitus ennallaan (rivat 0,5, akku → juotokset 20,5)

## Kokoonpano
- T-Displayn piikkejä ei tarvitse katkaista.
- ULN-piikkejä ei tarvitse katkaista.

## Tiedostot
- `*_v3_17.stl / .3mf / .step`: tulostettavat osat
- `arcade_kotelo_v3_17_assembly_optionA/B.step`: kokoonpanot elektroniikan kanssa
- `electronics_reference_v3_17.step`: elektroniikan referenssimalli
- `elektroniikkapino_mitoitus_v3_17.pdf/.png`: pinon mittapiirros
- `checks_v3_17.txt`: tarkistukset
- PNG-renderöinnit: B2, S1, U1, L1 ja L2 (pitkät TD- ja ULN-piikit protolevyn alla)

Asetukset: PLA, 0,4 mm suutin, vähintään 2 seinämää, ei tukia.
