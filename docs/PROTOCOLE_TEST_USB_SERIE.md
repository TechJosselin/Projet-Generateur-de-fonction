# Protocole de test — USB série, catalogue des signaux et EEPROM

> **Projet :** Générateur de fonctions, Équipe 1 — XIAO ESP32-C6.  
> **Référence logicielle :** `main` au commit `baec48f25f4faf9eb46862ab4ad48e3c577e1cdf` (code analysé le 14/11/2026).  
> **Objectif :** obtenir des **preuves sur la carte réelle** que la lecture, l'écriture, l'ajout, la liste complète et la sauvegarde/relecture EEPROM fonctionnent selon la page 3 du *Cahier des charges.pdf*.  
> **Statut initial :** **NON EXÉCUTÉ SUR PROTOTYPE**. Cocher les essais et joindre les captures/logs au fur et à mesure.

Voir aussi [le bilan de conformité](CONFORMITE_FIRMWARE_CAHIER_DES_CHARGES.md), [le guide technique](GUIDE_TECHNIQUE.md) et les [preuves logicielles antérieures](VALIDATION.md).

## 0. Identification de l'essai

| Champ | À renseigner |
|---|---|
| Date / heure | |
| Opérateur(s) | |
| Révision PCB / numéro de carte | |
| Commit réellement flashé (`git rev-parse HEAD`) | |
| Version du firmware / environnement PlatformIO | |
| Système du PC (Windows / Linux / macOS) | |
| Port série utilisé (ex. COM7 / /dev/ttyACM0) | |
| Source d'alimentation, VBUS, masse commune | |
| EEPROM présente à l'adresse I²C 0x50 ? | |
| Condition de WP (écriture autorisée ?) | |
| Nombre initial de presets | |
| Emplacement du log série / des captures | |

**Ne jamais déclarer « OK » une ligne sans avoir réellement constaté la réponse indiquée.**

## 1. Préparation, sécurité et précautions

### Matériel / logiciel

- Carte XIAO ESP32-C6 avec le firmware du projet **et son EEPROM 24LC32A montée et correctement alimentée**.
- Câble **USB-C de données**, port série détectable sur le PC.
- Terminal série transmettant de l'ASCII brut, ou **client Python du dépôt**.
- Possibilité de couper et rétablir proprement l'alimentation pour le test de persistance.
- Oscilloscope **uniquement** si les commandes d'activation et de sortie analogique sont testées en option ; ce protocole ne qualifie pas les tensions à VOUT.

**Transport utilisé :** USB **Serial/JTAG natif** de l'ESP32-C6 (port série virtuel), pas une liaison UART TX/RX externe. Le moniteur PlatformIO est réglé à **115200**, valeur de configuration du moniteur ; ce débit n'est pas une vitesse physique du bus USB.

Avant d'envoyer des commandes :

1. Vérifier l'alimentation, le câblage I²C, la présence de l'EEPROM (adresse attendue `0x50`) et l'absence de protection d'écriture empêchant les sauvegardes.
2. Utiliser le bon port série ; **un seul programme** doit ouvrir ce port à la fois (fermer le moniteur avant le client Python).
3. Ne pas brancher de charge sensible sur VOUT. Le `RESET` du DDS **ne garantit pas une tension de sortie nulle**.
4. **Sauvegarder l'état actuel des données avant toute modification** : relever la réponse de `$STATE?#` et conserver **toutes** les trames de `$?#` dans un fichier texte. Le tableau des dix presets du cahier n'est pas nécessairement l'état actuel de l'EEPROM.
5. Le protocole principal modifie intentionnellement le **signal 3** et peut ajouter un signal. Effectuer les tests destructifs (`DELETE`, `FACTORY`, catalogue à 100) **seulement sur une carte de test ou après sauvegarde**.
6. Après une écriture, ne pas couper l'alimentation avant la confirmation de sauvegarde. La simple réception d'un accusé ne suffit pas.

### Format des trames

~~~text
$N:TYPE:AMPLITUDE_MVPP:OFFSET_MV:FREQUENCE_HZ#   (écriture)
$N?#                                           (lecture unitaire)
$?#                                            (liste)
~~~

