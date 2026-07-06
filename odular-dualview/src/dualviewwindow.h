#ifndef ODULAR_DUALVIEWWINDOW_H
#define ODULAR_DUALVIEWWINDOW_H

#include <QMainWindow>

class QSplitter;
class QAction;
class PdfPageView;

// Top-level window: opens a primary PDF, checks its XMP metadata for an
// okular:LinkedPDF tag, and if present, opens the companion PDF in a
// second synchronized pane (equal 50/50 split, shared zoom & scroll).
class DualViewWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit DualViewWindow(QWidget *parent = nullptr);

    // Opens `path` as the primary document. If it declares a linked PDF
    // via XMP metadata, that document is loaded into the secondary pane.
    bool openPrimaryDocument(const QString &path);

private Q_SLOTS:
    void onOpenActionTriggered();

private:
    void setupUi();
    void setupActions();
    void connectSyncSignals();
    void loadLinkedDocumentIfAny();

    QSplitter *m_splitter;
    PdfPageView *m_primaryView;
    PdfPageView *m_secondaryView;

    QAction *m_openAction;
    QAction *m_nextPageAction;
    QAction *m_prevPageAction;
    QAction *m_zoomInAction;
    QAction *m_zoomOutAction;
};

#endif // ODULAR_DUALVIEWWINDOW_H
