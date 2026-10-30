# Guide technique — Firmware de finalisation

Version de format stockage : GFN2/1. Version profil : CAL1/1. Les valeurs électriques
réelles restent à qualifier sur la carte. Voir [VALIDATION.md](VALIDATION.md) et
[le correctif DDS sans profil](CORRECTION_DDS_SANS_PROFIL.md) et
[le modèle nominal](MODELE_NOMINAL.md).

## 1. Architecture et contrats

`main.c` possède la boucle applicative : événements encodeur, USB, progression
EEPROM, menu puis une page OLED. Une seule tâche modifie le catalogue et commande
les sorties. L'ISR encodeur ne fait que pousser des états A/B dans une file.

| Module | Responsabilité |
|---|---|
| `signaux` | Dix presets, catalogue de 1 à 100 signaux, plages syntaxiques |
| `integration` | Sélection, dernière commande réussie, défauts, révisions, sauvegarde |
| `analogique` | Validation du profil et conversion en deux codes EPOT, sans accès bus |
| `stockage` / `eeprom` | Transaction à deux banques / transport I²C 24LC32A |
| `usb_protocol` / `usb_service` / `usb_transport` | Syntaxe pure / commandes et files / USB Serial-JTAG |
| `ad9833`, `epot`, `sorties_digitales` | Pilotes matériels réels |
| `entree`, `encodeur`, `oled`, `menu` | Décodage, acquisition, dessin et sélection locale en lecture seule |
| `integration_esp32` | Adaptation des pilotes ESP-IDF aux contrats testables |

États exposés : `NON_INITIALISE`, `ARRET_COMMANDE`, `PRET`, `ACTIF_COMMANDE`,
`DDS_SEUL_COMMANDE`, `NON_CALIBRE`, `DEFAUT`. Ce sont des **états de commande**,
pas des mesures. En interne, `SYS_ACTIF_COMMANDE` possède trois modes identifiables :
commande calibrée (`commande_courante=true`), commande EPOT nominale non
mesurée (`nominal_courant=true`, `commande_courante=false`) et DDS seul
(les deux indicateurs faux). `integration_nom_etat()` expose
`ACTIF_COMMANDE`, `NOMINAL_NON_CALIBRE` ou `DDS_SEUL_COMMANDE`.
La dernière configuration entièrement commandée reste disponible séparément du
catalogue ; une activation DDS seule ne remplace ni cette configuration ni ses codes.
Une mutation USB du preset actif est refusée si l'arrêt échoue.
L'interface locale ne possède plus de brouillon ni de fonction d'édition : aucun
événement encodeur ne modifie le catalogue ou le profil de calibration.
Les erreurs matérielles restent mémorisées jusqu'à un arrêt confirmé par le pilote.

Le menu démarre sur la liste des signaux. La rotation ne fait que parcourir le
catalogue. Un clic appelle `integration_choisir()` puis, seulement si la sélection
réussit, `integration_activer()` dans la même action. Avec un profil mesuré, la fiche
affiche fréquence, amplitude crête-à-crête et offset sur trois grandes lignes.
Sans profil mais avec deux EPOT disponibles, elle affiche `NOM` et les valeurs
**nominales non mesurées** précédées de `~`. Si les EPOT sont absents, elle
affiche `DDS SEUL`, la fréquence, `A NON REGLEE` et `O NON REGLE`.
Un clic revient à la liste sans arrêt ; une rotation revient au choix suivant/précédent.
L'appui long demande l'arrêt de toutes les sorties depuis chaque écran.
Aucun sous-menu d'activation ou de confirmation n'est nécessaire.

Les menus d'édition, duplication, suppression, mémoire/profil, réglage des sorties
numériques et pas de réglage sont supprimés. Les diagnostics et les commandes
techniques restent accessibles par USB. La syntaxe USB et la distinction entre
`$SELECT:N#` et `$ON#` sont conservées : le raccourci est propre au clic local.
Le défilement de la liste et son animation n'envoient aucune commande de sortie.

