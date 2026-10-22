#pragma once
#include "CaptionDocument.h"
#include <cmath>
#include <functional>
#include <unordered_map>

namespace ayra::media_edit {

struct AudioBus { Id id; };
struct AudioSend { Id sourceBus, targetBus; };
struct DuckingRoute {
    Id triggerBus, targetBus;
    double thresholdDb {-24.0}, ratio {4.0}, attackMs {10.0}, releaseMs {200.0};
};
struct RoutingPlan {
    std::vector<AudioBus> buses;
    std::vector<AudioSend> sends;
    std::vector<DuckingRoute> ducking;
};

// Topology/parameter contract only: this does not process or render any audio.
// A trigger is a control dependency, not an audible send. Both graphs must be
// acyclic together: a bus cannot indirectly detect its own processed output.
inline void validateRouting(const RoutingPlan& plan) {
    using detail::require;
    detail::uniqueIds(plan.buses);
    std::unordered_map<Id, std::vector<Id>> edges;
    for (const auto& bus : plan.buses) edges.emplace(bus.id, std::vector<Id>{});
    auto connect = [&](const Id& from, const Id& to) {
        require(edges.contains(from) && edges.contains(to), "Unknown audio bus");
        require(from != to, "Audio routing self-cycle");
        edges.at(from).push_back(to);
    };
    std::vector<std::pair<Id, Id>> sendPairs, duckPairs;
    for (const auto& send : plan.sends) {
        auto pair = std::pair{send.sourceBus, send.targetBus};
        require(std::find(sendPairs.begin(), sendPairs.end(), pair) == sendPairs.end(), "Duplicate audio send");
        sendPairs.push_back(pair);
        connect(send.sourceBus, send.targetBus);
    }
    for (const auto& route : plan.ducking) {
        require(std::isfinite(route.thresholdDb) && route.thresholdDb <= 0.0 &&
                std::isfinite(route.ratio) && route.ratio >= 1.0 &&
                std::isfinite(route.attackMs) && route.attackMs >= 0.0 &&
                std::isfinite(route.releaseMs) && route.releaseMs > 0.0,
                "Invalid ducking parameters");
        auto pair = std::pair{route.triggerBus, route.targetBus};
        require(std::find(duckPairs.begin(), duckPairs.end(), pair) == duckPairs.end(), "Duplicate ducking route");
        duckPairs.push_back(pair);
        connect(route.triggerBus, route.targetBus);
    }
    // Kahn traversal avoids recursion depth limits for imported routing graphs.
    std::unordered_map<Id, std::size_t> indegree;
    for (const auto& bus : plan.buses) indegree.emplace(bus.id, 0);
    for (const auto& [from, targets] : edges) {
        (void) from;
        for (const auto& to : targets) ++indegree.at(to);
    }
    std::vector<Id> ready;
    for (const auto& [id, count] : indegree) if (count == 0) ready.push_back(id);
    std::size_t visited = 0;
    while (!ready.empty()) {
        auto id = std::move(ready.back()); ready.pop_back(); ++visited;
        for (const auto& to : edges.at(id)) if (--indegree.at(to) == 0) ready.push_back(to);
    }
    require(visited == plan.buses.size(), "Audio routing contains a feedback cycle");
}

} // namespace ayra::media_edit
