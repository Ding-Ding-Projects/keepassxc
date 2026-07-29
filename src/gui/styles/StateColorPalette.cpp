/*
 *  Copyright (C) 2021 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 or (at your option)
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "StateColorPalette.h"

#include "gui/material/MaterialTheme.h"

StateColorPalette::StateColorPalette()
{
    resolveFromTheme();
}

void StateColorPalette::resolveFromTheme()
{
    using Material::Health;
    using Material::Role;

    const auto& scheme = theme()->colors();

    // The three status families, as used by validation and messages.
    setColor(ColorRole::Error, scheme.color(Role::Error));
    setColor(ColorRole::Warning, scheme.color(Role::Amber));
    setColor(ColorRole::Info, scheme.color(Role::Primary));
    // "Nearly right" belongs to the warning family, not the error one.
    setColor(ColorRole::Incomplete, scheme.color(Role::Amber));

    // Password health, off the design's health scale.
    setColor(ColorRole::HealthCritical, scheme.healthColor(Health::Breached));
    setColor(ColorRole::HealthBad, scheme.healthColor(Health::Weak));
    setColor(ColorRole::HealthPoor, scheme.healthColor(Health::Weak));
    setColor(ColorRole::HealthWeak, scheme.color(Role::Amber));
    setColor(ColorRole::HealthOk, scheme.healthColor(Health::Ok));
    setColor(ColorRole::HealthExcellent, scheme.color(Role::Green));

    setColor(ColorRole::True, scheme.color(Role::Green));
    setColor(ColorRole::False, scheme.color(Role::Error));
}
