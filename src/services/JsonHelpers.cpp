#include "JsonHelpers.h"
#include <QJsonDocument>
#include <algorithm>

namespace JsonHelpers {

QByteArray jBytes(const QJsonValue &v) {
    switch (v.type()) {
    case QJsonValue::Object:
        return QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact);
    case QJsonValue::Array:
        return QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact);
    case QJsonValue::String:
        return v.toString().toUtf8();
    case QJsonValue::Double:
        return QByteArray::number(v.toDouble(), 'g', 16);
    case QJsonValue::Bool:
        return v.toBool() ? "true" : "false";
    default: /* Null / Undefined */
        return "null";
    }
}

QJsonObject canonObject(const QJsonObject &in) {
    QJsonObject out;
    QStringList keys = in.keys();
    std::sort(keys.begin(), keys.end(), [](const QString &a, const QString &b) {
        return a.localeAwareCompare(b) < 0;
    });
    for (const QString &k : keys)
        out.insert(k, canonJson(in.value(k)));
    return out;
}

QJsonArray canonArray(const QJsonArray &in) {
    QList<QJsonValue> lst;
    lst.reserve(in.size());
    for (const QJsonValue &v : in)
        lst.append(canonJson(v));

    std::sort(lst.begin(), lst.end(),
              [](const QJsonValue &a, const QJsonValue &b) {
                  return jBytes(a) < jBytes(b);
              });

    QJsonArray out;
    for (const QJsonValue &v : std::as_const(lst))
        out.append(v);
    return out;
}

QJsonValue canonJson(const QJsonValue &v) {
    if (v.isObject())
        return canonObject(v.toObject());
    if (v.isArray())
        return canonArray(v.toArray());
    /* treat empty string and null as equivalent */
    if (v.isString() && v.toString().trimmed().isEmpty())
        return QJsonValue();
    return v; // primitive number/bool/null or undefined
}

QJsonObject canonicalise(const QJsonObject &obj) {
    return canonObject(obj);
}

bool jsonEqual(const QJsonValue &a, const QJsonValue &b) {
    const QJsonValue ca = canonJson(a), cb = canonJson(b);

    if (ca.type() != cb.type()) {
        /* tolerate number <-> string if content matches */
        if ((ca.isDouble() && cb.isString()) || (ca.isString() && cb.isDouble()))
            return ca.toString() == cb.toString();
        return false;
    }
    return jBytes(ca) == jBytes(cb);
}

} // namespace JsonHelpers
