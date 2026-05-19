#ifndef INVENESIS_JSONHELPERS_H
#define INVENESIS_JSONHELPERS_H

#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QByteArray>

namespace JsonHelpers {

    /**
     * @brief Converts ANY QJsonValue to stable compact bytes for sorting/comparison.
     */
    QByteArray jBytes(const QJsonValue &v);

    /**
     * @brief Returns a deterministically ordered QJsonObject.
     */
    QJsonObject canonObject(const QJsonObject &in);

    /**
     * @brief Returns an array sorted by canonical byte string.
     */
    QJsonArray canonArray(const QJsonArray &in);

    /**
     * @brief Dispatches a QJsonValue to its appropriate canonicalizer.
     */
    QJsonValue canonJson(const QJsonValue &v);

    /**
     * @brief Canonicalises a QJsonObject for deterministic writing.
     */
    QJsonObject canonicalise(const QJsonObject &obj);

    /**
     * @brief Deep equality check, ignoring order and numeric-string mismatch.
     */
    bool jsonEqual(const QJsonValue &a, const QJsonValue &b);

} // namespace JsonHelpers

#endif // INVENESIS_JSONHELPERS_H
