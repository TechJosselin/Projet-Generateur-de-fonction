# P01 — Suivi d'exécution

Date : 29 octobre 2026. Branche de travail : `finalisation/p01-socle`.
Base conservée : `fe47f86fe5af626a0da5a1c0aa1b16a198df4b62` (`integration/full-system`).

**Statut : P01 clôturée pour le socle logiciel du dépôt distant.** Tests natifs
vérifiés et firmware complet compilé, puis reconstruit avec les versions fixées.
**Aucun essai sur carte ni validation de VOUT.** Prochaine phase : P02.
Ce suivi complète le [plan initial](PLAN_FINALISATION_PROJET.md), conservé à l'identique ;
il prévaut sur les cases et statuts initiaux de P01 dans ce plan.

## État des tâches

| Tâche | État | Preuve / limite |
|---|---|---|
| P01-01 | Réalisé pour le dépôt distant | Huit branches initiales inventoriées ; comparaison des sept autres avec la base. Le travail non poussé sur le poste utilisateur n'est pas accessible et n'a pas été modifié. |
| P01-02 | Réalisé | Branche dédiée créée depuis le SHA de référence ; aucune fusion ni écrasement de `main`. |
| P01-03 | Réalisé | Compilation complète de la configuration existante réussie dans le run 40871626330, sans modification des sources fonctionnelles. |
| P01-04 | Réalisé | Versions de PlatformIO Core, plateforme, ESP-IDF et outils fixées à partir du premier build ; reconstruction propre réussie dans le run 40872618039. Limites de reproductibilité précisées ci-dessous. |
| P01-05 | Réalisé | Archive examinée, quatre sources comparées aux blobs GitHub, banc intégré et relancé ; 338 assertions satisfaites localement et en CI. |
| P01-06 | Réalisé | Runner natif, commandes documentées et workflow CI opérationnels ; deux jobs réussis sur la configuration fixée, journaux archivés. |

## Référence et branches préservées

Comparaisons effectuées avec `base=fe47f86...` et chaque branche comme `head`.
Les nombres indiquent les commits propres à chaque côté, pas une mesure de qualité.
Une divergence peut contenir du code déjà repris autrement : ne pas la fusionner aveuglément.

| Branche | SHA au début de P01 | Commits propres à la branche / propres à la base | Décision |
|---|---|---|---|
| `main` | `99550a4a8dd6d096b59487c2bb647be57c8f5d27` | 0 / 59 | Ancêtre ; ne pas repartir de cette version. |
| `feature/ad9833-signal-integration` | `816dda7b9e8fdc1b134f0f990c2f740717a7d183` | 0 / 80 | Ancêtre conservé. |
| `feature/epot-driver-integration` | `1f3cd5f7a406d9800358c667b3ef6db6f8027eef` | 0 / 74 | Ancêtre conservé. |
| `feature/analog-chain-epot` | `88b31b0e5db5bebab67c149c8272e88145985f44` | 4 / 90 | Divergence : examiner les changements EPOT/main en P03–P04, sans fusion globale. |
| `feature/encoder-live-control` | `70fe11b8c3890e6bb8da48622114ee9332120ed7` | 1 / 59 | Divergence incluant un ancien composant OLED, des fichiers générés et un autre chemin de commande. Ne pas réintroduire en bloc. |
| `feature/epot-signal-integration` | `2b72c7b15234a9c0eef8c2888342fe84f70c1013` | 6 / 80 | Divergence EPOT/signaux/configuration/main ; revue ciblée P03–P04. |
| `feature/epot-single-test` | `86a21e545516989c57c6b67be89cdf32a4d815f4` | 1 / 80 | Banc EPOT ancien ; conserver comme référence, pas comme programme principal. |
| `integration/full-system` | `fe47f86fe5af626a0da5a1c0aa1b16a198df4b62` | Référence | Base retenue pour préserver l'interface intégrée. |

## Plan ajouté sans perte

Le fichier `docs/PLAN_FINALISATION_PROJET.md` est identique au fichier fourni :
43229 octets ; blob Git `652cfd68cc354c8dd06ef0876a9e1dabe9cd939b`.
Commit d'ajout : `b4b521332f797d322646d0520e32527dee43899b`.
Les cases du plan initial ne constituent pas le suivi courant ; consulter ce document.

## Fichiers ajoutés ou modifiés

