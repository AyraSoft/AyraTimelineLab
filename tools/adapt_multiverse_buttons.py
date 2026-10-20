#!/usr/bin/env python3
"""Build-directory-only adapter. Paint bodies stay byte-identical to the source.

This is not a copied renderer checked into the Lab. No private source is emitted
into the source tree or public repository; only an explicit checkout is read.
"""
from __future__ import annotations
import argparse
import hashlib
import pathlib
import re

def adapt(source: pathlib.Path, destination: pathlib.Path) -> None:
    text = source.read_text(encoding="utf-8")
    methods = re.findall(r"^(?:void|juce::Font)\s+ApplicationLookAndFeel::(\w+)\s*\(", text, re.MULTILINE)
    expected = {"drawButtonBackground", "drawButtonText", "getTextButtonFont", "drawToggleButton",
                "drawTickBox", "drawTabbedButtonBarBackground", "drawTabButton"}
    if set(methods) != expected or len(methods) != len(expected):
        raise ValueError(f"Unsupported Multiverse button contract: {methods}; update the adapter explicitly")
    include = '#include "ApplicationLookAndFeel.h"'
    if text.count(include) != 1:
        raise ValueError("Expected exactly one application include; refusing a silent fallback")
    adapted = text.replace(include, '#include "SharedChromeLookAndFeel.h"')
    adapted = adapted.replace("ApplicationLookAndFeel::", "ayra::lab::SharedChromeLookAndFeel::")
    destination.parent.mkdir(parents=True, exist_ok=True)
    provenance = f"// GENERATED from original Multiverse buttons; SHA256 {hashlib.sha256(source.read_bytes()).hexdigest()}\n"
    destination.write_text(provenance + adapted, encoding="utf-8")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("destination", type=pathlib.Path)
    args = parser.parse_args()
    try:
        adapt(args.source, args.destination)
    except (OSError, UnicodeError, ValueError) as error:
        parser.exit(1, f"Multiverse visual source rejected: {error}\n")
