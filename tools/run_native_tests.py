#!/usr/bin/env python3
"""Compile et execute les tests C sur les sources courantes, sans ESP-IDF."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shlex
import shutil
import subprocess
import sys
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[1]
FIRMWARE = ROOT / "EQ1_Projet_Generateur_Fonction"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", default=os.environ.get("CC", "gcc"),
                        help="Executable GCC ou Clang (sans options).")
    parser.add_argument("--sanitize", action="store_true",
                        help="Activer AddressSanitizer et UndefinedBehaviorSanitizer.")
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build" / "native")
    args = parser.parse_args()
    build = args.build_dir.resolve()
    build.mkdir(parents=True, exist_ok=True)
    log_path = build / "tests.log"
    report_path = build / "results.json"
    report = {
        "started_utc": datetime.now(timezone.utc).isoformat(),
        "host": platform.platform(),
        "python": platform.python_version(),
        "sanitizers": args.sanitize,
        "status": "FAIL",
        "sources_sha256": {},
    }
    files = [FIRMWARE / "src" / "signaux.c", FIRMWARE / "src" / "usb_protocol.c",
             FIRMWARE / "test" / "native" / "test_baseline.c"]
    headers = [FIRMWARE / "include" / "signaux.h", FIRMWARE / "include" / "usb_protocol.h"]
    exit_code = 1
    with log_path.open("w", encoding="utf-8") as log:
        def record(text: str) -> None:
            print(text, flush=True)
            log.write(text + "\n")
            log.flush()

        def run(command: list[str], timeout: int = 60) -> None:
            record("$ " + shlex.join(command))
            env = os.environ.copy()
            if args.sanitize:
                env["ASAN_OPTIONS"] = "detect_leaks=1:halt_on_error=1"
                env["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"
            result = subprocess.run(command, cwd=ROOT, env=env, text=True,
                                    encoding="utf-8", errors="replace",
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                    timeout=timeout, check=False)
            record(result.stdout.rstrip())
            if result.returncode:
                raise RuntimeError(f"Commande en echec (code {result.returncode}).")

        try:
            compiler = shutil.which(args.cc)
            if compiler is None:
                raise RuntimeError(f"Compilateur introuvable : {args.cc}")
            for source in files + headers:
                report["sources_sha256"][str(source.relative_to(ROOT))] = hashlib.sha256(
                    source.read_bytes()).hexdigest()
            git = shutil.which("git")
            if git:
                ref = subprocess.run([git, "rev-parse", "HEAD"], cwd=ROOT,
                                     text=True, capture_output=True, timeout=10, check=False)
                report["git_commit"] = ref.stdout.strip() if ref.returncode == 0 else None
            run([compiler, "--version"])
            executable = build / ("test_baseline.exe" if os.name == "nt" else "test_baseline")
            # No cached binary is executed if compilation fails.
            executable.unlink(missing_ok=True)
            flags = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-Wpedantic", "-O1", "-g"]
            if args.sanitize:
                flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                          "-fno-sanitize-recover=all"]
                if sys.platform.startswith("linux"):
                    flags += ["-fno-pie", "-no-pie"]
            command = [compiler, *flags, "-I", str(FIRMWARE / "include"),
                       *(str(path) for path in files), "-o", str(executable)]
            run(command)
            run([str(executable)])
            report["status"] = "PASS"
            exit_code = 0
        except (OSError, RuntimeError, subprocess.TimeoutExpired) as exc:
            report["error"] = str(exc)
            record("ERREUR : " + str(exc))
        finally:
            report["finished_utc"] = datetime.now(timezone.utc).isoformat()
            report_path.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n",
                                   encoding="utf-8")
            record(f"{report['status']} — rapport : {report_path}")
    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())
