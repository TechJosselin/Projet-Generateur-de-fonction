# Plan de finalisation du firmware — Générateur de fonctions

> **Statut : plan d'exécution, pas attestation de firmware terminé.**
> Les cases décrivent le travail restant à effectuer et à vérifier. La création de ce document ne modifie pas le code ni les branches GitHub.

| Référence | Valeur |
|---|---|
| Projet | CFPT — Équipe 1 — Générateur de fonctions |
| Date de préparation | 29 octobre 2026 |
| Dépôt | `TechJosselin/Projet-Generateur-de-fonction` |
| Base retenue | `integration/full-system` |
| Commit de référence, branche revérifiée | `fe47f86fe5af626a0da5a1c0aa1b16a198df4b62` |
| Répertoire du firmware | `EQ1_Projet_Generateur_Fonction/` |
| Cible | Seeed Studio XIAO ESP32-C6, C, ESP-IDF / PlatformIO |
| Emplacement proposé pour ce document | `docs/PLAN_FINALISATION_PROJET.md`, à la racine du dépôt |
| Prochaine phase d'exécution | **P01 — Référence, compilation et socle de tests** |

## 1. Objectif et règle de finalisation

Finaliser les parties logicielles du générateur en conservant les modules utiles : génération DDS, amplitude et offset, catalogue de signaux, persistance EEPROM, communication USB, interface locale, diagnostics et sorties numériques prévues au schéma bloc.

Le résultat attendu est une chaîne complète :

```text
Encodeur / USB
      ↓
Validation et catalogue des signaux
      ↓
Coordinateur d'application
      ├── AD9833 : forme et fréquence
      ├── MCP45HV51 : gain et offset
      ├── 24LC32A : configurations persistantes
      └── État commandé et erreurs → OLED / USB
```

**Une fonction n'est pas terminée parce que son menu existe ou qu'elle renvoie un succès.** Elle doit effectuer l'action, propager les erreurs, posséder des tests et indiquer honnêtement son niveau de validation.

Trois niveaux de preuve sont distincts :

- **LOGICIEL VÉRIFIÉ** : tests automatisés pertinents réussis.
- **CIBLE COMPILÉE** : construction complète du firmware pour l'ESP32-C6 réussie.
- **MATÉRIEL VALIDÉ** : essai réel documenté sur la carte et, pour l'analogique, sur VOUT.

Une simulation ne vaut pas une mesure. Un transfert SPI réussi ne prouve pas que le signal physique est présent. Un ACK I²C ne prouve pas que l'étage analogique produit les bonnes tensions.

### Consignes pour exécuter ce plan

Conserver le travail existant et avancer par modifications testables. Pour chaque phase : examiner les fichiers concernés, implémenter les fonctions, ajouter les tests, compiler, puis renseigner le journal de suivi. Ne pas remplacer un problème réel par un retour `true`, une valeur arbitraire ou un périphérique simulé dans la version matérielle.

Lorsqu'une donnée matérielle manque, continuer les parties indépendantes, isoler précisément la dépendance et préparer le code et les tests correspondants. Ne pas marquer la validation matérielle comme terminée. Ne pas réécrire l'ensemble du projet ni ajouter de fonctionnalités sans rapport avec le besoin.

## 2. Périmètre et exigences de référence

### 2.1. Fonctions à livrer

| ID | Exigence | Phases principales |
|---|---|---|
| EX01 | Dix presets initiaux conformes au cahier des charges | P02, P10 |
| EX02 | Catalogue modifiable, capacité maximale de 100 signaux | P02, P05, P06, P07 |
| EX03 | Sinus, triangle et carré ; fréquence commandée au DDS | P03, P09, P10 |
| EX04 | Amplitude crête-à-crête et offset appliqués à la chaîne analogique | P04, P09, P10 |
| EX05 | Paramètres conservés en EEPROM et restaurés après redémarrage | P05, P09, P10 |
| EX06 | Écriture USB et lecture d'un signal ou de tous les signaux | P06, P10 |
| EX07 | Sélection et modification par encodeur et OLED | P07, P09 |
| EX08 | État de fonctionnement et erreurs cohérents entre UI, USB et pilotes | P02, P03, P07, P09 |
| EX09 | Gestion des deux sorties numériques du schéma bloc selon leur fonction confirmée | P08, P10 |
| EX10 | Construction reproductible, tests, documentation et livraison | P01, P10, P11 |

EX01 à EX06 proviennent du cahier des charges, notamment pages 1 à 3 [S1]. Les sorties numériques figurent dans le schéma bloc, mais leur comportement n'y est pas spécifié [S1, S2]. La robustesse et l'organisation proposées ici sont des choix d'implémentation, pas de nouvelles exigences de l'enseignant.

### 2.2. Presets de référence à préserver

Les unités du tableau sont celles à utiliser dans le modèle logiciel et le protocole.

| N° | Forme | Amplitude (mVpp) | Offset (mV) | Fréquence (Hz) |
|---:|---|---:|---:|---:|
| 1 | SIN | 1000 | 0 | 10000 |
| 2 | SIN | 5000 | 0 | 1000000 |
| 3 | SIN | 2000 | 3000 | 1000 |
| 4 | SIN | 200 | 10000 | 500 |
| 5 | TRI | 20000 | 0 | 5000 |
| 6 | TRI | 4000 | 0 | 100 |
| 7 | TRI | 5000 | -5000 | 200000 |
| 8 | CAR | 10000 | 0 | 50 |
| 9 | CAR | 1000 | 0 | 300 |
| 10 | CAR | 4000 | 6000 | 1000 |

