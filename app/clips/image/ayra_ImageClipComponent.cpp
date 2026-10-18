namespace ayra::lab::clips
{

ImageClipComponent::ImageClipComponent (
    std::string clipId, ImageClipPresentationProvider& provider)
    : ClipComponent (std::move (clipId)),
      presentationProvider (provider)
{
}

void ImageClipComponent::paint (juce::Graphics& g)
{
    const auto visual = presentationProvider.makeImageClipVisual (
        getClipId(), getBodyBounds(), getParentOrigin());
    getClipLookAndFeel().drawImageClip (g, visual, *this);
}

ImageClipComponent::LookAndFeelMethods& ImageClipComponent::getClipLookAndFeel()
{
    if (auto* methods = dynamic_cast<LookAndFeelMethods*> (&getLookAndFeel()))
        return *methods;

    return ClipDefaultLookAndFeel::getDefaultInstance();
}

} // namespace ayra::lab::clips