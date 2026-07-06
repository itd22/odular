#ifndef ODULAR_PDFPAGEVIEW_H
#define ODULAR_PDFPAGEVIEW_H

#include <QWidget>
#include <QScopedPointer>
#include <memory>

class QLabel;
class QScrollArea;

namespace Poppler {
class Document;
}

// A single scrollable, zoomable PDF page view. Two of these are placed
// side-by-side in DualViewWindow's QSplitter and kept in sync.
class PdfPageView : public QWidget
{
    Q_OBJECT

public:
    explicit PdfPageView(QWidget *parent = nullptr);
    ~PdfPageView() override;

    // Loads a document from disk. Returns true on success.
    bool loadDocument(const QString &path);

    Poppler::Document *document() const;
    QString filePath() const { return m_filePath; }

    qreal zoom() const { return m_zoom; }
    int currentPage() const { return m_currentPage; }
    int pageCount() const;

public Q_SLOTS:
    void setZoom(qreal zoom);
    void setScrollPosition(const QPoint &pos);
    void goToPage(int pageNumber);
    void nextPage();
    void previousPage();

Q_SIGNALS:
    void zoomChanged(qreal zoom);
    void scrollPositionChanged(const QPoint &pos);
    void pageChanged(int pageNumber);
    void documentLoaded(const QString &path);

private:
    void renderCurrentPage();

    QString m_filePath;
    std::unique_ptr<Poppler::Document> m_document;
    int m_currentPage = 0;
    qreal m_zoom = 1.0;

    QScrollArea *m_scrollArea;
    QLabel *m_imageLabel;

    bool m_syncingScroll = false;
};

#endif // ODULAR_PDFPAGEVIEW_H
