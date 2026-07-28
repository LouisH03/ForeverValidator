#include "format/pack/block_info_catalog/replay_decoration_size.h"

#include <iostream>

namespace {

bool Check(bool condition, const char *message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

bool TestExpandedMapSelectionAndAmbiguity() {
    CatalogDecorationSizeDefinition candidate;
    candidate.mapSize = {45u, 36u, 45u};
    candidate.defaultZoneHeight = 8u;
    candidate.selectedDecorationPath = "Bay\\ConstructionDecoration\\Bay.TMDecoration.Gbx";

    const GmNat3 serializedMapSize{100u, 100u, 100u};
    const std::optional<CatalogDecorationSizeDefinition> selected =
            SelectReplayDecorationSizeCandidate(
                    candidate, 1u, serializedMapSize);
    bool okay = Check(
            selected.has_value() && selected->mapSize.x == 100u &&
                    selected->mapSize.y == 100u && selected->mapSize.z == 100u &&
                    selected->defaultZoneHeight == 8u &&
                    selected->selectedDecorationPath ==
                            candidate.selectedDecorationPath,
            "expanded replay grid did not override nominal decoration size");

    okay &= Check(
            !SelectReplayDecorationSizeCandidate(
                    candidate, 2u, serializedMapSize),
            "ambiguous decoration candidates were accepted");
    return okay;
}

}  // namespace

int main() {
    return TestExpandedMapSelectionAndAmbiguity() ? 0 : 1;
}