- `TYPE` = `SIN`, `TRI` ou `CAR`.
- `N` de 1 à 100 ; les ajouts ne peuvent se faire qu'à la **prochaine position libre**.
- Amplitude : **100 à 20000 mVpp** ; offset : **−10000 à +10000 mV** ; fréquence : **1 à 12000000 Hz**.
- ASCII, sans espace dans la trame ; chaque requête commence par `$` et se termine par `#` ; requête **< 96 octets**.
- Ne pas ajouter de texte autour de la trame. Un retour CR/LF **après** `#` est toléré, pas entre les champs.
- Le firmware abandonne une trame inachevée après **1 seconde d'inactivité**.
- `$?#` renvoie les réponses **accolées**, sans séparateurs de ligne et sans trame finale.

### Utilisation du client PC fourni

Depuis la racine du dépôt (Python et dépendances de `requirements-pc.txt` installées) :

~~~sh
python -m pip install -r requirements-pc.txt
python tools/generateur.py --port COM7 state
python tools/generateur.py --port COM7 list
python tools/generateur.py --port COM7 read 3
python tools/generateur.py --port COM7 write 3 TRI 3456 1250 1234
~~~

Remplacer `COM7` par le véritable port. La commande `write` du client attend normalement la **confirmation de persistance**. L'option `--ram-only` supprime cette attente côté client, mais **n'empêche pas** l'autosauvegarde du firmware. **Ne pas lancer le client et le moniteur en parallèle.**

## 2. Comprendre les réponses avant de tester

### Acquittements et erreurs

- Modification acceptée en RAM : `$OK:RAM:R#`, où `R` est le numéro de révision (variable).
- Demande de sauvegarde acceptée : `$OK:SAVE_DEMANDE:R#`. **Ce n'est pas une sauvegarde terminée.**
- Rechargement accepté : `$OK:LOAD:R#`.
- Erreur : `$ERR:CODE#`, par exemple `$ERR:ARGUMENT#`, `$ERR:INDEX#`, `$ERR:ABSENT#`, `$ERR:IO#`, `$ERR:TIMEOUT#`.
- Lecture unitaire : une trame de type `$3:SIN:2000:+3000:1000#`.

### Format de l'état

~~~text
$STATE:ETAT:SELECTION:NOMBRE:REV_RAM:REV_SAUVEE:DIRTY:SAVING:ERR_SAVE:ERR_DERNIERE:ERR_ARRET#
~~~

| Champ | Signification / critère |
|---|---|
| `ETAT` | État du système (`NON_CALIBRE`, `PRET`, `NOMINAL_NON_CALIBRE`, etc.), dépendant du matériel |
| `SELECTION` | 0 si aucune sélection, sinon numéro de signal |
| `NOMBRE` | Nombre de signaux, de 1 à 100 |
| `REV_RAM` | Révision courante du catalogue/profil |
| `REV_SAUVEE` | Dernière révision confirmée comme persistée |
| `DIRTY` | 1 = différences RAM non confirmées en EEPROM ; 0 = aucune différence signalée |
| `SAVING` | 1 = écriture EEPROM en cours ; 0 = pas d'écriture en cours |
| `ERR_SAVE` | Doit valoir `OK` pour considérer la sauvegarde réussie |
| `ERR_DERNIERE` / `ERR_ARRET` | Diagnostics des dernières commandes / de l'arrêt |

**Critère indispensable :** pour la révision `R` écrite, attendre

~~~text
REV_RAM = R
REV_SAUVEE = R
DIRTY = 0
SAVING = 0
ERR_SAVE = OK
~~~

Puis couper/remettre l'alimentation **et relire les valeurs**. Les révisions peuvent changer entre les étapes : **ne pas comparer à un nombre d'exemple fixe**.

## 3. Protocole principal — tests à exécuter dans l'ordre

**Hypothèse des exemples :** catalogue initial de **10** signaux. Si votre appareil possède déjà d'autres valeurs ou davantage de signaux, **ne pas écraser le catalogue sans sauvegarde** : adapter les indices après consultation de `$?#`.

### SER-01 — Détection du port et interrogation de l'état

**Commande :**

~~~text
$STATE?#
~~~

