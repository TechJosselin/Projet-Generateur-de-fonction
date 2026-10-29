import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]/"EQ1_Projet_Generateur_Fonction"
class InterfaceTroisLignes(unittest.TestCase):
    def test_disposition_preservee(self):
        s=(ROOT/"src/menu_parts/menu_01.inc").read_text()
        self.assertIn("for(int i=0;i<3;i++)",s)
        self.assertIn("premiere_ligne_visible+3",s)
        self.assertIn("titre(titre_s)",s)
        self.assertIn("oled_texte_grand",s)
    def test_actions_raccordees(self):
        self.assertIn("integration_modifier",(ROOT/"src/menu_parts/menu_05.inc").read_text())
        s=(ROOT/"src/main.c").read_text()
        self.assertIn("integration_periodique",s)
        self.assertIn("usb_transport_periodique",s)
if __name__=="__main__":unittest.main()