Avec un profil mesuré : valider profil et consigne → RESET DDS → forme/fréquence
→ codes EPOT d'après le profil → stabilisation → activation DDS. Sans profil,
si les EPOT sont présents : calcul de codes **nominaux** depuis les hypothèses
du montage (600 mVpp, gain x50, rails ±15 V), puis la même séquence avec gain
avant offset, sans délai de stabilisation caractérisé. Avec EPOT absents :
repli DDS seul, sans commander gain et offset. Les erreurs de plage du
profil approuvé ne basculent jamais en nominal. Les erreurs matérielles
sont conservées. L'estimation nominale ne garantit ni bande passante,
ni amplitude à la charge, ni offset mesuré. Voir [MODELE_NOMINAL.md](MODELE_NOMINAL.md).

Les attentes de bus sont bornées ; cela ne fait pas de la boucle un système temps
réel dur. Une activation peut prendre plusieurs transactions et jusqu'à 100 ms de
stabilisation. La file encodeur a 128 états, compte les pertes et se resynchronise
après débordement. Le rendu OLED transfère au plus une page modifiée par itération.

## 2. Brochage de référence conservé

Ces valeurs viennent du prototype logiciel P01, pas d'une vérification du PCB.
Le fichier de référence est `include/hardware_config.h`.

| Fonction | Configuration |
|---|---|
| I²C partagé | SDA GPIO22, SCL GPIO23, périphériques à 100 kHz |
| OLED SH1106 | 0x3C, adresse alternative 0x3D |
| EPOT gain / offset | 0x3F / 0x3E |
| EEPROM 24LC32A | 0x50, pas de sélection automatique d'une autre mémoire |
| Encodeur A / B / bouton | GPIO1 / GPIO2 / GPIO0 ; inversion de sens conservée |
| AD9833 SPI2 | MOSI GPIO18, SCLK GPIO19, FSYNC GPIO21, SPI mode 2, 1 MHz |
| AD9833 MCLK | Hypothèse conservée : 25 MHz, à vérifier sur le module |
| USB intégré | GPIO12/13 réservés au contrôleur USB, jamais réaffectés |
| Sorties numériques | `SYSTEM_DIGITAL_1_GPIO=-1`, `SYSTEM_DIGITAL_2_GPIO=-1` |

Vérifier alimentations, masse, résistances de tirage, adresse EEPROM et WP, ainsi
que WLAT/SHDN des MCP45HV51. Une relecture de registre EPOT ne mesure pas la tension
ni la résistance et ne prouve pas le bon état de ces broches de contrôle.
Le firmware n'est pas une protection contre les courts-circuits.

L'option numérique disponible est **deux PWM indépendants 3,3 V**, par LEDC avec
un timer par canal, horloge XTAL, de 10 Hz à 1 MHz, rapport 0…1000 pour mille.
0 % et 100 % sont des niveaux fixes. Les plages de fréquence et résolution sont
couplées ; les valeurs d'état sont les consignes, pas des mesures. Une erreur de
fréquence calculée supérieure à 1 % est refusée. Le rôle des sorties dans le produit
n'étant pas défini, ce mode reste une proposition configurable. N'affecter les GPIO
qu'après revue du schéma ; les GPIO 16/17 employés dans les tests **ne sont pas un
choix de brochage du produit**. Les broches déjà utilisées, USB, flash et straps sont
rejetées par le pilote. Aucun PWM n'est activé au démarrage.

## 3. Persistance

L'EEPROM est divisée en deux banques de 2048 octets. Une banque maximale contient
32 octets d'en-tête, 100 × 13 octets de signaux et 512 octets de profil, soit
**1844 octets**. Le reste est réservé. Les nombres multi-octets sont sérialisés en
little-endian ; l'adresse mémoire du transport I²C est envoyée MSB d'abord.

| Décalage dans la banque | Champ |
|---:|---|
| 0…3 | Signature `GFN2` |
| 4…5 | Version 1 |
| 6…7 | Longueur de charge utile |
| 8…11 | Génération 32 bits |
| 12…13 | Nombre de signaux |
| 14…15 | Taille de profil : 512 |
| 16…19 | Réservé |
| 20…23 | CRC32 de la charge utile |
| 24…27 | CRC32 des octets 0…23 |
| 28…30 | Réservé |
| 31 | Marqueur final 0xA5 |
| 32… | Signaux : forme u8, amplitude i32, offset i32, fréquence u32 ; puis profil |

