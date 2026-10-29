#!/usr/bin/env python3
"""Tests natifs du firmware courant. Les faux pilotes restent sous test/native."""
from __future__ import annotations
import argparse
import datetime as dt
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
FW = ROOT / 'EQ1_Projet_Generateur_Fonction'
CORE = ['gf', 'signaux', 'analogique', 'stockage', 'integration', 'usb_protocol', 'usb_service']

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--cc', default=os.environ.get('CC', 'gcc'))
    a = parser.parse_args()
    out = ROOT/'build/native'; out.mkdir(parents=True, exist_ok=True)
    report = {'time_utc':dt.datetime.now(dt.timezone.utc).isoformat(), 'host':platform.platform(),
              'sanitize':a.sanitize, 'status':'FAIL', 'commands':[]}
    def run(cmd: list[str], log) -> None:
        log.write('$ '+' '.join(cmd)+'\n');log.flush()
        result = subprocess.run(cmd, cwd=ROOT, text=True, encoding='utf-8', errors='replace',
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=180)
        log.write(result.stdout); log.flush(); print(result.stdout, end='')
        report['commands'].append({'command':cmd, 'returncode':result.returncode})
        if result.returncode:
            raise RuntimeError(f'Echec ({result.returncode}) : {cmd[0]}')
    try:
        with (out/'tests.log').open('w',encoding='utf-8') as log:
            cc = shutil.which(a.cc)
            if not cc:
                raise RuntimeError(f'Compilateur C introuvable : {a.cc}')
            run([cc,'--version'],log)
            flags = ['-std=c11','-Wall','-Wextra','-Werror','-Wpedantic','-O1','-g']
            if a.sanitize:
                flags += ['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-sanitize-recover=all']
                if sys.platform.startswith('linux'): flags += ['-fno-pie','-no-pie']
            suites = [('systeme',CORE,[]),('ui',CORE+['entree','menu'],[]),
                      ('oled_render',['oled'],['-I',str(FW/'test/native/fakes')]),
                      ('drivers',['ad9833','epot','eeprom','oled','sorties_digitales'],
                       ['-I',str(FW/'test/native/fakes'),'-DSYSTEM_DIGITAL_1_GPIO=16','-DSYSTEM_DIGITAL_2_GPIO=17'])]
            for name,sources,extra in suites:
                exe=out/('test_'+name+('.exe' if os.name=='nt' else ''))
                exe.unlink(missing_ok=True)  # Ne jamais reutiliser un ancien binaire apres erreur.
                cmd=[cc,*flags,*extra,'-I',str(FW/'include')]
                cmd += [str(FW/'src'/f'{s}.c') for s in sources]
                cmd += [str(FW/'test/native'/f'test_{name}.c'),'-lm','-o',str(exe)]
                run(cmd,log);run([str(exe)],log)
            run([sys.executable,'-m','unittest','discover','-s','tools','-p','test_*.py','-v'],log)
            # L'image produite par l'outil PC doit etre acceptee par le vrai decodeur C.
            from generateur import calibration_image
            from test_generateur import profile
            fixture=out/'profil-test.bin';fixture.write_bytes(calibration_image(profile()))
            exe=out/('test_codec.exe' if os.name=='nt' else 'test_codec');exe.unlink(missing_ok=True)
            run([cc,*flags,'-I',str(FW/'include'),str(FW/'src/gf.c'),str(FW/'src/signaux.c'),
                 str(FW/'src/analogique.c'),str(FW/'test/native/test_codec.c'),'-o',str(exe)],log)
            run([str(exe),str(fixture)],log)
        report['status']='PASS'
    except (OSError,RuntimeError,subprocess.TimeoutExpired) as exc:
        report['error']=str(exc);print(str(exc),file=sys.stderr)
    finally:
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return 0 if report['status']=='PASS' else 1
if __name__=='__main__':raise SystemExit(main())
