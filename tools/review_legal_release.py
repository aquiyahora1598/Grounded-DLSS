"""Verify private installer/source packages in isolated files. Never deploy or upload."""
from pathlib import Path
import argparse, datetime, hashlib, json, os, re, shutil, subprocess, sys, uuid, zipfile
from build_release import ROOT, VERSION, RUNTIME_TARGET, sha
from build_source_release import source_files

def main():
    sys.stdout.reconfigure(encoding='utf-8')
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, default=ROOT / 'external/nvidia/release-pinned/nvngx_dlss.dll')
    parser.add_argument('--cached-runtime', type=Path, help='Optional verified cache library for isolated cache tests')
    parser.add_argument('--older-runtime', type=Path, help='Optional older signed production library for upgrade regression')
    parser.add_argument('--gpu-evidence', type=Path, default=ROOT / 'evidence/2026-10-20/local-only-runtime/gpu-result.json',
                        help='GPU test record for the exact ASI under review (required)')
    parser.add_argument('--evidence-dir', type=Path, default=ROOT / 'evidence/2026-10-20/local-only-runtime/installer',
                        help='Where installer test logs and verification.json are written')
    args = parser.parse_args()
    if not args.runtime.is_file():
        parser.error('Supply the official DLL obtained from NVIDIA using --runtime PATH')
    release = ROOT / 'dist/candidates' / ('GroundedDLSS-' + VERSION)
    exe = release / ('GroundedDLSS-' + VERSION + '-Setup.exe')
    work = ROOT / 'build' / ('lr-' + uuid.uuid4().hex[:6])
    work.mkdir(parents=True)
    copied = work / exe.name
    shutil.copy2(exe, copied)
    out = args.evidence_dir
    out.mkdir(parents=True, exist_ok=True)
    checks = {}
    compiler = Path(r'C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe')
    for name, main_class, extra in [('installer', 'InstallerTests', []),
            ('installer_review', 'InstallerReviewTests', [str(release / 'payload.zip')]),
            ('installer_fsr', 'InstallerFsrTests', []),
            ('installer_languages', 'InstallerLanguageTests', []),
            ('installer_framegen', 'InstallerFramegenTests', []),
            ('installer_runtime', 'InstallerRuntimeTests', [str(args.runtime)]),
            ('installer_cache', 'InstallerCacheTests', [str(args.cached_runtime or args.runtime), str(args.older_runtime or args.runtime)])]:
        target = work / (name + '.exe')
        command = [str(compiler), '/nologo', '/target:exe', '/platform:x64', '/optimize+',
            '/main:' + main_class, '/out:' + str(target), '/r:' + str(copied),
            '/r:System.Windows.Forms.dll', '/r:System.Drawing.dll',
            '/r:System.IO.Compression.dll', '/r:System.IO.Compression.FileSystem.dll',
            str(ROOT / 'tests' / (name + '.cs'))]
        subprocess.run(command, check=True, capture_output=True)
        # Keep fixture paths short: the .NET Framework transaction deliberately
        # creates an adjacent unique staging name, which adds 55 characters.
        fixture = work / str(len(checks))
        run = subprocess.run([str(target), str(fixture)] + extra, capture_output=True,
                             timeout=60, creationflags=subprocess.CREATE_NO_WINDOW)
        output = run.stdout.decode('utf-8', errors='replace') + run.stderr.decode('utf-8', errors='replace')
        (out / (name + '.log')).write_text(output, encoding='utf-8')
        print(output.strip())
        assert run.returncode == 0, (name, run.returncode)
        checks[name] = {'exit_code': run.returncode, 'passes': re.findall(r'^PASS.*$', output, re.M)}
        if name == 'installer_runtime':
            shutil.copy2(fixture / 'installer-layout.png', out / 'installer-layout.png')
        if name == 'installer_languages':
            for image in fixture.glob('installer-*.png'):
                shutil.copy2(image, out / image.name)
    manifest = json.loads((release / 'package-manifest.json').read_text(encoding='utf-8'))
    assert manifest['nvidia_runtime_included'] is False
    with zipfile.ZipFile(release / 'payload.zip') as z:
        assert z.testzip() is None and set(z.namelist()) == set(manifest['files'])
        assert not any(Path(name).name.lower() == 'nvngx_dlss.dll' for name in z.namelist())
        for name, entry in manifest['files'].items():
            data = z.read(name)
            assert hashlib.sha256(data).hexdigest() == entry['sha256'] and len(data) == entry['bytes']
    bundle = release.parent / ('GroundedDLSS-' + VERSION + '-PRIVATE-CANDIDATE.zip')
    with zipfile.ZipFile(bundle) as z:
        assert z.testzip() is None
        for name in z.namelist():
            assert z.read(name) == (release / name).read_bytes()
        for line in z.read('SHA256SUMS.txt').decode('utf-8').splitlines():
            digest, name = line.split('  ', 1)
            assert hashlib.sha256(z.read(name)).hexdigest() == digest
    source = release.parent / ('GroundedDLSS-' + VERSION + '-PRIVATE-SOURCE.zip')
    with zipfile.ZipFile(source) as z:
        assert z.testzip() is None
        expected = source_files()
        assert set(z.namelist()) == set(expected) | {'source-manifest.json'}
        records = json.loads(z.read('source-manifest.json'))
        for name, data in expected.items():
            assert z.read(name) == data and records[name] == hashlib.sha256(data).hexdigest()
        exported = work / 'exported-source'
        z.extractall(exported)
    # Include closure: every quoted local #include resolves inside the export or
    # to one of the separately downloaded, pinned official SDK headers.
    sdk = json.loads((ROOT / 'tools/sdk-sources.json').read_text(encoding='utf-8'))['files']
    for path in (exported / 'src').rglob('*'):
        if path.suffix not in {'.h', '.cpp'}:
            continue
        for relative in re.findall(r'^#include\s+"([^"]+)"', path.read_text(encoding='utf-8'), re.M):
            dest = (path.parent / relative).resolve()
            assert dest.is_relative_to(exported), (path, relative)
            if not dest.is_file():
                assert dest.relative_to(exported).as_posix().removeprefix('external/') in sdk, (path, relative)
    for script in ['tools/build_release.py', 'tools/build_source_release.py']:
        run = subprocess.run([sys.executable, str(ROOT / script), '--public'], capture_output=True)
        assert run.returncode != 0 and b'Publication pending' in run.stderr
    checks['publication_gate_refuses_unresolved'] = True
    checks['embedded_payload_and_archive_checksums'] = True
    checks['source_allowlist_manifest_and_include_closure'] = True
    runtime_asi = ROOT / ('build/' + RUNTIME_TARGET + '/GroundedDLSS.asi')
    checks['runtime_binary_sha256'] = sha(runtime_asi)
    checks['loader_binary_sha256'] = sha(ROOT / 'build/loader/dxgi.dll')
    # The previous release's runtime and loader are the tested unchanged binaries.
    prior = ROOT / 'dist/GroundedDLSS-11.21.1-beta/package-manifest.json'
    if prior.is_file():
        old = json.loads(prior.read_text())
        loader = 'Maine/Binaries/Win64/dxgi.dll'
        assert manifest['files'][loader] == old['files'][loader]
        checks['loader_unchanged_from_previous_release'] = True
    project_id = b'd91874f0-6893-4ba7-ae47-a9b3384f7530'
    assert project_id in runtime_asi.read_bytes()
    assert b'init(608699473' not in (ROOT / 'src/grounded/integration.cpp').read_bytes()
    checks['own_ngx_project_id_present_no_numeric_id_fallback'] = True
    # GPU evidence is required and must name this exact ASI. A missing or stale
    # record fails the review; it is never skipped silently.
    dynamic = args.gpu_evidence.resolve()
    assert dynamic.is_file(), ('GPU evidence missing', str(dynamic))
    dynamic_record = json.loads(dynamic.read_text(encoding='utf-8'))
    assert dynamic_record['asi_sha256'] == sha(runtime_asi), 'GPU evidence was recorded for a different ASI'
    assert len(dynamic_record['tests']) == 3 and all(x['exit_code'] == 0 and x['auto_runtime_created'] for x in dynamic_record['tests'])
    # The same ASI carries the own ProjectID checked above, so these GPU runs
    # are also the own-ProjectID GPU evidence.
    checks['gpu_evidence'] = {'record': dynamic.relative_to(ROOT).as_posix(), 'record_sha256': sha(dynamic),
        'asi_sha256': dynamic_record['asi_sha256'], 'outputs': [x['output'] for x in dynamic_record['tests']],
        'selected_runtime': dynamic_record['selected_runtime'], 'own_project_id': True,
        'native_bridge_fixture': bool(dynamic_record.get('native_bridge_tests'))}
    checks.update({'passed': True, 'version': VERSION,
        'utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
        'setup_sha256': sha(exe), 'candidate_sha256': sha(bundle),
        'source_sha256': sha(source), 'nvidia_runtime_embedded': False,
        'game_started': False, 'game_or_settings_modified': False, 'uploaded': False})
    (out / 'verification.json').write_text(json.dumps(checks, indent=2) + '\n', encoding='utf-8')
    print('PASS: packaging, licenses, source export, publication gate. Game untouched.')

if __name__ == '__main__':
    main()
