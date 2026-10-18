#!/usr/bin/env python3
"""Package sources and evidence without builds, libraries, media or font files."""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess
import sys
import zipfile

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--output', type=Path, default=root.parent / 'Ayra_Timeline_Lab_CPP_JUCE.zip')
args = parser.parse_args()
subprocess.run([sys.executable, str(root / 'tools/verify_sources.py')], check=True)
skip = {'build', '__pycache__', '.git', '.cache'}
paths = [p for p in sorted(root.rglob('*')) if p.is_file() and not skip.intersection(p.relative_to(root).parts)]
source_paths = [p for p in paths if (p.suffix in {'.cpp', '.h', '.py', '.js', '.css', '.html', '.json'} or p.name=='CMakeLists.txt') and 'verification' not in p.relative_to(root).parts]
manifest = {
    'repositoryReference': 'AyraSoft/ayra_multiverse@5db35b89ddb73da6da2c5b6d3184f5a3878bda1b',
    'verification': 'See STATUS.md; native GUI and JUCE bridge NOT_RUN',
    'sources': {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest() for p in source_paths},
}
(root / 'verification/source-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
paths = [p for p in sorted(root.rglob('*')) if p.is_file() and not skip.intersection(p.relative_to(root).parts)]
with zipfile.ZipFile(args.output, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
    for p in paths:
        if p.suffix.lower() in {'.ttf', '.otf', '.woff', '.woff2', '.zip', '.pyc'}:
            raise SystemExit('Unexpected distributable: ' + str(p))
        z.write(p, Path('AyraTimelineLab') / p.relative_to(root))
print(f'Packaged {len(paths)} files: {args.output}')
