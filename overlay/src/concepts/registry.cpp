#include "concepts/concepts.hpp"

namespace hui::app {

std::span<const ConceptFactory> concept_registry() {
    static constexpr ConceptFactory factories[] = {concepts::make_ps5discord};
    return factories;
}

} // namespace hui::app
