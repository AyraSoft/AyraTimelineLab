/** Target per la creazione di un container Glue, risolti contro l'intero
    documento. Il viewport non puo' cambiare il perimetro dell'operazione. */
#pragma once
#include "TimelineModel.h"
namespace ayra::timeline_panel
{
inline Result glueContainerTargets(const Snapshot& snapshot, const Id& lane,
                                   const Id& clicked, const std::vector<EventRef>& selected,
                                   std::vector<EventRef>& output)
{
    output.clear();
    std::vector<const Event*> ordered;
    for(const auto& e:snapshot.events)if(e.ref.lane==lane)ordered.push_back(&e);
    std::sort(ordered.begin(),ordered.end(),[](const Event* a,const Event* b){
        return std::tie(a->start,a->end,a->ref.clip)<std::tie(b->start,b->end,b->ref.clip);});
    const auto at=std::find_if(ordered.begin(),ordered.end(),[&](const Event* e){return e->ref.clip==clicked;});
    if(at==ordered.end())return Result::fail("Clip non piu' disponibile");
    std::set<EventRef> wanted;
    for(const auto& ref:selected)if(ref.lane==lane) {
        if(std::none_of(ordered.begin(),ordered.end(),[&](const Event* e){return e->ref==ref;}))
            return Result::fail("Selezione clip non piu' valida");
        wanted.insert(ref);
    }
    if(wanted.size()<2) {
        wanted.clear();wanted.insert((*at)->ref);
        if(std::next(at)==ordered.end())return Result::fail("Nessuna clip successiva nella lane");
        wanted.insert((*std::next(at))->ref);
    }
    if(wanted.size()<2)return Result::fail("Servono almeno due clip nella stessa lane");
    for(const auto* e:ordered)if(wanted.contains(e->ref)) {
        if(e->locked)return Result::fail("Clip bloccata: raggruppamento annullato");
        output.push_back(e->ref);
    }
    return {};
}
} // namespace ayra::timeline_panel