Source : cahier des charges, page 3 [S1]. Ne pas réduire silencieusement un preset pour masquer une limite matérielle : signaler et traiter l'écart.

### 2.3. Protocole à préserver

```text
Écriture :      $N:TYPE:AMPLITUDE_MVPP:OFFSET_MV:FREQUENCE_HZ#
Lecture unique : $N?#
Lecture totale : $?#

Exemple : $3:SIN:2000:+3000:1000#
```

La réponse à une lecture utilise le format d'écriture. La lecture totale doit produire les trames de tous les signaux présents, dans l'ordre, pas uniquement `OK:LISTE` [S1].

**Décision de ce plan :** accepter les indices décimaux de 1 à 100, malgré la largeur indicative d'un octet dans le tableau du cahier. Les validations de plages déjà présentes dans `signaux.h` sont une base logicielle, pas une garantie que toutes les combinaisons sont réalisables sur VOUT.

### 2.4. Hors périmètre de cette finalisation du code

Ne pas ajouter Wi-Fi, Bluetooth, interface web, signaux arbitraires, mise à jour distante ou synchronisation complexe sans besoin explicite. Ne pas présenter du code comme une protection matérielle contre les courts-circuits. La fabrication du PCB, le boîtier et les documents de leadership restent des travaux du projet global ; leur état ne peut pas être déduit de la compilation du firmware.

## 3. État de départ et dépendances matérielles

L'audit du commit de référence a identifié les points suivants : catalogue et interface présents ; commandes DDS présentes ; amplitude/offset non appliqués dans l'intégration ; sauvegarde/chargement EEPROM non réalisés ; parseur USB sans transport ; sorties numériques non réalisées [R1–R4].

Le plan ne présume pas l'absence de travaux locaux non poussés : P01 commence par les préserver et les comparer.

| ID | Donnée nécessaire | Usage | Comportement tant qu'elle manque |
|---|---|---|---|
| MAT01 | Schéma détaillé de l'étage de gain/offset, valeurs et connexions des EPOT | Conversion volts → codes et ordre des commandes | Modèle et tests simulés possibles ; pas de coefficients de production inventés |
| MAT02 | Mesures de calibration, charge de référence et limites de VOUT | Exactitude, enveloppe amplitude/offset/fréquence | État `NON_CALIBRE` ; activation analogique normale refusée sans profil exploitable |
| MAT03 | Existence et câblage d'un éventuel mute/enable matériel | Définition réelle de l'arrêt | Ne pas annoncer « VOUT = 0 V » ou haute impédance sur la seule base du RESET DDS |
| MAT04 | GPIO, rôle, polarité et niveaux des sorties numériques | Implémentation P08 | Pilote testable sur simulation ; sorties physiques non affectées arbitrairement |
| MAT05 | Carte montée : adresses, broches WP/SHDN/WLAT éventuelles, alimentations et résistances de tirage | Validation des pilotes et du démarrage | Paramètres centralisés ; pas de déclaration de conformité physique |
| MAT06 | Tolérances de fréquence, amplitude, offset et qualité de forme | Recette finale | Préparer les relevés ; ne pas inventer les tolérances du cahier |

Les adresses et GPIO du commit servent de référence provisoire, pas de preuve du câblage du PCB. Retenir ESP32-C6 et OLED SH1106 1,3 pouce d'après le cahier et le code ; signaler la divergence ESP32-C3/OLED 0,96 pouce sur la page 2 du document de schéma bloc [S1, S2].

**MAT01 à MAT06 ne bloquent pas toute la programmation.** Elles conditionnent les fonctions matérielles concernées et leur recette. Une dépendance non résolue ne doit toutefois pas être effacée du bilan final.

## 4. Phases et dépendances

| Phase | Livrable | Dépendances | Statut initial |
|---|---|---|---|
| P01 | Base conservée, environnement reproductible, tests de départ | Aucune | À faire |
| P02 | Modèle de données et coordinateur d'état cohérents | P01 | À faire |
| P03 | Pilotes I²C, AD9833 et EPOT fiabilisés | P01, interfaces P02 | À faire |
| P04 | Application analogique et mécanisme de calibration | P02, P03 ; MAT01–03 pour la carte | À faire |
| P05 | EEPROM et stockage persistant robuste | P02, bus P03 | À faire |
| P06 | USB complet et outil de test PC | P02, P05 pour la persistance | À faire |
| P07 | Interface OLED/encodeur raccordée aux fonctions réelles | P02 ; raccordements P04–06 progressifs | À faire |
| P08 | Sorties numériques configurables et testées | P02, MAT04 pour la carte | À faire |
| P09 | Démarrage, transitions et défauts intégrés | P03–08, selon capacités disponibles | À faire |
| P10 | Tests de non-régression et recette de l'appareil | Tests dès P01 ; recette complète après P09 | À faire |
| P11 | Documentation, version livrable et bilan de conformité | P09, P10 | À faire |

Ordre conseillé : P01 → P02 → P03, puis avancer sur P04, P05 et P06 sans attendre les mesures nécessaires à la clôture matérielle de P04. P07 et P08 se raccordent ensuite au même coordinateur. P09 réunit les chemins ; P10 et P11 apportent les preuves et le livrable.

---

## P01 — Conserver la référence et rendre la compilation reproductible

**But :** travailler sur la bonne version sans perdre le code existant.

**Fichiers :** `platformio.ini`, `sdkconfig.defaults`, `CMakeLists.txt`, `src/CMakeLists.txt`, `test/`, futur `README.md`.

