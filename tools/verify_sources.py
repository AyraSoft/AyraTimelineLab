#!/usr/bin/env python3
"""Check deliverable completeness and derived inputs. Does not compile the GUI."""
from pathlib import Path
import json
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
required = [
    'CMakeLists.txt', 'CMakePresets.json', 'README.md', 'NOTICE.md',
    'app/Main.cpp', 'app/LabComponent.h', 'app/LabComponent.cpp',
    'app/TrackHeader.h', 'app/TrackHeader.cpp',
    'app/LabLookAndFeel.h', 'app/LabLookAndFeel.cpp', 'app/CanonicalTheme.cpp',
    'app/SessionCodec.h', 'app/SessionCodec.cpp',
    'app/clips/ayra_clips.h', 'app/clips/ayra_clips.cpp',
    'app/clips/component/ayra_ClipComponent.h',
    'app/clips/component/ayra_ClipComponent.cpp',
    'app/clips/audio/ayra_AudioClipComponent.h',
    'app/clips/audio/ayra_AudioClipComponent.cpp',
    'app/clips/audio/ayra_AudioClipCanvas.cpp',
    'app/clips/visual/ayra_ClipVisual.h',
    'app/clips/look_and_feel/ayra_ClipLookAndFeelMethods.h',
    'app/clips/look_and_feel/ayra_ClipLookAndFeelMethods.cpp',
    'app/clips/look_and_feel/ayra_ClipDefaultLookAndFeel.h',
    'app/clips/look_and_feel/ayra_ClipDefaultLookAndFeel.cpp',
    'core/LabModel.h', 'core/LabModel.cpp',
    'fixtures/session.json', 'fixtures/theme.json',
    'web/ayra-header-lab.html', 'web/index.template.html', 'web/style.css', 'web/app.js',
    'tests/CoreTests.cpp', 'tests/CodecTests.cpp', 'tests/ClipComponentTests.cpp',
    'tests/browser_test.py',
    'docs/PROVENIENZA.md', 'docs/INTEGRAZIONE.md', 'docs/VERIFICHE_NATIVE.md',
    'verification/STATUS.md',
]
missing = [path for path in required if not (ROOT / path).is_file()]
if missing:
    raise SystemExit('Missing files: ' + ', '.join(missing))
subprocess.run([sys.executable, str(ROOT / 'tools/generate.py'), '--check'], check=True)
html = (ROOT / 'web/ayra-header-lab.html').read_text(encoding='utf-8')
if re.search(r'/\*__(?:THEME|CSS|JS|SESSION)__\*/', html):
    raise SystemExit('Unresolved HTML build tokens')
if re.search(r'<(?:script|link|img)\b[^>]*(?:src|href)=["\']https?://', html, re.I):
    raise SystemExit('An HTML resource requires the network')
fixture = json.loads((ROOT / 'fixtures/session.json').read_text(encoding='utf-8'))
embedded = json.loads(re.search(r'<script[^>]*id="lab-state"[^>]*>(.*?)</script>', html, re.S).group(1))
if fixture != embedded:
    raise SystemExit('Embedded HTML fixture differs from the canonical fixture')
for path in ROOT.rglob('*'):
    if 'build' in path.relative_to(ROOT).parts:
        continue
    if path.suffix.lower() in {'.ttf', '.otf', '.woff', '.woff2'}:
        raise SystemExit('Font file must not be distributed: ' + str(path))
print('PASS: source completeness, deterministic fixtures and standalone HTML resources')
