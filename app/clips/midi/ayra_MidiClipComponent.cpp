namespace ayra::lab::clips
{

MidiClipComponent::MidiClipComponent (
    std::string clipId, MidiClipPresentationProvider& provider)
    : ClipComponent (std::move (clipId)),
      presentationProvider (provider)
{
}

void MidiClipComponent::paint (juce::Graphics& g)
{
    const auto visual = presentationProvider.makeMidiClipVisual (
        getClipId(), getBodyBounds(), getParentOrigin());
    getClipLookAndFeel().drawMidiClip (g, visual, *this);
}

MidiClipComponent::LookAndFeelMethods& MidiClipComponent::getClipLookAndFeel()
{
    if (auto* methods = dynamic_cast<LookAndFeelMethods*> (&getLookAndFeel()))
        return *methods;

    return ClipDefaultLookAndFeel::getDefaultInstance();
}

} // namespace ayra::lab::clips