- [ ] **P01-01** Examiner toutes les branches pertinentes et les modifications locales ; consigner le commit réellement utilisé. Ne pas repartir de `main` par défaut.
- [ ] **P01-02** Créer une branche de travail dédiée à partir de la base retenue. Ne pas fusionner automatiquement les branches de fonctionnalités ni écraser le travail local.
- [ ] **P01-03** Reproduire la compilation actuelle ESP32-C6, archiver sa sortie et identifier les erreurs préexistantes.
- [ ] **P01-04** Fixer la version PlatformIO/espressif32 effectivement testée ; consigner la version ESP-IDF résolue. Ne pas choisir un numéro de version non vérifié.
- [ ] **P01-05** Examiner l'archive `Tests_revue_generateur_fe47f86.zip`, intégrer les tests pertinents et les relancer. Ne pas reprendre un ancien nombre de tests comme preuve d'une nouvelle exécution.
- [ ] **P01-06** Ajouter une commande reproductible pour les tests natifs et une compilation automatique minimale en CI, avec dépendances fixées.

**Critère de fin :** référence conservée, compilation cible réussie ou blocage exact consigné, tests de base exécutables et commandes documentées. Un blocage de compilation interdit de déclarer P01 terminée, sans empêcher l'analyse des phases suivantes.

**Commande existante à vérifier depuis la racine du dépôt :**

```sh
pio run -d EQ1_Projet_Generateur_Fonction -e seeed_xiao_esp32c6
```

## P02 — Unifier le catalogue, les commandes et les états

**But :** éviter qu'une valeur affichée soit confondue avec celle appliquée au matériel.

**Fichiers :** `signaux.c/.h`, `integration.c/.h`, appels depuis `main.c`, `menu.c` et `usb_protocol.c`.

- [ ] **P02-01** Conserver les unités entières : mVpp, mV signés et Hz ; vérifier les dix presets et la capacité de 100 signaux.
- [ ] **P02-02** Distinguer brouillon d'édition, catalogue validé, sélection et copie de la dernière configuration commandée avec succès.
- [ ] **P02-03** Donner à `integration.c` la responsabilité unique des changements de sortie. UI et USB passent par la même API applicative, sans accès direct aux pilotes.
- [ ] **P02-04** Introduire des états explicites : `NON_INITIALISE`, `ARRET_COMMANDE`, `PRET`, `ACTIF_COMMANDE`, `NON_CALIBRE`, `DEFAUT`. Ces noms ne constituent pas une mesure de VOUT.
- [ ] **P02-05** Mettre à jour sélection et configuration appliquée uniquement après réussite de la séquence concernée. Exposer l'étape et l'erreur d'un échec.
- [ ] **P02-06** Adopter une politique commune : modifier le preset actif demande d'abord l'arrêt, valide la nouvelle configuration en RAM, puis impose une réactivation explicite. En cas d'échec d'arrêt, refuser la mutation et signaler l'état incertain.
- [ ] **P02-07** Séparer validation syntaxique/plages du catalogue et validation physique d'application : le catalogue peut être édité hors ligne, mais une configuration hors capacité ne doit pas être activée.
- [ ] **P02-08** Gérer suppression, duplication et remplacement du catalogue sans indices périmés. Toute suppression décale les indices : annuler la sélection après arrêt confirmé, comme politique simple initiale.
- [ ] **P02-09** Prévoir des révisions de catalogue et un indicateur « modifié / sauvegardé » ; supprimer cet indicateur uniquement après sauvegarde vérifiée de la même révision.

**Tests :** changement d'un preset actif, échec d'arrêt, suppression avant l'indice sélectionné, remplacement du catalogue, dernière entrée, limite de 100, erreurs propagées sans faux succès.

**Critère de fin :** UI, USB et matériel commandé ne peuvent plus diverger silencieusement ; les chemins de mutation ont des tests avec pilotes simulés.

## P03 — Fiabiliser les pilotes matériels existants

**But :** rendre les commandes déterministes et les erreurs exploitables, sans réécriture inutile.

**Fichiers :** `ad9833.c/.h`, `epot.c/.h`, `i2c_commun.c/.h`, `initialisation.c/.h`, `hardware_config.h`.

- [ ] **P03-01** AD9833 : initialiser explicitement les registres utilisés, dont PHASE0 ; vérifier mode SPI, ordre des octets et séquence RESET / fréquence / phase / contrôle [D2].
- [ ] **P03-02** AD9833 : rejeter les entrées non finies et hors plage avant conversion ; tester arrondi et découpage du mot de fréquence.
- [ ] **P03-03** AD9833 : mettre à jour l'état interne à chaque étape pertinente ; après échec partiel, invalider la configuration appliquée et tenter l'arrêt sans masquer un nouvel échec.
- [ ] **P03-04** EPOT : conserver détection et configuration TCON ; vérifier les commandes et broches de contrôle contre la fiche technique de la référence exacte avant de changer leur comportement.
- [ ] **P03-05** EPOT : ajouter la relecture utile des registres, selon le composant, pour distinguer code demandé, code relu et valeur électrique non mesurée. Définir les codes de démarrage seulement après examen de MAT01.
- [ ] **P03-06** I²C/SPI : contrôler les durées d'attente, libérations et réinitialisations ; éviter la duplication de bus et toute boucle de reprise infinie.
- [ ] **P03-07** Centraliser adresses, broches et options matérielles ; vérifier conflits, broches USB réservées et niveaux électriques lors de la validation de carte.
- [ ] **P03-08** Ajouter l'injection d'erreurs sur chaque écriture DDS et chaque étape EPOT, sans laisser les doubles de test accessibles dans la version matérielle normale.

**Critère de fin :** les pilotes ont un contrat clair, des tests de séquence et d'erreur, et compilent pour la cible. Leur présence logicielle n'est jamais présentée comme une mesure physique.

