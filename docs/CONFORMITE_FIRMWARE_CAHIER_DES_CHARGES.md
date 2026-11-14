# Conformité du firmware au cahier des charges — Équipe 1

> **Référence d'audit :** branche `main`, code au commit `baec48f25f4faf9eb46862ab4ad48e3c577e1cdf`, examiné le **14 novembre 2026**.  
> **Objet :** confronter les exigences du *Cahier des charges.pdf* (notamment pages 1 à 3), les *Consignes pour les présentations orales des projets en équipe* et l'implémentation C/ESP-IDF du dépôt.  
> **Périmètre :** revue statique du code, et prise en compte des rapports de tests logiciels déjà présents ; **aucune mesure sur prototype ni essai USB/EEPROM réel n'a été effectué dans cet audit**. Les modifications sur une autre branche ne sont pas couvertes.

## 1. Verdict synthétique

**Les fonctions logicielles fondamentales existent :** catalogue de 10 signaux prédéfinis extensible à 100, sélection par encodeur, consultation OLED, écriture et lecture par le port USB série, stockage en EEPROM 24LC32A et rechargement au démarrage.

**La conformité du produit complet n'est pas encore démontrée :** les tensions de sortie (amplitude, offset), la forme et la fréquence des 10 signaux doivent être mesurées ; la persistance doit être prouvée sur l'EEPROM physique après redémarrage.

Trois niveaux de preuve sont à distinguer :

- **Implémenté :** un chemin d'exécution existe dans le code.
- **Testé logiciel :** des essais sur PC ou simulations sont documentés ; ils ne garantissent pas le comportement électrique.
- **Validé sur carte :** essai matériel tracé avec résultats/mesures. **Aucune validation de ce niveau n'est attestée ici.**

### Matrice des exigences

| Exigence / source | Où la fonction est réalisée | Statut |
|---|---|---|
| Générer sinus, triangle et carré — cahier p. 1 et 3 | `signaux.c`, `ad9833.c`, `integration.c` | Implémenté ; VOUT non mesurée |
| Sélectionner un signal parmi 10 presets — cahier p. 1 et 3 | `signaux.c`, `menu.c`, `encodeur.c` | Implémenté ; ergonomie réelle à vérifier |
| Modifier fréquence et amplitude — cahier p. 1 | Commandes USB et conversion analogique | Paramètres modifiables ; valeurs physiques non garanties |
| Modifier l'offset — tableau p. 3 | Commandes USB, `analogique.c`, EPOT | Paramètre modifiable ; sortie à calibrer |
| Conserver/modifier les paramètres en mémoire — cahier p. 3 | `integration.c`, `stockage.c`, `eeprom.c` | Implémenté ; EEPROM physique à tester |
| Écriture série au format imposé — cahier p. 3 | `usb_protocol.c`, `usb_service.c`, `usb_transport.c` | Implémenté ; USB réel à vérifier |
| Lecture d'un signal `$N?#` — cahier p. 3 | `usb_protocol.c`, `usb_service.c` | Implémenté ; USB réel à vérifier |
| Lecture complète `$?#` — cahier p. 3 | `usb_service.c` | Implémenté ; USB réel à vérifier |
| Jusqu'à 100 configurations — cahier p. 3 | `SIGNAL_MAX=100` ; format EEPROM dimensionné | Implémenté et prévu en tests PC ; essai sur carte à faire |
| OLED : liste et détails — schéma bloc et consignes orales | `menu.c`, `oled.c` | Implémenté ; affichage physique à vérifier |
| Sorties numériques 1/2 — schéma bloc p. 2 | `sorties_digitales.c` | Pilote présent ; affectation des GPIO non validée par défaut |
| Signal DC — mentionné dans les consignes orales, non listé parmi les dix presets du cahier | Aucune forme `DC` dans `signaux.h` | **Écart / exigence à clarifier** |
| Valeurs analogiques imposées **sur VOUT** — tableau p. 3 | Chaîne AD9833 → EPOT → sommateur → LT1206 | **Mesures et calibration à réaliser** |

Le Wi-Fi et le Bluetooth sont représentés dans le schéma de principe de l'ESP32, mais aucun protocole Wi-Fi/Bluetooth n'est explicitement imposé pour la configuration des signaux dans le texte du cahier. Ne pas les présenter comme des fonctions de commande réalisées sans preuve supplémentaire.

