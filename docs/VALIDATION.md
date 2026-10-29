# Bilan de finalisation — P02 à P11

Date : **29 octobre 2026**. Dépôt : `TechJosselin/Projet-Generateur-de-fonction`.
Branche : **`finalisation/p02-p11`**. Base P01 : `aae80662756e3b3eaa6b15bc1e339ff477b8e8ef`.
Code et configuration testés : **`56d240cda2831d1072b1fa5eb736435e2cfda221`**.

**Le firmware est implémenté et compilé pour l'ESP32-C6 ; les suites logicielles passent.
La conformité de l'appareil physique n'est pas attestée.** Aucun flash, aucune mesure
VOUT ni coupure réelle d'alimentation n'ont été effectués dans cette intervention.
Ce bilan décrit l'avancement courant ; les cases du plan initial et le suivi P01
restent des documents historiques, pas une déclaration d'achèvement global.

## 1. Livraison et décisions

Le commit `8ec10821169c97c2da94262f83a9fae44560c34c` introduit les services, pilotes,
interface et tests. `56d240cda2831d1072b1fa5eb736435e2cfda221` corrige la déclaration
CMake des sources pour le mode script d'analyse des dépendances ESP-IDF.
Les documents de livraison sont ajoutés ensuite sans changer ce code testé.
Aucune fusion dans `main` ou `integration/full-system` n'est effectuée.

La logique métier reste commune à l'encodeur et à l'USB. Les pilotes de production
utilisent les API ESP-IDF, pas les doubles de tests. L'ancien parseur modifiant
seul la RAM et les retours EEPROM provisoires ont été remplacés. Les six fragments
`menu_parts/*.inc` sont remplacés par `menu.c` ; le catalogue et les dix valeurs
initiales sont conservés. L'attribution du pilote AD9833 d'origine est conservée.

Le moteur analogique utilise un modèle provisoire à tables mesurées. Sans schéma
électrique détaillé, son adéquation au circuit n'est pas établie. **Aucun profil réel
n'est livré et ON retourne `NON_CALIBRE` avant import d'un profil approuvé.** Les
profils synthétiques des tests ne doivent jamais servir à contourner ce verrou.
Les sorties numériques disposent d'un pilote PWM optionnel, mais les deux GPIO
restent à `-1` : leur fonction produit et leur brochage doivent être confirmés.

## 2. Statut des phases

« Vérifié logiciellement » signifie exécution des scénarios décrits plus bas,
pas preuve exhaustive de tous les comportements ni validation des tensions réelles.

| Phase | Travail livré | Statut et réserve |
|---|---|---|
| P01 | Versions principales fixées, branche conservée, runner et CI | Réalisée dans `P01_SUIVI.md` ; socle utilisé ici. |
| P02 | Modèle, coordinateur unique, états commandés, révisions et conflits | Implémenté et vérifié logiciellement. |
| P03 | DDS avec PHASE0 et erreurs, EPOT avec relecture, bus partagé | Implémenté, testé avec doubles de bus, compilé ; câblage et signaux réels non vérifiés. |
| P04 | Conversion par forme, excursion, quantification, import/export du profil et séquence d'application | Moteur implémenté et testé ; schéma, modèle électrique réel et calibration restent ouverts. |
| P05 | Pilote 24LC32A, deux banques, CRC, écriture différée et reprise | Implémenté et testé par simulation de coupures ; mémoire physique non essayée. |
| P06 | Parseur pur, transport USB Serial/JTAG, réponses complètes, client PC | Implémenté, tests de protocole et client réussis, pilote compilé ; liaison physique non essayée. |
| P07 | Menu raccordé, brouillons, arrêt long, OLED par pages, encodeur et diagnostics | Implémenté et testé sur hôte ; ergonomie et réactivité sur carte non mesurées. |
| P08 | Deux PWM indépendants configurables, validation de GPIO, commande et diagnostics | Proposition logicielle implémentée ; définition produit, affectation et essais restent ouverts. |
| P09 | Démarrage sans activation, propriétaire unique, modes dégradés et défauts mémorisés | Intégration compilée, scénarios d'erreur simulés ; démarrage électrique, charge et watchdog sur carte non essayés. |
| P10 | Tests instrumentés, compilation cible, matrice de preuves et relevé des presets | Partie logicielle réalisée ; recette physique non exécutée. |
| P11 | README, guide, protocole, formats, client, tests et binaire CI identifié | Livraison logicielle réalisée ; dossier matériel et profil réel manquent pour livrer le produit qualifié. |

