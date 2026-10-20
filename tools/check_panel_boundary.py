#!/usr/bin/env python3
"""Boundary audit. Runtime and graphical assertions live in NativePanelTests.cpp."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
model=(root/'modules/ayra_timeline_panel/TimelineModel.h').read_text()
surface=(root/'modules/ayra_timeline_panel/TimelinePanel.h').read_text()
demo=(root/'demo/TimelinePanelDemo.cpp').read_text()
controls=(root/'demo/TimelineLaneControls.h').read_text()
cmake=(root/'CMakeLists.txt').read_text()
assert 'juce::' not in model
assert '#include <ayra_timeline/ayra_timeline_presentation.h>' in model
assert 'LabComponent' not in surface and 'LabModel' not in surface
assert 'TimelineRowResidency' in surface and 'controller.visitRows' in surface
assert 'layoutTimelineLaneChrome' in surface
assert 'NameInput' not in surface and 'nameEditor' not in surface and 'beginRename' not in surface
assert 'containsClipAt' in surface and 'containsEditableAt' in surface and 'PointerObserver' in surface
assert surface.count('private juce::Timer')==1
assert 'identityDue' in surface and 'scheduleIdentity' in surface
assert 'getIdentityReadout' in surface and 'setIdentityNavigation' in surface
status=(root/'demo/TimelineStatusArtwork.h').read_text()
assert 'nodeButtonGlyph' in status and 'nodeButtonActiveColour' in status
assert 'juce::Colour(0x' not in status and 'juce::Timer' not in status
for path in ('app/clips/continous/ayra_ContinuousLineCanvas.cpp','app/clips/keyframe/ayra_KeyframeLaneCanvas.cpp'):
    source=(root/path).read_text()
    assert source.index('containsPanelEditableAt')<source.index('private:')
assert 'Every canvas must expose its real editable-content hit geometry' in demo
assert 'TimelineIdentityHoverState' in surface and 'notifyCanvasContentChanged' in surface
assert 'demo/TimelinePanelDemo.cpp' in cmake and '\n    app/LabComponent.cpp\n' not in cmake
assert demo.count('#include "../app/LabComponent.cpp"')==1
assert 'drawVMeter' in demo and 'drawAccentSpine' in demo and 'drawFlagToolbarWell' in controls
assert 'PanelClipWindow' in demo and 'capturePanelState' in demo
assert 'supportsDocumentEvents=false' in demo and 'std::erase_if(result.events' in demo
assert controls.count('ayra::drawFlagToolbarWell(')==1
assert 'rewind' not in demo and 'glyphs::rewind' not in controls
assert 'rail.x+rail.width-2' not in surface
assert 'minimumTimelineLaneChromeHeight' in model
assert 'AudioDeviceManager' not in demo
assert 'legacyLab' in (root/'app/Main.cpp').read_text()
print('PASS provider, ownership, no-header and existing-artwork source boundaries')
