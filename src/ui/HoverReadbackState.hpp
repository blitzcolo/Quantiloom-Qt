#pragma once

#include <renderer/PixelReading.hpp>

/**
 * Latest-wins state for the viewport's asynchronous hover readback.
 *
 * GPU slots may finish after the cursor moved. The SDK rejects readings from
 * an old image; this additionally rejects a valid image read for an old cursor
 * position. MainWindow owns the timer and the SDK calls, while this class keeps
 * their request identity independent of UI timing.
 */
class HoverReadbackState {
public:
    void moveTo(int x, int y) {
        if (m_valid && x == m_x && y == m_y && !m_retrySameCoordinate) return;
        m_valid = true;
        m_x = x;
        m_y = y;
        ++m_requestId;
        m_needsSubmit = true;
        m_submitted = false;
        m_retrySameCoordinate = false;
    }

    void reset() {
        ++m_requestId;
        m_valid = false;
        m_x = -1;
        m_y = -1;
        m_needsSubmit = false;
        m_submitted = false;
        m_retrySameCoordinate = false;
    }

    [[nodiscard]] bool accepts(const quantiloom::PixelReading& reading) const {
        return m_valid && reading.requestId == m_requestId &&
               reading.x == static_cast<quantiloom::u32>(m_x) &&
               reading.y == static_cast<quantiloom::u32>(m_y);
    }

    void markSubmitted() {
        m_needsSubmit = false;
        m_submitted = true;
        m_retrySameCoordinate = false;
    }

    void markComplete() {
        m_submitted = false;
        m_retrySameCoordinate = false;
    }
    void markFailed() {
        m_needsSubmit = false;
        m_submitted = false;
        m_retrySameCoordinate = true;
    }

    [[nodiscard]] bool valid() const { return m_valid; }
    [[nodiscard]] bool needsSubmit() const { return m_needsSubmit; }
    [[nodiscard]] bool active() const { return m_needsSubmit || m_submitted; }
    [[nodiscard]] int x() const { return m_x; }
    [[nodiscard]] int y() const { return m_y; }
    [[nodiscard]] quantiloom::u64 requestId() const { return m_requestId; }

private:
    bool m_valid = false;
    bool m_needsSubmit = false;
    bool m_submitted = false;
    bool m_retrySameCoordinate = false;
    int m_x = -1;
    int m_y = -1;
    quantiloom::u64 m_requestId = 0;
};
