# Générateur de fonctions — CFPT Équipe 1

Firmware C / ESP-IDF pour **Seeed Studio XIAO ESP32-C6**. Branche de finalisation :
`finalisation/p02-p11`, issue de `finalisation/p01-socle` au commit `aae8066`.
Les branches `main` et `integration/full-system` ne sont pas remplacées.

## État de livraison

Catalogue, commande coordonnée, conversion par profil, stockage EEPROM à deux
banques, USB, menus, pilotes et tests sont implémentés. Le bilan des preuves et
les tâches matérielles ouvertes sont dans [VALIDATION.md](docs/VALIDATION.md).

**Aucun profil analogique réel n'est livré. L'activation normale est donc refusée
avec `NON_CALIBRE` tant qu'un profil mesuré n'a pas été importé.** Les deux sorties
numériques restent non affectées (`-1`) jusqu'à confirmation de leur fonction et
de leur câblage. Ce n'est pas une version certifiée du produit physique.

Un arrêt du DDS ne garantit **ni VOUT = 0 V, ni une sortie déconnectée**. Ne pas
brancher une charge sensible avant vérification de l'étage analogique et du boot.

## Reconstruction

Utiliser Python 3.11.9 (version CI), Git et GCC pour les tests natifs. Depuis la
racine du dépôt, avec un répertoire de travail propre ou les modifications locales
préalablement sauvegardées :

```sh
git fetch origin
git switch finalisation/p02-p11
python -m venv .venv
```

Activer `.venv` : `source .venv/bin/activate` sous Linux/macOS ou
`.venv\Scripts\Activate.ps1` sous PowerShell. Ensuite :

```sh
python -m pip install -r requirements-build.txt -r requirements-pc.txt
python tools/run_native_tests.py --sanitize
python -m platformio run -d EQ1_Projet_Generateur_Fonction -e seeed_xiao_esp32c6
```

Sous un hôte sans ASan/UBSan, lancer le runner sans `--sanitize` ; cela n'équivaut
pas au contrôle instrumenté exécuté en CI. Les versions principales sont fixées :
PlatformIO Core 6.2.0, plateforme espressif32 8.0.0, ESP-IDF 6.1.0. Les dépendances
Python transitives ne rendent pas le build totalement hermétique ; consulter les
journaux de résolution. Aucune installation PlatformIO globale n'est nécessaire.

**Migration depuis P01 :** la console USB a été désactivée au profit du protocole.
Un ancien `sdkconfig` / `sdkconfig.seeed_xiao_esp32c6` généré localement peut
supplanter `sdkconfig.defaults`. Conserver les éventuelles personnalisations puis
repartir d'une copie propre de cette branche, ou supprimer seulement les fichiers
sdkconfig générés et `.pio` dans cette copie. Ne pas supprimer `sdkconfig.defaults`.
La configuration finale doit avoir `CONFIG_ESP_CONSOLE_NONE=y`, pas de console
secondaire et une pile de tâche principale de 8192 octets.

Flash, après vérification du câblage et avec VOUT sans charge sensible :

```sh
python -m platformio run -d EQ1_Projet_Generateur_Fonction -e seeed_xiao_esp32c6 -t upload
```

Les binaires, journaux et résultats de tests sont également produits par le workflow
**Firmware and native tests**. Ne pas flasher un ancien artefact sur la seule base
de son nom : vérifier le SHA du commit associé. Les artefacts CI expirent après 14 jours.

## Utilisation

L'encodeur navigue, le clic valide et l'appui long demande l'arrêt de toutes les
sorties puis revient en arrière. Sélectionner un signal **ne l'active pas**.
L'activation se fait dans **SORTIE / ARRET**, après confirmation. Modifier un
signal actif impose d'abord son arrêt et ne le réactive pas automatiquement.
L'aperçu est théorique, pas une acquisition de VOUT.

Chaque modification validée demande une sauvegarde différée de 300 ms. Les crans
dans un brouillon ne provoquent pas d'écriture EEPROM. `RAM*` indique des données
non encore confirmées en mémoire. Après un échec, `SAUVER MAINTENANT` redemande un
essai ; le firmware ne réécrit pas en boucle une mémoire défaillante.

Fermer le moniteur série avant d'utiliser le client PC ; remplacer COM7 par le port
réel (`/dev/ttyACM0` sous Linux, par exemple) :

```sh
python tools/generateur.py --port COM7 state
python tools/generateur.py --port COM7 list
python tools/generateur.py --port COM7 read 3
python tools/generateur.py --port COM7 write 3 SIN 2000 3000 1000
python tools/generateur.py --port COM7 off
```

`write` attend la confirmation de sauvegarde, sauf `--ram-only`, qui supprime
seulement cette attente côté PC : l'autosauvegarde du firmware reste active.
L'acquittement `OK:RAM` n'est pas une preuve de persistance.

## Documents et organisation

- [Guide technique](docs/GUIDE_TECHNIQUE.md) : architecture, brochage, stockage,
  protocole exact, calibration, commandes et dépannage.
- [Validation et phases](docs/VALIDATION.md) : preuves, matrice EX01–EX10, écarts,
  procédures et statut des phases P01–P11.
- [Relevé des dix presets](docs/recette_presets.csv) : valeurs demandées et champs
  de mesure volontairement vides.
- [Plan initial](docs/PLAN_FINALISATION_PROJET.md) et [preuve P01](docs/P01_SUIVI.md) :
  historique conservé ; les anciens statuts initiaux ne décrivent pas l'avancement actuel.

Le dossier `test/native/fakes/` contient uniquement les doubles utilisés sur PC.
Ils ne sont pas inclus dans la construction matérielle. Les anciens fragments
`menu_parts/*.inc` ont été remplacés par un menu unique, raccordé au coordinateur.
