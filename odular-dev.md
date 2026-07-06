# Okular dual-view build using XMP metadata (Linked PDF feature)

> **Status: design proposal — not yet implemented.**
> Nothing in this document currently exists in the `odular` branch (verified by
> grepping the tree for `LinkedPDF` / `linkedPdf` / `DualDocumentController` —
> zero matches outside this file). Treat every code block below as a patch to
> write, not a description of existing behavior. This revision corrects paths,
> class names, and API signatures against the actual repo (a fork of
> `KDE/okular`, currently on Qt6 / KF6) so the plan is at least compilable
> as a starting point.

This describes how to modify and compile Okular so that a PDF can declare a second linked PDF via XMP metadata, and Okular opens it in a synchronized second view.

## 1. Concept

A PDF contains XMP metadata like:

```xml
<x:xmpmeta xmlns:x="adobe:ns:meta/">
  <rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#">
    <rdf:Description rdf:about=""
      xmlns:okular="http://okular.kde.org/xmp/1.0/">
      <okular:LinkedPDF>second.pdf</okular:LinkedPDF>
    </rdf:Description>
  </rdf:RDF>
</x:xmpmeta>
```

Behavior:

- Open `main.pdf`
- Okular reads XMP tag `okular:LinkedPDF`
- Automatically opens `second.pdf`
- Displays both in a split view
- Keeps zoom/page layout synchronized

## 2. Required Okular architecture changes

### 2.1 Extend metadata extraction

Modify PDF generator (Poppler backend):

📁 `generators/poppler/generator_pdf.h` / `generators/poppler/generator_pdf.cpp`

The class is `PDFGenerator` (capital PDF), not `PdfGenerator`. Also, Poppler's
Qt6 API has no `xmpMetadata()` method — the raw metadata stream is retrieved
with `metadata()`, which you then have to parse yourself as XML:

```cpp
// generators/poppler/generator_pdf.cpp
QString PDFGenerator::linkedPdfFromXmp(const Poppler::Document *doc)
{
    const QString xmp = doc->metadata();
    if (xmp.isEmpty()) {
        return {};
    }

    QDomDocument xml;
    if (!xml.setContent(xmp)) {
        return {};
    }

    const QDomNodeList nodes = xml.elementsByTagName(QStringLiteral("okular:LinkedPDF"));
    if (!nodes.isEmpty()) {
        return nodes.at(0).toElement().text();
    }

    return {};
}
```

Declare `linkedPdfFromXmp()` in `generator_pdf.h` alongside the other
`PDFGenerator` methods.

### 2.2 Extend Okular document metadata interface

📁 `core/document.h` / `core/document.cpp`

`Okular::Document` uses a private-impl (`DocumentPrivate * const d`) pattern —
any new field needs to be added to `DocumentPrivate` in `core/document_p.h`,
not just referenced from `document.cpp`:

```cpp
// core/document_p.h (add a member)
QString linkedPdf;
```

```cpp
// core/document.cpp
QString Document::linkedPdf() const
{
    return d->linkedPdf;
}
```

