#!/usr/bin/env python3
"""Client serie du generateur. Un acquittement RAM ne prouve pas une sauvegarde.
Usage : python tools/generateur.py --port COM7 list
Ne pas ouvrir simultanement un moniteur serie sur le meme port.
"""
from __future__ import annotations
import argparse
import json
import struct
import sys
import time
import zlib
from pathlib import Path
from typing import Any

class ProtocolError(RuntimeError):
    pass

def integer(value: Any, low: int, high: int, field: str) -> int:
    if type(value) is not int or not low <= value <= high:
        raise ValueError(f"{field}: entier attendu dans [{low}, {high}]")
    return value

def calibration_image(data: dict[str, Any]) -> bytes:
    """Encode le profil CAL1. Donnees mesurees et approbation explicite requises."""
    if data.get('approved') is not True:
        raise ValueError('approved=true requis apres validation reelle de la carte et de la charge')
    name = str(data.get('board', '')).encode('ascii')
    if not 1 <= len(name) <= 23 or any(c < 33 or c > 126 for c in name):
        raise ValueError('board: identifiant ASCII sans espace, 1..23 caracteres')
    order = data.get('gain_before_offset')
    if type(order) is not bool:
        raise ValueError('gain_before_offset: ordre de transition valide sur la carte requis')
    b = bytearray(512)
    b[:4] = b'CAL1'
    struct.pack_into('<HBB', b, 4, 1, 1, int(order))
    b[8:8+len(name)] = name
    load = integer(data['load_ohm'], 1, 100000000, 'load_ohm')
    lo = integer(data['output_min_mv'], -15000, 14999, 'output_min_mv')
    hi = integer(data['output_max_mv'], lo+1, 15000, 'output_max_mv')
    settle = integer(data['settle_ms'], 0, 100, 'settle_ms')
    struct.pack_into('<IiiH', b, 32, load, lo, hi, settle)
    forms = data.get('forms', {})
    if not isinstance(forms, dict) or not forms or set(forms) - {'SIN', 'TRI', 'CAR'}:
        raise ValueError('forms: au moins une forme SIN, TRI ou CAR')
    for f, name in enumerate(('SIN', 'TRI', 'CAR')):
        if name not in forms:
            continue
        form = forms[name]
        mn = integer(form['freq_min_hz'], 1, 12000000, 'freq_min_hz')
        mx = integer(form['freq_max_hz'], mn, 12000000, 'freq_max_hz')
        ae = integer(form['amplitude_error_mvpp'], 0, 20000, 'amplitude_error_mvpp')
        oe = integer(form['offset_error_mv'], 0, 10000, 'offset_error_mv')
        gain, offset = form['gain'], form['offset']
        if not 2 <= len(gain) <= 8 or not 2 <= len(offset) <= 8:
            raise ValueError(f'{name}: 2..8 points pour gain et offset')
        p = 48 + 144*f
        struct.pack_into('<IIIIBB', b, p, mn, mx, ae, oe, len(gain), len(offset))
        for table, gain_mode in ((gain, True), (offset, False)):
            values: list[int] = []
            codes: list[int] = []
            for i, row in enumerate(table):
                if len(row) != (3 if gain_mode else 2):
                    raise ValueError('gain=[code,amplitude_mvpp,zero_mv], offset=[code,delta_mv]')
                code = integer(row[0], 0, 255, 'code')
                v = integer(row[1], 1 if gain_mode else -30000, 30000, 'tension')
                codes.append(code); values.append(v)
                if gain_mode:
                    zero = integer(row[2], -30000, 30000, 'zero_mv')
                    struct.pack_into('<Bii', b, p+24+9*i, code, v, zero)
                else:
                    struct.pack_into('<Bi', b, p+96+5*i, code, v)
            if any(a >= c for a, c in zip(codes, codes[1:])):
                raise ValueError('Les codes doivent etre strictement croissants')
            if not (all(a < c for a, c in zip(values, values[1:])) or
                    all(a > c for a, c in zip(values, values[1:]))):
                raise ValueError('Les tensions doivent etre strictement monotones')
    struct.pack_into('<I', b, 508, zlib.crc32(b[:508]))
    return bytes(b)

