# Modèle nominal gain / offset — révision de la breadboard (10 novembre 2026)

## Relevé expérimental

Le catalogue conserve les 10 signaux du cahier des charges : en particulier,
**signal 1 = sinus 1 Vpp, 10 kHz, offset demandé 0 V**.

Essai des sorties AD9833 et MCP45HV51 avec les étages suivants déconnectés
et les sondes d'oscilloscope x10 :

| Point | Vmax | Vmin | Vpp obtenu par différence |
|---|---:|---:|---:|
| CH1 AD9833 avant C1 | +748,53 mV | +80,20 mV | 668,33 mV |
| CH2 après C1 | +371,86 mV | −343,40 mV | 715,26 mV |
| CH3 PW0 Gain, ancien code 9 | +49,320 mV | −42,853 mV | 92,173 mV |
| CH4 PW0 Offset, ancien code 128 | +212,04 mV | +73,746 mV | 138,29 mV, dominé par des pics |

Le milieu des extrema de CH4 est +142,89 mV : **ce n'est pas la moyenne DC**.
Le montage U1 → sommateur → LT1206 n'a pas été caractérisé lors de ces mesures.
La différence Vpp entre CH1 et CH2 demande une nouvelle mesure contrôlée.

## Hypothèses provisoires du calcul

- C1 supprime la composante DC du sinus, visible entre CH1 et CH2. NE5532P
  toujours supposé suiveur ; son comportement chargé doit être vérifié.
- Source AD9833 après couplage : **715 mVpp** (arrondi de CH2 à 10 kHz).
  Ce chiffre n'est **pas connu** pour les autres formes/fréquences.
- MCP45HV51 Gain : diviseur supposé idéal avec PB0 = GND, PA0 = source
  et PW0 = entrée du THS4032. La mesure de CH3 au code 9 (~92 mVpp)
  ne suit pas le rapport idéal 9/255 (~25 mVpp). Une mesure unique ne permet
  donc **pas de calibrer** la loi code → amplitude.
- Premier THS4032 : deux résistances 1 kOhm / 1 kOhm, gain non-inverseur
  **2**. L'ancien modèle 490 kOhm / 10 kOhm, gain **50**, est abandonné
  pour cette version du montage.
- MCP45HV51 Offset : PA0/PB0 supposés respectivement +15 V et −15 V ;
  le rapport idéal code/255 et les tensions réelles PW0 restent à vérifier.
- Sommateur non-inverseur : gain unitaire de sommation sous réserve
  du câblage, des suiveurs, de l'absence d'oscillation et de la charge.
- LT1206 : non inclus dans les mesures ; il n'est pas possible d'en
  déduire une amplitude ou un offset réels à la BNC.

## Conversion 8 bits sans profil mesuré

    Vpp_max_provisoire = 715 mVpp * 2 = 1430 mVpp
    code_gain = arrondi(amplitude_mVpp * 255 / 1430)
    code_offset = arrondi((offset_mV + 15000) * 255 / 30000)
    A_estimee_mVpp = arrondi(code_gain * 1430 / 255)
    O_estime_mV = -15000 + arrondi(code_offset * 30000 / 255)

Le logiciel refuse `GF_PLAGE` si l'amplitude demandée dépasse
**1430 mVpp** ou si l'excursion demandée/estimée sort des rails ±15 V.
Ce n'est qu'un contrôle mathématique ; la tension de sortie réelle n'est
pas garantie.

Le zéro d'offset correspondrait au code 127,5, inaccessible en 8 bits.
Le code choisi est 128, soit +58,82 mV **estimés**, mais la **consigne du
signal 1 reste 0 V**. Le pas idéal de cette hypothèse est 117,65 mV.
Le contrôle physique de l'offset nécessite au minimum une mesure `Mean`
en couplage DC, puis un étalonnage à plusieurs codes.

| Signal | Consigne du cahier des charges | Code Gain | Code Offset | Résultat nominal |
|---|---|---:|---:|---|
| 1 SIN 10 kHz | 1 Vpp, 0 V | 178 | 128 | ~998 mVpp, ~+59 mV, non mesurés en VOUT |
| 2 SIN 1 MHz | 5 Vpp, 0 V | — | — | `GF_PLAGE` |
| 3 SIN 1 kHz | 2 Vpp, +3 V | — | — | `GF_PLAGE` |
| 4 SIN 500 Hz | 0,2 Vpp, +10 V | 36 | 213 | ~202 mVpp, ~+10059 mV, non mesurés en VOUT |
| 5 TRI 5 kHz | 20 Vpp, 0 V | — | — | `GF_PLAGE` |
| 6 TRI 100 Hz | 4 Vpp, 0 V | — | — | `GF_PLAGE` |
| 7 TRI 200 kHz | 5 Vpp, −5 V | — | — | `GF_PLAGE` |
| 8 CAR 50 Hz | 10 Vpp, 0 V | — | — | `GF_PLAGE` |
| 9 CAR 300 Hz | 1 Vpp, 0 V | 178 | 128 | ~998 mVpp, ~+59 mV ; forme non mesurée |
| 10 CAR 1 kHz | 4 Vpp, +6 V | — | — | `GF_PLAGE` |

