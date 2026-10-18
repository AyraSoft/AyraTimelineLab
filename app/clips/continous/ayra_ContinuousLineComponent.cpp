namespace ayra::lab::clips
{

ContinuousLineComponent::ContinuousLineComponent (
    std::string laneId, ContinuousLinePresentationProvider& provider)
    : ClipComponent (std::move (laneId)),
      presentationProvider (provider)
{
    setDecorationOverflows (0.0f, 0.0f, 0.0f);
}

void ContinuousLineComponent::paint (juce::Graphics& g)
{
    const auto visual = presentationProvider.makeContinuousLineVisual (
        getClipId(), getBodyBounds(), getParentOrigin());
    getClipLookAndFeel().drawContinuousLine (g, visual, *this);
}

ContinuousLineComponent::LookAndFeelMethods& ContinuousLineComponent::getClipLookAndFeel()
{
    if (auto* methods = dynamic_cast<LookAndFeelMethods*> (&getLookAndFeel()))
        return *methods;

    return ClipDefaultLookAndFeel::getDefaultInstance();
}

} // namespace ayra::lab::clips