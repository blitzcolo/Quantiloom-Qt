#include "vulkan/ViewportSampleBatchScheduler.hpp"

#include <limits>

int main() {
    vkview::ViewportSampleBatchScheduler scheduler;
    scheduler.reset(2);
    if (scheduler.warmupCallbacks() != 3) return 1;
    for (int i = 0; i < 3; ++i) {
        scheduler.observe(0.25f);
        if (scheduler.recommended() != 1) return 2;
    }

    scheduler.observe(1.0f); if (scheduler.recommended() != 2) return 3;
    scheduler.observe(1.0f); if (scheduler.recommended() != 4) return 4;
    scheduler.observe(1.0f); if (scheduler.recommended() != 8) return 5;
    scheduler.observe(1.0f); if (scheduler.recommended() != 8) return 6;
    if (scheduler.chooseForFrame(32, 35, true, false, false) != 3) return 7;
    if (scheduler.chooseForFrame(0, 0, true, false, false) != 8) return 8;
    if (scheduler.chooseForFrame(0, 35, true, true, false) != 1) return 9;
    if (scheduler.chooseForFrame(0, 35, true, false, true) != 1) return 10;
    if (scheduler.chooseForFrame(0, 35, false, false, false) != 1) return 11;

    scheduler.observe(10.0f);
    if (scheduler.recommended() != 1) return 12;

    scheduler.reset(0);
    scheduler.observe(std::numeric_limits<float>::quiet_NaN());
    if (scheduler.recommended() != 1) return 13;
    for (int i = 0; i < 8; ++i) scheduler.observe(0.1f);
    if (scheduler.recommended() != 16) return 14;

    scheduler.reset(3);
    for (int i = 0; i < 4; ++i) scheduler.observe(0.1f);
    if (scheduler.recommended() != 1) return 15;
    return 0;
}
