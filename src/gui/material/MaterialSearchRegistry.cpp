#include "MaterialSearchRegistry.h"

#include "MaterialSearchBar.h"

#include <QCoreApplication>

namespace Material
{
    SearchRegistry::SearchRegistry(QObject* parent)
        : QObject(parent)
    {
    }

    SearchRegistry* SearchRegistry::instance()
    {
        static SearchRegistry* registry = new SearchRegistry(QCoreApplication::instance());
        return registry;
    }

    bool SearchRegistry::registerBar(SearchBar* bar)
    {
        if (!bar || bar->searchId().isEmpty() || bar->searchLabel().isEmpty()) {
            return false;
        }
        const auto existing = m_bars.value(bar->searchId());
        if (existing && existing != bar) {
            return false;
        }
        m_bars.insert(bar->searchId(), bar);
        connect(bar, &SearchBar::builderRequested, this, [this, bar] {
            setCurrent(bar);
            emit builderRequested(bar);
        });
        return true;
    }

    void SearchRegistry::unregisterBar(SearchBar* bar)
    {
        if (!bar) return;
        if (m_bars.value(bar->searchId()) == bar) {
            m_bars.remove(bar->searchId());
        }
        if (m_current == bar) {
            m_current.clear();
            emit currentChanged(nullptr);
        }
    }

    SearchBar* SearchRegistry::bar(const QString& id) const { return m_bars.value(id); }

    QList<SearchBar*> SearchRegistry::bars() const
    {
        QList<SearchBar*> result;
        for (const auto& bar : m_bars) if (bar) result.append(bar);
        return result;
    }

    SearchBar* SearchRegistry::current() const { return m_current; }

    void SearchRegistry::setCurrent(SearchBar* bar)
    {
        if (bar && m_bars.value(bar->searchId()) != bar) return;
        if (m_current == bar) return;
        m_current = bar;
        emit currentChanged(bar);
    }

    QString SearchRegistry::currentLabel() const { return m_current ? m_current->searchLabel() : QString(); }
} // namespace Material
