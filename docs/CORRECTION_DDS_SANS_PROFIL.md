# Correctif du 29 octobre 2026 — DDS sans profil importé

Code et tests : commit `7e24609d91d98a885684982b777ab23c76600756` sur
`finalisation/p02-p11`.

## Ce qui est corrigé

L'absence de profil ne bloque plus toute la génération. Un clic conserve le parcours
sélection + activation et exécute RESET DDS → configuration forme/fréquence → activation.
Les pilotes EPOT ne sont pas commandés dans ce mode. Les erreurs DDS continuent à être
signalées ; les erreurs de plage d'un profil présent ne déclenchent pas de repli.
La calibration réelle conserve sa priorité pour appliquer amplitude et offset.

L'état est `DDS_SEUL_COMMANDE` par USB, et `DDS SEUL` sur la fiche OLED.
Les deux grandes lignes analogiques indiquent `A NON REGLEE` et `O NON REGLE`.
`commande_courante=false` signifie que la configuration analogique complète n'est
pas appliquée, et non que le DDS est forcément arrêté. Pour déterminer l'activité,
consulter `etat` / `integration_nom_etat()`. `derniere` et `codes` conservent la
dernière commande complète réussie, s'il en existe une.

Le démarrage reste sans activation. Un arrêt, une nouvelle sélection, une modification
USB du signal actif, un chargement, une restauration ou un changement de profil
conservent leurs séquences d'arrêt. Aucun paramètre ne devient éditable à l'encodeur.
Les trois trames du cahier des charges et le format EEPROM ne changent pas.

## Limite matérielle explicite

Ce correctif est un déblocage du DDS, **pas une conversion nominale gain/offset**.
Les fichiers fournis décrivent les blocs fonctionnels et les composants, mais ne
donnent pas le schéma électrique détaillé de l'étage d'adaptation, ses résistances,
les connexions A/W/B des EPOT, la référence d'offset et ses rapports de sommation.
Les anciennes branches d'essai ne contiennent pas non plus cette conversion.

Déduire « code 255 = 20 Vpp » ou « code 128 = 0 V » de la seule plage du cahier des
charges serait inventer le comportement du montage. Aucun de ces coefficients n'est
ajouté. Les EPOT restent dans leur état courant ou de démarrage ; la présence d'un
signal à VOUT, son amplitude et son offset ne sont donc pas garantis par `DDS SEUL`.
Contrôler d'abord la sortie du module AD9833 puis la chaîne analogique, sans charge
sensible. OFF n'est ni un dispositif de déconnexion ni une garantie de VOUT nulle.

Pour terminer la commande amplitude/offset : établir la loi du montage réel depuis
son schéma et ses composants, ou importer un profil mesuré conforme au guide technique.
Les fixtures synthétiques des tests ne sont pas des profils à installer sur la carte.

## Vérifications ajoutées

Les suites natives vérifient les dix presets sans profil, la séquence S-D-E sans
écriture gain/offset, l'absence de mutation du profil et du catalogue, les défauts
à chaque étape, les pilotes manquants, l'arrêt lors d'une modification, l'import et
l'effacement de profil, la conservation de la dernière commande complète et l'absence
de repli pour une plage refusée. Les tests OLED vérifient les nouveaux libellés,
le clic direct, l'appui long et le passage vers le mode calibré. Le protocole teste
la distinction `DDS_SEUL_COMMANDE` dans la réponse `$STATE?#`.

Exécution : `python tools/run_native_tests.py --sanitize`.
Le workflow associé au commit conserve les journaux et les résultats de compilation.
Ces tests logiciels ne remplacent pas une mesure électrique ni un essai sur l'écran réel.
