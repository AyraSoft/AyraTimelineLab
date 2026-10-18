namespace ayra::lab::clips
{

AudioClipComponent::AudioClipComponent (
    std::string clipId, AudioClipPresentationProvider& provider)
    : ClipComponent (std::move (clipId)),
      presentationProvider (provider)
{
}

void AudioClipComponent::paint (juce::Graphics& g)
{
    const auto visual = presentationProvider.makeAudioClipVisual (
        getClipId(), getBodyBounds(), getParentOrigin());
    getClipLookAndFeel().drawAudioClip (g, visual, *this);
}

AudioClipComponent::LookAndFeelMethods& AudioClipComponent::getClipLookAndFeel()
{
    if (auto* methods = dynamic_cast<LookAndFeelMethods*> (&getLookAndFeel()))
        return *methods;

    return ClipDefaultLookAndFeel::getDefaultInstance();
}

} // namespace ayra::lab::clips

