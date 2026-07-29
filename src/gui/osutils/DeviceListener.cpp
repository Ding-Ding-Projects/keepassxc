/*
 * Copyright (C) 2023 KeePassXC Team <team@keepassxc.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 or (at your option)
 * version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "DeviceListener.h"
#include <QTimer>

DeviceListener::DeviceListener(QWidget* parent)
    : QWidget(parent)
{
}

DeviceListener::~DeviceListener()
{
}

void DeviceListener::connectSignals(DEVICELISTENER_IMPL* listener)
{
    connect(listener, &DEVICELISTENER_IMPL::devicePlugged, this, [&](bool state, void* ctx, void* device) {
        // Wait a few ms to prevent USB device access conflicts
        QTimer::singleShot(50, this, [&] { emit devicePlugged(state, ctx, device); });
    });
}

DeviceListener::Handle
DeviceListener::registerHotplugCallback(bool arrived, bool left, int vendorId, int productId, const QUuid* deviceClass)
{
    auto* listener = new DEVICELISTENER_IMPL(this);
    const auto handle = reinterpret_cast<Handle>(listener);
    m_listeners[handle] = listener;
    m_listeners[handle]->registerHotplugCallback(arrived, left, vendorId, productId, deviceClass);
    connectSignals(m_listeners[handle]);
    return handle;
}

void DeviceListener::deregisterHotplugCallback(Handle handle)
{
    if (m_listeners.contains(handle)) {
        m_listeners[handle]->deregisterHotplugCallback();
        m_listeners.remove(handle);
    }
}

void DeviceListener::deregisterAllHotplugCallbacks()
{
    while (!m_listeners.isEmpty()) {
        deregisterHotplugCallback(m_listeners.constBegin().key());
    }
}
