#!/usr/bin/env python3
"""Turn a built VST3 bundle into resources compiled into the installer executable."""
import argparse
from pathlib import Path


def quote(value):
    return value.replace('\\', '\\\\').replace('"', '\\"')


def generate(bundle: Path, output: Path, artwork: Path, manifest: Path):
    bundle = bundle.resolve()
    if not bundle.is_dir() or bundle.name != 'D.A AURA.vst3':
        raise ValueError('Expected the built D.A AURA.vst3 bundle directory')
    files = sorted(p for p in bundle.rglob('*') if p.is_file())
    # Fail the build rather than produce an installer that silently installs nothing.
    if not files or not (bundle / 'Contents' / 'x86_64-win' / 'D.A AURA.vst3').is_file():
        raise ValueError('The Windows x64 VST3 binary is missing from the bundle')
    if any(p.stat().st_size == 0 for p in files):
        raise ValueError('The VST3 bundle contains an empty file')
    if not artwork.is_file() or not manifest.is_file():
        raise ValueError('Installer artwork or manifest is missing')
    if len(files) > 60000:
        raise ValueError('Too many files for Windows resource IDs')
    output.mkdir(parents=True, exist_ok=True)
    resource = [f'1 24 "{manifest.resolve().as_posix()}"',
                f'100 RCDATA "{artwork.resolve().as_posix()}"']
    header = ['#pragma once', 'struct PayloadFile { int resourceId; const wchar_t* relativePath; };',
              'static const PayloadFile payloadFiles[] = {']
    for index, path in enumerate(files, 101):
        relative = path.relative_to(bundle)
        if any(part in ('.', '..') for part in relative.parts):
            raise ValueError('Invalid payload path')
        resource.append(f'{index} RCDATA "{path.resolve().as_posix()}"')
        header.append(f'    {{{index}, L"{quote(str(relative))}"}},')
    header.append('};')
    (output / 'payload.rc').write_text('\n'.join(resource) + '\n', encoding='utf-8')
    (output / 'payload.h').write_text('\n'.join(header) + '\n', encoding='utf-8')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--bundle', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--artwork', required=True, type=Path)
    parser.add_argument('--manifest', required=True, type=Path)
    args = parser.parse_args()
    generate(args.bundle, args.output, args.artwork, args.manifest)
