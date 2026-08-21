/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 or (at your option)
 *  version 3 of the License.
 */

#ifndef KEEPASSXC_TESTMATERIALBREAKPOINTS_H
#define KEEPASSXC_TESTMATERIALBREAKPOINTS_H

#include <QObject>

class TestMaterialBreakpoints : public QObject
{
    Q_OBJECT

private slots:
    void boundaries();
    void capabilities();
};

#endif // KEEPASSXC_TESTMATERIALBREAKPOINTS_H
