# Integration fonctionnelle sur menu 3 lignes — 29/10/2026

- **Base fonctionnelle** : finalisation/p02-p11 (DDS, EPOT, EEPROM, USB,
  stockage CRC, profils de calibration et PWM optionnel).
- **Interface conservee** : feature/menu-oled-3-lignes : fonctions de rendu,
  navigation et dessin des trois lignes avec titre fixe inchangees.
- Les changements dans menu_00.inc, menu_04.inc et menu_05.inc ne portent
  que sur la liaison au coordinateur (revisions, EEPROM, edition, usine).
- La commande « Activer DDS test » appelle desormais le chemin de sortie
  **calibre et protege**. Sans profil analogique valide, elle refuse ON.
- Les textes statiques des pages d'informations de l'ancien menu parlent
  encore d'« EEPROM inactif », « USB OFF » et « amplitude non appliquee »,
  alors que les pilotes sont maintenant presents. Ils sont conserves par
  consigne, mais sont devenus obsoletes.
- « EEPROM OK » dans le menu confirme la **demande** de sauvegarde :
  l'ecriture est asynchrone et peut echouer. Le statut reel est consultable
  par la trame USB `$STATE?#`.
- Les deux PWM sont des fonctions optionnelles. Leurs GPIO restent a -1
  jusqu'a validation du schema et des niveaux electriques.
- Aucun flash ni validation oscilloscope, etalonnage reel ou test de coupure
  physique n'a ete effectue ici.
- La suite UI historique de P02 testait un AUTRE menu. Elle est remplacee
  pour cette branche par les tests des evenements d'encodeur et un test
  statique qui controle la fenetre de 3 lignes. Le rendu sur carte devra
  encore etre controle.

Voir docs/VALIDATION.md pour les limites du moteur P02-P11.
