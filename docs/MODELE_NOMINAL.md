# Modèle nominal de commande gain / offset (30 octobre 2026)

## Hypothèses fournies

- Sortie AD9833 : 600 mVpp, offset +320 mV (hypothèse provisoire pour SIN/TRI/CAR).
- C1 = 10 µF en série et R11 = 220 kΩ vers la masse : couplage AC ;
  après établissement (τ ≈ 2,2 s), les 320 mV continus sont bloqués.
- U4 et U3 : NE5532P montés en suiveurs.
- EPOT gain MCP45HV51-103E/ST : A = sortie U4, B = GND, W = entrée + de U1.
- U1 THS4032 : non-inverseur, R2 = 490 kΩ, R1 = 10 kΩ :
  gain = 1 + R2/R1 = 50.
- EPOT offset MCP45HV51-103E/ST : A = +15 V, B = -15 V, W = entrée + de U3.
- U2 THS4032 : sommateur non-inverseur R3 = R5 = R4 = R7 = 10 kΩ ;
  sortie U2 = sortie U1 + sortie U3.
- LT1206 placé ensuite ; gain unitaire nominal supposé et éventuelles
  protections/atténuations de sortie **non spécifiées**.

## Conversion de la consigne en codes 8 bits

```text
gain_code   = arrondi(amplitude_mvpp * 255 / 30000)
offset_code = arrondi((offset_mv + 15000) * 255 / 30000)
A_estimee   = arrondi(gain_code * 30000 / 255) mVpp
O_estime    = -15000 + arrondi(offset_code * 30000 / 255) mV
```

L'absence de code 127,5 pour le zéro idéal produit, avec l'arrondi choisi,
un offset théorique de +59 mV pour une consigne de 0 V. Un pas représente
environ 117,65 mVpp ou 117,65 mV d'offset dans ces hypothèses.

| Signal | Gain (code) | Offset (code) | Amplitude nominale (mVpp) | Offset nominal (mV) |
|---|---:|---:|---:|---:|
| 1 | 9 | 128 | 1059 | +59 |
| 2 | 43 | 128 | 5059 | +59 |
| 3 | 17 | 153 | 2000 | +3000 |
| 4 | 2 | 213 | 235 | +10059 |
| 5 | 170 | 128 | 20000 | +59 |
| 6 | 34 | 128 | 4000 | +59 |
| 7 | 43 | 85 | 5059 | -5000 |
| 8 | 85 | 128 | 10000 | +59 |
| 9 | 9 | 128 | 1059 | +59 |
| 10 | 34 | 179 | 4000 | +6059 |

Le firmware refuse une consigne si les pics **demandés ou calculés**
dépassent mathématiquement ±15 V. Cette garde ne prouve pas que les
amplificateurs puissent atteindre les rails, ni que la source d'alimentation
puisse fournir le courant exigé par la charge.

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

1. L'entrée DC +320 mV n'est éliminée par C1/R11 qu'en régime établi :
   τ ≈ 2,2 s et stabilisation de plusieurs constantes de temps. Des
   transitoires à la mise sous tension peuvent être amplifiés et
   saturer les étages ; aucune mise en sourdine LT1206 n'est câblée
   ou garantie par cette version.
2. Le courant de polarisation du NE5532P dans R11=220 kΩ peut créer
   une tension continue parasite notable, amplifiée ensuite par U1.
3. La bande passante des EPOT 10 kΩ et la stabilité de U1 avec une
   contre-réaction de 490 kΩ doivent être mesurées, surtout aux
   fréquences de 200 kHz et 1 MHz. Le modèle 600 mVpp commun
   aux trois formes n'est pas vérifié.
4. LT1206 : il s'agit d'un amplificateur à **contre-réaction en courant**.
   Le montage buffer AV=1 constructeur exige une résistance RF choisie
   selon la charge et l'alimentation ; **ne pas relier directement OUT
   à l'entrée inverseuse**. Le datasheet illustre RF ≈ 825 Ω dans un
   essai à ±15 V et charge 50 Ω, valeur à qualifier dans le montage.
   Contrôler SHDN, COMP, découplages et protection de la sortie.
5. La source TRACO ±15 V de 65 mA par branche mentionnée dans le cahier
   des charges ne peut pas fournir sans vérification 20 Vpp sous 50 Ω :
   ce cas exige 200 mA crête sur la charge, hors consommations internes.
   Tester d'abord avec l'entrée oscilloscope 1 MΩ et sans charge sensible.
6. Le MCP45HV51 doit disposer de ses alimentations analogiques ±15 V
   et logique appropriées, des bonnes connexions A/B/W, WLAT et SHDN.
   Une réponse I2C OK ne prouve pas les tensions effectivement obtenues.

Ces résultats ne sont pas une calibration métrologique. Les coefficients
du modèle doivent être remplacés ou complétés par des mesures sur la carte.

Sources techniques :
- AD9833 : https://www.analog.com/media/en/technical-documentation/data-sheets/ad9833.pdf
- MCP45HVX1 : https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP45HVX1-Data-Sheet-DS20005304.pdf
- LT1206 : https://www.analog.com/media/en/technical-documentation/data-sheets/1206fb.pdf