**Attendu :** une trame complète commençant par `$STATE:` et terminant par `#`, présentant les 10 champs après `STATE`. Relever `NOMBRE`, `REV_RAM`, `REV_SAUVEE`, `DIRTY`, `SAVING` et `ERR_SAVE`.

- [ ] Le port USB série est reconnu par le PC.
- [ ] La requête est reçue sans erreur ni message de débogage parasite dans la réponse.
- [ ] Les champs sont présents et cohérents.

**Relevé :** état brut = ________________________________________________

### SER-02 — Lecture d'un signal

**Commande :**

~~~text
$3?#
~~~

**Attendu si l'EEPROM contient encore les valeurs d'usine :**

~~~text
$3:SIN:2000:+3000:1000#
~~~

Si le signal 3 a été personnalisé, la réponse **doit correspondre aux données sauvegardées**, pas nécessairement au tableau d'usine.

- [ ] Une unique trame complète est reçue.
- [ ] Les cinq champs sont conformes et l'offset porte un signe explicite.
- [ ] La réponse correspond au catalogue de référence relevé avant l'essai.

**Réponse réelle :** _____________________________________________________

### SER-03 — Lecture de tous les signaux

**Commande :**

~~~text
$?#
~~~

**Attendu :** exactement `NOMBRE` trames de configuration, dans l'ordre `1` à `NOMBRE`, sans manque ni duplication. La réception peut ressembler à `$1:...#$2:...#$3:...#` **sans saut de ligne**.

Pour un catalogue d'usine intact, voici les **dix trames attendues**, présentées sur des lignes distinctes uniquement pour faciliter le contrôle :

~~~text
$1:SIN:1000:+0:10000#
$2:SIN:5000:+0:1000000#
$3:SIN:2000:+3000:1000#
$4:SIN:200:+10000:500#
$5:TRI:20000:+0:5000#
$6:TRI:4000:+0:100#
$7:TRI:5000:-5000:200000#
$8:CAR:10000:+0:50#
$9:CAR:1000:+0:300#
$10:CAR:4000:+6000:1000#
~~~

- [ ] Nombre de trames égal au champ `NOMBRE` de `STATE`.
- [ ] Indices contigus, formes, amplitudes, offsets et fréquences cohérents.
- [ ] La liste est complète même si plusieurs trames arrivent dans le même paquet USB.

**Nombre reçu :** ______ / **nombre attendu :** ______

### SER-04 — Modification d'un signal par l'USB

**Avant le test :** recopier ici la trame **originale du signal 3** (issue de SER-02) pour la restauration :

~~~text
$3:__________________________________________________#
~~~

