/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 or (at your option)
 *  version 3 of the License.
 */

#ifndef KEEPASSXC_TESTAPPLICATIONLOGO_H
#define KEEPASSXC_TESTAPPLICATIONLOGO_H

#include <QObject>

class TestApplicationLogo : public QObject
{
    Q_OBJECT
private slots:
    void cleanup();
    void importsValidatedLocalImageAndPersistsOnlyDerivedPath();
    void rejectsInvalidAndOversizedSourcesWithoutReplacingActiveLogo();
    void fitAndBackgroundRegenerateThenReset();
    void secondWriteFailureKeepsPriorLogoAndSettings();
    void presentationFailureKeepsPriorSettings();
    void resetFailureKeepsActiveLogo();
    void linkedCacheDirectoryIsRefusedWithoutTouchingExternalTarget();
};

#endif // KEEPASSXC_TESTAPPLICATIONLOGO_H
