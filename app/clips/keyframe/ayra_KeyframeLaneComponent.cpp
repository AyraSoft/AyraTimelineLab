namespace ayra::lab::clips
{

KeyframeLaneComponent::KeyframeLaneComponent (
    std::string laneId,
    KeyframeLanePresentationProvider& provider,
    const bool spanToNextEvent,
    const KeyframeLaneType laneType)
    : ClipComponent (std::move (laneId)),
      presentationProvider (provider),
      isSpanKeyframeMode (spanToNextEvent && laneType != KeyframeLaneType::points),
      currentLaneType (spanToNextEvent ? laneType : KeyframeLaneType::points)
{
    setDecorationOverflows (0.0f, 0.0f, 0.0f);
}

void KeyframeLaneComponent::paint (juce::Graphics& g)
{
    const auto visual = presentationProvider.makeKeyframeLaneVisual (
        getClipId(), getBodyBounds(), getParentOrigin());
    getClipLookAndFeel().drawKeyframeLane (g, visual, *this);
}

KeyframeLaneComponent::LookAndFeelMethods& KeyframeLaneComponent::getClipLookAndFeel()
{
    if (auto* methods = dynamic_cast<LookAndFeelMethods*> (&getLookAndFeel()))
        return *methods;

    return ClipDefaultLookAndFeel::getDefaultInstance();
}

} // namespace ayra::lab::clips
