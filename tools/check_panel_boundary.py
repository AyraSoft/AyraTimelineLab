#!/usr/bin/env python3
"""Boundary audit. Runtime and graphical assertions live in NativePanelTests.cpp."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
model=(root/'modules/ayra_timeline_panel/TimelineModel.h').read_text()
surface=(root/'modules/ayra_timeline_panel/TimelinePanel.h').read_text()
demo=(root/'demo/TimelinePanelDemo.cpp').read_text()
cmake=(root/'CMakeLists.txt').read_text()
assert 'juce::' not in model
assert '#include <ayra_timeline/ayra_timeline_presentation.h>' in model
assert 'LabComponent' not in surface and 'LabModel' not in surface
assert 'TimelineRowResidency' in surface and 'controller.visitRows' in surface
assert 'layoutTimelineLaneChrome' in surface and 'getNumberOfClicks()==2' in surface
assert 'demo/TimelinePanelDemo.cpp' in cmake and '\n    app/LabComponent.cpp\n' not in cmake
assert demo.count('#include "../app/LabComponent.cpp"')==1
assert 'drawVMeter' in demo and 'drawAccentSpine' in demo and 'drawFlagToolbarWell' in demo
assert 'PanelClipWindow' in demo and 'capturePanelState' in demo
assert 'supportsDocumentEvents=false' in demo and 'std::erase_if(result.events' in demo
assert 'AudioDeviceManager' not in demo
assert 'legacyLab' in (root/'app/Main.cpp').read_text()
print('PASS provider, ownership, no-header and existing-artwork source boundaries')