## P04 — Terminer amplitude, offset et calibration

**But :** produire une commande complète du signal, pas seulement forme et fréquence.

**Fichiers :** nouveau module proposé `analogique.c/.h`, profil de calibration versionné, `integration.c/.h`, `epot.c/.h`, tests associés.

### P04-A — Implémentation indépendante du banc

- [ ] **P04-01** Définir la fonction de transfert à partir de MAT01 : effet du curseur de gain, effet du curseur d'offset, centrage du DDS et éventuelles interactions.
- [ ] **P04-02** Implémenter une conversion pure consigne → commandes, avec paramètres de profil injectables, limites de codes et résultat explicite en cas de consigne non réalisable.
- [ ] **P04-03** Tenir compte de la forme d'onde ; ne pas supposer le même niveau de départ pour sinus, triangle et carré.
- [ ] **P04-04** Vérifier l'enveloppe physique avec une arithmétique suffisamment large :

```text
Vmin = offset - amplitude_crête_à_crête / 2
Vmax = offset + amplitude_crête_à_crête / 2

Application autorisée seulement si :
Vmin >= limite_basse_validée
Vmax <= limite_haute_validée
et fréquence / forme / charge dans le domaine validé du profil.
```

- [ ] **P04-05** Ne jamais corriger silencieusement une demande en la saturant. Retourner la raison d'un refus et, si utile, les limites admissibles.
- [ ] **P04-06** Distinguer résolution de saisie et résolution physique des EPOT ; exposer une estimation quantifiée seulement si le modèle la justifie, sans la qualifier de mesure.
- [ ] **P04-07** Préparer une séquence d'application : validation complète, arrêt/mute selon matériel, commandes analogiques et DDS dans l'ordre justifié par le schéma, activation explicite, mise à jour d'état en dernier.
- [ ] **P04-08** Implémenter l'import et la validation d'un profil réel. Sans profil valide, le mode normal reste `NON_CALIBRE`. Un mode de banc explicite peut permettre des commandes brutes contrôlées, sans prétendre fournir une amplitude calibrée.

### P04-B — Paramétrage et validation sur le matériel

- [ ] **P04-09** Mesurer plusieurs codes, amplitudes, offsets, fréquences et formes ; commencer par les dix presets. Documenter charge, sonde et référence de carte.
- [ ] **P04-10** Choisir le modèle le plus simple compatible avec les mesures : coefficients ou table/interpolation si nécessaire. Ne pas introduire une table complexe sans preuve de son utilité.
- [ ] **P04-11** Vérifier la bande passante de l'étage utilisant les EPOT, la tenue des amplitudes et la conservation des formes ; ne pas tenter de compenser par logiciel une limite matérielle non compensable.
- [ ] **P04-12** Définir et mesurer le comportement d'arrêt. Le RESET de l'AD9833 ne garantit ni déconnexion ni VOUT nul [D2]. Vérifier aussi la séquence d'alimentation avant que le firmware puisse agir.

**Tests logiciels :** profils invalides, limites, quantification, combinaisons amplitude/offset, forme inconnue, refus de la combinaison 20 Vpp / +10 V sur une chaîne ±15 V, échecs entre les deux EPOT et le DDS.

**Critère de fin :** moteur de conversion testé et compilation réussie ; clôture matérielle seulement après profil réel et mesures sur VOUT. Une bibliothèque de conversion seule ne suffit pas à déclarer EX04 satisfaite.

## P05 — Implémenter l'EEPROM et la persistance

**But :** conserver les configurations, détecter les données invalides et récupérer après une écriture interrompue.

**Fichiers :** `eeprom.c/.h`, nouveau `stockage.c/.h`, `signaux.c/.h`, `integration.c/.h`.

La 24LC32A possède 4096 octets, des pages d'écriture de 32 octets et un cycle d'écriture annoncé jusqu'à 5 ms [D3]. Le pilote doit traiter ces contraintes, pas les masquer par une attente bloquante arbitraire.

- [ ] **P05-01** Implémenter lectures, écritures fractionnées aux frontières de pages, contrôle des bornes et attente de fin d'écriture par interrogation avec délai maximal.
- [ ] **P05-02** Vérifier WP et l'adresse de la carte ; remonter absence, timeout, protection ou erreur de relecture.
- [ ] **P05-03** Définir une sérialisation explicite : ordre des octets, version, nombre de signaux, longueurs, compteur de génération et CRC. Ne pas écrire directement une structure C avec ses octets de remplissage.
- [ ] **P05-04** Prévoir deux emplacements alternés : conserver l'ancien valide pendant l'écriture du nouveau, relire et vérifier, puis terminer par un marqueur de validité. Protéger aussi les métadonnées utilisées pour choisir la génération.
- [ ] **P05-05** Tester une coupure à chaque étape d'écriture, y compris au marqueur final. Au démarrage, choisir le plus récent des emplacements entièrement valides, sinon fournir un état dégradé explicite.
- [ ] **P05-06** Charger le catalogue persistant au démarrage, avec sortie inactive. Si aucune donnée valide n'existe, charger les dix presets en RAM et annoncer le motif. Sur une mémoire réellement vierge et accessible, sauvegarder l'initialisation une seule fois puis la vérifier. Ne pas écraser automatiquement des données corrompues ou de version inconnue ; ne pas réécrire sans fin une EEPROM absente ou défaillante.
- [ ] **P05-07** Mettre en place la politique retenue ici : validation finale d'une modification locale ou d'une trame USB → RAM puis demande de sauvegarde. Aucun accès EEPROM à chaque cran pendant l'édition d'un brouillon.
- [ ] **P05-08** Ne répondre « sauvegardé » qu'après vérification ; conserver RAM et indicateur de modification après échec. Éviter les écritures lorsque les données sont inchangées.
- [ ] **P05-09** Effectuer la sauvegarde par étapes courtes ou tâche dédiée si nécessaire ; elle ne doit pas monopoliser les entrées UI/USB. Utiliser un instantané et une révision pour ne pas perdre les changements survenus entre-temps.