**Commande de test (volontairement différente du preset d'usine n° 3) :**

~~~text
$3:TRI:3456:+1250:1234#
~~~

**Attendu :** `$OK:RAM:R#` si la modification est acceptée. Elle est d'abord appliquée au **catalogue RAM**.

**Contrôle immédiat :**

~~~text
$3?#
~~~

**Attendu :**

~~~text
$3:TRI:3456:+1250:1234#
~~~

- [ ] Acquittement `OK:RAM` reçu avec une révision.
- [ ] La lecture correspond **exactement** aux quatre paramètres envoyés.
- [ ] Aucun message d'erreur n'apparaît.

**Révision après écriture (`R`) :** __________________

### SER-05 — Validation d'une vraie sauvegarde EEPROM

**Commande :**

~~~text
$SAVE#
~~~

**Attendu :** `$OK:SAVE_DEMANDE:R#` — **demande**, pas confirmation définitive.

**Puis interroger périodiquement :**

~~~text
$STATE?#
~~~

**Attendu en fin d'écriture :** `REV_RAM = REV_SAUVEE = R`, `DIRTY=0`, `SAVING=0`, `ERR_SAVE=OK`.

La temporisation initiale est d'environ **300 ms sans modification**, **plus** la durée des écritures et vérifications EEPROM ; ne pas conclure à un échec juste après 300 ms. Un délai pratique de surveillance de plusieurs secondes est approprié (le client Python prévoit une attente de confirmation).

- [ ] `SAVE_DEMANDE` reçu.
- [ ] `SAVING` n'est plus actif.
- [ ] Les deux numéros de révision correspondent.
- [ ] `DIRTY=0` et `ERR_SAVE=OK`.

**Dernier `STATE` obtenu :** ______________________________________________

**Si la sauvegarde ne se confirme pas :** conserver les données en RAM, noter l'erreur, contrôler EEPROM/I²C/WP/alimentation ; **ne pas redémarrer en pensant que les données sont persistantes**.

### SER-06 — Persistance après coupure d'alimentation

**Prérequis :** SER-05 validé.

1. Couper l'alimentation de la carte **après** la fin de la sauvegarde.
2. Attendre l'extinction complète du montage.
3. Remettre l'alimentation et rouvrir le port série (son nom peut avoir changé).
4. Transmettre :

~~~text
$3?#
~~~

**Attendu :**

~~~text
$3:TRI:3456:+1250:1234#
~~~

5. Interroger aussi `$STATE?#` puis `$?#` pour confirmer que la liste est cohérente.

- [ ] Les valeurs modifiées sont identiques après redémarrage.
- [ ] Le nombre de signaux reste inchangé.
- [ ] Aucun retour intempestif aux valeurs d'usine n'a été observé.

**C'est cet essai qui prouve la relecture effective de l'EEPROM**, au-delà de la seule présence du code de stockage.

### SER-07 — Ajout d'un nouveau signal

**Prérequis :** connaître le nombre actuel `NOMBRE`. Le test ci-dessous suppose **10** signaux déjà présents. Si ce n'est pas le cas, utiliser l'indice **NOMBRE + 1** (uniquement s'il est ≤ 100) et noter la nouvelle trame.

**Commande :**

~~~text
$11:CAR:1200:-500:250#
~~~

**Attendu :** `$OK:RAM:R#`, puis la lecture

~~~text
$11?#
~~~

doit répondre

~~~text
$11:CAR:1200:-500:250#
~~~

**Contrôles :** `$STATE?#` doit annoncer `NOMBRE=11`, et `$?#` doit renvoyer exactement 11 trames.

**Persistance :** refaire SER-05 puis redémarrer et vérifier la présence du signal n° 11 dans `$11?#` et dans la liste complète.

- [ ] Ajout accepté à la prochaine position libre.
- [ ] Lecture du signal ajouté correcte.
- [ ] `NOMBRE` et `$?#` cohérents.
- [ ] Le signal ajouté survit au redémarrage.

**Indice réellement utilisé :** ______ / **réponse :** _____________________

### SER-08 — Rechargement explicite de l'EEPROM

**Commande :**

~~~text
$LOAD#
~~~

**Attendu :** `$OK:LOAD:R#` si une image EEPROM valide est disponible. Contrôler ensuite `$3?#`, `$STATE?#` et `$?#`.

**Remarque :** l'autosauvegarde est active ; `LOAD` ne doit **pas** être présenté comme une annulation fiable d'une modification déjà envoyée. Ne pas réaliser ce test en supposant que des changements non sauvegardés attendront indéfiniment en RAM.

- [ ] `LOAD` est accepté.
- [ ] Les signaux rechargés correspondent à ceux précédemment sauvegardés.

### SER-09 — Commandes invalides et robustesse du parseur

Effectuer ces essais **sans changer la configuration** :

| Trame | Réponse attendue | Test |
|---|---|---|
| `$0?#` | `$ERR:ARGUMENT#` : indice hors limites | [ ] |
| `$101?#` | `$ERR:ARGUMENT#` : indice hors limites | [ ] |
| `$1:XYZ:1000:+0:1000#` | `$ERR:ARGUMENT#` : forme inconnue | [ ] |
| `$1:SIN:0:+0:1000#` | `$ERR:ARGUMENT#` : amplitude trop faible | [ ] |
| `$1:SIN:1000:+10001:1000#` | `$ERR:ARGUMENT#` : offset trop élevé | [ ] |
| `$1:SIN:1000:+0:12000001#` | `$ERR:ARGUMENT#` : fréquence trop élevée | [ ] |
| `$1:SIN:1000:+0:0#` | `$ERR:ARGUMENT#` : fréquence nulle | [ ] |

**En plus, si moins de 100 presets existent :** envoyer `$100?#` ; le numéro est syntaxiquement valide mais non créé, donc la réponse attendue est `$ERR:INDEX#`.

**Reprise :** après les erreurs, envoyer `$STATE?#` puis `$3?#`. Les réponses doivent encore être valides et les configurations ne doivent pas avoir été altérées.

- [ ] Les erreurs sont refusées proprement.
- [ ] Aucun signal n'est modifié par une trame invalide.
- [ ] Le dialogue USB reste opérationnel après les erreurs.

## 4. Essais complémentaires (optionnels et encadrés)

| ID | Essai | Attendu / précaution |
|---|---|---|
| **SER-10** | `$SELECT:3#`, puis `$ON#` et `$OFF#` | Acquittements et états cohérents. **Ne faire que si le montage analogique est sûr** ; `ON_COMMANDE` ne certifie pas VOUT. |
| **SER-11** | `$DUP:3#` puis suppression du dernier par `$DELETE:N#` | Nombre augmente puis diminue. Sauvegarder et vérifier les numéros ; operation modifiant l'EEPROM. |
| **SER-12** | Catalogue à **100** entrées sur une mémoire de test | Ajouts contigus jusqu'à 100, `$100?#` correct, `$?#` contient 100 trames, sauvegarde et redémarrage validés. **Ne pas faire sur une EEPROM contenant des données à conserver sans sauvegarde préalable**. |
| **SER-13** | Trame `$1?` sans `#`, puis 1 s d'inactivité | `$ERR:TIMEOUT#` et reprise sur une nouvelle trame valide. |
| **SER-14** | Vérification des deux sorties `$DIG?#` | Le firmware doit indiquer leur disponibilité réelle ; des canaux sans GPIO attribué ne doivent pas être annoncés comme fonctionnels. |
| **SER-15** | Test d'échec EEPROM (WP, NACK, coupure contrôlée) | **Uniquement avec protocole matériel séparé** ; l'EEPROM précédente doit rester récupérable selon le scénario. Ne jamais débrancher I²C sous tension sans analyse du risque. |

**Ne pas lancer `$FACTORY#` comme un simple test innocent.** Cette commande restaure les dix presets initiaux **et** déclenche l'autosauvegarde ; elle peut remplacer définitivement le catalogue personnel.

## 5. Restauration des données après essai

Après SER-07 ou tout essai modifiant le catalogue :

1. Si un signal a été **ajouté en dernière position** pendant l'essai, le retirer par `$DELETE:N#`, avec `N` égal à son **numéro actuel**. Ne pas supprimer une entrée qui existait avant l'essai.
2. Réécrire la **trame originale du signal 3** enregistrée à SER-04. **Ne pas supposer qu'elle était identique aux valeurs d'usine.**
3. Relire `$?#` et comparer **trame par trame** au fichier de sauvegarde initial.
4. Envoyer `$SAVE#` ; attendre la confirmation dans `$STATE?#`.
5. Redémarrer et vérifier une dernière fois `$?#`.

- [ ] Liste finale identique au catalogue initial sauvegardé.
- [ ] Sauvegarde finale confirmée par les indicateurs.
- [ ] Liste toujours identique après redémarrage.

**Si le catalogue initial contenait déjà 100 entrées**, ne pas tenter SER-07. **Si vous avez supprimé ou réordonné des entrées existantes**, reconstruire l'intégralité du catalogue depuis la sauvegarde avant de déclarer la restauration réussie.

## 6. Fiche de résultats à renseigner

Légende : **OK** = observé et conforme ; **KO** = observé et non conforme ; **NT** = non testé. Remplacer les tirets par les résultats et joindre un log brut, de préférence horodaté.

| Test | Résultat OK / KO / NT | Réponse réellement reçue / fichier de preuve | Remarque ou anomalie |
|---|---|---|---|
| SER-01 — port / état | NT | | |
| SER-02 — lecture unitaire | NT | | |
| SER-03 — liste complète | NT | | |
| SER-04 — écriture RAM | NT | | |
| SER-05 — validation EEPROM | NT | | |
| SER-06 — redémarrage / persistance | NT | | |
| SER-07 — ajout d'un signal | NT | | |
| SER-08 — chargement EEPROM | NT | | |
| SER-09 — erreurs / robustesse | NT | | |
| SER-10 à SER-15 — optionnels | NT | | |
| Restauration du catalogue initial | NT | | |

**Anomalie n° :** ______  
**Étape / trame :** _____________________________________________________  
**Attendu :** __________________________________________________________  
**Observé :** __________________________________________________________  
**Hypothèse technique :** ______________________________________________  
**Action corrective / commit / date du nouveau test :** __________________

## 7. Critères de validation et conclusion

| Fonction du cahier des charges | Critère minimal | Validation |
|---|---|---|
| **Écriture via USB** | SER-04 : accusé `OK:RAM` et lecture identique | [ ] |
| **Lecture unitaire** | SER-02 : réponse correspondant à la configuration | [ ] |
| **Lecture complète** | SER-03 : exactement toutes les trames, sans perte | [ ] |
| **Ajout jusqu'à 100** | SER-07 pour l'ajout ; SER-12 si réception matérielle des 100 requise | [ ] |
| **Sauvegarde EEPROM** | SER-05 : révision sauvée, `DIRTY=0`, `SAVING=0`, sans erreur | [ ] |
| **Relecture après redémarrage** | SER-06 : les données persistantes correspondent aux commandes | [ ] |
| **Rechargement manuel** | SER-08 : `LOAD` et contenu lus corrects | [ ] |
| **Traitement des erreurs** | SER-09 : rejet correct et dialogue intact | [ ] |

**Conclusion de l'opérateur :**

- Liaison USB série : **NON VALIDÉE / VALIDÉE / ÉCHEC** (rayer les mentions inutiles).
- Gestion du catalogue : **NON VALIDÉE / VALIDÉE / ÉCHEC**.
- Sauvegarde et relecture EEPROM : **NON VALIDÉES / VALIDÉES / ÉCHEC**.
- Conformité des tensions/fréquences sur VOUT : **HORS PÉRIMÈTRE DE CE PROTOCOLE**.

Signature(s), date et référence du log : __________________________________

## 8. Dépannage rapide

| Symptôme | Vérifications prioritaires |
|---|---|
| Aucun port COM / ttyACM | Câble USB-C **de données**, alimentation, port et pilotes, firmware effectivement flashé |
| Aucun retour à `$STATE?#` | Un seul programme connecté, bonne trame `$...#`, console ESP-IDF désactivée sur le canal protocole, bon commit |
| `$ERR:ARGUMENT#` | Nombre de séparateurs, forme `SIN/TRI/CAR`, unités en **mV/mVpp/Hz**, limites et absence d'espaces |
| `$ERR:INDEX#` | Numéro dans 1..100 mais absent du catalogue ; vérifier `NOMBRE` |
| `$ERR:ABSENT#` à `SAVE` | Vérifier détection EEPROM, alimentation, bus I²C, adresse configurée 0x50 |
| `ERR_SAVE=IO / TIMEOUT / VERIFICATION` | Examiner I²C, WP, câblage, temporisations d'écriture, alimentation et mesures |
| `$?#` semble illisible | Les trames arrivent **collées** ; les séparer par les séquences `$` ... `#`, pas par des retours à la ligne |
| Lecture après reboot différente | Ne pas conclure trop vite : vérifier que la sauvegarde avait **réellement** fini, puis EEPROM et format |
| `$ON#` retourne OK mais sortie incorrecte | Commande non équivalente à une mesure ; vérifier mode `STATE`, calibration, EPOT, AD9833 et LT1206 |

**Fichiers de référence :** [`usb_protocol.c`](../EQ1_Projet_Generateur_Fonction/src/usb_protocol.c), [`usb_service.c`](../EQ1_Projet_Generateur_Fonction/src/usb_service.c), [`usb_transport.c`](../EQ1_Projet_Generateur_Fonction/src/usb_transport.c), [`integration.c`](../EQ1_Projet_Generateur_Fonction/src/integration.c), [`stockage.c`](../EQ1_Projet_Generateur_Fonction/src/stockage.c), [`eeprom.c`](../EQ1_Projet_Generateur_Fonction/src/eeprom.c), [`tools/generateur.py`](../tools/generateur.py).
