#!/usr/bin/env python3
"""Source boundary audit only. Does not compile or certify JUCE interactions."""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
model = (root/'modules/ayra_timeline_panel/TimelineModel.h').read_text()
surface = (root/'modules/ayra_timeline_panel/TimelinePanel.h').read_text()
demo = (root/'demo/TimelinePanelDemo.cpp').read_text()
cmake = (root/'CMakeLists.txt').read_text()
assert 'juce' not in model.lower().replace('no juce', '')
assert 'LabComponent' not in surface and 'LabModel' not in surface
assert 'demo/TimelinePanelDemo.cpp' in cmake
assert '\n    app/LabComponent.cpp\n' not in cmake
assert demo.count('#include "../app/LabComponent.cpp"') == 1
assert 'supportsDocumentEvents=false' in demo
assert 'std::erase_if(result.events' in demo
assert 'component->isMouseButtonDown(true)' in surface
assert 'Stale row session' in surface and 'Monitoring menu expired' in surface
assert 'ASSET' not in model and 'AudioDeviceManager' not in demo
assert 'legacyLab' in (root/'app/Main.cpp').read_text()
print('PASS source boundaries only; native compile/gestures NOT_RUN')
