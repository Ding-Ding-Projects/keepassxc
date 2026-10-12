/*
 *  Copyright (C) 2013 Francois Ferrand
 *  Copyright (C) 2017 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "BrowserEntryConfig.h"

#include "core/Entry.h"
#include "core/Global.h"
#include "core/Tools.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariant>

static const char KEEPASSXCBROWSER_NAME[] = "KeePassXC-Browser Settings";

BrowserEntryConfig::BrowserEntryConfig(QObject* parent)
    : QObject(parent)
{
}

QStringList BrowserEntryConfig::allowedHosts() const
{
    return m_allowedHosts.values();
}

void BrowserEntryConfig::setAllowedHosts(const QStringList& allowedHosts)
{
    m_allowedHosts = Tools::asSet(allowedHosts);
}

QStringList BrowserEntryConfig::deniedHosts() const
{
    return m_deniedHosts.values();
}

void BrowserEntryConfig::setDeniedHosts(const QStringList& deniedHosts)
{
    m_deniedHosts = Tools::asSet(deniedHosts);
}

bool BrowserEntryConfig::isAllowed(const QString& host) const
{
    return m_allowedHosts.contains(host);
}

void BrowserEntryConfig::allow(const QString& host)
{
    m_allowedHosts.insert(host);
    m_deniedHosts.remove(host);
}

bool BrowserEntryConfig::isDenied(const QString& host) const
{
    return m_deniedHosts.contains(host);
}

void BrowserEntryConfig::deny(const QString& host)
{
    m_deniedHosts.insert(host);
    m_allowedHosts.remove(host);
}

QString BrowserEntryConfig::realm() const
{
    return m_realm;
}

void BrowserEntryConfig::setRealm(const QString& realm)
{
    m_realm = realm;
}

bool BrowserEntryConfig::load(const Entry* entry)
{
    m_allowedHosts.clear();
    m_deniedHosts.clear();
    m_realm.clear();
    if (!entry->customData()->contains(KEEPASSXCBROWSER_NAME)) {
        return false;
    }
    const auto doc = QJsonDocument::fromJson(entry->customData()->value(KEEPASSXCBROWSER_NAME).toUtf8());
    if (!doc.isObject()) {
        return false;
    }
    const auto object = doc.object();
    for (const auto& key : {QStringLiteral("Allow"), QStringLiteral("Deny")}) {
        const auto value = object.value(key);
        if (value.isUndefined()) {
            continue;
        }
        if (!value.isArray()) {
            return false;
        }
        QStringList hosts;
        for (const auto& host : value.toArray()) {
            if (!host.isString()) {
                return false;
            }
            hosts.append(host.toString());
        }
        if (key == QStringLiteral("Allow")) {
            setAllowedHosts(hosts);
        } else {
            setDeniedHosts(hosts);
        }
    }
    const auto realm = object.value(QStringLiteral("Realm"));
    if (!realm.isUndefined() && !realm.isString()) {
        return false;
    }
    setRealm(realm.toString());
    return true;
}

void BrowserEntryConfig::save(Entry* entry)
{
    QVariantMap v = Tools::qo2qvm(this);
    QJsonObject o = QJsonObject::fromVariantMap(v);
    QByteArray json = QJsonDocument(o).toJson(QJsonDocument::Compact);
    entry->customData()->set(KEEPASSXCBROWSER_NAME, json);
}