### Tâches du plan qui ne peuvent pas être déclarées closes

| Référence | Écart restant |
|---|---|
| P03-04/05/07 | Vérifier WLAT/SHDN, adresses, alimentations, tirages et brochage sur la carte. La relecture EPOT ne remplace pas ce contrôle. |
| P04-01, P04-07 et P04-09 à P04-12 | Identifier le transfert réel, approuver l'ordre des commandes, mesurer les codes et la bande passante, définir OFF et les transitoires. |
| P05-02 | Vérifier l'adresse et WP sur la vraie 24LC32A ; le pilote détecte une relecture incohérente mais ne connaît pas le câblage physique. |
| P08-01 et P08-05 | Confirmer rôle, GPIO et niveaux de chaque sortie ; mesurer les fonctions demandées. P08-03 utilise LEDC seulement pour la proposition PWM indépendante. |
| P09-01 et P09-06 | Confirmer le séquencement électrique, mesurer pile/temps de boucle/pertes et observer les watchdogs sur carte. Une pile configurée à 8192 octets n'est pas une mesure de marge. |
| P10-02 | Injection couvrant les scénarios listés, pas une preuve exhaustive de toutes les positions d'erreur possibles de chaque API ou de toute corruption physique. |
| P10-04 à P10-08 | USB réel, alimentation interrompue, dix presets sur VOUT, transitions/OFF et essai prolongé. |
| P11-04 et P11-06 | Associer schéma/PCB/BOM réels, profil mesuré, captures et binaire effectivement flashé. |

Les autres tâches logicielles des phases sont couvertes par les modules, tests et
documents ci-dessous, sous ces réserves. Aucun pourcentage de conformité physique
n'est déduit du nombre d'assertions.

## 3. Tests exécutés

Commande depuis la racine :

```sh
python tools/run_native_tests.py --sanitize
```

Exécution locale sur les sources publiées : GCC 14.2.0, C11, `-Wall -Wextra -Werror
-Wpedantic`, AddressSanitizer et UndefinedBehaviorSanitizer, échec immédiat en cas
d'anomalie. Journaux : `build/native/tests.log`, `build/native/results.json`.
Le runner compile les sources courantes, supprime l'ancien exécutable et propage
les erreurs ; il ne teste pas une copie figée du firmware.

| Suite | Résultat local | Scénarios principaux |
|---|---|---|
| `test_systeme.c` | **84 513 assertions réussies** | Dix presets exacts, catalogue 100, CRC, profil, conversion, états, persistance et USB. |
| `test_ui.c` | **81 744 assertions réussies** | Quadrature, rebonds, clic/long, horloge modulo, conflit brouillon/USB, catalogue redimensionné, navigation. |
| `test_drivers.c` | **511 assertions réussies** | Vrais fichiers pilotes compilés avec API simulées : SPI, EPOT, EEPROM, OLED, PWM. |
| `tools/test_generateur.py` | **8 tests réussis** | Écritures partielles, bruit, erreurs, confirmation de sauvegarde, listes et profils. |
| `test_codec.c` | **Réussi** | Profil de 512 octets encodé par Python puis décodé/réencodé identiquement par le C. |

Les compteurs comprennent les vérifications répétées dans les boucles. Ce ne sont
pas des milliers de fonctions indépendantes. Aucun défaut ASan/UBSan n'a été signalé
sur les chemins exécutés.

La suite stockage simule **1 846 positions de coupure après octets écrits** pour une
transaction de 100 presets et profil, puis redémarre et exige un ancien ou nouveau
catalogue entier. Elle vérifie aussi WP simulé, NACK, timeout, CRC et version inconnue.
Cette simulation n'émule pas l'analogique interne d'une EEPROM lors d'une sous-tension.

