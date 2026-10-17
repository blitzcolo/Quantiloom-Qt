/**
 * @file CompactSpinBox.hpp
 * @brief A QDoubleSpinBox that shows the shortest text for its value
 *
 * QDoubleSpinBox sizes itself from its range and its decimals, and prints every
 * decimal it has. A field that has to hold 0 to 1e6 at two decimals, or keep
 * fifteen decimals so a value read back is not quantised, came out thirty
 * characters wide -- which set the minimum width of the dock around it -- and
 * showed a column of trailing zeros.
 *
 * This keeps the precision in the value and shows up to @c significant digits,
 * switching to exponent notation only where fixed would be longer. It reads
 * what it writes, exponents included, which the stock validator rejects.
 *
 * @author blitzcolo
 */

#pragma once

#include <QDoubleSpinBox>
#include <QLocale>
#include <QValidator>

#include <algorithm>

namespace uistyle {

class CompactDoubleSpinBox : public QDoubleSpinBox {
public:
    explicit CompactDoubleSpinBox(QWidget* parent = nullptr, int significant = 12)
        : QDoubleSpinBox(parent), m_significant(significant) {}

protected:
    [[nodiscard]] QString textFromValue(double value) const override {
        QString text = locale().toString(value, 'g', m_significant);
        // Spaces and group separators would not survive a round trip.
        text.remove(locale().groupSeparator());
        return text;
    }

    [[nodiscard]] double valueFromText(const QString& text) const override {
        bool ok = false;
        const double value = locale().toDouble(stripped(text), &ok);
        return ok ? std::clamp(value, minimum(), maximum()) : this->value();
    }

    [[nodiscard]] QValidator::State validate(QString& text, int& /*pos*/) const override {
        const QString body = stripped(text);
        if (body.isEmpty() || body == QStringLiteral("-") || body == QStringLiteral("+")) {
            return QValidator::Intermediate;
        }
        bool ok = false;
        const double value = locale().toDouble(body, &ok);
        if (!ok) {
            // "1e" or "1.5e-" on the way to a number.
            const QChar last = body.back();
            const bool partial = last == QLatin1Char('e') || last == QLatin1Char('E') ||
                                 last == QLatin1Char('-') || last == QLatin1Char('+') ||
                                 last == locale().decimalPoint().front();
            return partial ? QValidator::Intermediate : QValidator::Invalid;
        }
        return (value >= minimum() && value <= maximum()) ? QValidator::Acceptable
                                                          : QValidator::Intermediate;
    }

private:
    [[nodiscard]] QString stripped(const QString& text) const {
        QString body = text;
        if (!prefix().isEmpty() && body.startsWith(prefix())) body.remove(0, prefix().size());
        if (!suffix().isEmpty() && body.endsWith(suffix())) body.chop(suffix().size());
        return body.trimmed();
    }

    int m_significant;
};

}  // namespace uistyle
