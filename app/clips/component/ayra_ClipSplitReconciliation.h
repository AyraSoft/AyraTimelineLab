namespace ayra::lab::clips
{
/** Rebuilds derived per-canvas state when another linked lane originated the
    document split. The lineage is projected by the document owner; canvas-local
    state never infers structural semantics from names or geometry. */
template<class SplitRange,class HasState,class ApplySplit>
void reconcileDerivedClipSplits(const SplitRange& splits,HasState&& hasState,ApplySplit&& applySplit)
{
    for(const auto& split:splits)
        if(hasState(split.leftClipId)&&!hasState(split.rightClipId))
            applySplit(split.leftClipId,split.rightClipId,split.fraction);
}
}
