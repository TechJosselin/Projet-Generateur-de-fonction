# P01 — Suivi d'exécution

Date : 29 octobre 2026. Branche de travail : `finalisation/p01-socle`.
Base conservée : `fe47f86fe5af626a0da5a1c0aa1b16a198df4b62` (`integration/full-system`).

**Statut : en cours.** Les tests natifs sont vérifiés ; la compilation cible
et le verrouillage de la plateforme restent à confirmer. Aucune validation matérielle.
Ce suivi complète le [plan initial](PLAN_FINALISATION_PROJET.md), conservé à l'identique.

## État des tâches

| Tâche | État | Preuve / limite |
|---|---|---|
| P01-01 | Réalisé pour le dépôt distant | Huit branches inventoriées ; comparaison des sept autres avec la base. Le travail non poussé sur le poste utilisateur n'est pas accessible et n'a pas été modifié. |
| P01-02 | Réalisé | Branche dédiée créée depuis le SHA de référence ; aucune fusion ni écrasement de `main`. |
| P01-03 | En cours | Compilation de la configuration existante lancée dans le run CI 40871626330. |
| P01-04 | Partiel | Python 3.11.9 et PlatformIO Core 6.2.0 fixés en CI ; plateforme espressif32 à fixer après constat des paquets réellement résolus. |
| P01-05 | Réalisé | Archive examinée, quatre sources comparées aux blobs GitHub, banc intégré et relancé ; 338 assertions satisfaites localement et en CI. |
| P01-06 | Partiel | Runner natif et CI installés ; actions épinglées par SHA. Fin du verrouillage et résultat cible encore attendus. |

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

- Exécution locale : GCC 14.2.0, C11, `-Wall -Wextra -Werror -Wpedantic`, ASan et UBSan ; résultat PASS.
- Exécution CI : GCC 13.3.0, mêmes options, Python 3.11.9 ; résultat PASS.
- Job natif : `123439857628`, run `40871626330`, commit `f0f505fdeca08b39118ac890d10d198571806a20`.
- Vérifications locales du runner : compilateur absent, assertion volontairement fausse et erreur de compilation retournent 1 ; une erreur de compilation ne réutilise pas l'ancien binaire. Exécution réussie dans un répertoire comportant des espaces.

Les scénarios négatifs du runner ont été exécutés dans des copies temporaires,
pas dans les sources publiées. Les 338 assertions ne signifient pas 338 fonctions
indépendantes ni validation du transport USB, de l'EEPROM ou de l'analogique.
Les limitations constatées dans les messages `CONSTAT` restent ouvertes pour les phases suivantes.

## Compilation cible et traçabilité

Le conteneur de travail ne dispose ni de PlatformIO ni de la chaîne ESP-IDF ;
la tentative de clone direct a échoué avec `Could not resolve host: github.com`.
Les modifications du dépôt passent donc par le connecteur GitHub, et la
construction complète par GitHub Actions, sans simuler un succès local.

Le premier run utilise volontairement le `platformio.ini` d'origine, non épinglé.
Il permet de constater la résolution actuelle ; ce n'est pas une preuve de la
version anciennement installée sur le poste de l'équipe.

[Run de référence](https://github.com/TechJosselin/Projet-Generateur-de-fonction/actions/runs/40871626330)

Le workflow archive commandes, versions Python/PlatformIO, `pip freeze`,
paquets résolus, journal de compilation et fichiers de construction disponibles.
Les artefacts expirent après 14 jours ; conserver les rapports utiles pour le dossier.
Une étape de compilation en erreur fait échouer le job, même si l'archivage réussit.

## Travail restant pour clôturer P01

1. Lire le résultat et le journal de compilation ESP32-C6 ; corriger seulement les problèmes nécessaires au build.
2. Fixer les versions de plateforme et paquets correspondant à une construction réussie, puis reconstruire depuis un environnement propre.
3. Consigner les versions, le commit testé, les éventuels avertissements et le lien de preuve ; clôturer P01 seulement après réussite.

Aucun schéma matériel, coefficient de calibration, brochage numérique ni comportement
fonctionnel de P02–P11 n'a été inventé ou modifié dans cette étape.
