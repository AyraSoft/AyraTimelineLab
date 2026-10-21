/** The lightweight note mark used by the read-only MIDI overview.
    JUCE graphics must be included by the consumer. No fonts, document or media. */
#pragma once
namespace ayra::lab::clips
{
inline void paintMidiPreviewNote(juce::Graphics& g, juce::Rectangle<float> bounds,
                                 float drumRadius, juce::Colour colour)
{
    if (bounds.isEmpty()) return;
    g.setColour(colour);
    if (drumRadius>0)
    {
        const auto c=bounds.getCentre();
        juce::Path diamond;
        diamond.startNewSubPath(c.x,c.y-drumRadius);
        diamond.lineTo(c.x+drumRadius,c.y);
        diamond.lineTo(c.x,c.y+drumRadius);
        diamond.lineTo(c.x-drumRadius,c.y);
        diamond.closeSubPath();
        g.fillPath(diamond);
    }
    else g.fillRoundedRectangle(bounds,0.5f);
}
}
