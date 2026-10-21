# Compact MIDI note preview — 21 October 2026

Base: `48502a95c92ce10a8a974dc792bae45027ab0ab0`.
Implementation: `4d120989851426004333c142e0ffdb9281d4bf3f`.
Source tree: `bf4aefed246d40a7480f8f7aa4178eb778994ad7`.
Verification workflow: https://github.com/AyraSoft/AyraTimelineLab/actions/runs/39580784363
Artifact: `midi-preview-verification` (12336591014).

## Defect and change

The compact lane's 18px clip body reserved 16px for a title. The 2px remaining
piano area conflicted with note geometry that forced a minimum 10px body and
3px notes. Some pitches were projected outside the clipped body; dark outlines
and title text further obscured the overview.

Preview now reserves a title band only when at least 16px remain for notes.
The minimum lane uses the complete body for a read-only note overview. Production
preview geometry fits the actual pitch range inside the available rectangle,
keeps short notes visible, and draws clear bars or drum diamonds. Editing geometry,
zoom presets, document data, velocity editors and cut/warp gestures are unchanged.
Clip widths below the existing outer painter's 4px cutoff are outside this fix;
this does not promise individual-note readability at arbitrarily small horizontal
zoom or in arbitrarily dense overlapping material.

## Executed on this revision

- GCC C++20 portable production geometry: 96,784 assertions PASS.
- Same geometry with AddressSanitizer and UndefinedBehaviorSanitizer: PASS.
- Official JUCE 8.0.12 Graphics production preview primitive: 1,284 pixel assertions,
  CTest 1/1 PASS, including 100/125/150/200% scale, extreme pitches and drum mode.
- Provider/source boundaries, three visual-adapter Python tests, generated-source
  consistency and patch whitespace: PASS.
- The above focused tests were executed locally on Linux and again by the linked
  GitHub Actions workflow on the exact implementation tree.

The PNG files in the artifact exercise the actual preview primitive in an isolated
fixture. They are NOT screenshots of the complete running application.

## Added but not executed here

`CanvasInteractionTests.cpp::midiPreviewPaintContracts` compares the full MIDI
painter with and without notes at 32/48/64/96px lane heights and checks that preview
note hit targets remain non-editable. This requires the complete native Lab build
with its external providers. That full target, the entire application and macOS/
Windows builds were NOT executed for this revision. Previous delivery's 11/11
native results must not be counted as verification of this change.

Run the complete native canvas suite before merging this PR into the application
baseline. No integration into another product or playback implementation is part
of this MIDI rendering fix.
