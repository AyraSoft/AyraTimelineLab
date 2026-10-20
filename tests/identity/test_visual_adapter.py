"""Structural adapter tests; no private Multiverse implementation is bundled."""
from __future__ import annotations
import importlib.util
from pathlib import Path
import tempfile
import unittest
import hashlib

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('visual_adapter', ROOT / 'tools/adapt_multiverse_buttons.py')
assert spec and spec.loader
adapter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(adapter)
METHODS = ('drawButtonBackground', 'drawButtonText', 'getTextButtonFont', 'drawToggleButton',
           'drawTickBox', 'drawTabbedButtonBarBackground', 'drawTabButton')

class VisualAdapterTest(unittest.TestCase):
    def fixture(self) -> str:
        # An intentionally synthetic TU tests syntax routing, not paint parity.
        return '#include "ApplicationLookAndFeel.h"\n' + ''.join(
            f'{"juce::Font" if name == "getTextButtonFont" else "void"} ApplicationLookAndFeel::{name}() {{ /* fixture-{name} */ }}\n'
            for name in METHODS)

    def test_only_qualified_class_and_include_change(self):
        with tempfile.TemporaryDirectory() as d:
            source, dest = Path(d)/'source.cpp', Path(d)/'build/generated.cpp'
            original = self.fixture(); source.write_text(original, encoding='utf-8')
            adapter.adapt(source, dest)
            header, body = dest.read_text(encoding='utf-8').split('\n', 1)
            self.assertIn(hashlib.sha256(source.read_bytes()).hexdigest(), header)
            restored = body.replace('ayra::lab::SharedChromeLookAndFeel::', 'ApplicationLookAndFeel::').replace(
                '#include "SharedChromeLookAndFeel.h"', '#include "ApplicationLookAndFeel.h"')
            self.assertEqual(original, restored)
            self.assertEqual(original, source.read_text(encoding='utf-8'))

    def test_unknown_missing_or_duplicate_method_is_atomic_reject(self):
        for variant in (self.fixture().replace('drawTabButton', 'unknownPainter'),
                        self.fixture().splitlines()[0]+'\n',
                        self.fixture()+'void ApplicationLookAndFeel::drawTabButton() {}\n'):
            with self.subTest(variant=variant), tempfile.TemporaryDirectory() as d:
                source, dest = Path(d)/'source.cpp', Path(d)/'generated.cpp'
                source.write_text(variant, encoding='utf-8'); dest.write_text('previous', encoding='utf-8')
                with self.assertRaises(ValueError): adapter.adapt(source, dest)
                self.assertEqual('previous', dest.read_text(encoding='utf-8'))

    def test_missing_or_duplicate_include_rejected(self):
        for includes in ('', '#include "ApplicationLookAndFeel.h"\n#include "ApplicationLookAndFeel.h"'):
            with tempfile.TemporaryDirectory() as d:
                source, dest = Path(d)/'source.cpp', Path(d)/'generated.cpp'
                source.write_text(self.fixture().replace('#include "ApplicationLookAndFeel.h"', includes), encoding='utf-8')
                with self.assertRaises(ValueError): adapter.adapt(source, dest)
                self.assertFalse(dest.exists())

if __name__ == '__main__': unittest.main()