Le protocole reçoit **10 000 entrées pseudo-aléatoires** ; l'interface reçoit
**10 000 événements de navigation pseudo-aléatoires**. Les tests vérifient aussi les
trames fragmentées, la lecture de 100 signaux, les délais et le tampon TX. Le pilote
USB physique et l'ISR FreeRTOS sont compilés pour la cible, pas exécutés sur une carte
par ces tests. Le test OLED valide les transferts et la reprise simulés ; il ne
constitue pas une photographie du rendu réel.

## 4. Compilation et preuves GitHub Actions

Le conteneur local ne disposait pas d'ESP-IDF et l'accès réseau direct ne permettait
pas d'installer la chaîne. Les constructions cibles ont réellement été effectuées
par GitHub Actions, et leurs résultats ont été relus via le connecteur GitHub.

| Exécution | Commit | Natif | ESP32-C6 |
|---|---|---|---|
| [40880696772](https://github.com/TechJosselin/Projet-Generateur-de-fonction/actions/runs/40880696772) | `8ec1082` | Réussi | Échec de configuration CMake : `CONFIGURE_DEPENDS` incompatible avec le mode script ESP-IDF. |
| [40881471010](https://github.com/TechJosselin/Projet-Generateur-de-fonction/actions/runs/40881471010) | **`56d240c`** | **Réussi** | **Réussi après déclaration explicite des sources.** |

Second run : job natif **123473805822**, terminé le 29 octobre 2026 à 13:23:46 UTC ;
job cible **123473805357**, terminé à 13:26:17 UTC. Le job cible a confirmé compilation,
relevé des paquets et publication des artefacts. Cela ne prouve pas un flash.

Versions principales : Python CI 3.11.9, PlatformIO Core 6.2.0, espressif32 8.0.0,
framework-espidf 4.60100.0 / ESP-IDF 6.1.0. Voir `platformio.ini` et les journaux pour
les outils exacts. Le build n'est pas déclaré hermétique octet pour octet : image
Ubuntu et dépendances Python transitives restent soumises aux contraintes amont.
Aucun résultat « zéro avertissement global » n'est revendiqué pour les dépendances.

Artefacts du run réussi, conservés 14 jours sur GitHub :

| Élément | Référence |
|---|---|
| Firmware et preuves cible | `esp32c6-56d240cda2831d1072b1fa5eb736435e2cfda221`, ID `12901030664`, ZIP 1 820 556 octets |
| SHA-256 ZIP cible annoncé par GitHub | `468e5bd9a746801d5c12b5a2a1e636165d489cc7682ab52f26faacc6e37dba33` |
| Tests natifs et rapport | `native-56d240cda2831d1072b1fa5eb736435e2cfda221`, ID `12900796156`, ZIP 2 482 octets |
| SHA-256 ZIP natif annoncé par GitHub | `fe732b51035cc4fd2803915ed31e98c854836f6f800ae957c1925f397db3b956` |

[Artefact cible](https://github.com/TechJosselin/Projet-Generateur-de-fonction/actions/runs/40881471010/artifacts/12901030664).
Expiration annoncée : **12 novembre 2026**. Conserver les archives utiles au dossier.
Le hash ci-dessus est celui du ZIP annoncé par GitHub, pas celui de `firmware.bin`.
Ne pas prendre l'archive du premier run en échec pour un firmware utilisable.

## 5. Matrice exigences → preuves

| ID | Preuve logicielle | Statut physique |
|---|---|---|
| EX01 — dix presets | Valeurs exactes comparées dans `test_systeme.c` | VOUT des dix presets non mesuré. |
| EX02 — 100 signaux modifiables | Bornes, duplication, suppression, ajout sans trous et sérialisation testés | Manipulation sur interface réelle à vérifier. |
| EX03 — formes/fréquence DDS | Mots SPI, PHASE0, formes, valeurs non finies et échecs testés | MCLK, amplitude DDS et fréquences réelles non mesurés. |
| EX04 — amplitude/offset | Convertisseur, limites, quantification et séquence testés | Modèle électrique et calibration réelle manquants ; activation verrouillée. |
| EX05 — persistance | Pilote réel compilé, deux banques et coupures simulées testées | Coupure réelle et relecture à faire. |
| EX06 — USB | Parseur, flux, vraies listes et client testés ; transport cible compilé | Énumération et échanges sur carte à faire. |
| EX07 — OLED/encodeur | Décodage, navigation, conflits et transfert d'affichage simulés | Lisibilité, sens et latence à constater sur prototype. |
| EX08 — états cohérents | Erreurs injectées, arrêt avant mutation, révisions et données commandées | État électrique OFF et transitoires non attestés. |
| EX09 — deux sorties numériques | PWM optionnel à deux timers, erreurs et états simulés | Fonction produit et GPIO non définis : exigence non close. |
| EX10 — construction/livraison | CI réussie, sources/tests/outil PC/guide/binaire identifié | Profil réel et dossier matériel requis pour réception du produit. |

## 6. Recette à effectuer sur la carte

Ces étapes sont une procédure à exécuter, **pas un compte rendu d'essais réalisés**.
Fixer d'abord charge de référence, tolérances et critères de forme avec l'encadrant.
Ne pas imposer 50 ohms ou une tolérance arbitraire absente du cahier.

1. **Avant alimentation :** identifier révision de carte et schéma, vérifier masses,
   alimentations, connecteurs, adresses et broches de contrôle. Débrancher les charges
   sensibles ; vérifier la protection de sortie indépendamment du firmware.
2. **Démarrage :** flasher le commit identifié depuis une configuration propre.
   Contrôler RESET DDS, niveaux numériques et tension VOUT pendant boot/reset et sans
   commande ON. Un DDS arrêté ne garantit pas VOUT nul ou déconnecté.
3. **USB et interface :** lire état/presets, éditer puis confirmer la persistance,
   comparer lecture unique et totale. Essayer fragments, suites de trames, déconnexion
   et PC non lecteur. Vérifier navigation, clic/long, erreurs et compteur de pertes.
4. **Mémoire :** modifier un preset, attendre la révision sauvegardée, couper puis
   redémarrer ; vérifier données et absence d'activation. Interrompre des sauvegardes
   sur une copie de travail et vérifier le repli sans catalogue partiel. Tester WP.
5. **Analogique :** caractériser le transfert et l'ordre EPOT dans un banc maîtrisé,
   avec moyen de coupure et observation VOUT. Approuver un profil uniquement si le
   modèle additif et les limites sont vérifiés. Sinon corriger modèle/tests/format.
   Importer le profil réel et remplir `recette_presets.csv` pour les dix presets.
6. **Transitions et endurance :** observer changements de forme/fréquence/gain/offset,
   OFF, arrêt en défaut et redémarrage sous les charges convenues. Effectuer un essai
   prolongé en notant durée, resets, chauffe, pertes d'événements et erreurs de bus.
   Mesurer marge de pile, temps de boucle et comportement watchdog.
7. **Numérique et réception :** préciser les deux fonctions attendues ; adapter le
   pilote si elles ne sont pas des PWM indépendants, affecter les GPIO après revue,
   puis mesurer les deux canaux. Archiver captures, profils, source du binaire flashé,
   réglages de mesure et verdicts, y compris les écarts.

## 7. Dépendances et conclusion de livraison

MAT01–MAT03 : schéma analogique, calibration/charge et définition physique de l'arrêt.
MAT04 : rôle et câblage des deux sorties numériques. MAT05 : carte et broches réellement
montées. MAT06 : tolérances de recette. Aucune de ces données n'a été inventée.

La base logicielle est disponible pour l'intégration au banc ; la sortie analogique
normale demeure volontairement indisponible sans données de calibration approuvées.
Une sortie attendue mais hors capacités doit produire un écart documenté, pas une
réduction silencieuse du preset. La fabrication, le boîtier, le dossier matériel et
les évaluations de leadership ne sont pas réalisés par cette intervention de code.

Pour poursuivre : [README](../README.md), [guide technique](GUIDE_TECHNIQUE.md),
[plan initial](PLAN_FINALISATION_PROJET.md), [relevé de recette](recette_presets.csv).