L'affichage OLED NOM présente `A=` et `O=` avec les **consignes**
du catalogue, et non les prédictions ; il ne présente donc plus
`+0.059 V` comme un offset qui aurait été demandé par l'utilisateur.

## Modes et contrat d'état

1. Profil mesuré et approuvé : il a priorité et utilise le convertisseur
   calibré ; état USB `ACTIF_COMMANDE`, `commande_courante=true`.
2. Aucun profil approuvé, deux EPOT disponibles : consignes nominales
   calculées, commandes EPOT appliquées ; état `NOMINAL_NON_CALIBRE`,
   `nominal_courant=true`, `commande_courante=false`.
3. Aucun profil, EPOT indisponibles : forme/fréquence seulement sur DDS,
   état `DDS_SEUL_COMMANDE`. Les deux codes EPOT ne sont pas envoyés.

Dans le mode nominal, `codes` conserve l'estimation correspondant aux
codes envoyés. `derniere` conserve la dernière commande calibrée réussie,
non remplacée par une sortie nominale. Le catalogue, les trames USB
(`$N:TYPE:AMPLITUDE:OFFSET:FREQUENCE#`, `$N?#`, `$?#`) et le format
EEPROM ne changent pas. Aucune édition par encodeur.

## Vérifications physiques avant d'utiliser la BNC

1. Valider les tensions d'alimentation des NE5532P, THS4032, MCP45HV51
   et du LT1206. Découpler les AOP rapides au plus près de leurs broches.
2. Mesurer la sortie du suiveur NE5532P, puis PA0/PB0/PW0 du MCP Gain pour
   plusieurs codes (par ex. 0, 32, 64, 128, 192, 255), à 10 kHz, d'abord
   sans charge puis avec l'étage suivant. Ne pas extrapoler un seul point.
3. Mesurer au multimètre DC et avec la fonction Mean de l'oscilloscope
   PA0/PB0/PW0 du MCP Offset pour des codes autour de 127/128, ainsi que
   pour des codes plus éloignés. Contrôler le bruit et la linéarité.
4. Rebrancher ensuite THS4032 et mesurer séparément U1 et le sommateur ;
   ne pas utiliser les amplitudes théoriques comme valeurs acquises.
   La stabilité HF sur breadboard reste problématique avec le THS4032.
5. Le LT1206 fonctionne à contre-réaction en courant ; choisir sa
   résistance de contre-réaction selon la fiche technique et vérifier
   SHDN, COMP, découplages, surcharge et échauffement. Ne pas court-circuiter
   directement OUT et entrée inverseuse pour le gain unitaire.
6. La source TRACO ±15 V du cahier des charges fournit 65 mA par branche.
   Une charge 50 ohms à 20 Vpp exigerait 200 mA crête : cette sortie
   doit donc être qualifiée séparément. Essais initiaux sur 1 MOhm.
7. Les états `NOMINAL_NON_CALIBRE` et `DDS_SEUL_COMMANDE` ne signifient
   ni une valeur VOUT certifiée ni une sortie physiquement déconnectée
   après OFF. Ne pas raccorder de charge sensible avant validation.

**À obtenir pour la calibration réelle** : tableaux code MCP Gain →
amplitude VOUT (au moins plusieurs codes et fréquences, séparément
pour sinus/triangle/carré), code MCP Offset → moyenne DC VOUT,
amplitude/offset après suiveurs, sommateur et LT1206, mesures sur charge
réelle. Ces données pourront alimenter le profil de calibration existant
plutôt que remplacer un facteur global approximatif.

Sources techniques :
- AD9833 : https://www.analog.com/media/en/technical-documentation/data-sheets/ad9833.pdf
- MCP45HVX1 : https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP45HVX1-Data-Sheet-DS20005304.pdf
- LT1206 : https://www.analog.com/media/en/technical-documentation/data-sheets/1206fb.pdf