Le CRC utilise le polynôme réfléchi 0xEDB88320 et les inversions initiale/finale.
L'ancienne banque valide reste intacte pendant l'écriture de la nouvelle. Invalider
et relire le marqueur cible ; écrire le corps par pages de 32 octets ; écrire l'en-tête
sans marqueur ; tout relire ; écrire le marqueur final ; vérifier l'en-tête.
Après chaque écriture, interroger l'acquittement jusqu'à réussite ou délai de 50 ms.
Les tests simulent des coupures à chaque octet, pas tous les modes physiques de
corruption possibles d'une EEPROM réelle.

Au boot, choisir la génération valide la plus récente, avec comparaison modulo 2^32.
Deux banques effacées à 0xFF déclenchent une initialisation unique depuis les presets.
Une corruption/version inconnue sans banque valide recharge seulement les presets en
RAM et signale le motif : aucune réécriture automatique au démarrage. Une modification
ultérieure ou `SAVE` constitue une demande explicite d'écriture. Une banque valide
ancienne peut être utilisée avec une alerte sur l'autre banque.

Sauvegarde par instantané : révision RAM et révision sauvegardée sont distinctes.
Une modification USB pendant la sauvegarde n'est pas effacée du suivi lorsque l'ancien
instantané termine. La navigation, la sélection et l'état ON ne sont pas persistés ;
après redémarrage, aucune sélection ni activation n'est restaurée.

## 4. Protocole USB

ASCII, sans espaces, délimiteurs `$` et `#`, moins de 96 octets par requête.
CR/LF sont permis **entre** les trames. Un nouveau `$` resynchronise la réception.
Une trame incomplète est abandonnée après 1 seconde d'inactivité. Le catalogue
accepte les valeurs syntaxiquement valides même hors profil ; seule l'application
complète avec profil vérifie les possibilités analogiques. Sans profil, amplitude
et offset sont stockés mais ne sont pas appliqués aux EPOT.

La configuration d'un signal se fait exclusivement par USB, avec la trame du cahier :
`$N:TYPE:AMPLITUDE:OFFSET:FREQUENCE#`. `TYPE` vaut `SIN`, `TRI` ou `CAR` ; les unités
sont respectivement mV crête-à-crête, mV et Hz. Les deux lectures du cahier sont
`$N?#` et `$?#`. Les commandes supplémentaires du tableau sont des extensions
conservées pour la maintenance, le diagnostic et le contrôle depuis le PC.

| Requête | Réponse / effet |
|---|---|
| `$3:SIN:2000:+3000:1000#` | Modifier N existant, ou ajouter exactement à nombre+1 ; `OK:RAM:revision` |
| `$3?#` | `$3:SIN:2000:+3000:1000#` |
| `$?#` | Toutes les vraies trames, dans l'ordre, sans marqueur de fin ajouté |
| `$STATE?#` | État détaillé ci-dessous, distingue DDS seul et commande complète |
| `$SELECT:3#` | Sélection en RAM, sans activation |
| `$ON#` | Avec profil : commande complète ; sans profil : forme/fréquence DDS seules |
| `$OFF#` | Arrêt DDS et des sorties numériques configurées |
| `$SAVE#` | Demander une sauvegarde ; ne pas confondre demande et réussite |
| `$LOAD#` | Arrêter si nécessaire puis remplacer la RAM par l'EEPROM valide |
| `$FACTORY#` | Restaurer les dix presets en conservant le profil ; sauvegarde demandée |
| `$DUP:3#` / `$DELETE:3#` | Dupliquer / supprimer ; garder au moins un signal |
| `$DIG:1:1000:500#` / `$DIG:1:OFF#` | PWM optionnel / arrêt du canal |
| `$DIG?#` | Deux états de canal |
| `$CAL:BEGIN#` | Démarrer un import temporaire, capturer la révision |
| `$CAL:DATA:0:64_CHIFFRES_HEX#` | 32 octets ; offsets 0,32,…,480 |
| `$CAL:COMMIT#` | Valider les 16 blocs, le CRC et le profil ; modifier atomiquement en RAM |
| `$CAL:ABORT#` / `$CAL:CLEAR#` | Abandonner l'import / effacer le profil actif après arrêt |
| `$CAL?#` | 16 trames DATA puis `$CAL:END#` ; profil absent représenté par 512 zéros |

