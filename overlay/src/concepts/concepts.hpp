#pragma once

#include "app/concept.hpp"
#include <memory>

namespace hui::concepts {
std::unique_ptr<app::Concept> make_ps5discord(app::Context& context);
} // namespace hui::concepts
