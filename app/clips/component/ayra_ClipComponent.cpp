namespace ayra::lab::clips
{

ClipComponent::ClipComponent (std::string clipId)
    : id (std::move (clipId))
{
    // Il canvas resta l'unico owner delle gesture durante questa estrazione.
    setInterceptsMouseClicks (false, false);
}

void ClipComponent::setDecorationOverflows (const float top, const float bottom, const float horizontal)
{
    currentTopOverflow = top;
    currentBottomOverflow = bottom;
    currentHorizontalOverflow = horizontal;
}

void ClipComponent::setBodyBoundsInParent (const juce::Rectangle<float> bodyBounds)
{
    const auto visualBounds = bodyBounds.withLeft (bodyBounds.getX() - currentHorizontalOverflow)
                                        .withRight (bodyBounds.getRight() + currentHorizontalOverflow)
                                        .withTop (bodyBounds.getY() - currentTopOverflow)
                                        .withBottom (bodyBounds.getBottom() + currentBottomOverflow);
    const auto integerBounds = visualBounds.getSmallestIntegerContainer();

    localBodyBounds = bodyBounds.translated (-static_cast<float> (integerBounds.getX()),
                                             -static_cast<float> (integerBounds.getY()));
    setBounds (integerBounds);
}

} // namespace ayra::lab::clips