`64_CHIFFRES_HEX` est un emplacement explicatif, pas un texte à transmettre.
L'import expire après 30 s ; il est refusé si la révision du catalogue a changé.
Il n'existe aucun protocole d'authentification : le port USB est une interface de
commande locale de confiance, y compris les commandes d'effacement.

Acquittement générique : `$OK:ACTION:REVISION#`. Erreur : `$ERR:CODE#`.
Codes : `ARGUMENT`, `INDEX`, `CAPACITE`, `ABSENT`, `IO`, `TIMEOUT`, `CRC`,
`VERSION`, `VIDE`, `NON_CALIBRE`, `PLAGE`, `CONFLIT`, `OCCUPE`, `VERIFICATION`,
`SELECTION`, `NON_SUPPORTE`. Les acquittements et extensions sont des choix du
firmware, pas des trames imposées par le cahier.

```text
$STATE:ETAT:SELECTION:NOMBRE:REV_RAM:REV_SAUVEE:DIRTY:SAVING:ERR_SAVE:ERR_DERNIERE:ERR_ARRET#
$DIG:CANAL:DISPONIBLE:CONNU:ACTIF:FREQ_DEMANDEE:RAPPORT_DEMANDE:ERREUR#
```

Sélection vaut 0 sans sélection, sinon 1…100. Les indicateurs valent 0 ou 1.
La sauvegarde est confirmée seulement si `DIRTY=0`, `SAVING=0` et
`REV_SAUVEE=REV_RAM` pour la révision demandée. Le client PC effectue ce contrôle.
Un `ON_COMMANDE` confirme la séquence de pilotes, jamais une mesure VOUT.
Cet acquittement est conservé dans les deux modes pour compatibilité ; consulter
`$STATE?#` pour distinguer `DDS_SEUL_COMMANDE` et `ACTIF_COMMANDE`.

Le tampon TX applicatif est borné à 8192 octets. Les écritures partielles sont
reprises. Après 2 s sans progression d'une réponse en attente, abandonner la file
et réinitialiser le transport ; aucune commande ne réactive la sortie pour autant.
Le client doit travailler en requête/réponse, lire les réponses et réinterroger
l'état après reconnexion. La console ESP-IDF est désactivée sur le canal protocole ;
le client se resynchronise sur `$` face à d'éventuels messages ROM de démarrage.

## 5. Calibration réelle

Le moteur est un **modèle provisoire à tables monotones**, non une identification
vérifiée du circuit. Pour chaque forme, les tables donnent l'amplitude et le niveau
moyen issus du gain, puis le décalage ajouté par l'offset. Ce modèle suppose que
l'offset est additif et suffisamment indépendant du gain et de la fréquence dans
la plage déclarée. Si les mesures contredisent ces hypothèses, adapter le modèle,
les tests et la version du format avant de l'approuver.

Le moteur refuse l'extrapolation, les points non monotones, les consignes hors
fréquence qualifiée, l'excursion impossible et une erreur de quantification au-delà
du profil. La vérification d'excursion porte sur la consigne et sur l'estimation
quantifiée. Cela ne constitue pas une garantie métrologique ni une compensation de
bande passante ; les tolérances électriques doivent inclure les incertitudes réelles.

Préparer un JSON avec ces champs, issus de mesures :

| Champ | Contenu |
|---|---|
| `approved` | `true` seulement après revue des mesures et transitions |
| `board` | Identifiant ASCII sans espaces, 1…23 caractères |
| `load_ohm` | Charge de référence positive |
| `output_min_mv`, `output_max_mv` | Excursion admissible mesurée, dans les rails ±15000 mV |
| `settle_ms` | Délai validé de 0…100 ms |
| `gain_before_offset` | Ordre validé de mise à jour des EPOT |
| `forms` | Objet contenant les formes réellement qualifiées parmi SIN/TRI/CAR |
| Par forme : `freq_min_hz`, `freq_max_hz` | Plage vérifiée ; ne pas inscrire 12 MHz sans preuve |
| `amplitude_error_mvpp`, `offset_error_mv` | Erreurs maximales de quantification acceptées |
| `gain` | 2…8 lignes `[code, amplitude_mvpp, zero_mv]` |
| `offset` | 2…8 lignes `[code, delta_mv]` |

