#!/usr/bin/env python3
"""Assemble a self-contained HTML from reviewable CSS/JS and shared fixtures."""
from pathlib import Path
import json
root=Path(__file__).resolve().parents[1]
s=(root/'web/index.template.html').read_text(encoding='utf-8')
values={'THEME':(root/'generated/theme.css').read_text(encoding='utf-8'),'CSS':(root/'web/style.css').read_text(encoding='utf-8'),'JS':(root/'web/app.js').read_text(encoding='utf-8'),'SESSION':json.dumps(json.loads((root/'fixtures/session.json').read_text(encoding='utf-8')),ensure_ascii=False).replace('<','\\u003c')}
for k,v in values.items():s=s.replace('/*__'+k+'__*/',v)
(root/'web/ayra-header-lab.html').write_text(s, encoding='utf-8')
print('Wrote web/ayra-header-lab.html')