| Fichier | Rôle |
|---|---|
| `docs/PLAN_FINALISATION_PROJET.md` | Plan fourni, ajouté intégralement. |
| `docs/P01_SUIVI.md` | Décisions, avancement et preuves de P01. |
| `README.md` | Branche, installation, tests, compilation, flash et limites. |
| `EQ1_Projet_Generateur_Fonction/test/native/test_baseline.c` | Banc de référence et contrôle des dix presets exacts. |
| `tools/run_native_tests.py` | Compilation des sources courantes, exécution et rapports. |
| `requirements-build.txt` | Version de PlatformIO Core. |
| `EQ1_Projet_Generateur_Fonction/platformio.ini` | Versions testées de la plateforme, du framework et des outils. |
| `.github/workflows/firmware.yml` | Tests natifs et construction ESP32-C6 dans deux jobs indépendants. |
| `.gitignore` | Exclusion des environnements et résultats locaux générés. |

Les sources fonctionnelles dans `src/` et `include/` sont inchangées par P01.
Les limitations d'amplitude/offset, EEPROM, transport USB et sorties numériques
restent à traiter dans les phases prévues ; cette étape ne les déclare pas résolues.

## Tests natifs

Le banc de l'archive `Tests_revue_generateur_fe47f86.zip` a été inspecté.
Aucune copie des sources du firmware n'a été ajoutée dans le dossier de tests.
Le runner compile les fichiers du projet courant.

Les quatre fichiers de l'archive utilisés pour l'exécution locale ont été
vérifiés par calcul de leur blob Git, identique à celui de la base distante :

| Fichier | Blob Git |
|---|---|
| `include/signaux.h` | `6e8cbec9d6f84659e23622fa14697fb771f7916e` |
| `include/usb_protocol.h` | `3f564f7132c3293eedf19a328a224153bd25f0e5` |
| `src/signaux.c` | `c2a594e8acc04e4873b5082513b689e55153d438` |
| `src/usb_protocol.c` | `1ba524a645dee33e494c316951beb4400abff966` |

Le banc initial est complété par 22 assertions : comparaison exacte des dix
presets et contrôle des accès hors catalogue. Exécution nouvelle : **338
assertions satisfaites**, dont 23 trames invalides rejetées sans mutation.

Commande depuis la racine :

```sh
python tools/run_native_tests.py --sanitize
```

- Exécution locale : GCC 14.2.0, C11, `-Wall -Wextra -Werror -Wpedantic`, AddressSanitizer et UndefinedBehaviorSanitizer ; résultat PASS.
- Exécution CI : GCC 13.3.0, mêmes options, Python 3.11.9 ; résultat PASS sur les deux runs.
- Premier job natif : `123439857628`, run `40871626330`, commit `f0f505fdeca08b39118ac890d10d198571806a20`.
- Second job natif : `123443280310`, run `40872618039`, commit `90e48b05648087ea7bc188e9765a5fa6d8be89fb`.
- Vérifications locales du runner : compilateur absent, assertion volontairement fausse et erreur de compilation retournent 1 ; une erreur de compilation ne réutilise pas l'ancien binaire. Exécution réussie dans un répertoire comportant des espaces.

Les scénarios négatifs du runner ont été exécutés dans des copies temporaires,
pas dans les sources publiées. Les 338 assertions ne signifient pas 338 fonctions
indépendantes ni validation du transport USB, de l'EEPROM ou de l'analogique.
Les limitations constatées dans les messages `CONSTAT` restent ouvertes pour les phases suivantes.

## Compilation cible et versions réellement vérifiées

Le conteneur de travail ne disposait ni de PlatformIO ni de la chaîne ESP-IDF ;
la tentative de clone direct a échoué avec `Could not resolve host: github.com`.
Les modifications du dépôt ont donc été publiées par le connecteur GitHub, et les
constructions complètes exécutées par GitHub Actions, pas dans le conteneur local.

Le premier run utilise le `platformio.ini` d'origine, non épinglé. Il constate
la résolution actuelle ; ce n'est pas une preuve de la version anciennement
installée sur le poste de l'équipe. Les versions observées ont ensuite été fixées,
puis reconstruites sur un nouvel environnement CI sans cache de compilation.