Les codes sont strictement croissants (0…255) ; les amplitudes/deltas sont strictement
monotones, croissants ou décroissants. `zero_mv` est le niveau moyen de référence
avant l'offset ajouté par la seconde table, pas une constante supposée nulle.
Les exemples synthétiques dans les tests servent uniquement à tester les algorithmes.
Ne jamais les importer sur une carte comme s'il s'agissait d'une calibration réelle.

```sh
python tools/generateur.py --port COM7 cal-import profil_mesure.json
python tools/generateur.py --port COM7 cal-export profil_archive.bin
python tools/generateur.py --port COM7 select 1
python tools/generateur.py --port COM7 on
python tools/generateur.py --port COM7 off
```

L'import CLI lit un JSON ; l'export est une archive binaire CAL1, pas un JSON.
Conserver le JSON source, les mesures et le binaire avec l'identifiant de carte.
Le client refuse d'écraser un fichier d'export existant.

CAL1 occupe 512 octets : signature 0…3, version u16 à 4, approbation à 6, ordre à 7,
identifiant sur 24 octets à 8, charge u32 à 32, excursions i32 à 36/40, délai u16 à 44.
Trois blocs de 144 octets commencent à 48. Dans chaque bloc : fréquences u32 à 0/4,
erreurs u32 à 8/12, nombres de points u8 à 16/17 ; gain à 24 (8 × 9 octets), offset
à 96 (8 × 5 octets). CRC32 des octets 0…507 à 508. Octets réservés à zéro.

## 6. Dépannage et limites pratiques

`NON_CALIBRE` à l'arrêt signifie qu'aucun profil approuvé n'est chargé. Un clic ou
`$ON#` permet maintenant la génération DDS seule. Pour commander aussi l'amplitude
et l'offset, importer le profil mesuré. `DDS_SEUL_COMMANDE` ne prouve pas qu'un signal
correct est présent à VOUT : vérifier d'abord le module DDS puis l'étage analogique.
`PLAGE` : revoir la consigne ou la capacité documentée, pas le seuil pour faire
disparaître l'erreur. `VERIFICATION` : contrôler WP/EPOT et câblage.
`DEFAUT` : consulter l'étape et les deux erreurs ; résoudre le défaut puis commander
OFF avant ON. L'appui long demande OFF depuis tous les écrans du menu local.
Les DDS/EPOT/EEPROM absents au boot nécessitent actuellement un redémarrage pour
une nouvelle initialisation. L'OLED et le transport USB ont une reprise périodique.

Sans EEPROM, la RAM reste utilisable et signalée non sauvegardée par l'état USB.
Sans OLED, l'USB reste disponible. Sans client USB, l'application ne doit pas attendre
indéfiniment une lecture. L'arrêt logiciel n'est pas un dispositif de sécurité :
le comportement avant le démarrage du processeur dépend du circuit.

## 7. Références

Le cahier fourni, pages 2–3, fixe les composants, les dix presets et les trois trames.
Les choix d'architecture, sauvegarde et extensions USB ci-dessus sont des décisions
logicielles. Références constructeur consultées lors de l'implémentation :

- Analog Devices, AN-1070 : https://www.analog.com/en/resources/app-notes/an-1070.html
- Microchip, MCP45HVX1 DS20005304B : https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP45HVX1-Data-Sheet-DS20005304.pdf
- Microchip, 24LC32A DS20001713 : https://ww1.microchip.com/downloads/aemDocuments/documents/MPD/ProductDocuments/DataSheets/24AA32A-24LC32A-32-Kbit-I2C-Serial-EEPROM-DS20001713.pdf
- Espressif, USB Serial/JTAG ESP32-C6 : https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/api-guides/usb-serial-jtag-console.html
- Espressif, LEDC ESP32-C6 : https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32c6/api-reference/peripherals/ledc.html
