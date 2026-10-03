#ifndef KEEPASSXC_MATERIALMENU_H
#define KEEPASSXC_MATERIALMENU_H

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QStringList>

class QAction;
class QApplication;
class QLabel;
class QMenu;
class QWidget;
class QWidgetAction;

namespace Material
{
    class RegexBuilder;
    class SearchBar;
    class Select;

    /** Adds local search without cloning, reparenting or replacing command actions. */
    class MenuSearch : public QObject
    {
        Q_OBJECT
    public:
        static void install(QApplication* application);
        static MenuSearch* attach(QMenu* menu);
        static bool openBuilderFor(SearchBar* bar);
        static void resizePopup(QMenu* menu);
        SearchBar* searchBar() const;
        QStringList history() const;
        int resultCount() const;
        void refresh();

    protected:
        bool eventFilter(QObject* watched, QEvent* event) override;

    private:
        explicit MenuSearch(QMenu* menu);
        void prepare();
        void filter();
        void restore();
        void showBuilder();
        void restoreBuilderWidth();
        void remember();
        void updateStatus(const QString& error = {});
        bool isCommand(QAction* action) const;
        bool canActivate(QAction* action) const;
        void moveSelection(int direction);

        QPointer<QMenu> m_menu;
        QPointer<SearchBar> m_search;
        QPointer<Select> m_select;
        QPointer<QWidgetAction> m_searchAction;
        QPointer<QWidgetAction> m_statusAction;
        QPointer<QLabel> m_status;
        QPointer<QWidget> m_opener;
        QPointer<QWidget> m_builderContainer;
        QPointer<QWidgetAction> m_builderAction;
        QPointer<RegexBuilder> m_builder;
        QHash<QAction*, bool> m_originalVisibility;
        QSet<QAction*> m_matches;
        QStringList m_history;
        bool m_ownsSearch = false;
        bool m_changing = false;
        bool m_prepared = false;
        bool m_builderOpen = false;
        int m_popupMinimumWidth = 0;
        int m_popupMaximumWidth = 16777215;
        int m_resultCount = 0;
        quint64 m_popupGeneration = 0;
    };
}

#endif
