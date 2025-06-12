#ifndef INVENESIS_TECANWINDOW_H
#define INVENESIS_TECANWINDOW_H
/**
 *  @file tecanwindow.h
 *  @brief Main UI class driving the Tecan automation interface.
 *
 *  Refactored April 2025 – functionality preserved, structure & comments improved.
 */

#include <QMainWindow>
#include <QJsonObject>
#include <QSet>
#include <memory>          // std::unique_ptr
#include "matrixplatecontainer.h"

QT_BEGIN_NAMESPACE
class QSqlQueryModel;
class QVBoxLayout;
QT_END_NAMESPACE

namespace Ui { class TecanWindow; }

/**
 * @class TecanWindow
 * @brief Main window controlling test‑request handling, plate layouts and GWL generation.
 */
class TecanWindow final : public QMainWindow
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TecanWindow)

public:
    explicit TecanWindow(QWidget *parent = nullptr);
    ~TecanWindow() override;

    /** Load selected test requests into the UI. */
    void loadTestRequests(const QStringList &requestIDs);

private slots:
    void on_clearPlatesButton_clicked();
    void on_actionSave_triggered();
    void on_actionLoad_triggered();
    void on_actionGenerate_GWL_triggered();

    void on_actionCreate_Plate_Map_triggered();

private:            /* ---------- helper types ---------- */
    using SqlModelPtr = std::unique_ptr<QSqlQueryModel>;

private:            /* ---------- helper GUI ----------
                       (all raw‑pointers are Qt‑owned)   */
    Ui::TecanWindow       *ui = nullptr;
    SqlModelPtr            testRequestModel;
    SqlModelPtr            compoundQueryModel;

    MatrixPlateContainer  *matrixPlateContainer = nullptr;
    QWidget               *daughterPlatesContainerWidget = nullptr;
    QVBoxLayout           *daughterPlatesLayout = nullptr;

    /* ---------- cached state ---------- */
    QJsonObject            lastSavedExperimentJson;

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

    /* ------ JSON (de)serialisation helpers ------ */
    void loadTestRequestsFromJson(const QJsonArray &array);
    void loadCompoundsFromJson(const QJsonArray &array);
    void loadMatrixPlatesFromJson(const QJsonObject &obj);
    void loadDaughterPlatesFromJson(const QJsonArray &array, bool readOnly);

    QJsonObject buildCurrentExperimentJson(const QString &experimentCode,
                                           const QString &username);
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