class Client:
    """Accepte un port pyserial ou un faux port pour les tests hors materiel."""
    def __init__(self, serial_port: Any, timeout: float = 5.0):
        self.port = serial_port
        self.timeout = timeout
        self.partial = bytearray()

    def frame(self) -> str:
        until = time.monotonic() + self.timeout
        while time.monotonic() < until:
            c = self.port.read(1)
            if not c:
                continue
            if c == b'$':
                self.partial[:] = c
            elif self.partial:
                if not 32 <= c[0] <= 126 or len(self.partial) >= 256:
                    self.partial.clear()
                    continue
                self.partial += c
                if c == b'#':
                    answer = self.partial.decode('ascii')
                    self.partial.clear()
                    if answer.startswith('$ERR:'):
                        raise ProtocolError(answer)
                    return answer
        raise TimeoutError('Aucune trame complete recue avant le delai')

    def send(self, frame: str) -> None:
        raw = frame.encode('ascii')
        if not frame.startswith('$') or not frame.endswith('#') or len(raw) >= 96:
            raise ValueError('Trame invalide/trop longue')
        until = time.monotonic() + self.timeout
        pos = 0
        while pos < len(raw):
            if time.monotonic() >= until:
                raise TimeoutError('Ecriture serie bloquee')
            n = self.port.write(raw[pos:])
            if n:
                pos += n

    def request(self, frame: str) -> str:
        self.send(frame)
        return self.frame()

    def ack(self, frame: str) -> int:
        answer = self.request(frame)
        parts = answer[1:-1].split(':')
        if len(parts) != 3 or parts[0] != 'OK' or not parts[2].isdigit():
            raise ProtocolError(f'Acquittement inattendu : {answer}')
        return int(parts[2])

    def state(self) -> dict[str, Any]:
        answer = self.request('$STATE?#')
        p = answer[1:-1].split(':')
        if len(p) != 11 or p[0] != 'STATE':
            raise ProtocolError(f'Etat inattendu : {answer}')
        try:
            return dict(state=p[1], selected=int(p[2]), count=int(p[3]), revision=int(p[4]),
                        saved_revision=int(p[5]), dirty=bool(int(p[6])), saving=bool(int(p[7])),
                        save_error=p[8], error=p[9], stop_error=p[10])
        except ValueError as exc:
            raise ProtocolError(f'Etat mal forme : {answer}') from exc

    def saved(self, revision: int, timeout: float = 15.0) -> dict[str, Any]:
        until = time.monotonic()+timeout
        while time.monotonic() < until:
            s = self.state()
            if s['revision'] != revision:
                raise ProtocolError('Modification concurrente : revision RAM differente')
            if s['saved_revision'] == revision and not s['dirty'] and not s['saving']:
                return s
            if not s['saving'] and s['save_error'] != 'OK':
                raise ProtocolError('Parametres en RAM uniquement : '+s['save_error'])
            time.sleep(0.05)
        raise TimeoutError('Sauvegarde non confirmee ; les parametres peuvent rester uniquement en RAM')

    def signals(self) -> list[str]:
        initial = self.state()
        count = integer(initial['count'], 1, 100, 'nombre de presets')
        self.send('$?#')
        result = [self.frame() for _ in range(count)]
        for i, f in enumerate(result, 1):
            if not f.startswith(f'${i}:') or len(f[1:-1].split(':')) != 5:
                raise ProtocolError('Liste incomplete ou ordre inattendu : '+f)
        final = self.state()
        if initial['revision'] != final['revision'] or initial['count'] != final['count']:
            raise ProtocolError('Le catalogue a change pendant la lecture ; recommencer')
        return result

    def import_profile(self, image: bytes) -> int:
        if len(image) != 512:
            raise ValueError('Le profil doit contenir exactement 512 octets')
        self.ack('$CAL:BEGIN#')
        try:
            for offset in range(0, 512, 32):
                self.ack(f'$CAL:DATA:{offset}:{image[offset:offset+32].hex().upper()}#')
            return self.ack('$CAL:COMMIT#')
        except Exception:
            # L'import incomplet reste dans le tampon, pas dans le profil actif.
            try:
                self.ack('$CAL:ABORT#')
            except (ProtocolError, TimeoutError, OSError):
                pass
            raise

    def export_profile(self) -> bytes:
        self.send('$CAL?#')
        result = bytearray(512)
        for offset in range(0, 512, 32):
            f = self.frame()[1:-1].split(':')
            if len(f) != 4 or f[:3] != ['CAL', 'DATA', str(offset)] or len(f[3]) != 64:
                raise ProtocolError('Export de calibration mal forme')
            try:
                result[offset:offset+32] = bytes.fromhex(f[3])
            except ValueError as exc:
                raise ProtocolError('Donnees hexadecimales invalides') from exc
        if self.frame() != '$CAL:END#':
            raise ProtocolError('Fin de calibration absente')
        if any(result) and zlib.crc32(result[:508]) != struct.unpack_from('<I', result, 508)[0]:
            raise ProtocolError('CRC calibration invalide')
        return bytes(result)

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True, help='COM7, /dev/ttyACM0, ...')
    parser.add_argument('--timeout', type=float, default=5)
    sub = parser.add_subparsers(dest='action', required=True)
    for name in ('state', 'list', 'off', 'on', 'save', 'load'):
        sub.add_parser(name)
    read = sub.add_parser('read'); read.add_argument('index', type=int)
    select = sub.add_parser('select'); select.add_argument('index', type=int)
    write = sub.add_parser('write')
    write.add_argument('index', type=int); write.add_argument('shape', choices=['SIN', 'TRI', 'CAR'])
    write.add_argument('amplitude', type=int); write.add_argument('offset', type=int); write.add_argument('frequency', type=int)
    write.add_argument('--ram-only', action='store_true')
    for name in ('cal-import', 'cal-export'):
        cmd = sub.add_parser(name); cmd.add_argument('path', type=Path)
    dig = sub.add_parser('digital'); dig.add_argument('channel', type=int, choices=[1, 2])
    dig.add_argument('frequency', type=int); dig.add_argument('duty', type=int, help='0..1000 ; frequence 0 = OFF')
    a = parser.parse_args()
    try:
        if not 0 < a.timeout <= 60:
            raise ValueError('timeout doit etre compris entre 0 et 60 secondes')
        import serial
        with serial.Serial(port=None, baudrate=115200, timeout=min(0.1, a.timeout), write_timeout=a.timeout,
                           rtscts=False, dsrdtr=False) as port:
            port.dtr = False; port.rts = False
            port.port = a.port; port.open()
            port.reset_input_buffer()
            c = Client(port, a.timeout)
            if a.action == 'state': print(json.dumps(c.state(), indent=2))
            elif a.action == 'list': print('\n'.join(c.signals()))
            elif a.action == 'read':
                integer(a.index, 1, 100, 'index'); print(c.request(f'${a.index}?#'))
            elif a.action == 'write':
                integer(a.index, 1, 100, 'index'); integer(a.amplitude, 100, 20000, 'amplitude')
                integer(a.offset, -10000, 10000, 'offset'); integer(a.frequency, 1, 12000000, 'frequency')
                rev = c.ack(f'${a.index}:{a.shape}:{a.amplitude}:{a.offset:+d}:{a.frequency}#')
                print(f'RAM modifiee : revision {rev}')
                if not a.ram_only: print(json.dumps(c.saved(rev), indent=2))
            elif a.action == 'select':
                integer(a.index, 1, 100, 'index'); print(c.ack(f'$SELECT:{a.index}#'))
            elif a.action == 'cal-import':
                image = calibration_image(json.loads(a.path.read_text(encoding='utf-8')))
                rev = c.import_profile(image); print(json.dumps(c.saved(rev), indent=2))
            elif a.action == 'cal-export':
                image = c.export_profile()
                # Refuser d'ecraser silencieusement une calibration existante.
                with a.path.open('xb') as output: output.write(image)
                print(f'Profil binaire CAL1 : {a.path}')
            elif a.action == 'digital':
                integer(a.duty, 0, 1000, 'duty')
                if a.frequency == 0: print(c.ack(f'$DIG:{a.channel}:OFF#'))
                else:
                    integer(a.frequency, 10, 1000000, 'frequency')
                    print(c.ack(f'$DIG:{a.channel}:{a.frequency}:{a.duty}#'))
            else:
                rev = c.ack('$'+a.action.upper()+'#')
                print(json.dumps(c.saved(rev), indent=2) if a.action == 'save' else f'Commande acquittee : revision {rev}')
        return 0
    except (ImportError, ValueError, KeyError, TypeError, OSError, ProtocolError, TimeoutError) as exc:
        print(f'ERREUR : {exc}', file=sys.stderr)
        return 1

if __name__ == '__main__':
    raise SystemExit(main())
