#ifndef KEEPASSXC_MATERIALTABOVERFLOW_H
#define KEEPASSXC_MATERIALTABOVERFLOW_H

#include "MaterialOverlay.h"
#include "MaterialTabDescriptor.h"

#include <QList>
#include <QSet>

class QLabel;
class QScrollArea;
class QVBoxLayout;

namespace Material
{
    class SearchBar;

    class TabOverflow : public Overlay
    {
        Q_OBJECT

    public:
        explicit TabOverflow(QWidget* parent = nullptr);
        void setTabs(const QList<TabDescriptor>& tabs,
                     const QString& currentRuntimeId,
                     const QSet<QString>& hiddenRuntimeIds);

    signals:
        void tabActivated(const QString& runtimeId);
        void tabPinRequested(const QString& runtimeId, bool pinned);

    protected:
        void aboutToOpen() override;

    private:
        void rebuild();
        void clearRows();

        QWidget* m_sheet = nullptr;
        SearchBar* m_search = nullptr;
        QScrollArea* m_scroll = nullptr;
        QVBoxLayout* m_rows = nullptr;
        QLabel* m_empty = nullptr;
        QList<TabDescriptor> m_tabs;
        QString m_current;
        QSet<QString> m_hidden;
    };
} // namespace Material

#endif
