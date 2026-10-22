"""Export an explicit source allowlist, without SDK headers or research data."""
from pathlib import Path
import argparse, hashlib, json, re, zipfile
from build_release import ROOT, VERSION, require_clearance

def source_files():
    # Runtime dependencies only. No probes, disassembly, captures, tools or saves.
    headers = ['contract', 'integration', 'prepare_shader', 'gpu_identity', 'preset_policy',
        'scale_presets', 'scale_control', 'viewport_scale', 'sharpness_control',
        'focus_control', 'focus_time', 'native_memory', 'native_ngx',
        'native_temporal_bridge', 'native_temporal_signatures', 'frame_capture', 'local_dlss']
    names = ['LICENSE', '.gitignore', 'src/grounded/runtime.cpp',
        'src/grounded/integration.cpp', 'src/grounded/focus_step.asm',
        'src/render_probe/pipeline_stream.h', 'src/loader/dxgi_proxy.cpp',
        'src/loader/dxgi_proxy.asm', 'src/loader/dxgi_proxy.def',
        'installer/Setup.cs', 'installer/LocalRuntime.cs', 'installer/I18n.cs', 'installer/app.manifest', 'installer/GroundedDLSS.ini',
        'installer/LEEME.en.txt', 'installer/LEEME.pt-BR.txt', 'installer/TERMINOS.en.txt', 'installer/TERMINOS.pt-BR.txt',
        'installer/LEEME.txt', 'installer/TERMINOS.txt', 'installer/VALIDACION.txt',
        'installer/THIRD-PARTY-NOTICES.txt', 'licenses/NVIDIA-RTX-SDK-LICENSE.txt',
        'tools/build.py', 'tools/build_release.py', 'tools/build_source_release.py',
        'tools/fetch_sdk.py', 'tools/sdk-sources.json', 'tools/review_legal_release.py',
        'publication/clearance.json', 'publication/LEGAL.md',
        'src/framegen/fsr_runtime.h', 'src/framegen/camera.h',
        'src/framegen/scene_probe.h', 'src/framegen/presentation_probe.h',
        'src/framegen/hudless_capture.h',
        'licenses/AMD-FidelityFX-SDK-LICENSE.txt', 'tools/fetch_framegen_sdks.py',
        'tests/framegen_camera.cpp', 'tests/framegen_fsr_present.cpp',
        'tools/framegen_fsr_dispatch.cpp', 'tests/installer_framegen.cs',
        'tests/local_runtime.cpp', 'tests/installer.cs', 'tests/installer_review.cs', 'tests/installer_fsr.cs', 'tests/installer_languages.cs', 'tests/installer_runtime.cs', 'tests/installer_cache.cs',
        'tests/scale_presets.cpp', 'tests/viewport_scale.cpp', 'tests/sharpness_control.cpp',
        'tests/preset_policy.cpp', 'tests/focus_time.cpp', 'tests/focus_abi.cpp',
        'tests/focus_abi.asm', 'tests/native_temporal_bridge.cpp',
        'tests/loader_compat.cpp', 'tests/loader_early.cpp', 'tests/loader_smoke.cpp']
    names += ['src/grounded/' + name + '.h' for name in headers]
    files = {name: (ROOT / name).read_bytes() for name in names}
    files['README.md'] = (ROOT / 'publication/README.md').read_bytes()
    files['LEGAL.md'] = (ROOT / 'publication/LEGAL.md').read_bytes()
    files['THIRD-PARTY-NOTICES.txt'] = (ROOT / 'installer/THIRD-PARTY-NOTICES.txt').read_bytes()
    # The public build entry point offers only included targets. Research targets
    # are not part of the release and must not point at nonpublished tools.
    allowed = {'local-runtime-test', 'loader', 'dlss', 'scale-presets', 'viewport-scale', 'sharpness-control',
               'dlss-fsr-framegen', 'framegen-camera-test', 'framegen-fsr-present', 'framegen-fsr-dispatch',
               'preset-policy', 'focus-time-test', 'focus-abi-test',
               'native-temporal-probe-test', 'loader-compat', 'loader-early', 'loader-smoke'}
    import build
    selected = {name: spec for name, spec in build.TARGETS.items() if name in allowed}
    text = files['tools/build.py'].decode('utf-8').replace('\r\n', '\n')
    start = text.index('TARGETS = {')
    end = text.index('\n\n\ndef find_vcvars', start)
    text = text[:start] + 'TARGETS = ' + repr(selected) + text[end:]
    text = text.replace('default="probe"', 'default="dlss-fsr-framegen"')
    files['tools/build.py'] = text.encode('utf-8')
    return files

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--public', action='store_true')
    args = parser.parse_args()
    if args.public:
        require_clearance()
    files = source_files()
    for name, data in files.items():
        if name.startswith(('external/', 'evidence/', 'snapshots/', 'config/')) or Path(name).suffix in {'.dll', '.asi', '.exe', '.obj', '.dmp'}:
            raise RuntimeError('Non-source publication entry: ' + name)
        if re.search(rb'[A-Za-z]:[\\/]+Users[\\/]+[^\\/\s"\x27]+', data):
            raise RuntimeError('Personal workspace path in source export: ' + name)
    files['source-manifest.json'] = (json.dumps({name: hashlib.sha256(data).hexdigest()
        for name, data in files.items()}, indent=2) + '\n').encode('utf-8')
    out = ROOT / 'dist/candidates'
    out.mkdir(parents=True, exist_ok=True)
    suffix = '-Source.zip' if args.public else '-PRIVATE-SOURCE.zip'
    target = out / ('GroundedDLSS-' + VERSION + suffix)
    with zipfile.ZipFile(target, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for name, data in files.items():
            z.writestr(name, data)
    print('Source archive:', target)
    print('Entries:', len(files), 'SDK headers: none. Uploaded: no.')

if __name__ == '__main__':
    main()
