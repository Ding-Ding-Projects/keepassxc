#ifndef KEEPASSXC_MATERIALCHOICEPOPUP_H
#define KEEPASSXC_MATERIALCHOICEPOPUP_H

#include <QModelIndex>
#include <QPointer>
#include <QWidget>

class QAbstractItemModel;
class QLabel;
class QListView;
class QTimer;

namespace Material
{
    class ComboBox;
    class ChoiceProxy;
    class RegexBuilder;
    class SearchBar;

    // An independent view of a combo's source model. No choice data is copied
    // into another model, persisted, logged or supplied to the regex workbench.
    class ChoicePopup final : public QWidget
    {
        Q_OBJECT
    public:
        explicit ChoicePopup(ComboBox* owner);
        ~ChoicePopup() override;
        static void installAccessibility();
        void open();

    signals:
        void choiceActivated(const QPersistentModelIndex& index);
        void dismissed();

    protected:
        bool eventFilter(QObject* watched, QEvent* event) override;
        void hideEvent(QHideEvent* event) override;
        void paintEvent(QPaintEvent* event) override;

    private:
        bool bindingIsCurrent() const;
        void refresh();
        void activateCurrent();
        void activateIndex(const QModelIndex& index);
        void moveSelection(int direction);
        void showBuilder();
        void positionPopup();
        void updateCopy();
        void updateTheme();
        void announceState();

        QPointer<ComboBox> m_owner;
        QPointer<QAbstractItemModel> m_source;
        QPersistentModelIndex m_root;
        bool m_hadRoot = false;
        int m_column = 0;
        ChoiceProxy* m_proxy = nullptr;
        SearchBar* m_search = nullptr;
        QListView* m_list = nullptr;
        QLabel* m_status = nullptr;
        QWidget* m_builderContainer = nullptr;
        RegexBuilder* m_builder = nullptr;
        QTimer* m_bindingTimer = nullptr;
        QList<QMetaObject::Connection> m_connections;
        bool m_refreshing = false;
        bool m_closing = false;
        QString m_error;
    };
}

#endif