**Budget proposé, à vérifier avec le format retenu :** deux banques de 2048 octets. Avec 100 enregistrements sérialisés de 12 octets, 32 octets d'en-tête, au plus 512 octets de profil et 128 octets de réglages, une banque occuperait 1872 octets. Ce budget est une proposition d'implémentation, pas un format existant. Ajouter des assertions de taille et adapter le format si le profil réel l'exige, sans dépasser 4096 octets ni supprimer la capacité de 100 signaux.

**Critère de fin :** catalogue complet sauvegardé/relu, tests de corruption et coupure réussis sur mémoire simulée, puis conservation démontrée après redémarrage réel.

## P06 — Raccorder le protocole au transport USB et fournir un outil PC

**But :** rendre les trames du cahier effectivement utilisables depuis un ordinateur.

**Fichiers :** `usb_protocol.c/.h`, nouveau `usb_transport.c/.h`, `sdkconfig.defaults`, `src/CMakeLists.txt`, futur `tools/generateur_cli.py`.

La configuration actuelle sélectionne `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG` [R5]. Retenir le contrôleur USB Serial/JTAG de l'ESP32-C6, documenté comme un périphérique à fonctions fixes ; ne pas transposer une solution USB-OTG/TinyUSB destinée à une autre cible [D1]. Utiliser les API de la version ESP-IDF réellement fixée en P01.

- [ ] **P06-01** Garder le parseur indépendant du transport ; faire passer les mutations par le coordinateur de P02, et non directement par le catalogue.
- [ ] **P06-02** Recevoir les octets progressivement : recherche de `$`, accumulation bornée, fin sur `#`, resynchronisation sur un nouveau `$`, délai d'abandon et gestion des trames trop longues.
- [ ] **P06-03** Accepter une trame fragmentée, plusieurs trames dans une lecture et CR/LF entre trames. Rejeter les caractères et valeurs invalides sans mutation partielle.
- [ ] **P06-04** Réaliser `$N?#` et `$?#` avec les vraies réponses. Utiliser un instantané cohérent du catalogue pour une lecture totale en présence de modifications UI.
- [ ] **P06-05** Respecter N=1…100. Proposition d'ajout sans trous : modifier une entrée existante ou ajouter à `nombre + 1` tant que la capacité le permet ; rejeter un saut d'indice plutôt qu'inventer les entrées manquantes.
- [ ] **P06-06** Documenter les acquittements d'écriture et erreurs, absents du cahier : distinguer modification RAM, sauvegarde réussie et sauvegarde échouée. Ne pas confondre ces réponses avec les trames de lecture.
- [ ] **P06-07** Gérer écritures TX partielles, délais, déconnexion et PC qui ne lit plus. Une absence de client ne doit pas arrêter le générateur ni bloquer indéfiniment le menu.
- [ ] **P06-08** Organiser un propriétaire unique de RX et un chemin TX sérialisé ; interdire l'intercalage des `printf` dans les réponses. En fonctionnement normal, désactiver ou rediriger les logs applicatifs concurrents ; traiter aussi les messages de démarrage côté outil PC.
- [ ] **P06-09** Écrire un outil PC avec port paramétrable, lecture unique/totale, écriture, délais, erreurs lisibles et tests de non-régression. Documenter sa dépendance série et sa version testée.

**Tests :** requêtes nominales, indices 1/10/100, signes d'offset, champs manquants, dépassements, données non ASCII, trame incomplète, bruit, flux concaténé, fragmentation octet par octet, saturation TX et EEPROM défaillante.

**Critère de fin :** les trois commandes du cahier fonctionnent de bout en bout, les modifications validées sont persistées selon P05, et les réponses ne sont pas des substituts de diagnostic.

## P07 — Finaliser l'OLED, l'encodeur et les menus

**But :** raccorder les écrans aux fonctions effectives et rendre l'état de l'appareil compréhensible.

**Fichiers :** `menu.c`, `menu_parts/*.inc`, `oled.c/.h`, `encodeur.c/.h`.

- [ ] **P07-01** Remplacer la fragmentation arbitraire des `.inc` par un `menu.c` lisible, ou un découpage navigation/rendu si sa taille le justifie. Conserver le comportement utile et tester les parcours.
- [ ] **P07-02** Raccorder sélection, activation, arrêt, édition, duplication, suppression, chargement et sauvegarde aux API réelles.
- [ ] **P07-03** Afficher séparément signal choisi, consigne modifiée, configuration commandée, état de sauvegarde, calibration et erreur. Ne pas présenter les données du catalogue comme une mesure DDS.
- [ ] **P07-04** Prévoir des messages spécifiques : EEPROM absente, RAM non sauvegardée, profil absent, consigne hors capacité, erreur de bus et arrêt non confirmé.
- [ ] **P07-05** Ajouter les glyphes utilisés, notamment `$` et `#` ; vérifier les textes réellement visibles dans les 128×64 pixels et supprimer les troncatures ambiguës.
- [ ] **P07-06** Faire remonter les échecs OLED, conserver les zones à redessiner en cas d'échec et prévoir une reprise bornée. Ne pas laisser l'indicateur de présence figé après une panne détectée.
- [ ] **P07-07** Compter les pertes de file encodeur, tester rebonds/sens/clic/appui long et réduire les blocages liés aux rafraîchissements. Ne pas augmenter l'I²C sans vérifier les périphériques et le câblage.
- [ ] **P07-08** Rendre l'arrêt rapidement accessible depuis les écrans normaux, selon une règle UI documentée, sans confondre retour arrière et activation.

