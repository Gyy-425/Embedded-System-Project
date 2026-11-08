"""Assemble with the existing SDK's pioasm sources; no downloads/install needed."""
import argparse
import hashlib
from pathlib import Path
import subprocess
import tempfile


def assemble(sdk, output):
    source = sdk / "tools/pioasm"
    names = ["main.cpp", "pio_assembler.cpp", "pio_disassembler.cpp",
             "gen/lexer.cpp", "gen/parser.cpp", "c_sdk_output.cpp",
             "python_output.cpp", "hex_output.cpp", "json_output.cpp",
             "ada_output.cpp", "go_output.cpp"]
    digest = hashlib.sha256()
    for path in sorted(source.rglob("*")):
        if path.is_file() and path.suffix in (".h", ".in", ".cpp", ".hpp"):
            digest.update(str(path.relative_to(source)).encode())
            digest.update(path.read_bytes())
    cache = Path(tempfile.gettempdir()) / ("aai05-pioasm-" + digest.hexdigest()[:16])
    cache.mkdir(exist_ok=True)
    executable = cache / "pioasm"
    if not executable.exists():
        (cache / "version.h").write_text(
            '#define PIOASM_VERSION_STRING "SDK-2.3.0-local-source"\n')
        subprocess.run(["g++", "-std=c++11", "-O2", "-I" + str(source),
                        "-I" + str(source / "gen"), "-I" + str(cache),
                        *[str(source / name) for name in names],
                        "-o", str(executable)], check=True)
    repo = Path(__file__).resolve().parents[1]
    subprocess.run([str(executable), "-o", "c-sdk",
                    str(repo / "app_program/subsystem2/encoder_counter.pio"),
                    str(output)], check=True)
    # Canonical EOF only; the SDK emits an extra empty line after its header.
    output.write_text(output.read_text().rstrip() + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", required=True)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    destination = Path(__file__).resolve().parents[1] / "app_program/subsystem2/encoder_counter.pio.h"
    with tempfile.TemporaryDirectory(prefix="aai05-pio-header-") as temporary:
        generated = Path(temporary) / destination.name
        assemble(Path(args.sdk).resolve(), generated)
        if args.check:
            if generated.read_bytes() != destination.read_bytes():
                raise SystemExit("Generated PIO header differs; assemble and review it first")
        else:
            destination.write_bytes(generated.read_bytes())
    print("PIO assembler/header check: PASS")


if __name__ == "__main__":
    main()
