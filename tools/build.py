"""Build in this workspace. This script never installs a plugin into the game."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import shutil
import sys

ROOT = Path(__file__).resolve().parents[1]
TARGETS = {'framegen-camera-test': ('tests/framegen_camera.cpp', 'framegen-camera-test.exe', False), 'dlss-fsr-framegen': ('src/grounded/runtime.cpp', 'GroundedDLSS.asi', True), 'framegen-fsr-present': ('tests/framegen_fsr_present.cpp', 'framegen-fsr-present.exe', False), 'framegen-fsr-dispatch': ('tools/framegen_fsr_dispatch.cpp', 'framegen-fsr-dispatch.exe', False), 'local-runtime-test': ('tests/local_runtime.cpp', 'local-runtime-test.exe', False), 'loader-compat': ('tests/loader_compat.cpp', 'loader-compat.exe', False), 'loader-early': ('tests/loader_early.cpp', 'loader-early.exe', False), 'focus-abi-test': ('tests/focus_abi.cpp', 'focus-abi-test.exe', False), 'focus-time-test': ('tests/focus_time.cpp', 'focus-time-test.exe', False), 'loader': ('src/loader/dxgi_proxy.cpp', 'dxgi.dll', True), 'loader-smoke': ('tests/loader_smoke.cpp', 'loader-smoke.exe', False), 'scale-presets': ('tests/scale_presets.cpp', 'scale-presets.exe', False), 'viewport-scale': ('tests/viewport_scale.cpp', 'viewport-scale.exe', False), 'sharpness-control': ('tests/sharpness_control.cpp', 'sharpness-control.exe', False), 'preset-policy': ('tests/preset_policy.cpp', 'preset-policy.exe', False), 'native-temporal-probe-test': ('tests/native_temporal_bridge.cpp', 'native-temporal-probe-test.exe', False), 'dlss': ('src/grounded/runtime.cpp', 'GroundedDLSS.asi', True)}


def find_vcvars():
    configured = os.environ.get("GROUNDED_VCVARS")
    if configured:
        return Path(configured)
    vswhere = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
    if vswhere.exists():
        result = subprocess.run([str(vswhere), "-latest", "-products", "*", "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"], capture_output=True, text=True, check=True)
        if result.stdout.strip():
            return Path(result.stdout.strip()) / "VC/Auxiliary/Build/vcvars64.bat"
    return Path(r"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("target", choices=TARGETS, nargs="?", default="dlss-fsr-framegen")
    args = parser.parse_args()
    vcvars = find_vcvars()
    if not vcvars.is_file():
        parser.error("MSVC not found. Set GROUNDED_VCVARS to vcvars64.bat.")
    source, output_name, dll = TARGETS[args.target]
    output_dir = ROOT / "build" / args.target
    output_dir.mkdir(parents=True, exist_ok=True)
    # cmd is needed only for the Visual Studio environment. Reject shell tokens.
    for path in (ROOT, vcvars):
        if any(char in str(path) for char in '%!&|<>^"\r\n'):
            parser.error("Build path contains unsupported cmd metacharacters.")
    output = output_dir / output_name
    # LINK replaces its import library through a temporary sibling. Remove only
    # these reproducible intermediates before rebuilding in the managed workspace.
    if dll:
        for suffix in (".lib", ".exp"):
            output.with_suffix(suffix).unlink(missing_ok=True)
    command = ["cl.exe", "/nologo", "/std:c++17", "/O2", "/EHsc", "/W4", "/DWIN32_LEAN_AND_MEAN", "/DNOMINMAX"]
    if args.target == "framegen-nvidia-check":
        command += [f"/I{ROOT / 'external/framegen/streamline-2.14.16/include'}"]
    if args.target in ("framegen-fsr-check", "framegen-fsr-dispatch", "dlss-fsr-framegen", "framegen-fsr-present"):
        command += [f"/I{ROOT / 'external/framegen/fsr-2.3.0/Kits/FidelityFX/api/include'}",
                    f"/I{ROOT / 'external/framegen/fsr-2.3.0/Kits/FidelityFX/framegeneration/include'}"]
    if args.target == "renderdoc-api":
        command.append("/DRENDERDOC_PLATFORM_WIN32")
        import pefile
        runtime = ROOT / "external/analysis/renderdoc-1.46/RenderDoc_1.46_64/renderdoc.dll"
        pe = pefile.PE(str(runtime), fast_load=True)
        pe.parse_data_directories(directories=[0])
        definition = output_dir / "renderdoc.def"
        definition.write_text("LIBRARY renderdoc.dll\nEXPORTS\n" + "".join(s.name.decode("ascii") + "\n" for s in pe.DIRECTORY_ENTRY_EXPORT.symbols if s.name))
        library = output_dir / "renderdoc.lib"
        library.unlink(missing_ok=True)
        library.with_suffix(".exp").unlink(missing_ok=True)
        import_command = ["lib.exe", "/nologo", f"/def:{definition}", "/machine:x64", f"/out:{library}"]
        subprocess.run(f'call "{vcvars}" >nul && {subprocess.list2cmdline(import_command)}', shell=True, cwd=output_dir, check=True)
        command += [str(library)]
        # Capture children keep the SDK DLL loaded; an identical existing copy
        # needs no replacement when only rebuilding this separate API client.
        runtime_copy = output_dir / "renderdoc.dll"
        if not runtime_copy.exists() or runtime_copy.read_bytes() != runtime.read_bytes():
            shutil.copy2(runtime, runtime_copy)
    if dll:
        command.append("/LD")
    if args.target == 'loader-early':
        command.append('d3d11.lib')
    if args.target == "dlss-gpu":
        command.append("/DGROUNDED_INTEGRATION_TEST")
    if args.target == "upstream-probe":
        command.append("/DGROUNDED_UPSTREAM_PROBE")
    if args.target == 'focus-probe':
        command.append('/DGROUNDED_FOCUS_PROBE')
    if args.target == "dlss-native-test":
        command.append("/DGROUNDED_NATIVE_BRIDGE_TEST")
    if args.target == "dlss-capture":
        command.append("/DGROUNDED_FRAME_CAPTURE")
    if args.target in ("dlss-framegen-probe", "dlss-framegen-scene-probe", "dlss-framegen-color-capture", "dlss-fsr-framegen"):
        command.append("/DGROUNDED_FRAMEGEN_PROBE")
    if args.target in ("dlss-framegen-scene-probe", "dlss-framegen-color-capture", "dlss-fsr-framegen"):
        command.append("/DGROUNDED_FRAMEGEN_SCENE_PROBE")
    if args.target == "dlss-framegen-color-capture":
        command.append("/DGROUNDED_FRAMEGEN_COLOR_CAPTURE")
    if args.target == "dlss-fsr-framegen":
        command.append("/DGROUNDED_FSR_FRAMEGEN")
    sources = [str(ROOT / source)]
    if args.target in ('dlss', 'dlss-native-test', 'dlss-capture', 'dlss-framegen-probe', 'dlss-framegen-scene-probe', 'dlss-framegen-color-capture', 'dlss-fsr-framegen', 'focus-abi-test'):
        obj = output_dir / 'focus_step.obj'
        assemble = ['ml64.exe', '/nologo', '/c', f'/Fo{obj}', str(ROOT / 'src/grounded/focus_step.asm')]
        subprocess.run(f'call "{vcvars}" >nul && {subprocess.list2cmdline(assemble)}', shell=True, cwd=output_dir, check=True)
        command += [str(obj), '/MT']
        if args.target == 'focus-abi-test':
            fixture_obj = output_dir / 'focus_abi_asm.obj'
            assemble = ['ml64.exe', '/nologo', '/c', f'/Fo{fixture_obj}', str(ROOT / 'tests/focus_abi.asm')]
            subprocess.run(f'call "{vcvars}" >nul && {subprocess.list2cmdline(assemble)}', shell=True, cwd=output_dir, check=True)
            command += [str(fixture_obj)]
    if args.target == "loader":
        assembly = ROOT / 'src/loader/dxgi_proxy.asm'
        obj = output_dir / 'dxgi_proxy_asm.obj'
        assemble = ['ml64.exe', '/nologo', '/c', f'/Fo{obj}', str(assembly)]
        subprocess.run(f'call "{vcvars}" >nul && {subprocess.list2cmdline(assemble)}', shell=True, cwd=output_dir, check=True)
        command += ['/MT', str(obj)]
    if args.target == "probe":
        sources.append(str(ROOT / "src/render_probe/feedback.cpp"))
        sources.append(str(ROOT / "src/render_probe/vtable_abi.cpp"))
    if args.target in ("dlss", "dlss-native-test", "dlss-capture", "dlss-framegen-probe", "dlss-framegen-scene-probe", "dlss-framegen-color-capture", "dlss-fsr-framegen", "dlss-smoke", "dlss-gpu"):
        sources.append(str(ROOT / "src/grounded/integration.cpp"))
    command += sources + [f"/Fo{output_dir.as_posix()}/", f"/Fe{output}", "d3d12.lib", "dxgi.lib", "user32.lib", "gdi32.lib", "d3dcompiler.lib", "/link", "/INCREMENTAL:NO"]
    if args.target == 'loader':
        command += [f'/DEF:{ROOT / "src/loader/dxgi_proxy.def"}']
    shell_command = f'call "{vcvars}" >nul && {subprocess.list2cmdline(command)}'
    result = subprocess.run(shell_command, shell=True, cwd=output_dir)
    if result.returncode:
        return result.returncode
    if args.target in ('loader-early', 'loader-compat'):
        shutil.copy2(ROOT/'build/loader/dxgi.dll',output_dir/'dxgi.dll')
    import hashlib
    manifest = {"target": args.target, "source": source, "output": str(output), "sha256": hashlib.sha256(output.read_bytes()).hexdigest(), "deployed": False}
    (output_dir / "build.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Built: {output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
