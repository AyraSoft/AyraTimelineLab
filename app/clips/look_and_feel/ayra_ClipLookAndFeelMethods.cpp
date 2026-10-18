namespace ayra::lab::clips
{

void AudioClipComponent::LookAndFeelMethods::drawAudioClip (
    juce::Graphics& g, const AudioClipVisual& visual, AudioClipComponent& component)
{
    ClipDefaultLookAndFeel::getDefaultInstance().drawAudioClip (g, visual, component);
}

void MidiClipComponent::LookAndFeelMethods::drawMidiClip (
    juce::Graphics& g, const MidiClipVisual& visual, MidiClipComponent& component)
{
    ClipDefaultLookAndFeel::getDefaultInstance().drawMidiClip (g, visual, component);
}

void LineClipComponent::LookAndFeelMethods::drawLineClip (
    juce::Graphics& g, const LineClipVisual& visual, LineClipComponent& component)
{
    ClipDefaultLookAndFeel::getDefaultInstance().drawLineClip (g, visual, component);
}

void KeyframeLaneComponent::LookAndFeelMethods::drawKeyframeLane (
    juce::Graphics& g, const KeyframeLaneVisual& visual, KeyframeLaneComponent& component)
{
    ClipDefaultLookAndFeel::getDefaultInstance().drawKeyframeLane (g, visual, component);
}

void ContinuousLineComponent::LookAndFeelMethods::drawContinuousLine (
    juce::Graphics& g, const ContinuousLineVisual& visual, ContinuousLineComponent& component)
{
    ClipDefaultLookAndFeel::getDefaultInstance().drawContinuousLine (g, visual, component);
}

void ImageClipComponent::LookAndFeelMethods::drawImageClip (
    juce::Graphics& g, const ImageClipVisual& visual, ImageClipComponent& component)
{
    ClipDefaultLookAndFeel::getDefaultInstance().drawImageClip (g, visual, component);
}

ClipLookAndFeelMethods& getClipLookAndFeel (juce::Component& component)
{
    if (auto* methods = dynamic_cast<ClipLookAndFeelMethods*> (&component.getLookAndFeel()))
        return *methods;

    return ClipDefaultLookAndFeel::getDefaultInstance();
}

} // namespace ayra::lab::clips