Populate it wherever the generator is queried after a successful load (e.g.
in `Document::openDocument()`'s success path):

```cpp
d->linkedPdf = generator->linkedPdfFromXmp(popplerDoc);
```

Note: `generator` here is an `Okular::Generator*`; exposing a
Poppler-specific method like `linkedPdfFromXmp()` through the generic
`Generator` interface will require adding a virtual method to
`core/generator.h` (or a capability/hook), since `Document` doesn't otherwise
know it's talking to the Poppler backend specifically.

### 2.3 Add dual-document controller

New file, e.g. `part/dualdocumentcontroller.h`:

```cpp
class DualDocumentController : public QObject
{
    Q_OBJECT

public:
    explicit DualDocumentController(QObject *parent = nullptr);

    void setPrimary(Okular::Document *doc);
    void loadLinked();

    Okular::Document *secondaryDocument() const { return secondary; }

private:
    Okular::Document *primary = nullptr;
    Okular::Document *secondary = nullptr;
};
```

### 2.4 Load secondary PDF from metadata

`Okular::Document::openDocument()` takes four arguments, not one — a file
path, a `QUrl`, a `QMimeType`, and an optional password. You need to resolve
the mimetype yourself (e.g. via `QMimeDatabase`):

```cpp
void DualDocumentController::loadLinked()
{
    const QString path = primary->linkedPdf();
    if (path.isEmpty()) {
        return;
    }

    const QUrl url = QUrl::fromLocalFile(path);
    QMimeDatabase mimeDb;
    const QMimeType mime = mimeDb.mimeTypeForUrl(url);

    secondary = new Okular::Document(nullptr /* widget */, nullptr /* config */);
    const Okular::Document::OpenResult result = secondary->openDocument(path, url, mime);
    if (result != Okular::Document::OpenSuccess) {
        delete secondary;
        secondary = nullptr;
    }
}
```

(Check `core/document.h` for `Document`'s actual constructor signature in
your checkout — it has changed across Okular versions — before assuming the
two-argument form above compiles as-is.)

### 2.5 UI split view integration

📁 `part/part.cpp` (not `part.cpp` at repo root — the file lives under `part/`)

`Part` already builds its main `PageView` like this:

```cpp
m_pageView = new PageView(rightContainer, m_document);
```

so a second view for the linked document follows the same constructor shape:

```cpp
auto *splitter = new QSplitter(Qt::Horizontal, rightContainer);

auto *secondaryPageView = new PageView(splitter, dualController->secondaryDocument());

splitter->addWidget(m_pageView);
splitter->addWidget(secondaryPageView);
splitter->setStretchFactor(0, 1);
splitter->setStretchFactor(1, 1);
splitter->setSizes({500, 500});

rightLayout->addWidget(splitter); // instead of rightLayout->addWidget(m_pageView);
```

Note `Part` already has an `m_splitter` used for the sidebar/content split —
give the new one a distinct name to avoid confusion.

### 2.6 Sync view behavior

**This is the part that needs the most new code.** `PageView` currently has
no `zoomChanged`, `scrollPositionChanged`, `setZoom`, or `setScrollPosition`
members (checked `part/pageview.h`'s `Q_SIGNALS:` block — only `rightClick`,
`mouseBackButtonClick`, `mouseForwardButtonClick`, `escPressed`,
`fitWindowToPage`, `triggerSearch`, `requestOpenNewlySignedFile`,
`signingStarted`, `signingFinished` exist today). You'd need to add these
signals/slots to `PageView` yourself before this connection is possible:

```cpp
// after adding the signals/slots to PageView:
connect(m_pageView, &PageView::zoomChanged,
        secondaryPageView, &PageView::setZoom);

connect(m_pageView, &PageView::scrollPositionChanged,
        secondaryPageView, &PageView::setScrollPosition);
```

## 3. Build instructions

The repo's `CMakeLists.txt` requires **Qt6 (≥ 6.6.0)**, **KF6 (≥ 5.240.0)**,
and Poppler's **Qt6** component — not Qt5/KF5 as an older draft of this doc
assumed.

### Install dependencies (Debian/Ubuntu package names)

```bash
sudo apt install \
  cmake extra-cmake-modules \
  qt6-base-dev qt6-declarative-dev \
  libkf6kio-dev libkf6parts-dev libkf6crash-dev \
  libkf6iconthemes-dev libkf6textwidgets-dev libkf6widgetsaddons-dev \
  libpoppler-qt6-dev
```

(Package names vary by distro/release — check `CMakeLists.txt`'s
`find_package(KF6 ... COMPONENTS ...)` block for the full component list if
`cmake` reports something missing.)

### Configure build

```bash
cmake -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=OFF
```

### Compile

```bash
cmake --build build -j$(nproc)
```

### Test XMP

```bash
exiftool -XMP-okular:LinkedPDF=second.pdf main.pdf
./build/bin/okular main.pdf
```

Note: the build target is still named `okular` (see
`add_executable(okular ...)` in `shell/CMakeLists.txt`), not `odular` — the
fork hasn't renamed the actual binary, only the repo.

### Result behavior

- Left pane → `main.pdf`
- Right pane → `second.pdf` (from XMP)
- Both:
  - same zoom
  - same page scaling
  - optionally synchronized scrolling

## 4. Optional: lazy loading

Only open the secondary document once the first page of the primary has
rendered, e.g. by hooking `DualDocumentController::loadLinked()` off
`Document`'s page-ready signal rather than calling it immediately after
`openDocument()` returns.
