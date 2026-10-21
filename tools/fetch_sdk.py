"""Fetch build headers from official NVIDIA repositories; never fetch a runtime."""
from pathlib import Path
import argparse, hashlib, json, urllib.request, urllib.parse

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--accept-nvidia-sdk-license', action='store_true')
    args = parser.parse_args()
    if not args.accept_nvidia_sdk_license:
        parser.error('Read licenses/NVIDIA-RTX-SDK-LICENSE.txt first. If you accept it, '
                     'run again with --accept-nvidia-sdk-license. No files downloaded.')
    manifest = json.loads((ROOT / 'tools/sdk-sources.json').read_text(encoding='utf-8'))
    fetched = []
    for name, spec in manifest['files'].items():
        relative = Path(name)
        if relative.is_absolute() or '..' in relative.parts or relative.suffix != '.h':
            raise RuntimeError('Unexpected SDK manifest path')
        target = ROOT / 'external' / relative
        if target.is_file() and hashlib.sha256(target.read_bytes()).hexdigest() == spec['sha256']:
            continue
        url = spec['url']
        parsed = urllib.parse.urlparse(url)
        if parsed.scheme != 'https' or parsed.netloc != 'raw.githubusercontent.com' or not parsed.path.startswith(('/NVIDIA/DLSS/', '/NVIDIA/nvapi/')):
            raise RuntimeError('Unexpected SDK origin')
        request = urllib.request.Request(url, headers={'User-Agent': 'GroundedDLSS-source-build'})
        with urllib.request.urlopen(request, timeout=45) as response:
            data = response.read(4_000_001)
        if len(data) > 4_000_000 or hashlib.sha256(data).hexdigest() != spec['sha256']:
            raise RuntimeError('SDK file differs from the pinned official version: ' + name)
        fetched.append((target, data))
    # Validate every download before writing any header.
    for target, data in fetched:
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    print('Pinned official SDK headers verified. Runtime DLL not downloaded.')

if __name__ == '__main__':
    main()
