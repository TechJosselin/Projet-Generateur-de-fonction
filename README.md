# Générateur de fonctions — Équipe 1

Firmware C / ESP-IDF pour Seeed Studio XIAO ESP32-C6.

- [Plan de finalisation P01–P11](docs/PLAN_FINALISATION_PROJET.md)
- [Suivi d'exécution et preuves P01](docs/P01_SUIVI.md)

Le plan est conservé tel que fourni, avec ses cases initiales. Le suivi P01 indique l'avancement réellement vérifié et prévaut sur ce statut initial.

## Branche de travail

`finalisation/p01-socle` part de `integration/full-system` au commit
`fe47f86fe5af626a0da5a1c0aa1b16a198df4b62`. P01 ajoute la documentation,
les tests et la construction automatique, sans réécrire la logique métier.
`main` et les autres branches ne sont pas fusionnées automatiquement.

Pour travailler dans un nouveau répertoire sans écraser un travail local :

```sh
git clone --branch finalisation/p01-socle https://github.com/TechJosselin/Projet-Generateur-de-fonction.git Generateur-P01
cd Generateur-P01
```

Avant de changer de branche dans une copie existante, examiner `git status`
et conserver ses modifications. Aucun outil distant ne vérifie le contenu
non poussé du poste de développement.

## Tests natifs, sans carte ni PlatformIO

Pré requis : Python 3.10 ou supérieur, GCC ou Clang. La CI utilise Python
3.11.9 sur Ubuntu 24.04. Le script utilise uniquement la bibliothèque standard.

Depuis la racine du dépôt :

```sh
python tools/run_native_tests.py
python tools/run_native_tests.py --sanitize
```

La deuxième commande active AddressSanitizer et UndefinedBehaviorSanitizer
et requiert leurs bibliothèques sur l'hôte. Aucun repli silencieux n'est
fait si elles sont indisponibles. `--cc clang` permet de sélectionner Clang.

Le script compile les **sources courantes** `src/signaux.c` et
`src/usb_protocol.c`, pas des copies figées dans le dossier de tests.
Il produit `build/native/tests.log` et `build/native/results.json`, avec
empreintes SHA-256, versions, commandes et résultat. Un échec de compilation
ou d'assertion retourne un code de sortie non nul.

Le banc P01 couvre les dix presets exacts, la modification RAM, la capacité
100, la suppression, les bornes, le formatage et 23 trames invalides.
Il caractérise aussi les limites existantes. Il ne valide ni les pilotes,
ni un échange USB physique, ni les tensions sur VOUT. Ces tests devront
évoluer avec les phases fonctionnelles sans transformer les limitations
actuelles en exigences définitives.

## Construction ESP32-C6

Créer un environnement Python isolé et installer la version de PlatformIO Core
indiquée dans `requirements-build.txt` :

```sh
python -m venv .venv
. .venv/bin/activate
python -m pip install -r requirements-build.txt
python -m platformio run -d EQ1_Projet_Generateur_Fonction -e seeed_xiao_esp32c6
```

Sous PowerShell, remplacer la ligne d'activation par
`.venv\Scripts\Activate.ps1`. L'installation initiale requiert un accès
Internet pour les paquets PlatformIO, la chaîne RISC-V et ESP-IDF.

Les versions réellement résolues et l'état de validation sont consignés dans
[le suivi P01](docs/P01_SUIVI.md). Une configuration déclarée n'est pas une
preuve de compilation : consulter le journal associé au commit.

Sur un poste relié à la carte, après validation du montage :

```sh
python -m platformio run -d EQ1_Projet_Generateur_Fonction -e seeed_xiao_esp32c6 -t upload
python -m platformio device monitor -d EQ1_Projet_Generateur_Fonction -e seeed_xiao_esp32c6
```

Ces commandes ne sont pas des tests matériels déjà réalisés dans P01.

## Intégration continue

`.github/workflows/firmware.yml` contient deux jobs indépendants : tests natifs
avec analyse mémoire, puis construction du firmware dans un autre job.
Une défaillance de l'un n'est pas masquée par la réussite de l'autre.
Les actions sont référencées par SHA et le jeton du workflow est en lecture seule.

Les journaux, versions résolues et fichiers de construction disponibles sont
conservés dans les artefacts du run pendant 14 jours. Les logs sont recueillis
même après un échec. Les comptes rendus durables sont dans `docs/`.

## Limites fonctionnelles de la référence

Au départ de P01 : amplitude/offset non appliqués, stockage EEPROM non intégré,
transport USB non raccordé et sorties numériques non définies. Le menu et le
parseur existent mais ne suffisent pas à déclarer le générateur terminé.
Le comportement analogique doit être validé sur VOUT avec le schéma et une
calibration réels. Voir les phases et dépendances matérielles du plan.