| Exécution | Commit testé | Tests natifs | Firmware ESP32-C6 |
|---|---|---|---|
| [Référence — 40871626330](https://github.com/TechJosselin/Projet-Generateur-de-fonction/actions/runs/40871626330) | `f0f505fdeca08b39118ac890d10d198571806a20` | Succès | Succès |
| [Versions fixées — 40872618039](https://github.com/TechJosselin/Projet-Generateur-de-fonction/actions/runs/40872618039) | `90e48b05648087ea7bc188e9765a5fa6d8be89fb` | Succès | Succès |

Le second job firmware `123443281603` a terminé le 29 octobre 2026 à 12:24:30 UTC.
Le binaire `firmware.bin` a été généré. Aucune correction des sources fonctionnelles
n'a été nécessaire pour obtenir ces deux constructions.

| Élément | Version testée / fixée |
|---|---|
| Python CI | `3.11.9` |
| PlatformIO Core | `6.2.0` |
| Plateforme `platformio/espressif32` | `8.0.0` |
| Paquet `platformio/framework-espidf` | `4.60100.0`, correspondant à ESP-IDF `6.1.0` |
| `platformio/toolchain-riscv32-esp` | `15.2.0+20251204` |
| `platformio/toolchain-esp32ulp` | `1.23800.240113` |
| CMake / Ninja | `3.30.2` / `1.7.1` |
| `platformio/tool-esptool` | `2.50400.0` (esptool `5.4.0`) |
| `platformio/tool-esp-rom-elfs` | `0.0.1+20241011` |
| GDB RISC-V et Xtensa | `12.1.0+20221002` |

Les actions checkout, setup-python et upload-artifact sont référencées par SHA.
Le paquet SCons résolu était `4.41101.0`. Les dépendances Python transitives et
l'image de l'hôte Ubuntu restent gérées par leurs contraintes amont : cette base
fixe les versions principales et a été reconstruite avec succès, mais n'est pas
une construction hermétique garantie identique octet pour octet à toute date future.
Les journaux enregistrent les versions résolues pour permettre leur comparaison.

### Taille rapportée par la compilation fixée

| Mesure | Valeur |
|---|---|
| RAM statique rapportée | 15 268 / 327 680 octets — 4,7 % |
| Flash de la partition applicative | 243 806 / 1 048 576 octets — 23,3 % |

Ces valeurs ne mesurent pas la consommation de tas ou de pile pendant l'exécution.
Le dénominateur Flash est la partition applicative, pas la totalité des 4 Mo de la carte.

### Journaux, artefacts et avertissements

Le workflow archive commandes, versions Python/PlatformIO, `pip freeze`, paquets
résolus, journaux de compilation et fichiers de construction disponibles.
Les artefacts expirent après 14 jours ; conserver ceux nécessaires au dossier.
Une compilation en erreur fait échouer le job, même si l'archivage réussit.

Artefact du second build :

- Nom : `esp32c6-90e48b05648087ea7bc188e9765a5fa6d8be89fb`.
- Identifiant : `12897395239` ; archive ZIP de 1 787 048 octets.
- SHA-256 de l'archive annoncé par GitHub Actions : `782f3bdc6ee9a6bafdedfe51a49986197c5e396767fd192a457cdd2723aa38d5`.
- [Ouvrir l'artefact](https://github.com/TechJosselin/Projet-Generateur-de-fonction/actions/runs/40872618039/artifacts/12897395239).

Des avertissements non bloquants restent présents : valeurs par défaut booléennes
Kconfig dans ESP-IDF (`BT_NIMBLE_MESH_PROVISIONER`, `FATFS_PRINT_LLI`,
`FATFS_PRINT_FLOAT`) et dépréciations de l'environnement des actions Node.
Ils n'ont pas provoqué d'échec. Cette phase n'affirme pas une compilation sans avertissement.

## Commandes de reprise

Depuis la racine, après installation des prérequis décrits dans le README :

```sh
python tools/run_native_tests.py --sanitize
python -m pip install -r requirements-build.txt
python -m platformio run -d EQ1_Projet_Generateur_Fonction -e seeed_xiao_esp32c6
```

Le compte rendu final est un changement documentaire après le commit testé
`90e48b05648087ea7bc188e9765a5fa6d8be89fb`. Il ne modifie ni sources, ni tests,
ni workflow, ni configuration de compilation ; les preuves ci-dessus restent
rattachées explicitement au commit effectivement construit.

## Passage à P02

La prochaine étape est d'unifier les mutations UI/USB et de distinguer catalogue,
sélection, configuration commandée et états de sortie. P02 n'est pas implémentée
par cette clôture. Les données de calibration, le schéma analogique et le rôle des
sorties numériques restent à vérifier dans leurs phases respectives.

Aucun schéma matériel, coefficient de calibration, brochage numérique ni comportement
fonctionnel de P02–P11 n'a été inventé ou modifié dans P01. Aucune carte n'a été flashée
ou mesurée dans le cadre de cette exécution.
