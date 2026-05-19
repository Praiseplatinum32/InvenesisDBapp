#ifndef INVENESIS_TECANWINDOW_H
#define INVENESIS_TECANWINDOW_H
/**
 *  @file tecanwindow.h
 *  @brief Main UI class driving the Tecan automation interface.
 *
 *  Refactored April 2025 – functionality preserved, structure & comments improved.
 */

#include <QWidget>
#include <QJsonObject>
#include <QSet>
#include <memory>          // std::unique_ptr
#include <QTabWidget>
#include <QComboBox>
#include "plate_management/matrixplatecontainer.h"
#include "TecanViewModel.h"

QT_BEGIN_NAMESPACE
class QSqlQueryModel;
class QVBoxLayout;
QT_END_NAMESPACE

namespace Ui { class TecanWindow; }

/**
 * @class TecanWindow
 * @brief Main window controlling test-request handling, plate layouts and GWL generation.
 */
class TecanWindow final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TecanWindow)

public:
    explicit TecanWindow(QWidget *parent = nullptr);
    ~TecanWindow() override;

    /** Load selected test requests into the UI. */
    void loadTestRequests(const QStringList &requestIDs);

signals:
    void backRequested();

private slots:
    void on_clearPlatesButton_clicked();
    void on_actionSave_triggered();
    void on_actionLoad_triggered();
    void on_actionGenerate_GWL_triggered();
    void on_actionCreate_Plate_Map_triggered();

    /** Checkable button below "Clear" – unchecked: 96-well, checked: 384-well. */
    void on_switchPlate_toggled(bool checked);

    void onQcSelectionChanged(const QString &qcName);

    bool markTestRequestsDoneFromJson(const QJsonObject &experimentJson, QString *errOut = nullptr);


private:            /* ---------- helper types ---------- */
private:            /* ---------- helper GUI ----------
                       (all raw-pointers are Qt-owned)   */
    Ui::TecanWindow       *ui = nullptr;
    QStandardItemModel     *testRequestModel = nullptr;
    QStandardItemModel     *compoundQueryModel = nullptr;

    MatrixPlateContainer  *matrixPlateContainer = nullptr;

    // Daughter Plates
    QWidget               *daughterPlatesContainerWidget = nullptr;
    QVBoxLayout           *daughterPlatesLayout = nullptr;

    // Test Plates
    QWidget               *testPlatesContainerWidget = nullptr;
    QVBoxLayout           *testPlatesLayout = nullptr;

    // QC Plates
    QWidget               *qcPlatesContainerWidget = nullptr;
    QVBoxLayout           *qcPlatesLayout = nullptr;
    QComboBox             *qcSelectionCombo = nullptr;

    std::unique_ptr<TecanViewModel> m_viewModel;

private:            /* ---------- query helpers ---------- */
    void querySolutionsFromTestRequests();
    void querySolutions(const QSet<QString> &compoundNames);
    int  resolveCompoundDuplicates(const QString &compoundName,
                                  const QList<QVariantMap> &duplicateSolutions);
    void populateCompoundTable(const QList<int> &solutionIds);

private:            /* ---------- plate helpers ---------- */
    void populateDaughterPlates(int dilutionSteps,
                                const QStringList& compoundList,
                                const QString& testType);

    void populateTestPlates(int dilutionSteps, const QString& testType, const QList<QMap<QString,QStringList>>& daughterPlates);
    void loadQcPlatesConfig();

    /// Rebuild daughter plates from whatever models are currently shown in the UI.
    void rebuildDaughterPlatesFromModels();

    /* ------ JSON (de)serialisation helpers ------ */
    void loadTestRequestsFromJson(const QJsonArray &array);
    void loadCompoundsFromJson(const QJsonArray &array);
    void loadMatrixPlatesFromJson(const QJsonObject &obj);
    void loadDaughterPlatesFromJson(const QJsonArray &array, bool readOnly);
    void generateGWLFromJson(const QJsonObject &experimentJson);
    void generateExperimentAuxiliaryFiles(const QJsonObject &experimentJson,
                                          const QString &outputFolder);

    /* ---------- convenience QMessageBox wrappers ---------- */
    static void showInfo   (QWidget *parent, const QString &title,
                         const QString &msg);
    static void showWarning(QWidget *parent, const QString &title,
                            const QString &msg);
    static void showError  (QWidget *parent, const QString &title,
                          const QString &msg);
};

#endif // INVENESIS_TECANWINDOW_H
