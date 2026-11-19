# Lisibilite du firmware — Equipe 1

## Objectif

Le firmware du generateur de fonctions est presente suivant l'exemple
`AD_Timer0_N.c` : sections identifiables, commentaires de fonctions,
indentation stable et conditions clairement visibles.

Cette refonte a pour but la **lecture humaine du C**, sans modification volontaire
des exigences electriques ni du protocole fonctionnel.

## Perimetre realise

- Les **20 modules `src/*.c`** sont restructures (accolades sur leur ligne, indentation de quatre espaces, instructions aerées).
- Les expressions conditionnelles `?:` des modules C ont ete converties en `if / else` explicites.
- Les principaux points d'entree et les fonctions critiques sont accompagnes d'un descriptif.
- Les en-tetes `include/*.h` compactes sont presentes sur plusieurs lignes.
- `include/initialisation.h` a ete conserve : ses commentaires et sa structure etaient deja conformes au style souhaite.
- `include/hardware_config.h` est volontairement conserve intact : un changement non commite a ete detecte dans la copie de travail locale.

## Architecture du code

| Module | Responsabilite |
| --- | --- |
| `main.c`, `initialisation.c`, `integration_esp32.c` | Demarrage ESP32-C6 et raccordement ESP-IDF |
| `integration.c` | Coordination du systeme et des sequences de securite |
| `signaux.c` | Catalogue des dix signaux initiaux et edition en RAM |
| `analogique.c` | Calibration et calcul gain / offset |
| `ad9833.c`, `epot.c` | DDS et potentiometres numeriques |
| `eeprom.c`, `stockage.c` | Ecriture, lecture et verification de la persistance |
| `usb_protocol.c`, `usb_service.c`, `usb_transport.c` | Dialogue USB serie |
| `menu.c`, `oled.c`, `encodeur.c`, `entree.c` | Interface utilisateur |
| `sorties_digitales.c` | Sorties PWM de l'ESP32 |

## Conventions de presentation

1. Une instruction par ligne, avec indentation de quatre espaces.
2. Les blocs `if`, `else`, `for` et `while` portent explicitement des accolades.
3. Les accolades des fonctions et des blocs commencent sur une ligne distincte (style Allman).
4. Les fonctions principales comportent un bandeau de commentaire indiquant leur role.
5. Les commentaires expliquent en priorite les choix electroniques, limites, temporisations et effets de bord.
6. Les noms publics et les constantes du protocole sont conserves pour ne pas casser les dependances.

La racine du depot contient un fichier `.clang-format` correspondant a cette convention.
Pour uniformiser ulterieurement la presentation des nouveaux changements,
utiliser `clang-format` dans un **commit distinct**, puis faire une revue des differences.

Les scripts temporaires utilises pour la migration ne sont pas conserves dans le depot :
ils ne font pas partie du produit. Le formatage ulterieur se fait avec `.clang-format`,
qui constitue la convention de style maintenue.

## Tests et outils du PC

Les fichiers de test et de maintenance ont egalement ete repris :

- `test/native/test_*.c` : scenarios unitaires C, commentaires de test, structures explicites et suppression des expressions ternaires.
- `test/native/fakes/**/*.h` : definitions et prototypes des pilotes simules pour les tests natifs. Ces broches ne decrivent pas le PCB.
- `tools/generateur.py` : client USB, commandes et gestion des calibrations.
- `tools/test_generateur.py` et `tools/run_native_tests.py` : tests PC et execution automatisee des suites C.
- `CMakeLists.txt`, `src/CMakeLists.txt` et `sdkconfig.defaults` : configuration commentee et regroupee par responsabilite.
- `.github/workflows/firmware.yml` : ajout des tests Python au controle automatise existant.

Les documents Markdown, fichiers de donnees et verrous de versions ont leur propre format ; ils n'ont pas a adopter l'indentation du C.

## Verification avant fusion

- Tests unitaires natifs et controles ASan / UBSan sur GitHub Actions.
- Compilation ESP32-C6 avec PlatformIO / ESP-IDF.
- Essais sur carte : demarrage, arret DDS, selection des profils, EEPROM, EPOT, ecran et commandes USB.
- Comparaison fonctionnelle avec la branche `main` sur le materiel reel.

Pull request de revue : https://github.com/TechJosselin/Projet-Generateur-de-fonction/pull/2

**Important :** une compilation ou des tests natifs reussis ne valident pas a eux seuls
les amplitudes, les offsets, la calibration analogique ou les comportements sur carte.
