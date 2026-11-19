# Lisibilite du firmware — Equipe 1

## Objectif
Rendre le firmware lisible pour une revue humaine, sans changer le comportement du materiel.

## Modules
- `src/main.c` : entree de l'application.
- `src/initialisation.c`, `src/integration_esp32.c` : demarrage et raccordement ESP-IDF.
- `src/integration.c` : coordination des sous-systemes et etats de fonctionnement.
- `src/signaux.c` : catalogue des signaux et parametres.
- `src/analogique.c` : calibration et calcul des commandes analogiques.
- `src/ad9833.c`, `src/epot.c` : pilotes du generateur DDS et des potentiometres.
- `src/eeprom.c`, `src/stockage.c` : persistance des reglages.
- `src/usb_protocol.c`, `src/usb_service.c`, `src/usb_transport.c` : trames, commandes et liaison serie.
- `src/menu.c`, `src/oled.c`, `src/encodeur.c` : interface locale.
- `src/sorties_digitales.c` : sorties logiques.

## Regles de lecture et de modification
1. Preferer une instruction par ligne et des blocs `if` / `else` explicites.
2. Utiliser des noms qui decrivent les grandeurs et leurs unites, sans changer arbitrairement les interfaces publiques.
3. Garder visibles les differents chemins d'erreur, en particulier en cas d'echec I2C/SPI/EEPROM.
4. Ne pas modifier sans tests la sequence d'arret du DDS, la politique de sauvegarde, les commandes analogiques et la gestion du protocole USB.
5. Commenter la raison d'une restriction ou d'un calcul, et non repeter ce que dit chaque instruction.

## Formatage reproductible
Une configuration `.clang-format` est fournie a la racine du depot.
Depuis la racine, apres installation de clang-format :

```sh
clang-format -i EQ1_Projet_Generateur_Fonction/src/*.c EQ1_Projet_Generateur_Fonction/include/*.h
```

Ce formatage additionnel doit etre effectue dans un commit distinct. Il n'a pas ete execute automatiquement lors de cette passe.

## Verification avant fusion
- Compiler le firmware avec PlatformIO et ESP-IDF pour `seeed_xiao_esp32c6`.
- Executer les tests natifs du depot / la CI GitHub.
- Comparer le comportement du menu, des sorties, de la memoire EEPROM et des trames USB avec `main`.
- Verifier sur carte les signaux et les etats d'erreur.

La refonte est une **mise en lisibilite** et ne constitue pas une validation du fonctionnement electronique ou du respect du cahier des charges.
