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
    /// Start a new request for the pixel under the cursor. A cursor that did
    /// not move starts nothing -- unless the last read of this same pixel
    /// failed, in which case the move that never happened still owes a retry.
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

    /// The SDK only rejects readings taken from an old image, not ones
    /// answering an old request -- so both the request id and the
    /// coordinates must match before a result may reach the label.
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
    /// A read that failed or could not be submitted arms one retry of the
    /// same pixel: the next moveTo() re-requests it even if the cursor has
    /// not moved.
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
