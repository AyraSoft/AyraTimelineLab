namespace ayra::lab::clips
{

LineClipComponent::LineClipComponent (
    std::string clipId, LineClipPresentationProvider& provider)
    : ClipComponent (std::move (clipId)),
      presentationProvider (provider)
{
}

void LineClipComponent::paint (juce::Graphics& g)
{
    const auto visual = presentationProvider.makeLineClipVisual (
        getClipId(), getBodyBounds(), getParentOrigin());
    getClipLookAndFeel().drawLineClip (g, visual, *this);
}

LineClipComponent::LookAndFeelMethods& LineClipComponent::getClipLookAndFeel()
{
    if (auto* methods = dynamic_cast<LookAndFeelMethods*> (&getLookAndFeel()))
        return *methods;

    return ClipDefaultLookAndFeel::getDefaultInstance();
}

} // namespace ayra::lab::clips