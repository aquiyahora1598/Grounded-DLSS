"""Fetch pinned official FG SDKs for local development. Does not install into a game.

These dependencies are separate from the released mod's local SR discovery.
Never add external/framegen to a release/source export allowlist wholesale.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import urllib.request
from zipfile import ZipFile

ROOT = Path(__file__).resolve().parents[1]
SDK = {
    'nvidia': dict(archive='streamline-sdk-v2.14.16.zip', directory='streamline-2.14.16',
        url='https://github.com/NVIDIA-RTX/Streamline/releases/download/v2.14.16/streamline-sdk-v2.14.16.zip',
        sha256='32041e51db0cab709a271b72a352281b690c2abbd669a827809401a5dd23956b'),
    'amd': dict(archive='fsr-sdk-v2.3.0-source.zip', directory='fsr-2.3.0',
        url='https://codeload.github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/zip/refs/tags/v2.3.0',
        sha256='e7f274930086405071c01b4e7e1de8cd3bb9fbcb987d711ebf65169464c28455'),
}


def selected(kind, name):
    if kind == 'nvidia':
        modules = {'sl.interposer.dll', 'sl.common.dll', 'sl.dlss_g.dll', 'sl.reflex.dll', 'sl.pcl.dll', 'nvngx_dlssg.dll'}
        return (name.startswith('include/') or name in {'license.txt', '3rd-party-licenses.md',
            'lib/x64/sl.interposer.lib', 'external/ngx-sdk/license.txt',
            'docs/ProgrammingGuideDLSS_G.md', 'docs/ProgrammingGuideManualHooking.md', 'docs/ProgrammingGuideReflex.md'}
            or (PurePosixPath(name).parent.as_posix() == 'bin/x64' and PurePosixPath(name).name in modules))
    return name.startswith(('Kits/FidelityFX/api/include/', 'Kits/FidelityFX/framegeneration/include/',
                            'Kits/FidelityFX/framegeneration/fsr3/include/')) or name in {
        'Kits/FidelityFX/docs/techniques/frame-interpolation-api.md',
        'Kits/FidelityFX/docs/techniques/frame-interpolation-swap-chain.md',
        'docs/license.md', 'Kits/FidelityFX/signedbin/amd_fidelityfx_framegeneration_dx12.dll',
        'Kits/FidelityFX/signedbin/amd_fidelityfx_loader_dx12.dll', 'Kits/FidelityFX/signedbin/amd_fidelityfx_loader_dx12.lib'}


def fetch(kind):
    spec = SDK[kind]
    out = ROOT/'external/framegen'
    out.mkdir(parents=True, exist_ok=True)
    archive = out/spec['archive']
    if not archive.exists():
        part = archive.with_suffix('.download')
        if part.exists():
            raise RuntimeError(f'Preserving incomplete download: {part}')
        request = urllib.request.Request(spec['url'], headers={'User-Agent': 'Grounded-DLSS-development'})
        with urllib.request.urlopen(request, timeout=60) as response, part.open('xb') as target:
            size = 0
            while block := response.read(1024*1024):
                target.write(block)
                size += len(block)
                if size % (16*1024*1024) == 0:
                    print(f'{kind}: {size//1024//1024} MiB', flush=True)
        with part.open('rb') as source:
            if hashlib.file_digest(source, 'sha256').hexdigest() != spec['sha256']:
                raise RuntimeError('SDK digest mismatch; preserved and not extracted.')
        part.rename(archive)
    with archive.open('rb') as source:
        if hashlib.file_digest(source, 'sha256').hexdigest() != spec['sha256']:
            raise RuntimeError('Existing archive differs from pinned SDK.')
    destination = out/spec['directory']
    destination.mkdir(exist_ok=True)
    records = []
    with ZipFile(archive) as zipped:
        for item in zipped.infolist():
            if item.is_dir():
                continue
            name = item.filename if kind == 'nvidia' else '/'.join(item.filename.split('/')[1:])
            pure = PurePosixPath(name)
            if pure.is_absolute() or '..' in pure.parts or '\\' in name or ':' in name:
                raise RuntimeError('Unsafe archive member.')
            if not selected(kind, name):
                continue
            target = destination.joinpath(*pure.parts)
            if not target.resolve().is_relative_to(destination.resolve()):
                raise RuntimeError('Extraction escapes the SDK directory.')
            data = zipped.read(item)
            if target.exists():
                if target.read_bytes() != data:
                    raise RuntimeError(f'Preserving modified SDK file: {target}')
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(data)
            records.append(dict(path=name, sha256=hashlib.sha256(data).hexdigest()))
    manifest = destination/'extraction.json'
    rendered = json.dumps(records, indent=2)
    if not manifest.exists():
        manifest.write_text(rendered, encoding='utf-8')
    print(f'{kind}: pinned SDK verified; {len(records)} development files. No game files changed.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('provider', choices=['nvidia', 'amd', 'all'])
    args = parser.parse_args()
    for provider in SDK if args.provider == 'all' else [args.provider]:
        fetch(provider)