## 2. Les dix presets demandés

Les valeurs du tableau `usine[]` dans [`signaux.c`](../EQ1_Projet_Generateur_Fonction/src/signaux.c) correspondent au tableau de la **page 3** du cahier. Les amplitudes y sont stockées en **mV crête-à-crête** et les offsets en **mV**.

| N° | Forme | Vpp | Offset | Fréquence |
|---:|---|---:|---:|---:|
| 1 | SIN | 1 V | 0 V | 10 kHz |
| 2 | SIN | 5 V | 0 V | 1 MHz |
| 3 | SIN | 2 V | +3 V | 1 kHz |
| 4 | SIN | 0,2 V | +10 V | 500 Hz |
| 5 | TRI | 20 V | 0 V | 5 kHz |
| 6 | TRI | 4 V | 0 V | 100 Hz |
| 7 | TRI | 5 V | −5 V | 200 kHz |
| 8 | CAR | 10 V | 0 V | 50 Hz |
| 9 | CAR | 1 V | 0 V | 300 Hz |
| 10 | CAR | 4 V | +6 V | 1 kHz |

**Important :** ce tableau décrit les **consignes**. Il ne constitue pas une preuve que la sortie BNC présente ces valeurs. Les dix cas doivent être mesurés à l'oscilloscope, en précisant la charge, l'alimentation et les tolérances retenues.

## 3. Architecture logicielle et circulation des commandes

~~~mermaid
flowchart TD
    PC["Ordinateur / terminal série"] <--> T["usb_transport.c<br/>USB Serial/JTAG natif"]
    T <--> S["usb_service.c<br/>réception / réponses"]
    S --> P["usb_protocol.c<br/>analyse des trames"]
    P --> I["integration.c<br/>état et coordination"]
    ENC["Encodeur"] --> M["menu.c"]
    M --> I
    M --> OLED["Écran OLED"]
    I <--> RAM["signaux.c<br/>catalogue actif en RAM"]
    I <--> ST["stockage.c<br/>2 banques / CRC"]
    ST <--> EE["eeprom.c<br/>EEPROM 24LC32A via I²C"]
    I --> HW["integration_esp32.c"]
    HW --> DDS["AD9833 / SPI"]
    HW --> EPOT["MCP45HV51 gain + offset / I²C"]
    HW --> DIG["Sorties digitales optionnelles"]
~~~

La boucle [`main.c`](../EQ1_Projet_Generateur_Fonction/src/main.c) traite les événements de l'encodeur, les octets USB, la progression des sauvegardes, le menu et l'écran. Le catalogue possède **un seul propriétaire logique** : les commandes USB et locales passent par `integration.c`, ce qui réduit les risques de modifications concurrentes.

Le menu actuel est **en lecture/commande uniquement** : rotation pour parcourir ; clic pour sélectionner et activer ; appui long pour arrêter. Les modifications de forme, fréquence, amplitude et offset se font **par USB**, et non avec l'encodeur.

## 4. L'USB série : ce qui est réellement présent

Le XIAO ESP32-C6 utilise ici **l'interface USB Serial/JTAG native**, exposée comme port série virtuel à l'ordinateur. Ce n'est **pas** un périphérique UART externe relié à deux broches TX/RX. Pour un terminal PC, l'usage reste un échange série de trames.

Les fichiers [`usb_transport.c`](../EQ1_Projet_Generateur_Fonction/src/usb_transport.c), [`usb_service.c`](../EQ1_Projet_Generateur_Fonction/src/usb_service.c) et [`usb_protocol.c`](../EQ1_Projet_Generateur_Fonction/src/usb_protocol.c) séparent le transport, l'exécution des commandes et l'analyse syntaxique.

### Format conforme au cahier

~~~text
Écriture :         $N:TYPE:AMPLITUDE_MVPP:OFFSET_MV:FREQUENCE_HZ#
Lecture unitaire : $N?#
Lecture totale :   $?#

Exemple : $3:SIN:2000:+3000:1000#
~~~

