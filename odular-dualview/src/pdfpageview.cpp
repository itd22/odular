#include "pdfpageview.h"

#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QImage>
#include <QPixmap>

#include <poppler-qt5.h>

namespace {
constexpr qreal kBaseDpi = 72.0;
constexpr qreal kMinZoom = 0.1;
constexpr qreal kMaxZoom = 6.0;
}

PdfPageView::PdfPageView(QWidget *parent)
    : QWidget(parent)
{
    m_imageLabel = new QLabel(this);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setBackgroundRole(QPalette::Base);
    m_imageLabel->setScaledContents(false);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidget(m_imageLabel);
    m_scrollArea->setWidgetResizable(false);
    m_scrollArea->setAlignment(Qt::AlignCenter);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_scrollArea);
    setLayout(layout);

    connect(m_scrollArea->horizontalScrollBar(), &QScrollBar::valueChanged, this, [this](int) {
        if (m_syncingScroll) {
            return;
        }
        Q_EMIT scrollPositionChanged(QPoint(m_scrollArea->horizontalScrollBar()->value(),
                                             m_scrollArea->verticalScrollBar()->value()));
    });
    connect(m_scrollArea->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int) {
        if (m_syncingScroll) {
            return;
        }
        Q_EMIT scrollPositionChanged(QPoint(m_scrollArea->horizontalScrollBar()->value(),
                                             m_scrollArea->verticalScrollBar()->value()));
    });
}

PdfPageView::~PdfPageView() = default;

bool PdfPageView::loadDocument(const QString &path)
{
    std::unique_ptr<Poppler::Document> doc(Poppler::Document::load(path));
    if (!doc || doc->isLocked()) {
        return false;
    }

    doc->setRenderHint(Poppler::Document::Antialiasing, true);
    doc->setRenderHint(Poppler::Document::TextAntialiasing, true);

    m_document = std::move(doc);
    m_filePath = path;
    m_currentPage = 0;

    renderCurrentPage();
    Q_EMIT documentLoaded(path);
    return true;
}

Poppler::Document *PdfPageView::document() const
{
    return m_document.get();
}

int PdfPageView::pageCount() const
{
    return m_document ? m_document->numPages() : 0;
}

void PdfPageView::renderCurrentPage()
{
    if (!m_document) {
        return;
    }

    std::unique_ptr<Poppler::Page> page(m_document->page(m_currentPage));
    if (!page) {
        return;
    }

    const qreal dpi = kBaseDpi * m_zoom;
    const QImage image = page->renderToImage(dpi, dpi);
    m_imageLabel->setPixmap(QPixmap::fromImage(image));
    m_imageLabel->resize(image.size());
}

void PdfPageView::setZoom(qreal zoom)
{
    zoom = qBound(kMinZoom, zoom, kMaxZoom);
    if (qFuzzyCompare(zoom, m_zoom)) {
        return;
    }
    m_zoom = zoom;
    renderCurrentPage();
    Q_EMIT zoomChanged(m_zoom);
}

void PdfPageView::setScrollPosition(const QPoint &pos)
{
    m_syncingScroll = true;
    m_scrollArea->horizontalScrollBar()->setValue(pos.x());
    m_scrollArea->verticalScrollBar()->setValue(pos.y());
    m_syncingScroll = false;
}

void PdfPageView::goToPage(int pageNumber)
{
    if (!m_document) {
        return;
    }
    pageNumber = qBound(0, pageNumber, m_document->numPages() - 1);
    if (pageNumber == m_currentPage) {
        return;
    }
    m_currentPage = pageNumber;
    renderCurrentPage();
    Q_EMIT pageChanged(m_currentPage);
}

void PdfPageView::nextPage()
{
    goToPage(m_currentPage + 1);
}

void PdfPageView::previousPage()
{
    goToPage(m_currentPage - 1);
}