**Tests :** navigation complète, annulation de brouillon, édition d'un preset actif, changement externe par USB pendant l'édition, panne OLED, rotations rapides pendant sauvegarde, indices après suppression.

**Critère de fin :** aucun menu ne prétend exécuter une fonction non raccordée ; état réel de l'implémentation lisible ; parcours principaux testés.

## P08 — Terminer le module des sorties numériques

**But :** couvrir le schéma bloc sans inventer la fonction ni le câblage des deux sorties.

**Fichiers :** `sorties_digitales.c/.h`, `hardware_config.h`, `integration.c/.h`, menus et tests.

- [ ] **P08-01** Consigner pour chaque canal : rôle, GPIO, niveau inactif, polarité, plage de fréquence éventuelle, rapport cyclique éventuel et relation ou non avec le signal analogique.
- [ ] **P08-02** Écrire un pilote configurable avec validation des broches, initialisation déterministe, arrêt et états indépendants. Les broches ne doivent pas être choisies à partir de leur seule disponibilité apparente.
- [ ] **P08-03** Si une onde périodique est requise, choisir le périphérique matériel adapté à la fonction et à la version ESP-IDF ; pas de génération temporelle dans la boucle du menu.
- [ ] **P08-04** Raccorder les commandes et diagnostics au coordinateur, traiter les erreurs et tester les deux canaux avec un backend simulé.
- [ ] **P08-05** Valider les niveaux et la fonction à l'oscilloscope ou à l'analyseur logique une fois MAT04 résolu.

**Critère de fin :** pilote et intégration testés ; fonction physique validée sur le brochage confirmé. Si le rôle reste indéterminé, garder cette phase ouverte avec son motif : un `return false` ne clôture pas le besoin.

## P09 — Intégrer le démarrage et les comportements en défaut

**But :** transformer les modules en appareil cohérent.

**Fichiers :** `main.c`, `initialisation.c/.h`, `integration.c/.h`, configuration, diagnostics.

- [ ] **P09-01** Définir un démarrage ordonné : états d'arrêt accessibles le plus tôt possible, bus, DDS/EPOT, stockage/profil, UI/USB, puis boucle applicative. Ajuster l'ordre exact aux alimentations et à MAT01–03.
- [ ] **P09-02** Charger les données sans activer automatiquement la sortie. Une activation normale nécessite une commande explicite et tous les prérequis.
- [ ] **P09-03** Sérialiser les opérations UI et USB. Solution de départ : un propriétaire du modèle et des commandes, avec files d'événements ; ajouter seulement les tâches nécessaires au transport ou aux opérations lentes.
- [ ] **P09-04** Définir les modes dégradés : OLED absent → USB utilisable ; EEPROM absente → RAM signalée volatile ; EPOT/calibration indisponible → pas d'activation analogique normale ; sortie incertaine → défaut mémorisé.
- [ ] **P09-05** Mettre en place des délais bornés et des reprises limitées. Un retour du bus ne doit pas réactiver automatiquement une sortie après défaut.
- [ ] **P09-06** Vérifier watchdog, pile, charge des tâches et files saturées. Aucun scan I²C complet périodique dans le chemin critique du menu.
- [ ] **P09-07** Tester les interruptions de configuration : erreur après arrêt, après un EPOT, après écriture DDS, pendant sauvegarde et pendant déconnexion USB.

**Critère de fin :** aucune transition ne donne un succès injustifié ; boot, défaut et reprise ont un comportement documenté et testé.

## P10 — Constituer les preuves de fonctionnement

**But :** vérifier le logiciel continuellement, puis le produit complet.

**Fichiers :** `test/`, outils PC, workflow CI, futurs `docs/VALIDATION.md` et relevés de mesures.

- [ ] **P10-01** Exécuter à chaque phase les tests du catalogue, du protocole, de la sérialisation, des conversions et des états ; utiliser les détecteurs de débordement/mémoire disponibles sur les tests natifs.
- [ ] **P10-02** Tester toutes les positions d'échec des pilotes et du stockage. Inclure CRC faux, données tronquées, version inconnue, génération interrompue et EEPROM entièrement effacée.
- [ ] **P10-03** Compiler le firmware complet depuis un environnement propre avec les versions fixées. Archiver les commandes et résultats réellement obtenus.
- [ ] **P10-04** Exécuter les échanges USB sur carte, y compris lecture des 100 entrées, flux fragmenté et PC non lecteur.
- [ ] **P10-05** Vérifier sauvegarde, coupure réelle d'alimentation et rechargement ; vérifier que la sortie reste inactive au redémarrage.
- [ ] **P10-06** Mesurer les dix presets sur VOUT, pas seulement sur la sortie AD9833 : forme, fréquence, amplitude, offset, excursion et défauts visibles.
- [ ] **P10-07** Vérifier les transitions et le comportement OFF ; effectuer les essais sous les charges convenues, sans déduire une capacité de puissance de la seule fiche du LT1206.
- [ ] **P10-08** Faire un essai prolongé avec manipulations répétées, USB et sauvegardes. Consigner sa durée réelle, les redémarrages et les pertes d'événements.
- [ ] **P10-09** Rattacher chaque exigence EX01–EX10 à une preuve, un statut et les écarts éventuels. Fixer les tolérances avant de décider la conformité.