- `N` : numéro de signal **1 à 100**.
- `TYPE` : `SIN`, `TRI` ou `CAR`.
- Amplitude : de **100 à 20 000 mVpp**.
- Offset : de **−10 000 à +10 000 mV**.
- Fréquence : de **1 à 12 000 000 Hz**.
- Requête ASCII, sans espaces internes, commençant par `$` et terminant par `#` ; longueur strictement inférieure à **96 octets**.
- Une réponse de lecture utilise la même structure et **affiche toujours le signe de l'offset**, même nul : `+0`.
- `$?#` renvoie **une suite de trames accolées**, dans l'ordre, **sans retour à la ligne ni marqueur de fin supplémentaire**.

Les limites ci-dessus sont des **limites d'acceptation des configurations par le firmware**, pas une certification des plages réellement réalisables à la BNC.

### Commandes du firmware

| Requête | Effet / réponse |
|---|---|
| `$1?#` | Retourne la configuration du signal 1 |
| `$?#` | Retourne toutes les configurations existantes |
| `$3:SIN:2000:+3000:1000#` | Modifie le signal 3 **en RAM** ; répond `$OK:RAM:REVISION#` |
| `$11:CAR:1200:-500:250#` | Ajoute le 11e signal **si exactement 10 existent** |
| `$STATE?#` | État, nombre de signaux, révisions et statut de sauvegarde |
| `$SAVE#` | **Demande** une sauvegarde EEPROM, sans garantir son achèvement |
| `$LOAD#` | Recharge les données EEPROM valides dans la RAM |
| `$SELECT:3#` | Sélectionne le signal 3 sans l'activer |
| `$ON#` / `$OFF#` | Commande l'activation / l'arrêt |
| `$DUP:3#` | Duplique le signal 3 à la fin de la liste |
| `$DELETE:3#` | Supprime le signal 3 ; au moins un signal doit rester |
| `$FACTORY#` | Restaure les dix signaux d'usine **en RAM**, puis planifie leur sauvegarde |
| `$DIG?#` / `$DIG:1:1000:500#` | Diagnostics / commande PWM optionnelle |
| `$CAL?#` et `$CAL:...#` | Export/import/gestion d'un profil de calibration |

Les commandes de diagnostic et de calibration sont des **extensions du projet**, et non des formats imposés par la page 3 du cahier.

Les indices doivent rester **contigus** : avec 10 entrées, on peut ajouter la n° 11, mais pas directement la n° 15. Un numéro syntaxiquement valide mais absent retourne une erreur `INDEX`. Le tableau d'usine n'est pas recréé à chaque demande de lecture.

## 5. EEPROM : valeurs d'usine, lecture au démarrage et sauvegarde

### 5.1 Réponse à la question « Les signaux sont-ils codés en dur ? »

**Oui pour les valeurs de secours ; non pour le catalogue persistant utilisé en fonctionnement normal.**

[`signaux.c`](../EQ1_Projet_Generateur_Fonction/src/signaux.c) contient un tableau constant `usine[]` de **10 presets initiaux**, nécessaire pour fournir un appareil utilisable avec une EEPROM neuve ou illisible. Ce tableau n'est pas la seule source de données.

Le processus réel est le suivant :

1. Au démarrage, `signaux_initialiser()` initialise les dix presets d'usine **dans la RAM**.
2. `integration_initialiser_avec()` appelle `stockage_charger()` **si l'EEPROM a été détectée**.
3. Si une image valide est trouvée, `signaux_remplacer_ensemble()` **remplace tout le catalogue RAM** par les valeurs enregistrées, avec le profil de calibration.
4. Si les banques sont vierges, les dix presets d'usine restent utilisables et une première sauvegarde est demandée automatiquement.
5. Si le stockage est corrompu ou de version inconnue, le firmware conserve les valeurs d'usine en RAM **sans effacer automatiquement l'EEPROM suspecte**.
6. Les lectures USB et la sélection par menu travaillent ensuite dans **le catalogue RAM actif**, pas dans la constante `usine[]`.

**Il n'y a donc pas de lecture EEPROM à chaque changement de signal.** Le firmware lit l'EEPROM au lancement (ou sur `$LOAD#`), puis travaille en RAM et sauvegarde les modifications.

### 5.2 Format persistant

