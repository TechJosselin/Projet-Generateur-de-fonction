# ==============================================================================================
# CFPT - Equipe 1 - Generateur de fonctions
# Tests Python du dialogue serie et du format de calibration.
# ==============================================================================================
import json
import struct
import unittest
import zlib
from generateur import Client, ProtocolError, calibration_image


class FakePort:
    """Simule la reception et l'emission de trames sur un port serie."""

    def __init__(self, responses):
        """Configure un port serie simule a ecritures volontairement partielles."""
        self.responses = list(responses)
        self.buffer = bytearray()
        self.sent = bytearray()
        self.current = bytearray()

    def read(self, n):
        """Renvoie les octets disponibles depuis le tampon de reception."""
        data = self.buffer[:n]
        del self.buffer[:n]
        return bytes(data)

    def write(self, data):
        """Simule des ecritures courtes et des reponses apres la fin de trame."""
        # Simule les ecritures partielles, une reponse uniquement apres #.
        part = data[:3]
        self.sent += part
        self.current += part
        if self.current.endswith(b'#'):
            self.buffer += self.responses.pop(0).encode('ascii')
            self.current.clear()
        return len(part)


def state(rev=1, saved=1, dirty=0, busy=0, err='OK', count=2):
    """Construit une reponse STATE de test."""
    return f'$STATE:NON_CALIBRE:0:{count}:{rev}:{saved}:{dirty}:{busy}:{err}:OK:OK#'


def profile():
    """Retourne un profil synthetique utilisable dans les tests."""
    return dict(approved=True, board='TEST_ONLY', load_ohm=1000000, output_min_mv=-11000, output_max_mv=11000,
                gain_before_offset=True, settle_ms=1, forms={'SIN': dict(freq_min_hz=1, freq_max_hz=1000000,
                amplitude_error_mvpp=100, offset_error_mv=100, gain=[[0,100,0],[255,20000,0]],
                offset=[[0,-10000],[255,10000]])})


class Tests(unittest.TestCase):
    """Regroupe les tests unitaires du protocole USB PC."""

    def test_fragment_and_noise(self):
        """Verifie la recuperation d'une trame au milieu du bruit."""
        p=FakePort(['boot garbage$broken$OK:RAM:3#'])
        self.assertEqual(Client(p).ack('$SAVE#'),3)
        self.assertEqual(p.sent,b'$SAVE#')

    def test_errors(self):
        """Verifie la detection d'erreurs et de temporisations."""
        with self.assertRaises(ProtocolError):
            Client(FakePort(['$ERR:IO#'])).ack('$SAVE#')
        with self.assertRaises(ProtocolError):
            Client(FakePort(['$STATE:BAD#'])).state()
        with self.assertRaises(TimeoutError):
            Client(FakePort(['']),0.001).request('$STATE?#')

    def test_verified_save(self):
        """Impose la confirmation EEPROM avant de conclure une sauvegarde."""
        c=Client(FakePort([state(3,2,1,1),state(3,3)]))
        self.assertFalse(c.saved(3)['dirty'])
        with self.assertRaises(ProtocolError):
            Client(FakePort([state(4,3)])).saved(3)
        with self.assertRaises(ProtocolError):
            Client(FakePort([state(3,2,1,0,'IO')])).saved(3)

    def test_list_snapshot(self):
        """Detecte un catalogue modifie pendant sa lecture."""
        lines='$1:SIN:1000:+0:10000#$2:TRI:2000:+0:1000#'
        self.assertEqual(len(Client(FakePort([state(),lines,state()])).signals()),2)
        with self.assertRaises(ProtocolError):
            Client(FakePort([state(),lines,state(2,2)])).signals()

    def test_profile(self):
        """Verifie les contraintes du format CAL1."""
        b=calibration_image(profile())
        self.assertEqual(len(b),512)
        self.assertEqual(b[:4],b'CAL1')
        self.assertEqual(zlib.crc32(b[:508]),struct.unpack_from('<I',b,508)[0])
        d=profile()
        d['approved']=False
        with self.assertRaises(ValueError):
            calibration_image(d)
        d=profile()
        d['forms']['SIN']['gain'][1][0]=0
        with self.assertRaises(ValueError):
            calibration_image(d)
        d=profile()
        d['forms']['SIN']['gain'][1][1]=100
        with self.assertRaises(ValueError):
            calibration_image(d)

    def test_import_export(self):
        """Teste les commandes d'import et d'export de calibration."""
        image=calibration_image(profile())
        p=FakePort(['$OK:CAL_BEGIN:1#']+['$OK:CAL_DATA:1#']*16+['$OK:RAM:2#'])
        self.assertEqual(Client(p).import_profile(image),2)
        frames=''.join(f'$CAL:DATA:{i}:{image[i:i+32].hex()}#' for i in range(0,512,32))+'$CAL:END#'
        self.assertEqual(Client(FakePort([frames])).export_profile(),image)

    def test_import_rejected(self):
        """Controle l'annulation d'un transfert rejete."""
        c=Client(FakePort(['$OK:CAL_BEGIN:1#','$ERR:CONFLIT#','$OK:CAL_ABORT:1#']))
        with self.assertRaises(ProtocolError):
            c.import_profile(calibration_image(profile()))

    def test_create_fixture(self):
        """Compare deux encodages identiques d'un profil de test."""
        # Croisement optionnel : le runner fait decoder cette image par le C.
        self.assertEqual(calibration_image(json.loads(json.dumps(profile()))),calibration_image(profile()))


if __name__=='__main__':
    unittest.main()