**Modèle de relevé matériel :**

| Preset | Commit / carte / profil | Charge et sonde | f mesurée | Vpp mesurée | Offset mesuré | Forme / transitoires | Critère et verdict |
|---|---|---|---|---|---|---|---|
| À renseigner | À renseigner | À renseigner | — | — | — | — | Non testé |

**Critère de fin :** suite logicielle et compilation réussies ; recette matérielle remplie avec résultats, pas uniquement une liste de cases cochées.

## P11 — Livrer une version documentée et maintenable

**But :** rendre le projet utilisable par un autre membre de l'équipe et présentable sans ambiguïté.

- [ ] **P11-01** Rédiger le README : cible, branche/version, outils, compilation, flash, démarrage et dépannage.
- [ ] **P11-02** Documenter brochage, adresses, schéma de stockage, protocole et réponses, parcours UI, profil de calibration et procédure de mesure.
- [ ] **P11-03** Supprimer les anciens tests d'intégration bruts de la version normale, textes obsolètes et retours provisoires. Conserver un mode de banc uniquement s'il est explicitement identifié et maîtrisé.
- [ ] **P11-04** Vérifier que les fichiers matériels de référence correspondent à la carte utilisée ; documenter les limites non résolues au lieu de les dissimuler.
- [ ] **P11-05** Préparer le diff et les commits par ensemble cohérent. Ne publier, fusionner ou créer une version distante que dans le cadre de l'action demandée ; ne pas modifier `main` implicitement.
- [ ] **P11-06** Livrer code source, fichiers de configuration, tests et journaux, documentation, profil de calibration et binaire identifié seulement si la compilation a réellement réussi.
- [ ] **P11-07** Mettre à jour le bilan final : terminé logiciellement, compilé, validé matériellement, écarts et dépendances restantes. Les preuves techniques alimentent le dossier sans fabriquer les évaluations de leadership.

**Critère de fin :** un tiers peut reconstruire et utiliser la version avec les instructions fournies ; le bilan dit exactement ce qui a été vérifié.

---

## 5. Organisation cible des fichiers

Les noms marqués `nouveau` sont des propositions. Les conserver seulement s'ils apportent une responsabilité claire ; ne pas créer une nouvelle couche pour chaque fonction.

```text
Projet-Generateur-de-fonction/
├── docs/
│   ├── PLAN_FINALISATION_PROJET.md       # ce document
│   ├── PROTOCOLE_USB.md                 # nouveau
│   ├── CALIBRATION.md                   # nouveau
│   └── VALIDATION.md                    # nouveau
├── tools/
│   ├── generateur_cli.py                # nouveau
│   └── run_native_tests.py              # nouveau, ou équivalent documenté
├── EQ1_Projet_Generateur_Fonction/
│   ├── include/
│   │   ├── hardware_config.h
│   │   ├── integration.h
│   │   ├── analogique.h                 # nouveau
│   │   ├── stockage.h                   # nouveau
│   │   ├── usb_transport.h              # nouveau
│   │   └── ...                          # interfaces existantes conservées
│   ├── src/
│   │   ├── main.c                       # démarrage / orchestration
│   │   ├── integration.c                # coordinateur et état applicatif
│   │   ├── signaux.c                    # catalogue et validation de données
│   │   ├── analogique.c                 # conversions et profil
│   │   ├── ad9833.c
│   │   ├── epot.c
│   │   ├── eeprom.c                     # accès physique à la mémoire
│   │   ├── stockage.c                   # format et transactions persistantes
│   │   ├── usb_protocol.c               # syntaxe des trames
│   │   ├── usb_transport.c              # réception / émission
│   │   ├── sorties_digitales.c
│   │   ├── i2c_commun.c
│   │   ├── initialisation.c
│   │   ├── menu.c
│   │   ├── oled.c
│   │   └── encodeur.c
│   └── test/
│       ├── native/                      # logique pure et injection d'erreurs
│       └── hardware/                    # essais exécutables sur carte
└── .github/workflows/
    └── firmware.yml                     # nouveau, compilation + tests
```

## 6. Scénarios de recette prioritaires

| ID | Scénario | Résultat attendu |
|---|---|---|
| T01 | Démarrage avec EEPROM vide | Dix presets en RAM, motif signalé, sortie inactive |
| T02 | Modification locale puis sauvegarde et redémarrage | Nouvelle valeur restaurée, sortie inactive |
| T03 | Écriture USB puis lecture unique et redémarrage | Valeurs identiques, persistance confirmée |
| T04 | `$?#` avec 100 entrées | 100 trames cohérentes, sans perte ni log intercalé |
| T05 | Modification du preset actif | Arrêt demandé, modification contrôlée, pas de réactivation implicite |
| T06 | Échec SPI/I²C au milieu d'une application | Défaut explicite ; aucune configuration complète annoncée à tort |
| T07 | Configuration hors excursion disponible | Activation refusée, aucune saturation silencieuse |
| T08 | Coupure pendant une sauvegarde | Ancienne ou nouvelle banque valide récupérée, jamais un mélange accepté |
| T09 | EEPROM protégée ou débranchée | Erreur de sauvegarde, RAM conservée et signalée volatile |
| T10 | OLED indisponible / encodeur rapide | Pas de blocage global ; pertes d'événements détectables |
| T11 | USB déconnecté ou PC qui ne lit plus | Délais bornés, UI et génération continuent selon l'état commandé |
| T12 | Dix presets sur VOUT et arrêt | Mesures conformes aux critères convenus, comportement OFF documenté |
| T13 | Deux sorties numériques | Fonction et niveaux conformes à MAT04, broches sans conflit |