- Composant : **24LC32A**, 4096 octets, bus I²C, adresse configurée **0x50**.
- Organisation : **2 banques** de **2048 octets**.
- Par banque : en-tête **32 octets** + **13 octets/signal** + image de calibration **512 octets**.
- Occupation maximale : `32 + 100 × 13 + 512 = 1844 octets`, donc une banque de 2048 octets suffit.
- Protection : signature `GFN2`, version, CRC32, compteur de génération et marqueur final de validité.
- Écriture par pages de **32 octets**, avec contrôle de disponibilité, relecture et basculement vers la nouvelle banque validée.

La procédure vise à limiter les pertes lors d'une interruption de sauvegarde. **Elle ne remplace pas un essai de coupure d'alimentation sur la carte réelle.**

### 5.3 Quand une modification est-elle réellement sauvegardée ?

1. Une trame d'écriture valide appelle `integration_modifier()` et modifie **la RAM**.
2. L'accusé `$OK:RAM:REVISION#` confirme cette modification en RAM **uniquement**.
3. Une sauvegarde automatique est demandée après environ **300 ms sans nouvelle modification** ; `$SAVE#` permet d'en demander explicitement une.
4. `integration_periodique()` fait avancer les écritures en EEPROM de manière non bloquante ; une sauvegarde peut durer plus de 300 ms.
5. La sauvegarde est confirmée uniquement après la validation et la relecture de l'image.

Le message `$OK:SAVE_DEMANDE:REVISION#` **n'est pas une preuve de persistance**.

Consulter `$STATE?#`, dont les champs sont :

~~~text
$STATE:ETAT:SELECTION:NOMBRE:REV_RAM:REV_SAUVEE:DIRTY:SAVING:ERR_SAVE:ERR_DERNIERE:ERR_ARRET#
~~~

Pour une sauvegarde de la révision visée `R`, exiger simultanément :

- `REV_RAM = REV_SAUVEE = R` ;
- `DIRTY = 0` ;
- `SAVING = 0` ;
- `ERR_SAVE = OK` ;
- puis **relire le signal après un vrai redémarrage**.

Après une erreur de sauvegarde, le firmware ne réessaie pas indéfiniment : une nouvelle modification ou `$SAVE#` demande un nouvel essai. Avec une EEPROM absente, la RAM peut fonctionner mais ne conserve pas les changements après extinction.

**Attention :** `$FACTORY#` est **destructif pour le catalogue personnalisé**, car le retour d'usine en RAM déclenche aussi l'autosauvegarde.

## 6. Ce qui reste à qualifier sur le prototype

### 6.1 Chaîne analogique et VOUT

Le firmware connaît les consignes de gain et d'offset, mais **une commande EPOT n'est pas une mesure de la sortie**.

- Avec un profil de calibration réel approuvé : `analogique_convertir()` calcule les codes selon les tables mesurées et refuse certaines consignes hors plage.
- **Sans profil, si les deux EPOT sont disponibles :** `integration_activer()` appelle actuellement `analogique_convertir_nominal()` ; le système applique des codes **estimés** selon des hypothèses de 600 mVpp en entrée, gain nominal de 50 et rails ±15 V. L'affichage préfixe ces estimations par `~`. Ce **n'est pas** une calibration réelle.
- Sans profil **et** sans EPOT disponibles : le mode `DDS_SEUL_COMMANDE` ne règle pas amplitude et offset ; il peut encore commander forme et fréquence du DDS.
- `$ON#` peut répondre `OK:ON_COMMANDE` sans prouver que **VOUT** est correct.
- Le `RESET` de l'AD9833 n'est **pas** une coupure galvanique de la BNC ni une garantie de tension nulle sur VOUT.
- La charge, la bande passante, les saturations, les rails, la réponse en fréquence et l'étage LT1206 doivent être mesurés.

**Point documentaire à maintenir cohérent :** certains passages de [`GUIDE_TECHNIQUE.md`](GUIDE_TECHNIQUE.md) décrivent encore le repli « DDS seul » comme seul comportement sans profil. Le code de `main` inclut aussi **le pilotage EPOT nominal** si ces composants sont présents. En cas de désaccord, tester et documenter le **comportement du commit effectivement flashé**.

