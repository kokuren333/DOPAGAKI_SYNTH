"""Package an already validated Windows build; never install into a DAW directory."""
from pathlib import Path
import hashlib
import re
import zipfile

root = Path(__file__).resolve().parent.parent
version = re.search(r'project\(dopagaki_synth VERSION ([\d.]+)', (root / 'CMakeLists.txt').read_text()).group(1)
bundle = root / 'build/VST3/Release/dopagaki_synth.vst3'
dll = bundle / 'Contents/x86_64-win/dopagaki_synth.vst3'
info = bundle / 'Contents/Resources/moduleinfo.json'
# The official SDK writer permits trailing commas in moduleinfo.json.
versions = re.findall(r'"Version"\s*:\s*"([^"]+)"', info.read_text(encoding='utf-8'))
if not dll.is_file() or not versions or any(v != version for v in versions):
    raise SystemExit('Build the matching Release VST3 before packaging.')
output = root / 'dist' / f'dopagaki_synth-{version}-dev-win64.zip'
output.parent.mkdir(exist_ok=True)
files = [(p, Path(bundle.name) / p.relative_to(bundle)) for p in bundle.rglob('*') if p.is_file()]
# Keep source/reference material that is suitable for public distribution out of
# the release when it contains third-party product references.
excluded_public = {
    'README.md',
    'ASTRA_GOAL_SEEK_SYNTH_SPEC.md',
    'docs/FEATURE_MATRIX.md',
    'docs/HANDOFF.md',
    'docs/IMPLEMENTATION_STATUS.md',
    'docs/QUALITY_PLAN.md',
    'docs/UI_V04.md',
    'docs/VALIDATION.md',
    'docs/WORK_LOG.md',
}
for name in ('LICENSE', 'LICENSES'):
    path = root / name
    paths = [path] if path.is_file() else [p for p in path.rglob('*') if p.is_file()]
    files.extend((p, p.relative_to(root)) for p in paths
                 if p.relative_to(root).as_posix() not in excluded_public)
with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
    for path, relative in sorted(files):
        archive.write(path, relative.as_posix())
with zipfile.ZipFile(output) as archive:
    if archive.testzip() is not None:
        raise SystemExit('Archive integrity check failed.')
    assert archive.read('dopagaki_synth.vst3/Contents/x86_64-win/dopagaki_synth.vst3') == dll.read_bytes()
digest = hashlib.sha256(output.read_bytes()).hexdigest()
output.with_suffix('.zip.sha256').write_text(f'{digest}  {output.name}\n', encoding='ascii')
print(f'{output}\n{output.stat().st_size} bytes / {len(files)} files\nSHA256 {digest}')
