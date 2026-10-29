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

### Encodeur et écran : sélection, activation et consultation uniquement

L'appareil démarre directement sur la **liste des signaux**, sans activer de sortie.
L'affichage conserve trois lignes en grande police et le défilement horizontal de
la ligne pointée.

- **Tourner** : parcourir les signaux, sans modifier leurs paramètres ni la sortie active.
- **Cliquer sur un signal** : le sélectionner et l'activer immédiatement dans la même
  action, sans menu « Sortie / Arrêt » ni confirmation supplémentaire. La fiche affiche
  sa fréquence (`F`), son amplitude crête-à-crête (`A`, Vpp) et son offset (`O`).
- **Depuis la fiche** : cliquer revient à la liste sans arrêter la sortie ; tourner
  revient directement au choix suivant/précédent. Un clic applique ensuite ce choix.
- **Appui long, sur n'importe quel écran** : demander l'arrêt de toutes les sorties
  puis revenir à la liste. Un échec d'arrêt est affiché, jamais présenté comme réussi.

Aucun type, fréquence, amplitude, offset ou paramètre numérique n'est éditable depuis
l'encodeur. Il n'y a plus de duplication, suppression, restauration usine, chargement
EEPROM, effacement de profil ou réglage de pas dans le menu. Les configurations sont
modifiables **uniquement par USB**. Les commandes de diagnostic restent disponibles
par USB (`$STATE?#`, `$DIG?#`, `$CAL?#`).

Le bandeau indique le signal commandé ON, l'arrêt commandé OFF ou un défaut ; ce ne
sont pas des mesures de VOUT. L'activation conserve les contrôles de calibration, de
plage et de matériel. Un refus affiche le motif au lieu d'annoncer une sortie active.

### Configuration USB selon le cahier des charges

Écriture : `$N:TYPE:AMPLITUDE:OFFSET:FREQUENCE#`, avec `TYPE` égal à `SIN`, `TRI`
ou `CAR`, amplitude en mV crête-à-crête, offset en mV et fréquence en Hz.
Par exemple, signal 1 sinus, 1 Vpp, offset nul et 10 kHz :

```text
$1:SIN:1000:+0:10000#
```

Lire ce signal : `$1?#`. Lire tous les signaux : `$?#`. Les réponses de lecture
reprennent le format d'écriture. Les commandes étendues déjà présentes restent
inchangées ; notamment `$SELECT:N#` puis `$ON#` pour une commande de sortie depuis
le PC. Le raccourci sélection + activation concerne le clic local.

Chaque modification USB validée demande une sauvegarde différée de 300 ms.
Parcourir, sélectionner ou activer un signal ne réécrit pas ses paramètres en EEPROM.
Après un échec de sauvegarde, `$SAVE#` redemande un essai ; le firmware ne réécrit pas
en boucle une mémoire défaillante. Modifier par USB un signal actif impose d'abord
son arrêt et ne le réactive pas automatiquement : le sélectionner à nouveau pour
appliquer les nouvelles valeurs.

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