### 6.2 Autres limites

- Le type **DC** n'existe pas dans l'énumération `SIGNAL_SINUS / SIGNAL_TRIANGLE / SIGNAL_CARRE`. Les consignes orales demandent de l'évoquer : demander à l'enseignant si un mode DC doit effectivement être ajouté.
- Les sorties digitales sont prévues dans le logiciel, mais les GPIO de production sont actuellement non affectés par défaut : ne pas les déclarer fonctionnelles sans choix de brochage et essai.
- Les tests unitaires, les simulations de coupures et les rapports historiques figurent dans [`VALIDATION.md`](VALIDATION.md). Ils ne remplacent pas une capture série sur la vraie carte, ni un essai d'EEPROM et d'oscilloscope.
- Aucun résultat « fonctionne sur carte » ne doit être affirmé sans le compte rendu et les mesures correspondants.

## 7. Plan de clôture recommandé

| Priorité | Action | Preuve de réussite |
|---|---|---|
| **P0** | Exécuter le [protocole USB série et EEPROM](PROTOCOLE_TEST_USB_SERIE.md) | Échanges bruts enregistrés ; lecture après redémarrage identique |
| **P0** | Vérifier la détection I²C de la 24LC32A à 0x50 et la sauvegarde | `STATE` cohérent, aucun défaut EEPROM, lecture persistante |
| **P0** | Mesurer les 10 signaux imposés sur VOUT | Tableau consigne / fréquence / Vpp / Vmid / forme / charge / statut |
| **P0** | Calibrer réellement amplitude et offset sur le montage actuel | Profil de mesure traçable et écarts validés |
| **P1** | Valider l'encodeur et l'OLED sur la carte | Sélection, activation, arrêt et erreurs visibles |
| **P1** | Tester le catalogue jusqu'à 100 sur banc dédié | 100 configurations lisibles et persistantes |
| **P1** | Clarifier le cas DC et les sorties numériques | Exigence confirmée, décision documentée |
| **P2** | Aligner les anciens guides sur le firmware flashé | Aucun document contradictoire dans le dossier final |

## 8. Sources et points d'entrée du code

**Documents pédagogiques examinés :**

- *Cahier des charges.pdf* : p. 1 (objectif fonctionnel), p. 2 (schéma bloc et composants), p. 3 (dix signaux, EEPROM et trames USB), p. 4 (rendus).
- *Présentation orale projet en equipe.pdf* : p. 1 (fonctions clients, liaison série en lecture/écriture, sauvegarde mémoire, mention DC et tableau de bilan).

**Code :**

- [`signaux.c`](../EQ1_Projet_Generateur_Fonction/src/signaux.c) : catalogue et dix valeurs d'usine.
- [`integration.c`](../EQ1_Projet_Generateur_Fonction/src/integration.c) : orchestration, chargement, sauvegarde, activation.
- [`stockage.c`](../EQ1_Projet_Generateur_Fonction/src/stockage.c) et [`eeprom.c`](../EQ1_Projet_Generateur_Fonction/src/eeprom.c) : format persistant et pilote physique.
- [`usb_protocol.c`](../EQ1_Projet_Generateur_Fonction/src/usb_protocol.c), [`usb_service.c`](../EQ1_Projet_Generateur_Fonction/src/usb_service.c) et [`usb_transport.c`](../EQ1_Projet_Generateur_Fonction/src/usb_transport.c) : trames, commandes et USB.
- [`menu.c`](../EQ1_Projet_Generateur_Fonction/src/menu.c), [`analogique.c`](../EQ1_Projet_Generateur_Fonction/src/analogique.c), [`ad9833.c`](../EQ1_Projet_Generateur_Fonction/src/ad9833.c) : interface locale et génération.
- [`VALIDATION.md`](VALIDATION.md) et [`RAPPORT_ANALYSE_CODE.md`](RAPPORT_ANALYSE_CODE.md) : historique des preuves logicielles et analyse détaillée.

> **Conclusion :** la partie « USB + catalogue + EEPROM » est implémentée sur `main` ; elle doit maintenant être **validée matériellement**, surtout après coupure d'alimentation. La conformité électrique des signaux à VOUT reste une **tâche distincte et prioritaire**.
