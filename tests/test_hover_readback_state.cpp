#include "ui/HoverReadbackState.hpp"
#include "vulkan/PixelReadbackConversion.hpp"

#include <iostream>

#include <cmath>

int main() {
    HoverReadbackState state;
    state.moveTo(10, 20);
    const auto firstId = state.requestId();
    state.moveTo(11, 21);
    state.moveTo(12, 22);
    const auto latestId = state.requestId();
    state.moveTo(12, 22);
    if (!state.needsSubmit() || latestId != firstId + 2 ||
        state.requestId() != latestId || state.x() != 12 || state.y() != 22) {
        return 1;
    }

    // One timer tick submits only the latest coordinate. Completion for either
    // older coordinate must not replace it while the GPU catches up.
    state.markSubmitted();
    quantiloom::PixelReading stale;
    stale.requestId = latestId - 1;
    stale.x = 11;
    stale.y = 21;
    if (state.accepts(stale) || !state.active()) return 2;

    quantiloom::PixelReading wrongCoordinate;
    wrongCoordinate.requestId = latestId;
    wrongCoordinate.x = 12;
    wrongCoordinate.y = 23;
    if (state.accepts(wrongCoordinate) || !state.active()) return 3;

    quantiloom::PixelReading current;
    current.requestId = latestId;
    current.x = 12;
    current.y = 22;
    if (!state.accepts(current)) return 4;
    state.markComplete();
    if (state.active()) return 5;
    state.moveTo(12, 22);
    if (state.active() || state.requestId() != latestId) return 6;

    state.markFailed();
    if (state.active()) return 7; // an empty/unready scene must stop the timer
    state.moveTo(12, 22);
    if (!state.needsSubmit() || state.requestId() != latestId + 1) return 8;

    // Leaving the viewport invalidates an in-flight completion even if its
    // coordinates happen to match the next place the cursor visits.
    state.moveTo(30, 40);
    const auto leavingId = state.requestId();
    state.markSubmitted();
    state.reset();
    quantiloom::PixelReading afterLeave;
    afterLeave.requestId = leavingId;
    afterLeave.x = 30;
    afterLeave.y = 40;
    if (state.valid() || state.active() || state.accepts(afterLeave)) return 9;

    // The async path receives raw radiance. Its IR presentation must use the
    // same band and thermography inversion as the preserved explicit read.
    constexpr double expectedKelvin = 315.0;
    glm::vec4 raw{};
    raw.r = static_cast<float>(quantiloom::BlackbodyBandRadiance(
        8000.0, 12000.0, expectedKelvin));
    const auto kelvin = vkview::apparentTemperatureK(
        quantiloom::SpectralMode::LWIR_Fused, raw, {});
    if (!kelvin || std::abs(*kelvin - expectedKelvin) > 0.1) return 10;
    if (vkview::apparentTemperatureK(
            quantiloom::SpectralMode::RGB, raw, {}).has_value()) return 11;

    std::cout << "Hover readback coalescing PASS\n";
    return 0;
}
