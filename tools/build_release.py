"""Build a private candidate without NVIDIA runtime. --public requires clearance."""
from pathlib import Path
import argparse, hashlib, json, subprocess, zipfile

ROOT = Path(__file__).resolve().parents[1]
VERSION = '11.23.4-beta'
RUNTIME_VERSION = '11.23.2-beta'  # Installer-only change; retain the audited ASI.

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def require_clearance():
    state = json.loads((ROOT / 'publication/clearance.json').read_text(encoding='utf-8'))
    pending = [name for name, check in state['checks'].items()
               if check.get('complete') is not True or not check.get('evidence', '').strip()]
    if state['status'] != 'cleared-for-publication' or pending:
        raise RuntimeError('Publication pending: ' + ', '.join(pending))

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--public', action='store_true')
    args = parser.parse_args()
    if args.public:
        require_clearance()
    out = ROOT / 'dist/candidates' / ('GroundedDLSS-' + VERSION)
    out.mkdir(parents=True, exist_ok=True)
    prefix = 'Maine/Binaries/Win64/'
    files = {
        prefix + 'dxgi.dll': ROOT / 'build/loader/dxgi.dll',
        prefix + 'plugins/GroundedDLSS.asi': ROOT / 'build/dlss/GroundedDLSS.asi',
        prefix + 'plugins/GroundedDLSS.ini': ROOT / 'installer/GroundedDLSS.ini',
    }
    documents = {
        'LEEME.txt': ROOT / 'installer/LEEME.txt',
        'NVIDIA-RTX-SDK-LICENSE.txt': ROOT / 'licenses/NVIDIA-RTX-SDK-LICENSE.txt',
        'THIRD-PARTY-NOTICES.txt': ROOT / 'installer/THIRD-PARTY-NOTICES.txt',
        'LICENSE.txt': ROOT / 'LICENSE',
        'TERMINOS.txt': ROOT / 'installer/TERMINOS.txt',
        'LEEME.en.txt': ROOT / 'installer/LEEME.en.txt',
        'LEEME.pt-BR.txt': ROOT / 'installer/LEEME.pt-BR.txt',
        'TERMINOS.en.txt': ROOT / 'installer/TERMINOS.en.txt',
        'TERMINOS.pt-BR.txt': ROOT / 'installer/TERMINOS.pt-BR.txt',
    }
    for name, source in documents.items():
        files[prefix + 'GroundedDLSS-install/' + name] = source
    payload = out / 'payload.zip'
    with zipfile.ZipFile(payload, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for name, source in files.items():
            if Path(name).name.lower() == 'nvngx_dlss.dll':
                raise RuntimeError('NVIDIA runtime must not be embedded')
            z.writestr(name, source.read_bytes())
    setup = out / ('GroundedDLSS-' + VERSION + '-Setup.exe')
    compiler = Path(r'C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe')
    subprocess.run([str(compiler), '/nologo', '/target:winexe', '/platform:x64', '/optimize+',
        f'/out:{setup}', f'/win32manifest:{ROOT / "installer/app.manifest"}',
        f'/resource:{payload},payload.zip', '/r:System.Windows.Forms.dll',
        '/r:System.Drawing.dll', '/r:System.IO.Compression.dll',
        '/r:System.IO.Compression.FileSystem.dll', str(ROOT / 'installer/Setup.cs'), str(ROOT / 'installer/LocalRuntime.cs'), str(ROOT / 'installer/I18n.cs')], check=True)
    records = {name: {'sha256': sha(source), 'bytes': source.stat().st_size}
               for name, source in files.items()}
    manifest = {'version': VERSION, 'runtime_version': RUNTIME_VERSION, 'files': records,
        'fsr_disabled_on_install': True, 'fsr_previous_choice_restored_on_uninstall': True,
        'installer_languages': ['es', 'en', 'pt-BR'],
        'publication_status': 'cleared' if args.public else 'private-review-only',
        'optiscaler_included': False, 'nvidia_runtime_included': False,
        'nvidia_runtime_source': 'local NVIDIA NGX cache or active driver package only',
        'downloads_supported': False, 'backup_libraries_installed': False,
        'local_runtime_policy': 'newest signed production DLSS present at each game start; one working copy, never a private fallback',
        'nvidia_sdk_commit': '374959484e79a640feaba44c93ac8cfb0a03f5b5',
        'game_validation': 'VALIDACION.txt; beta limitations in LEEME.txt'}
    (out / 'package-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    public = [setup, out / 'package-manifest.json']
    documents.update({'VALIDACION.txt': ROOT / 'installer/VALIDACION.txt',
                      'LEGAL.md': ROOT / 'publication/LEGAL.md'})
    for name, source in documents.items():
        target = out / name
        target.write_bytes(source.read_bytes())
        public.append(target)
    sums = out / 'SHA256SUMS.txt'
    sums.write_text(''.join(sha(p) + '  ' + p.name + '\n' for p in public), encoding='utf-8')
    suffix = '-GitHub.zip' if args.public else '-PRIVATE-CANDIDATE.zip'
    bundle = out.parent / ('GroundedDLSS-' + VERSION + suffix)
    with zipfile.ZipFile(bundle, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for p in public + [sums]:
            z.write(p, p.name)
    print('Installer:', setup)
    print('Archive:', bundle)
    print('NVIDIA runtime embedded: no. Uploaded: no.')
    print('SHA256:', sha(bundle))

if __name__ == '__main__':
    main()
