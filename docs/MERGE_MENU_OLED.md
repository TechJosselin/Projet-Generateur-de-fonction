# Intégration du menu OLED à trois lignes

Date : 29 octobre 2026.

Source : `feature/menu-oled-3-lignes`, commit `f781dbf89e76a63add8b533b14a050a13a06d097`.
Cible : `finalisation/p02-p11`, base `44fe3b184d93e840316b91b8d44a364f877dcb83`.
Commit de fusion et code testé : **`9a9c09f4b345cf9387e70631c3f6e3fad202664e`**.

## Résolution de la fusion

La branche source diverge depuis une ancienne base. Son remplacement intégral
réintroduirait des versions antérieures de l'intégration, de l'EEPROM, de l'USB
et du système de compilation. La résolution conserve donc la cible hors affichage.
Le commit de fusion conserve les deux parents et l'historique du collègue.

L'affichage est adapté depuis `src/oled.c` et `src/menu_parts/menu_01.inc` de la
branche source, vers le `menu.c` du firmware finalisé. Les anciens fragments
`menu_parts` et les anciennes fonctions provisoires ne sont pas réintroduits.

## Affichage repris

- Police agrandie : hauteur 14 pixels, pas horizontal 9 pixels.
- Bandeau fixe en haut, séparation à y=10, trois lignes à y=14,31,48.
- Fenêtre verticale qui avance seulement lorsque la sélection sort des trois lignes.
- Libellé sélectionné défilant horizontalement après 900 ms, au pas de 350 ms.
- Indicateur `>` pour les libellés non sélectionnés trop longs ; fenêtre de 13 caractères.
- Ligne sélectionnée inversée sur toute la largeur.

Les listes concernées sont l'accueil, les signaux, les options d'un signal,
la sortie, la mémoire, les canaux numériques et les pas de réglage.
Les écrans d'édition, de confirmation et de diagnostic gardent la petite police,
comme dans la branche source.

Pour éviter de superposer des informations à la troisième ligne, les états détaillés
sont accessibles par `ETAT / DERNIERE CMD`, `ETAT RAM / PROFIL` et `ETAT DU CANAL`.
Les confirmations, l'arrêt long, les conflits d'édition USB et les commandes
centralisées sont conservés. Le défilement visuel n'applique aucun signal.

Le pilote OLED conserve les transferts d'une page modifiée par itération, la reprise
après erreur I²C et les glyphes USB. Les nouvelles primitives bornent les coordonnées
avant tout calcul, y compris pour les valeurs extrêmes des entiers.
`platformio.ini`, notamment le correctif Ninja pour Windows, reste inchangé.

## Périmètre vérifié

La comparaison de la fusion avec `44fe3b1` ne modifie que sept fichiers :
`include/oled.h`, `src/oled.c`, `src/menu.c`, `test/native/test_ui.c`,
`test/native/test_oled_render.c`, `tools/run_native_tests.py` et ce document.
Les cinq premiers chemins sont sous `EQ1_Projet_Generateur_Fonction/`.
Aucun changement aux pilotes DDS/EPOT/EEPROM, aux services USB, au coordinateur,
au catalogue, à la configuration matérielle ou à l'environnement PlatformIO.

## Vérification effectuée

La suite `test_ui.c` conserve les scénarios précédents et ajoute les tests de fenêtre,
défilement, réinitialisation, horloge modulo, catalogue 100 → 10 → 1 et pages d'état.
La nouvelle suite `test_oled_render.c` vérifie les pixels du vrai `oled.c` à travers
un bus simulé, les limites de dessin, l'inversion et les pages transmises.
Le runner conserve les autres suites et ajoute celle de rendu.

Commande : `python tools/run_native_tests.py --sanitize` depuis la racine.

### GitHub Actions

[Exécution 40907990692](https://github.com/TechJosselin/Projet-Generateur-de-fonction/actions/runs/40907990692)
sur le commit de fusion `9a9c09f4b345cf9387e70631c3f6e3fad202664e` :

| Vérification | Résultat |
|---|---|
| Job natif `123567916772`, GCC 13.3.0, ASan et UBSan | Succès |
| Catalogue, calibration, stockage, coordination et USB | 84 513 assertions satisfaites |
| Interface, encodeur et listes à trois lignes | 204 760 assertions satisfaites |
| Rendu OLED et bornes des pixels | 33 234 assertions satisfaites |
| Pilotes avec API de bus simulées | 511 assertions satisfaites |
| Client Python | 8 tests réussis |
| Profil Python décodé et réencodé par le C | Succès |
| Job firmware ESP32-C6 `123567917049` | Compilation réussie, terminé le 29 octobre 2026 à 16:08:12 UTC |

Les compteurs comprennent des vérifications répétées ; ce ne sont pas autant de
fonctions indépendantes. Aucun défaut ASan/UBSan n'a été signalé sur les chemins exécutés.
Le test de rendu OLED a également été exécuté localement avec les sanitizers :
33 234 assertions réussies. La construction complète ESP-IDF a été faite en CI,
pas dans le conteneur local. La CI utilise Ubuntu ; aucune construction Windows
supplémentaire ni photographie d'un écran physique n'est déclarée ici.

Artefacts de cette exécution, rétention 14 jours :
- `native-9a9c09f4b345cf9387e70631c3f6e3fad202664e`, identifiant `12912122970`.
- `esp32c6-9a9c09f4b345cf9387e70631c3f6e3fad202664e`, identifiant `12913115707`.

Les documents ont été complétés après la vérification, sans changement du code testé.
Les limitations de calibration et de câblage décrites dans `VALIDATION.md` restent inchangées.
Après récupération de la branche cible, compiler et téléverser avec PlatformIO,
puis vérifier les trois lignes, le sens de l'encodeur et les libellés défilants sur l'OLED réel.
