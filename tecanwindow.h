#ifndef TECANWINDOW_H
#define TECANWINDOW_H

#include <QMainWindow>
#include <QSqlQueryModel>
#include <QSet>
#include <QJsonObject>

#include "matrixplatecontainer.h"


namespace Ui {
class TecanWindow;
}

class TecanWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit TecanWindow(QWidget *parent = nullptr);
    ~TecanWindow();

    /**
     * @brief Load test requests into the table view based on selected request IDs.
     * @param requestIDs List of selected test request IDs.
     */
    void loadTestRequests(const QStringList &requestIDs);

private slots:
    void on_clearPlatesButton_clicked();

    void on_actionSave_triggered();

    void on_actionLoad_triggered();

    void on_actionGenerate_GWL_triggered();

private:
    Ui::TecanWindow *ui;

    // SQL models to manage data views
    QSqlQueryModel *testRequestModel;
    QSqlQueryModel *compoundQueryModel;

    // Custom Widgets for microplate display
    MatrixPlateContainer* matrixPlateContainer;
    QWidget* daughterPlatesContainerWidget;
    QVBoxLayout* daughterPlatesLayout;

    // check if experiment is saved
    QJsonObject lastSavedExperimentJson;

    /**
     * @brief Extracts unique compound names from loaded test requests and initiates querying of solutions.
     */
    void querySolutionsFromTestRequests();

    /**
     * @brief Queries the solutions table for each compound, handling duplicate solutions if necessary.
     * @param compoundNames Set of unique compound names to query.
     */
    void querySolutions(const QSet<QString> &compoundNames);

    /**
     * @brief Resolves cases where multiple solutions exist for a single compound name by prompting user selection.
     * @param compoundName Compound with multiple entries.
     * @param duplicateSolutions List of found solutions for the compound.
     * @return Selected solution_id, or -1 if canceled.
     */
    int resolveCompoundDuplicates(const QString &compoundName, const QList<QVariantMap> &duplicateSolutions);

    /**
     * @brief Populates the compound query table with selected solutions.
     * @param solutionIds List of selected solution IDs.
     */
    void populateCompoundTable(const QList<int> &solutionIds);

    void populateDaughterPlates(int dilutionSteps, const QStringList& compoundList, const QString& testType);

    //helper functions to load JSON file
    void loadTestRequestsFromJson(const QJsonArray& array);
    void loadCompoundsFromJson(const QJsonArray& array);
    void loadMatrixPlatesFromJson(const QJsonObject& obj);
    void loadDaughterPlatesFromJson(const QJsonArray& array, bool readOnly);

    // Helper function to track saved experiment
    QJsonObject buildCurrentExperimentJson(const QString& experimentCode, const QString& username);

    // create the GWL file
    void generateGWLFromJson(const QJsonObject& experimentJson);


};

#endif // TECANWINDOW_H
