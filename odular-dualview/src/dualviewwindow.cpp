#include "dualviewwindow.h"
#include "pdfpageview.h"
#include "xmpreader.h"

#include <QSplitter>
#include <QToolBar>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QStatusBar>

DualViewWindow::DualViewWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("odular 0.0.1 - dual view"));
    resize(1400, 900);

    setupUi();
    setupActions();
    connectSyncSignals();
}

void DualViewWindow::setupUi()
{
    m_splitter = new QSplitter(Qt::Horizontal, this);

    m_primaryView = new PdfPageView(m_splitter);
    m_secondaryView = new PdfPageView(m_splitter);

    m_splitter->addWidget(m_primaryView);
    m_splitter->addWidget(m_secondaryView);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes({500, 500});

    setCentralWidget(m_splitter);
    statusBar()->showMessage(QStringLiteral("Ready"));
}

void DualViewWindow::setupActions()
{
    auto *toolbar = addToolBar(QStringLiteral("Main"));

    m_openAction = toolbar->addAction(QStringLiteral("Open..."));
    connect(m_openAction, &QAction::triggered, this, &DualViewWindow::onOpenActionTriggered);

    toolbar->addSeparator();

    m_prevPageAction = toolbar->addAction(QStringLiteral("Previous Page"));
    connect(m_prevPageAction, &QAction::triggered, m_primaryView, &PdfPageView::previousPage);

    m_nextPageAction = toolbar->addAction(QStringLiteral("Next Page"));
    connect(m_nextPageAction, &QAction::triggered, m_primaryView, &PdfPageView::nextPage);

    toolbar->addSeparator();

    m_zoomOutAction = toolbar->addAction(QStringLiteral("Zoom -"));
    connect(m_zoomOutAction, &QAction::triggered, this, [this]() {
        m_primaryView->setZoom(m_primaryView->zoom() - 0.1);
    });

    m_zoomInAction = toolbar->addAction(QStringLiteral("Zoom +"));
    connect(m_zoomInAction, &QAction::triggered, this, [this]() {
        m_primaryView->setZoom(m_primaryView->zoom() + 0.1);
    });
}

void DualViewWindow::connectSyncSignals()
{
    // Keep zoom and scroll position mirrored from the primary pane onto
    // the secondary pane, matching the behavior described in odular-dev.md.
    connect(m_primaryView, &PdfPageView::zoomChanged, m_secondaryView, &PdfPageView::setZoom);
    connect(m_primaryView, &PdfPageView::scrollPositionChanged, m_secondaryView, &PdfPageView::setScrollPosition);
}

void DualViewWindow::onOpenActionTriggered()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Open PDF"), QString(),
                                                        QStringLiteral("PDF files (*.pdf)"));
    if (path.isEmpty()) {
        return;
    }
    openPrimaryDocument(path);
}

bool DualViewWindow::openPrimaryDocument(const QString &path)
{
    if (!m_primaryView->loadDocument(path)) {
        QMessageBox::warning(this, QStringLiteral("odular"),
                              QStringLiteral("Could not open document:\n%1").arg(path));
        return false;
    }

    statusBar()->showMessage(QStringLiteral("Loaded %1").arg(QFileInfo(path).fileName()));
    loadLinkedDocumentIfAny();
    return true;
}

void DualViewWindow::loadLinkedDocumentIfAny()
{
    const QString linked = XmpReader::linkedPdfFromDocument(m_primaryView->document());
    if (linked.isEmpty()) {
        return;
    }

    // Resolve relative to the primary document's directory, same as a
    // sibling file reference would be expected to work.
    QString linkedPath = linked;
    QFileInfo linkedInfo(linkedPath);
    if (linkedInfo.isRelative()) {
        const QFileInfo primaryInfo(m_primaryView->filePath());
        linkedPath = primaryInfo.dir().filePath(linked);
    }

    if (!QFileInfo::exists(linkedPath)) {
        statusBar()->showMessage(QStringLiteral("Linked PDF declared (%1) but not found").arg(linked));
        return;
    }

    if (m_secondaryView->loadDocument(linkedPath)) {
        statusBar()->showMessage(QStringLiteral("Loaded linked view: %1").arg(QFileInfo(linkedPath).fileName()));
    }
}
