#ifndef KEEPASSXC_TESTMATERIALAPPEARANCE_H
#define KEEPASSXC_TESTMATERIALAPPEARANCE_H

#include <QObject>

class TestMaterialAppearance : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();
    void themeTypographyAndResponsiveControls();
    void regexFilteringAndRegistration();
    void elementOverridePersistenceAndReset();

private:
    QString m_theme;
    QString m_family;
    QString m_seed;
    QString m_density;
    double m_scale = 1.0;
    int m_weight = 400;
    QString m_overrides;
};

#endif