## 7. Suivi d'exécution

Ce document est le support de reprise. Après chaque ensemble de modifications, renseigner une ligne avec un commit ou un diff identifiable, les commandes exécutées et les résultats. Une case n'est cochée qu'après réalisation et preuve pertinente.

| Phase | État | Commit / diff | Tests exécutés | Compilation cible | Matériel | Dépendance restante |
|---|---|---|---|---|---|---|
| P01 | À faire | — | — | Non exécutée dans ce plan | Sans objet | — |
| P02 | À faire | — | — | — | — | — |
| P03 | À faire | — | — | — | — | MAT05 |
| P04 | À faire | — | — | — | — | MAT01, MAT02, MAT03 |
| P05 | À faire | — | — | — | — | Câblage / WP |
| P06 | À faire | — | — | — | — | — |
| P07 | À faire | — | — | — | — | — |
| P08 | À faire | — | — | — | — | MAT04 |
| P09 | À faire | — | — | — | — | Selon sous-systèmes |
| P10 | À faire | — | — | — | — | MAT06 et accès au banc |
| P11 | À faire | — | — | — | — | Résultats des phases |

**États utilisables :** `À faire`, `En cours`, `Logiciel vérifié`, `Cible compilée`, `Matériel validé`, `Bloqué : motif`. Ajouter la date et le responsable réel dans le journal de travail ; ne pas inventer l'attribution des tâches ou un tournus de leadership.

### Conditions de clôture globale

- [ ] Toutes les fonctions logicielles du périmètre sont implémentées, sans succès fictif ni stub utilisé comme solution finale.
- [ ] Le firmware compile depuis l'environnement documenté et les tests automatisés passent.
- [ ] L'application des paramètres utilise une logique commune à l'UI et à l'USB.
- [ ] EEPROM, USB, interface et états de défaut sont testés de bout en bout.
- [ ] Les coefficients et limites de la chaîne analogique proviennent du matériel réel.
- [ ] Les dix presets sont validés sur VOUT et les deux sorties numériques selon leur définition confirmée.
- [ ] Le dossier de livraison décrit les preuves et toute limite résiduelle.

**Un firmware compilé avec calibration encore absente est un livrable logiciel intermédiaire, pas le générateur entièrement finalisé.**

## 8. Sources et traçabilité

### Documents fournis

- **[S1]** `Cahier des charges.pdf` : objectifs et fonctionnement page 1 ; matériel et schéma page 2 ; presets, mémoire et trames page 3 ; livrables page 4.
- **[S2]** `Schema bloc.pdf` : architecture et sorties numériques page 1 ; référence ESP32-C3/OLED 0,96 pouce divergente page 2.
- **[S3]** `Planning Projet Leadership.xlsx`, `Evaluation du Team Leader.pdf`, `Auto-évaluation Leader.pdf` : supports d'organisation du projet ; ils ne définissent pas les coefficients analogiques ni les paramètres des sorties numériques.

### Code de référence

Les constats de départ utilisent l'audit précédent du commit ci-dessous ; la présence de la branche à ce commit et la configuration USB ont été revérifiées le 29 octobre 2026. Une exécution ultérieure doit comparer toute évolution du dépôt avant d'appliquer le plan.

```text
[R1] Arbre de référence :
https://github.com/TechJosselin/Projet-Generateur-de-fonction/tree/fe47f86fe5af626a0da5a1c0aa1b16a198df4b62

[R2] Coordinateur existant :
https://github.com/TechJosselin/Projet-Generateur-de-fonction/blob/fe47f86fe5af626a0da5a1c0aa1b16a198df4b62/EQ1_Projet_Generateur_Fonction/src/integration.c

[R3] Catalogue existant :
https://github.com/TechJosselin/Projet-Generateur-de-fonction/blob/fe47f86fe5af626a0da5a1c0aa1b16a198df4b62/EQ1_Projet_Generateur_Fonction/src/signaux.c

[R4] Parseur existant :
https://github.com/TechJosselin/Projet-Generateur-de-fonction/blob/fe47f86fe5af626a0da5a1c0aa1b16a198df4b62/EQ1_Projet_Generateur_Fonction/src/usb_protocol.c

[R5] Configuration de console :
https://github.com/TechJosselin/Projet-Generateur-de-fonction/blob/fe47f86fe5af626a0da5a1c0aa1b16a198df4b62/EQ1_Projet_Generateur_Fonction/sdkconfig.defaults
```

### Documentation constructeur consultée pour ce plan

```text
[D1] Espressif — ESP32-C6, USB Serial/JTAG Controller Console :
https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/api-guides/usb-serial-jtag-console.html
À consulter ensuite dans la version correspondant à l'ESP-IDF retenu en P01.

[D2] Analog Devices — AN-1070, Programming the AD9833/AD9834 :
https://www.analog.com/en/resources/app-notes/an-1070.html
Initialisation sous RESET et programmation explicite des registres utilisés.

[D3] Microchip — 24AA32A/24LC32A, DS20001713P :
https://ww1.microchip.com/downloads/aemDocuments/documents/MPD/ProductDocuments/DataSheets/24AA32A-24LC32A-32-Kbit-I2C-Serial-EEPROM-DS20001713.pdf
Page 1 : organisation 4K × 8 bits, pages de 32 octets, cycle d'écriture.
```

Les découpages de modules, politiques de modification/sauvegarde, états, budget mémoire et scénarios de test ci-dessus sont des **décisions proposées pour l'implémentation**. Ils doivent être consignés avec leurs éventuels ajustements, et non attribués au cahier des charges.
