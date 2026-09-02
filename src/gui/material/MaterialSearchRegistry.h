#ifndef KEEPASSXC_MATERIALSEARCHREGISTRY_H
#define KEEPASSXC_MATERIALSEARCHREGISTRY_H

#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>

namespace Material
{
    class SearchBar;

    class SearchRegistry : public QObject
    {
        Q_OBJECT

    public:
        static SearchRegistry* instance();
        bool registerBar(SearchBar* bar);
        void unregisterBar(SearchBar* bar);
        SearchBar* bar(const QString& id) const;
        QList<SearchBar*> bars() const;
        SearchBar* current() const;
        void setCurrent(SearchBar* bar);
        QString currentLabel() const;
        void restoreCurrentFocus() const;

    signals:
        void builderRequested(Material::SearchBar* bar);
        void currentChanged(Material::SearchBar* bar);

    private:
        explicit SearchRegistry(QObject* parent = nullptr);
        QHash<QString, QPointer<SearchBar>> m_bars;
        QPointer<SearchBar> m_current;
        /** The bar that last asked for the builder; restoreCurrentFocus() returns there. */
        QPointer<SearchBar> m_builderOwner;
    };
} // namespace Material

#endif
