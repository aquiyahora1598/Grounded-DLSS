"""Build a small installer bound to a visible, verified external payload ZIP."""
from pathlib import Path
import argparse
import hashlib
import shutil
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]

def build_installer(payload, output):
    payload, output = Path(payload).resolve(), Path(output).resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    expected_prefix = 'Maine/Binaries/Win64/'
    expected = {'dxgi.dll', 'plugins/GroundedDLSS.asi', 'plugins/GroundedDLSS.ini',
        'plugins/FSRFrameGeneration/amd_fidelityfx_loader_dx12.dll',
        'plugins/FSRFrameGeneration/amd_fidelityfx_framegeneration_dx12.dll'}
    expected |= {'GroundedDLSS-install/' + name for name in (
        'LEEME.txt', 'LEEME.en.txt', 'LEEME.pt-BR.txt', 'TERMINOS.txt',
        'TERMINOS.en.txt', 'TERMINOS.pt-BR.txt', 'LICENSE.txt',
        'NVIDIA-RTX-SDK-LICENSE.txt', 'THIRD-PARTY-NOTICES.txt',
        'AMD-FidelityFX-SDK-LICENSE.txt')}
    documents = {}
    with zipfile.ZipFile(payload) as archive:
        assert len(archive.namelist()) == 15 and archive.testzip() is None
        assert set(archive.namelist()) == {expected_prefix + name for name in expected}
        documents = {name: archive.read(name) for name in archive.namelist() if '/GroundedDLSS-install/' in name}
    digest = hashlib.sha256(payload.read_bytes()).hexdigest()
    resources = output.parent / 'installer-resources'
    resources.mkdir(exist_ok=True)
    identity = resources / 'PackageIdentity.g.cs'
    identity.write_text('namespace GroundedDLSSSetup { public static class PackageIdentity { public const long Bytes=%dL; public const string Sha256="%s"; } }\n'
        % (payload.stat().st_size, digest), encoding='utf-8')
    docs = resources / 'documentation.zip'
    with zipfile.ZipFile(docs, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name, data in sorted(documents.items()):
            item = zipfile.ZipInfo(name, (2026, 10, 24, 0, 0, 0))
            item.compress_type = zipfile.ZIP_DEFLATED
            item.external_attr = 0o100644 << 16
            archive.writestr(item, data)
    sidecar = output.parent / 'GroundedDLSS-payload.zip'
    if payload != sidecar:
        shutil.copyfile(payload, sidecar)
    assert hashlib.sha256(sidecar.read_bytes()).hexdigest() == digest
    compiler = Path(r'C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe')
    sources = [ROOT / 'installer' / name for name in ('Setup.cs', 'LocalRuntime.cs', 'I18n.cs', 'PayloadPackage.cs')]
    subprocess.run([str(compiler), '/nologo', '/target:winexe', '/platform:x64', '/optimize+',
        '/out:' + str(output), '/win32manifest:' + str(ROOT / 'installer/app.manifest'),
        '/resource:' + str(docs) + ',documentation.zip', '/r:System.Windows.Forms.dll',
        '/r:System.Drawing.dll', '/r:System.IO.Compression.dll', '/r:System.IO.Compression.FileSystem.dll',
        *map(str, sources), str(identity)], check=True)
    return sidecar

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--payload', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    print('Sidecar:', build_installer(args.payload, args.output))
