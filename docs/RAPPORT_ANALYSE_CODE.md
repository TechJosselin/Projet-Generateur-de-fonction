# Rapport d'analyse du code — Générateur de fonctions

> **Périmètre :** dépôt `TechJosselin/Projet-Generateur-de-fonction`, branche `finalisation/p02-p11`, analysée le **3 novembre 2026**.  
> **Nature :** revue statique du code C/ESP-IDF, de la configuration PlatformIO, des outils PC et des documents présents. **Aucun flash, test instrumental ou compilation nouvelle n'a été exécuté pendant cette revue.**  
> **Référence produit :** cahier des charges CFPT / ES électronique, générateur de signaux pour exercices d'oscilloscope.

## Sommaire

1. [Objectif et état du projet](#1-objectif-et-état-du-projet)
2. [Arborescence et responsabilité des fichiers](#2-arborescence-et-responsabilité-des-fichiers)
3. [Architecture logicielle et flux d'exécution](#3-architecture-logicielle-et-flux-dexécution)
4. [Détail de chaque module](#4-détail-de-chaque-module)
5. [Scénarios de fonctionnement](#5-scénarios-de-fonctionnement)
6. [Échanges USB et formats de données](#6-échanges-usb-et-formats-de-données)
7. [Brochage, périphériques et limites électriques](#7-brochage-périphériques-et-limites-électriques)
8. [Conformité au cahier des charges](#8-conformité-au-cahier-des-charges)
9. [Analyse des risques et améliorations](#9-analyse-des-risques-et-améliorations)
10. [Tests et mise en route](#10-tests-et-mise-en-route)
11. [Glossaire et conclusion](#11-glossaire-et-conclusion)

## 1. Objectif et état du projet

Le système pilote un **AD9833** pour créer une forme d'onde sinus, triangle ou carré. L'utilisateur choisit un preset à l'aide d'un encodeur et d'un écran **OLED SH1106 128×64** ; le firmware pilote également deux potentiomètres numériques **MCP45HV51** destinés au gain et à l'offset, sauvegarde des configurations sur **EEPROM 24LC32A** et expose un protocole sur l'USB natif de l'**ESP32-C6**. Deux sorties numériques PWM figurent comme possibilité logicielle, non câblée par défaut.

La différence essentielle entre **demander une consigne** et **garantir un signal à la borne VOUT** doit être conservée. Le firmware commande le DDS et, selon les composants disponibles et la calibration, les EPOT ; il ne mesure pas VOUT. L'étage analogique, le LT1206, les rails d'alimentation, la charge et les tolérances ne sont pas asservis par cette version du programme.

Le projet dispose de dix signaux initiaux et d'une capacité de cent. Les principales fonctionnalités logicielles sont développées ; `docs/VALIDATION.md` décrit des tests natifs et une compilation ESP32-C6 réussis à la date du **29 octobre 2026**, tout en indiquant que la validation électrique réelle reste à faire. Ce rapport ne réinterprète pas ces éléments comme des essais matériels.

## 2. Arborescence et responsabilité des fichiers

Le firmware est principalement dans `EQ1_Projet_Generateur_Fonction/` :

```text
Projet-Generateur-de-fonction/
├── README.md                       Présentation et instructions générales
├── EQ1_Projet_Generateur_Fonction/
│   ├── platformio.ini              Cible, framework et versions des outils
│   ├── sdkconfig.defaults          Options ESP-IDF, console et pile
│   ├── CMakeLists.txt              Projet ESP-IDF
│   ├── include/                    Contrats et définitions des modules
│   ├── src/                        20 fichiers C + CMakeLists/idf_component
│   └── test/native/                Tests natifs et faux périphériques
├── tools/
│   ├── generateur.py               Client PC et fonctions de calibration
│   ├── test_generateur.py          Tests de l'outil PC
│   └── run_native_tests.py         Orchestrateur de tests
└── docs/                            Documents de suivi, de validation et de conception
```

| Source C | Responsabilité principale |
|---|---|
| `main.c` | Entrée `app_main`, boucle coopérative, ordre des services |
| `integration.c` | Coordinateur central, états, sélection, activation, arrêts, sauvegarde |
| `integration_esp32.c` | Raccordement du coordinateur aux pilotes ESP32 |
| `gf.c` | Codes de résultat et CRC32 partagé |
| `signaux.c` | Catalogue, presets usine, règles syntaxiques et affichage des unités |
| `analogique.c` | Profil de calibration, interpolation et modèle nominal |
| `ad9833.c` | Driver SPI du DDS |
| `epot.c` | Driver I²C des deux potentiomètres numériques |
| `eeprom.c` | Driver physique 24LC32A |
| `stockage.c` | Format persistant, deux banques, transaction d'écriture |
| `i2c_commun.c` | Création et partage du bus I²C maître |
| `initialisation.c` | Ancienne API de diagnostic et scan I²C |
| `entree.c` | Décodage pur quadrature et anti-rebond bouton |
| `encodeur.c` | GPIO, interruptions, file FreeRTOS, événements utilisateur |
| `oled.c` | Initialisation SH1106, police, framebuffer, transferts |
| `menu.c` | Affichage à trois lignes, navigation, clic pour activer, appui long pour arrêter |
| `sorties_digitales.c` | Pilotage optionnel de deux PWM par LEDC |
| `usb_protocol.c` | Analyse syntaxique et formatage des trames |
| `usb_service.c` | Exécution des commandes, réception et émission tamponnées |
| `usb_transport.c` | Interface physique USB Serial/JTAG de l'ESP32-C6 |

Les fichiers `.h` définissent types, constantes et prototypes : ils constituent le contrat entre les composants. `hardware_config.h` regroupe les pins et adresses, `board_options.h` laisse les PWM non affectés. `src/CMakeLists.txt` référence explicitement les vingt sources C ; il faut l'actualiser à chaque ajout de module.

## 3. Architecture logicielle et flux d'exécution

### 3.1 Vue en couches

```text
                     UTILISATEUR / PC
        Encodeur + bouton          USB Serial/JTAG
               │                         │
         encodeur / entree       usb_transport
               │                         │
             menu                 usb_service
               │                  usb_protocol
               └───────────┬─────────────┘
                           ▼
                    integration.c
               ┌───────────┼────────────┐
               ▼           ▼            ▼
           signaux     analogique      stockage
          (catalogue)  (conversion)   (EEPROM)
               │           │            │
               └───────────┼────────────┘
                           ▼
                   integration_esp32
               ┌───────────┼─────────────┐
               ▼           ▼             ▼
             AD9833       EPOT       sorties_digitales
              SPI         I²C             LEDC
                           │
                     i2c_commun
                         ├── EEPROM
                         └── OLED
```

Le trait architectural le plus important est **l'unicité du propriétaire des données et de la commande matérielle**. Les appels de menu et USB s'exécutent dans la même boucle. L'interruption de l'encodeur empile uniquement des états logiques, sans modifier le catalogue. Ce modèle évite les écritures simultanées sur les presets ou l'EEPROM ; il ne supprime pas le besoin de tester les erreurs de chaque opération I/O.

### 3.2 Démarrage et boucle principale — `main.c`

`app_main()` appelle, dans l'ordre :

1. `integration_esp32_initialiser()` : instancie DDS, I²C, EPOT, EEPROM et sorties numériques, charge le modèle et demande les sorties arrêtées ;
2. `oled_initialiser()` et `encodeur_initialiser()` ;
3. `menu_initialiser()` ;
4. `usb_service_initialiser()` et `usb_transport_initialiser()`.

Ensuite, chaque passage prend un horodatage en millisecondes avec `esp_timer_get_time()/1000` et exécute :

```c
menu_traiter(encodeur_lire_evenement());
usb_transport_periodique(now);
integration_periodique(now);
menu_periodique(now);
oled_periodique(now);
vTaskDelay(1);
```

La rotation peut donc être traitée avant une trame USB arrivée dans la même itération. Le stockage avance par petites étapes et l'OLED transmet au plus une page par itération. `vTaskDelay(1)` cède le processeur, mais ne garantit pas une période d'exécution fixe : la latence dépend du tick, du bus et des pilotes.

### 3.3 Gestion des résultats — `gf.c` / `gf.h`

`gf_result_t` harmonise les retours (`GF_OK`, `GF_ARGUMENT`, `GF_INDEX`, `GF_CAPACITE`, `GF_ABSENT`, `GF_IO`, `GF_TIMEOUT`, `GF_CRC`, `GF_VERSION`, `GF_VIDE`, `GF_NON_CALIBRE`, `GF_PLAGE`, `GF_CONFLIT`, `GF_OCCUPE`, `GF_VERIFICATION`, `GF_SELECTION`, `GF_NON_SUPPORTE`). `gf_result_nom()` fournit leurs noms pour l'USB et l'écran. `gf_crc32()` calcule le CRC32 avec le polynôme réfléchi `0xEDB88320`, utilisé dans les formats EEPROM et calibration.

## 4. Détail de chaque module

### 4.1 Catalogue — `signaux.c` / `signaux.h`

Le type `signal_config_t` comporte `forme`, `amplitude_mvpp`, `offset_mv` et `frequence_hz`. Le tableau statique `catalogue[100]` évite l'allocation dynamique. `total` compte les entrées et `changes[]` marque les modifications.

Les dix valeurs initiales sont :

| N° | Forme | Amplitude Vpp | Offset V | Fréquence |
|---:|---|---:|---:|---:|
| 1 | Sinus | 1 | 0 | 10 kHz |
| 2 | Sinus | 5 | 0 | 1 MHz |
| 3 | Sinus | 2 | +3 | 1 kHz |
| 4 | Sinus | 0,2 | +10 | 500 Hz |
| 5 | Triangle | 20 | 0 | 5 kHz |
| 6 | Triangle | 4 | 0 | 100 Hz |
| 7 | Triangle | 5 | −5 | 200 kHz |
| 8 | Carré | 10 | 0 | 50 Hz |
| 9 | Carré | 1 | 0 | 300 Hz |
| 10 | Carré | 4 | +6 | 1 kHz |

`signaux_valide()` impose une forme reconnue, **100 à 20 000 mVpp**, **−10 000 à +10 000 mV** et **1 à 12 000 000 Hz**. Ce sont des limites **logicielles de consigne**, non une preuve de faisabilité analogique.

`signaux_lire()` retourne un pointeur constant ou `NULL`. `signaux_modifier()`, `signaux_ajouter()`, `signaux_dupliquer()`, `signaux_supprimer()`, `signaux_remplacer_ensemble()` et `signaux_retablir_usine()` gèrent les changements. `signaux_marquer_sauves()` remet les indicateurs à zéro après commit EEPROM. `signaux_variation()` permet des incréments bornés dans le modèle, mais **le menu actuel ne l'appelle pas pour éditer les presets**. Les fonctions de formatage servent à l'affichage humain.

### 4.2 Coordinateur — `integration.c` / `integration.h`

Ce module est le **cœur fonctionnel**. Il stocke les structures statiques `hw` (backend matériel injecté), `memoire`, `profil`, `etat`, `digitales[2]` et `instantane`. L'injection de fonctions matérielles permet les tests hôtes sans ESP32.

Les états internes sont `SYS_NON_INITIALISE`, `SYS_ARRET_COMMANDE`, `SYS_PRET`, `SYS_ACTIF_COMMANDE`, `SYS_NON_CALIBRE`, `SYS_DEFAUT`. Le nom affiché distingue en outre `NOMINAL_NON_CALIBRE` et `DDS_SEUL_COMMANDE` à partir des indicateurs.

| Fonction | Rôle et précautions |
|---|---|
| `integration_initialiser_avec` | Remet les données à zéro, installe les dix presets, charge EEPROM si possible, puis commande l'arrêt des sorties |
| `integration_choisir` | Vérifie l'index, arrête l'ancienne sortie si active, mémorise le nouvel index |
| `integration_activer` | Valide la sélection, convertit la consigne, coupe DDS, configure forme/fréquence puis EPOT, attend si nécessaire et active DDS |
| `integration_arreter` | Tente arrêt DDS et arrêt des deux voies numériques, remonte un défaut si échec |
| `integration_modifier` | Vérifie l'index, la consigne et, si fournie, la révision attendue ; arrête avant de changer le preset sélectionné |
| `integration_dupliquer`, `integration_supprimer`, `integration_usine` | Changent le catalogue RAM, incrémentent la révision et programment la persistance |
| `integration_sauver`, `integration_charger` | Demandent un enregistrement ou rechargent une image valide |
| `integration_importer_calibration`, `integration_effacer_calibration` | Modifient le profil après arrêt |
| `integration_commander_digital` | Vérifie canal, fréquence et rapport cyclique, appelle le backend et conserve un état de commande |
| `integration_periodique` | Avance la machine de sauvegarde sans tout bloquer dans un seul appel |

**Activation :** trois chemins se distinguent. (1) Profil approuvé et EPOT présents : conversion par points mesurés et séquence complète. (2) Pas de profil, EPOT disponibles : calcul **nominal** et indication de valeurs estimées. (3) Pas de profil, EPOT indisponibles : programmation du **DDS seul**, amplitude et offset non réglés. Un profil présent mais incapable de représenter la consigne retourne `GF_PLAGE` ; il n'est pas remplacé silencieusement par le nominal.

**Défauts :** une erreur lors de la configuration de DDS/EPOT/activation déclenche une tentative d'arrêt et place le coordinateur en `SYS_DEFAUT`. Un nouvel `ON` est refusé jusqu'à une demande d'arrêt. Un arrêt commandé avec succès ne prouve toutefois pas physiquement que VOUT vaut zéro.

**Sauvegarde :** chaque modification incrémente `revision` et déclenche une écriture différée de **300 ms**. `revision_en_sauvegarde` mémorise le contenu exact parti en EEPROM ; une mutation survenue pendant cette écriture laisse les nouvelles données marquées modifiées. Une erreur de sauvegarde exige un nouveau changement ou `SAVE` pour retenter. Sur stockage corrompu/inconnu, le boot évite d'écraser automatiquement les données.

### 4.3 Adaptateur matériel — `integration_esp32.c`

Ce fichier transforme les `esp_err_t` des drivers en `gf_result_t`. Les fonctions statiques `arret`, `signal`, `gain`, `offset`, `activer`, `attendre` et `digital` remplissent la structure de callbacks de `integration.c`. `integration_esp32_initialiser()` lance le DDS, le bus partagé, les deux EPOT, l'EEPROM et le PWM. Chaque périphérique dispose d'un indicateur de disponibilité, de sorte qu'un composant absent ne doit pas être interprété comme disponible.

### 4.4 DDS — `ad9833.c` / `ad9833.h`

Le driver configure un bus **SPI2**, MOSI/SCLK/FSYNC, en **mode SPI 2**, puis utilise des mots de commande de 16 bits. Le DDS est maintenu en **RESET** pendant les changements de paramètres. `Ad9833_SetSignal()` :

1. vérifie l'initialisation, le type et la fréquence finie comprise entre 0 et `MCLK/2` ;
2. détermine les bits de forme (sinus, triangle, carré) ;
3. calcule le mot 28 bits par arrondi de `f × 2^28 / MCLK` ;
4. envoie RESET, les deux demi-mots de fréquence, le registre PHASE0 puis le contrôle de forme ;
5. conserve la fréquence, la forme et la validité du dernier réglage.

`Ad9833_EnableOutput(true/false)` manipule le bit RESET ; `Ad9833_Deinit()` exige également une mise au repos. Les transactions SPI ont un timeout borné. Le stockage statique de la transaction protège sa durée de vie même lorsqu'une opération demeure en attente. Un échec invalide l'état connu et suscite une tentative de RESET. **RESET désactive la production DDS commandée, mais ne remplace pas un interrupteur analogique de coupure VOUT.**

### 4.5 Potentiomètres numériques — `epot.c` / `epot.h`

Deux instances `devices[2]` représentent gain et offset aux adresses **0x3F** et **0x3E**. `Epot_Init()` sonde les deux composants, les déclare sur le bus puis écrit le registre 4 à `0xFF` avec contrôle de relecture. `Epot_SetRaw()` écrit le registre de curseur puis le relit : la réussite signifie que **le registre I²C** correspond au code demandé, pas que le gain ou l'offset analogique est exact. `Epot_ReadRaw()` permet une lecture directe ; `Epot_GetLastRaw()` expose un dernier code seulement s'il est valide. Les broches **WLAT**, **SHDN**, les niveaux de tension et le câblage doivent être vérifiés sur le circuit.

### 4.6 Conversion analogique — `analogique.c` / `analogique.h`

Un profil `calibration_t` représente une carte, une charge, des limites de sortie, un ordre gain/offset, une temporisation et des tables distinctes pour les trois formes d'onde. `calibration_valider()` contrôle la structure, le nombre de points, les codes croissants, les valeurs monotones et les domaines fréquentiels.

`analogique_convertir()` valide la consigne, sélectionne la table de la forme, vérifie la fréquence et l'excursion `offset ± amplitude/2`, puis recherche un code de gain minimisant l'erreur après interpolation linéaire entre points voisins. Le décalage de zéro associé au gain est inclus dans le calcul du code d'offset. La commande est rejetée si les seuils d'erreur du profil sont dépassés. Le résultat `analogique_commande_t` contient les deux codes 8 bits et **des estimations**, non des lectures d'instrument.

`calibration_encoder()` / `calibration_decoder()` convertissent le profil en une image **CAL1 de 512 octets**, avec version et CRC32. Le profil vide possède une représentation nulle.

`analogique_convertir_nominal()` est explicitement une **approximation**, fondée sur **600 mVpp** après C1/R11, un gain maximum théorique de **50**, et des rails **±15 V**. Il répartit les codes de gain entre 0 et 255 sur une plage de 0 à 30 Vpp, et les codes d'offset sur −15 à +15 V. Les effets de charge, bande passante, topologie réelle, tolérances, alimentations et LT1206 ne sont pas mesurés. En particulier, une limite « ±15 V » dans un calcul n'implique jamais un swing réel de ±15 V en sortie.

### 4.7 Bus I²C — `i2c_commun.c` et `initialisation.c`

`i2c_commun_initialiser()` crée une seule fois le bus maître avec les broches définies dans `hardware_config.h`; `i2c_commun_bus()` le donne aux périphériques. La désinitialisation le supprime. Les fonctions de probe/recherche d'adresse facilitent le diagnostic. `initialisation.c` conserve les anciennes fonctions `System_Init`, `System_Deinit`, `System_GetI2cBus`, `System_ScanI2cBus` et `System_IsInitialized` pour compatibilité. Son scan parcourt 0x08 à 0x77 et décrit les adresses attendues. Ce n'est pas une deuxième pile I²C indépendante.

### 4.8 EEPROM physique — `eeprom.c`

Le pilote cible une **24LC32A de 4096 octets**, adresse sur **16 bits**, pages d'écriture de **32 octets**. `eeprom_initialiser()` sonde l'adresse `0x50` à `0x57` et enregistre le périphérique I²C à 100 kHz. `lecture()` effectue des lectures par groupes de 128 octets maximum. `ecriture_page()` refuse les écritures qui dépassent 32 octets ou traversent une limite de page. `pret()` exécute un **ACK polling** ; `GF_OCCUPE` signifie que la mémoire n'a pas encore répondu. `eeprom_interface()` fournit ces fonctions à `stockage.c`.

### 4.9 Transactions persistantes — `stockage.c` / `stockage.h`

La mémoire est partagée en **deux banques de 2048 octets**. Chaque banque contient un en-tête de 32 octets, jusqu'à **100 enregistrements de 13 octets**, et l'image de calibration de 512 octets : taille maximum **1844 octets**, compatible avec la banque. L'en-tête inclut la signature `GFN2`, la version, un numéro de génération, les longueurs, les CRC et un marqueur de validation `0xA5` à l'octet 31.

`stockage_charger()` lit les deux banques, vérifie signatures/versions/CRC/presets/profil et retient la génération valide la plus récente. Une banque vierge est constituée d'octets `0xFF`.

`stockage_commencer()` prépare une image RAM de la banque **inactive**. `stockage_pas()` déroule la machine :

```text
INVALIDER → CONTROLER_INVALIDE → CORPS → HEADER
          → VERIFIER → VALIDER → CONTROLER_VALIDE → REPOS
```

L'ancien enregistrement valide est préservé jusqu'au commit de la nouvelle banque : invalidation du marqueur cible, écriture du corps, écriture de l'en-tête, relecture et comparaison, écriture finale du marqueur, relecture de contrôle. Une page est écrite par étape et la fin du cycle est attendue avec ACK polling, timeout de 50 ms. C'est une stratégie de robustesse face à une coupure pendant la sauvegarde ; seule une vraie coupure d'alimentation permet de qualifier complètement ce comportement sur matériel.

### 4.10 Encodeur et anti-rebond — `entree.c`, `encodeur.c`

`entree.c` est un module pur : table de 16 transitions quadrature possibles, accumulation d'un tour électrique complet, validation lorsque les entrées reviennent à leur position de détente ; anti-rebond bouton de **30 ms** et appui long à **800 ms**. Sur le matériel, `encodeur.c` configure A/B en entrées avec pull-up et interruptions sur chaque front, le bouton en entrée, et une **file FreeRTOS de 128 états**. L'ISR lit A/B et empile la valeur ; le décodage a lieu dans la boucle principale. En cas de débordement, le code compte les pertes, vide la file et resynchronise la quadrature. La direction est inversée selon `SYSTEM_ENCODER_REVERSE_DIRECTION`.

`encodeur_lire_evenement()` renvoie un événement (`UI_ROTATION_PLUS`, `UI_ROTATION_MOINS`, `UI_CLIC`, `UI_APPUI_LONG`, ou aucun) par itération. Si le bouton a un événement, celui-ci est prioritaire sur la rotation en attente.

### 4.11 Écran OLED — `oled.c` / `oled.h`

L'écran est détecté aux adresses **0x3C**, puis **0x3D**. Le driver envoie une séquence d'initialisation SH1106 et maintient deux framebuffers **8 × 128 octets** : image désirée et image déjà envoyée. `oled_effacer()`, `oled_pixel()`, `oled_segment()`, `oled_texte()`, `oled_ligne()`, `oled_texte_grand()` et `oled_inverser_zone()` constituent l'API graphique. La police de base est constituée de glyphes de cinq colonnes ; la police agrandie élargit les colonnes et double la hauteur des lignes.

`oled_periodique()` n'envoie **qu'une page modifiée au maximum** à chaque itération. Ce choix répartit les transferts I²C et réduit les rafraîchissements inutiles. En cas de défaut I²C, un compteur d'erreurs augmente et une reconnexion est tentée après 2 secondes. Les tracés et inversions de zones bornent leurs coordonnées pour éviter les dépassements du framebuffer.

### 4.12 Interface utilisateur — `menu.c` / `menu.h`

Le menu comporte trois pages internes : `LISTE`, `DETAIL`, `MESSAGE`. `LISTE` affiche un bandeau et **trois grandes lignes**, dont la ligne sélectionnée est inversée. Le scrolling horizontal ne concerne que cette ligne, après 900 ms d'inactivité, par pas d'environ 350 ms. Tourner l'encodeur change seulement `index_signal` et n'affecte pas physiquement la sortie.

**Clic depuis LISTE :** `integration_choisir(index_signal)`, puis `integration_activer()` uniquement si la sélection a réussi ; si tout fonctionne, passage à `DETAIL`. **Clic depuis DETAIL ou MESSAGE :** retour à LISTE. **Rotation depuis une fiche :** retour à LISTE et changement du pointeur. **Appui long :** `integration_arreter()` pour demander l'arrêt des sorties, avec affichage d'une erreur si l'arrêt n'est pas confirmé.

L'écran détail montre fréquence, amplitude et offset. En mode nominal, les tensions sont préfixées par `~` pour signaler une estimation. En DDS seul, il indique `A NON REGLEE` et `O NON REGLE`. Le menu ne contient plus d'édition de presets : cette fonction relève du protocole USB.

### 4.13 Sorties numériques — `sorties_digitales.c`

Le pilote exploite le périphérique **LEDC** pour deux sorties PWM indépendantes. La commande comprend canal, fréquence, rapport cyclique en **pour-mille (0 à 1000)** et activation. Les fréquences opérationnelles du pilote sont de **10 Hz à 1 MHz**. La résolution de timer est adaptée à la fréquence ; la fréquence réalisée est contrôlée avec une tolérance calculée de **1 %**. Les rapports 0 % et 100 % utilisent `ledc_stop()` pour forcer le niveau de sortie.

La sécurité de brochage repose sur une validation GPIO et une liste de pins réservées. **`board_options.h` met les deux pins à −1 par défaut**, donc les sorties sont **indisponibles sur la configuration livrée** : il ne faut pas conclure que deux sorties numériques physiques sont validées.

### 4.14 Protocole USB — `usb_protocol.c`

`usb_proto_decoder()` est un parseur sans effet de bord. Il exige une trame ASCII `$...#`, refuse les caractères de contrôle et les séparateurs mal placés, convertit les champs numériques avec limites et interprète les opérations. Les index USB sont **1 à 100** et deviennent des index C **0 à 99**. Les noms de formes sont `SIN`, `TRI`, `CAR`. `usb_proto_formater()` produit le format standard d'un signal, y compris le signe explicite de l'offset.

### 4.15 Service USB — `usb_service.c`

Le service reçoit les octets, reconstitue les trames, appelle le parseur puis **`integration.c`**. Il maintient des buffers RX/TX statiques, un compteur de trames et d'erreurs, et réserve une place de réponse avant les mutations pour éviter des modifications silencieuses. Il gère également les lectures de tous les presets, l'état du système, les diagnostics numériques et le transfert de calibration.

Une trame RX inachevée expire après **1 seconde** ; un transfert CAL inachevé après **30 secondes**. Un tampon TX bloqué pendant **2 secondes** entraîne son abandon et une demande de réinitialisation du transport. Le transfert d'un profil utilise seize blocs de 32 octets, un **masque de présence 16 bits** et une vérification de la révision du modèle avant commit. Une commande `CAL:COMMIT` n'importe donc pas un profil incomplet ou devenu obsolète pendant le transfert.

### 4.16 Transport USB — `usb_transport.c`

Le firmware utilise le pilote ESP-IDF **USB Serial/JTAG**. À chaque passage, le service de protocole est entretenu, au plus 64 octets sont lus du périphérique dans ce passage et un segment TX est écrit sans attente volontaire. Le driver est réinstallé périodiquement en cas de problème et après demande de réinitialisation. Le code sépare volontairement la syntaxe et les décisions métier du port USB réel, ce qui facilite les tests.

## 5. Scénarios de fonctionnement

### 5.1 Démarrage sans activation intempestive

Le programme initialise l'AD9833 avec RESET demandé, construit le catalogue en RAM, essaie de charger la dernière banque EEPROM valide, marque l'état de disponibilité des périphériques puis demande l'arrêt analogique et des sorties numériques. L'état initial effectif et la tension VOUT doivent être **observés sur carte** : les indicateurs logiciels ne sont pas des mesures.

### 5.2 Sélection et démarrage par l'encodeur

```text
Tourner → sélectionner une ligne d'écran (sans changer la sortie)
Clic → arrêter l'ancienne sortie si nécessaire
     → mémoriser le nouveau preset
     → vérifier le profil / choisir le mode
     → RESET DDS
     → configurer forme + fréquence
     → configurer EPOT si applicable
     → éventuellement attendre stabilisation
     → libérer RESET DDS
     → écran DETAIL ou MESSAGE d'erreur
```

Un clic en mode détail retourne à la liste **sans éteindre automatiquement** le signal. L'arrêt volontaire se fait par appui long ou `$OFF#`.

### 5.3 Modification distante puis sauvegarde

`$1:SIN:1000:+0:10000#` est décodée, validée par le coordinateur et appliquée **au catalogue en RAM** ; le service renvoie un accusé avec révision. Le système marque une sauvegarde à effectuer. `integration_periodique()` attend 300 ms sans nouvelle modification, capture le catalogue et le profil, puis écrit dans la banque EEPROM inactive. Une réponse `OK:RAM` ne signifie donc pas « écrit et vérifié en EEPROM ». Consulter `$STATE?#` et les indicateurs de sauvegarde pour connaître le résultat persistant.

### 5.4 Erreur de communication matérielle

Si un réglage EPOT échoue ou si le DDS ne répond pas, l'activation retourne une erreur, le code tente un RESET, conserve le diagnostic de phase et entre en défaut. Un réarmement n'est possible qu'après arrêt commandé accepté. Un diagnostic physique reste indispensable si l'état électronique réel est inconnu.

### 5.5 Coupure pendant l'écriture

Si la nouvelle banque n'a pas atteint le marqueur `0xA5` final, le chargeur peut retrouver la banque précédente valide. En cas de corruption des deux banques, il conserve les presets usine en RAM mais n'écrase pas automatiquement un contenu inconnu au démarrage. Cette logique suppose toutefois que l'EEPROM et la carte respectent les séquences d'écriture prévues.

## 6. Échanges USB et formats de données

Toutes les commandes sont encadrées par `$` et `#`. Les exemples suivants décrivent le code actuel, et pas seulement la trame minimale du cahier des charges.

| Commande | Exemple | Action |
|---|---|---|
| Écriture | `$1:SIN:1000:+0:10000#` | Modifie le preset 1 en RAM |
| Lecture unitaire | `$1?#` | Retourne le preset 1 |
| Lecture globale | `$?#` | Retourne tous les presets |
| État | `$STATE?#` | État, révision, sauvegarde, erreurs |
| Sélection | `$SELECT:1#` | Sélectionne sans activer |
| Activation / arrêt | `$ON#` / `$OFF#` | Commande la sortie / l'arrêt |
| Persistance | `$SAVE#` / `$LOAD#` | Demande sauvegarde / recharge EEPROM |
| Catalogue | `$DUP:1#`, `$DELETE:2#`, `$FACTORY#` | Duplique, supprime, rétablit les valeurs usine RAM |
| Numérique | `$DIG?#` | Diagnostics des deux voies |
| Numérique PWM | `$DIG:1:1000:500#` | Demande 1 kHz, 50 % sur canal 1 |
| Arrêt PWM | `$DIG:1:OFF#` | Arrête ce canal |
| Calibration | `$CAL?#` | Exporte le profil |
| Début / transfert | `$CAL:BEGIN#`, `$CAL:DATA:0:<64 hex>#` | Transfert par blocs de 32 octets |
| Fin / abandon | `$CAL:COMMIT#`, `$CAL:ABORT#` | Import validé / abandon |
| Effacement profil | `$CAL:CLEAR#` | Supprime la calibration RAM |

Réponses typiques : `$OK:RAM:<revision>#`, `$ERR:ARGUMENT#`, `$STATE:...#`, et, pour les lectures, une trame au format du preset. `$SAVE#` confirme **la demande**, pas nécessairement le commit terminé. Ne pas utiliser le mot « calibré » si le profil installé n'a pas été obtenu et approuvé sur le montage réel.

Le client Python `tools/generateur.py` constitue l'outil PC associé. `requirements-pc.txt` décrit ses dépendances. Le flux de calibration utilise le format binaire `CAL1` (512 octets), pas une simple suite de tensions en texte.

## 7. Brochage, périphériques et limites électriques

| Fonction | Paramètre du code |
|---|---|
| Carte | Seeed XIAO ESP32-C6 ; framework ESP-IDF |
| I²C SDA / SCL | GPIO22 / GPIO23 |
| OLED SH1106 | Adresse 0x3C, secours 0x3D ; 128×64 |
| EPOT offset / gain | 0x3E / 0x3F |
| EEPROM | 0x50, 4096 octets |
| Encodeur A / B / bouton | GPIO1 / GPIO2 / GPIO0 |
| DDS MOSI / SCLK / FSYNC | GPIO18 / GPIO19 / GPIO21 |
| DDS SPI / MCLK | 1 MHz / 25 MHz |
| Sorties digitales 1 et 2 | Non configurées (`-1`) |
| USB | Périphérique USB Serial/JTAG intégré |

La sortie analogique complète contient des **étages externes au microcontrôleur**. Les amplitudes annoncées par le catalogue concernent l'objectif de `VOUT`; leur obtention dépend du schéma et des mesures. Les risques de saturation, écrêtage, offset résiduel, transitoires de commutation, dépendance de la fréquence et du courant de sortie ne peuvent pas être tranchés par l'analyse de ce seul firmware.

Les options `sdkconfig.defaults` désactivent la console/log secondaire pour ne pas polluer les trames USB et configurent une pile principale de **8192 octets**. `platformio.ini` cible `platformio/espressif32@8.0.0`, `framework-espidf@4.60100.0`, et fixe explicitement plusieurs composants de la chaîne de compilation. Le paquet Ninja n'est pas figé à une version hôte unique, afin d'éviter le problème Windows déjà décrit dans le suivi.

## 8. Conformité au cahier des charges

| Exigence | État constaté sur les sources | Réserve |
|---|---|---|
| Trois formes de signal | Implémentées dans le catalogue et le driver DDS | Qualité et amplitude à VOUT non mesurées |
| Fréquence et amplitude modifiables | Paramètres modifiables par USB ; DDS règle la fréquence ; EPOT visent l'amplitude | La conversion amplitude → VOUT requiert un profil réel |
| Offset programmable | Modèle et commandes EPOT | Comportement analogique réel inconnu |
| Dix signaux initiaux | Correspondance explicite dans `signaux.c` | Vérification instrumentale des dix cas requise |
| Jusqu'à cent presets | Capacité statique et format EEPROM prévus | Valider temps de transfert et endurance |
| Mémoire non volatile | Deux banques 24LC32A, CRC, reprise en logiciel | Vérification alimentation/coupure réelle manquante |
| Écriture/lecture USB `$...#` | Décodeur et service implémentés | Essais câble/terminal/PC à mener |
| Navigation locale | Trois lignes, encodeur, activation et arrêt | Ergonomie sur boîtier à tester |
| Deux sorties numériques | Driver PWM optionnel | Fonction non définie précisément dans le cahier, pins non attribuées |
| Prototype final destiné aux apprentis | Architecture fonctionnelle produite | Sécurité d'emploi, conformité électrique et VOUT restent à qualifier |

**Point de vigilance de vocabulaire :** « implémenté » signifie présent dans les sources ; « testé logiciellement » signifie couvert par des tests ou documents de validation ; « validé matériellement » nécessiterait des relevés physiques. Il serait inexact de confondre ces statuts.

## 9. Analyse des risques et améliorations

### P0 — À traiter avant de présenter VOUT comme une sortie qualifiée

1. **Calibrer l'étage analogique réel.** Vérifier aux oscilloscopes les dix presets, plusieurs charges et fréquences, l'offset, l'écrêtage et le gain/offset appliqués par les MCP45HV51. Générer puis approuver un profil CAL1 à partir de relevés reproductibles, pas d'un profil synthétique de tests.
2. **Définir le comportement OFF au niveau électronique.** RESET AD9833 n'est pas un relais de déconnexion de VOUT. Mesurer la sortie pendant boot, reset, échec SPI/I²C, changement de preset, suppression de charge et extinction.
3. **Vérifier les hypothèses matérielles.** Adresses I²C, alimentation des composants, WP EEPROM, WLAT/SHDN EPOT, câblage et capacités LT1206, compatibilité des rails et charge.

### P1 — Fiabilité d'intégration

4. **Tester la persistance sur EEPROM réelle**, dont coupure à plusieurs étapes et restauration de la banque précédente ; vérifier que la fin de sauvegarde apparaît correctement dans `STATE?`.
5. **Mesurer temps de boucle et file encodeur** sous rafales USB, rafraîchissement OLED et écriture EEPROM. Les échanges I²C/SPI et `vTaskDelay()` restent susceptibles de modifier le temps de réponse.
6. **Exercer systématiquement les chemins d'erreur.** Débrancher périphériques, provoquer timeout ou absence d'ACK, essayer profils invalides, index extrêmes et trames tronquées, contrôler que l'arrêt et les indications de défaut sont cohérents.
7. **Revoir les sorties numériques avec le schéma réel** avant de définir les GPIO dans `board_options.h` ; s'assurer que « PWM 3,3 V » correspond au besoin fonctionnel.

### P2 — Lisibilité et maintenance

8. **Documenter la relation entre les révisions et la sauvegarde.** Une modification est immédiatement visible en RAM mais non durable tant que la transaction n'est pas confirmée.
9. **Conserver la séparation modèle / transport / pilotes.** Elle rend possibles les tests hôtes et limite les effets de bord. Toute nouvelle fonction devrait passer par `integration.c` plutôt que modifier directement le catalogue depuis une ISR ou le transport.
10. **Améliorer l'observabilité.** Ajouter aux recettes les résultats réels par preset, charge et fréquence, plus l'identifiant précis du firmware flashé. Éviter de remplacer les preuves par une estimation provenant du calcul nominal.
11. **Maintenir à jour les documents historiques.** Certains textes datés peuvent décrire l'état d'une version antérieure (par exemple le refus de `ON` sans profil). Le comportement du code examiné est celui décrit dans le présent rapport : nominal si EPOT disponibles, DDS seul dans le cas contraire.

### Observations techniques ciblées

- Les plages du parseur et du catalogue ne remplacent pas le contrôle de réalisabilité par les drivers et la calibration. Une consigne de 12 MHz est dans la plage syntaxique, mais la qualité d'onde dépend du DDS et du circuit.
- `integration_choisir()` et les modifications de preset sélectionné demandent un arrêt préalable : bon invariant logiciel, à confirmer sur VOUT.
- Le driver EPOT effectue une **relecture du registre**, ce qui détecte certaines erreurs I²C, pas un défaut de chaîne analogique.
- L'EEPROM utilise le CRC et un marqueur écrit en dernier : avantage de cohérence, avec coût en latence d'écriture et usure. La fréquence d'écriture différée limite les commits rapprochés.
- Le service USB s'efforce de réserver un espace de réponse avant mutation, mais une application PC doit gérer `ERR`, les expirations et les états de sauvegarde plutôt que supposer une commande toujours réussie.
- Les modules `initialisation.c` et `signaux_variation()` conservent des API utiles ou historiques ; ne pas leur attribuer une fonction dans le menu courant sans trouver leur appel effectif.

## 10. Tests et mise en route

### 10.1 Tests disponibles dans le dépôt

Le runner `tools/run_native_tests.py` compile des suites **C natives**, des tests Python du client, puis une vérification de compatibilité d'une image de calibration produite en Python avec le décodeur C. Les suites décrites dans le code comprennent :

- `test_systeme.c` : catalogue, calibration, états, stockage, USB ;
- `test_ui.c` : quadrature, bouton et menu ;
- `test_oled_render.c` : rendu et limites d'affichage ;
- `test_drivers.c` : pilotes avec faux bus et faux périphériques ;
- `test_codec.c` : décodage de l'image de profil ;
- `tools/test_generateur.py` : fonctions du client PC.

Commandes depuis la racine du dépôt :

```bash
python tools/run_native_tests.py --sanitize
cd EQ1_Projet_Generateur_Fonction
pio run -e seeed_xiao_esp32c6
```

Le runner utilise GCC en mode C11 avec `-Wall -Wextra -Werror -Wpedantic`. L'option `--sanitize` ajoute AddressSanitizer et UndefinedBehaviorSanitizer lorsque compatibles. La sortie se trouve dans `build/native/tests.log` et `build/native/results.json`. Les preuves et restrictions historiques de compilation sont dans `docs/VALIDATION.md` et `docs/P01_SUIVI.md`.

**Ces commandes sont fournies pour reproduction ; elles n'ont pas été exécutées lors de la rédaction de ce rapport.**

### 10.2 Séquence recommandée de vérification du prototype

1. Examiner le schéma et vérifier toutes les masses, rails, broches de commande et niveaux I²C/SPI.
2. Démarrer sans charge sensible sur VOUT ; confirmer au scope qu'aucune sortie dangereuse n'est présente avant activation.
3. Vérifier présence OLED, encodeur, DDS, deux EPOT et EEPROM ; contrôler les diagnostics `$STATE?#`.
4. Sélectionner un preset simple à faible fréquence, activer puis mesurer fréquence, Vpp, offset et forme réelle.
5. Tester `OFF`, changement de preset, défaut EPOT et boot avec conditions dégradées.
6. Mesurer les dix presets, notamment les grandes amplitudes et offsets, sur les charges prévues ; comparer aux tolérances acceptées.
7. Générer un profil réel de calibration, l'importer par USB et refaire les mesures.
8. Modifier puis sauvegarder un preset, redémarrer et vérifier sa restauration ; répéter avec coupures contrôlées.
9. Vérifier les transferts USB longs, l'OLED en activité, les interruptions encodeur et l'absence de pertes de commandes.
10. Si les sorties numériques sont retenues, définir leurs broches et effectuer une recette distincte avec oscilloscope.

Les résultats doivent être enregistrés avec la révision Git, le binaire, l'alimentation, la charge et l'instrument utilisé.

## 11. Glossaire et conclusion

**DDS** : générateur numérique direct de fréquence ; **EPOT** : potentiomètre numérique ; **Vpp** : tension crête-à-crête ; **offset** : composante continue ; **I²C** : bus série partagé ; **SPI** : liaison série synchronisée du DDS ; **LEDC** : générateur PWM matériel ESP32 ; **CRC** : contrôle d'intégrité ; **ISR** : routine d'interruption ; **preset** : signal préconfiguré ; **profil CAL1** : tables de réponse analogique validées et encodées.

L'architecture analysée est structurée autour d'un **coordinateur unique**, de pilotes matériels isolés, d'un **catalogue commun aux interfaces**, d'une persistance transactionnelle et de services périodiques. Cette séparation facilite la compréhension du code et les tests. Le principal travail restant n'est pas la génération de trames ou l'affichage du menu, mais la **qualification du comportement électrique réel** : calibration, vérification de VOUT, définition de l'arrêt et preuves de robustesse sur la carte.

---

### Références dans le dépôt

- [README général](../README.md)
- [Guide technique](GUIDE_TECHNIQUE.md)
- [Bilan de validation](VALIDATION.md)
- [Modèle nominal](MODELE_NOMINAL.md)
- [Correctif DDS sans profil](CORRECTION_DDS_SANS_PROFIL.md)
- [Suivi P01](P01_SUIVI.md)
- [Plan P02–P11](PLAN_FINALISATION_PROJET.md)
- [Sources firmware](../EQ1_Projet_Generateur_Fonction/src/)
- [Headers firmware](../EQ1_Projet_Generateur_Fonction/include/)
- [Outils et tests PC](../tools/)
