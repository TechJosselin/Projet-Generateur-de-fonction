# Modification d'affichage OLED - 29 octobre 2026

Projet repris directement depuis `EQ1_Projet_Generateur_Fonction-20261029T123238Z-1-001.zip`.

## Changements

- En accueil, bandeau `GENERATEUR SIGNAL` fixe en haut.
- Tous les menus de selection (accueil, signaux, options, sortie DDS, memoire, USB, reglages) montrent **exactement 3 elements au maximum**.
- Titres de menu en police agrandie (~9x14 px contre 6x8 px auparavant), avec ligne active en contraste inverse.
- La rotation de l'encodeur continue a selectionner les entrees de facon identique. La fenetre glisse uniquement quand le curseur sort des trois lignes visibles.
- Les intitules longs sont abreges visuellement avec `>` pour les lignes non selectionnees, puis **defilent horizontalement** quand ils sont selectionnes (apres 0.9 s).
- Les ecrans d'edition, de diagnostic et d'information conservent leur affichage dense original pour ne pas supprimer d'informations utiles.
- Aucune modification du pilotage analogique, EEPROM, SPI ni de la logique d'activation de la sortie.

## Utilisation

1. Ouvrir le dossier `GenerateurSignal_Menu_3_Lignes` dans **VS Code / PlatformIO**.
2. Choisir l'environnement `seeed_xiao_esp32c6`.
3. Compiler puis flasher le XIAO ESP32-C6.
4. Rotation : parcourir les choix ; clic court : valider ; clic long : revenir en arriere.

Le dossier `.pio` de l'archive originale n'est pas inclus : il contient des fichiers temporaires generes lors de la compilation. Les dossiers `src`, `include` et la configuration PlatformIO sont conserves.

**Limite :** l'affichage a ete modifie et verifie statiquement, mais le rendu definitif sur ton OLED SH1106 doit encore etre valide sur carte.
