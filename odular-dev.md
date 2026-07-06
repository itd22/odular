# Okular dual-view build using XMP metadata (Linked PDF feature)

This describes how to modify and compile Okular so that a PDF can declare a second linked PDF via XMP metadata,
and Okular opens it in a synchronized second view.

## 1. Concept

A PDF contains XMP metadata like:

<x:xmpmeta xmlns:x="adobe:ns:meta/">
  <rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#">

    <rdf:Description rdf:about=""
      xmlns:okular="http://okular.kde.org/xmp/1.0/">

      <okular:LinkedPDF>second.pdf</okular:LinkedPDF>

    </rdf:Description>
  </rdf:RDF>
</x:xmpmeta>

Behavior:
- Open main.pdf
- Okular reads XMP tag okular:LinkedPDF
- Automatically opens second.pdf
- Displays both in a split view
- Keeps zoom/page layout synchronized

## 2. Required Okular architecture changes

###2.1 Extend metadata extraction

Modify PDF generator (Poppler backend):

📁 generators/poppler/generator_pdf.cpp

Add XMP parsing:

```
QString PdfGenerator::linkedPdfFromXmp(const Poppler::Document *doc)
{
    const QByteArray xmp = doc->xmpMetadata();
    if (xmp.isEmpty()) return {};

    QDomDocument xml;
    xml.setContent(xmp);

    QDomNodeList nodes = xml.elementsByTagName("okular:LinkedPDF");
    if (!nodes.isEmpty()) {
        return nodes.at(0).toElement().text();
    }

    return {};
}
```

### 2.2 Extend Okular document metadata interface

📁 core/document.cpp

```
QString Document::linkedPdf() const
{
    return d->linkedPdf; // stored during load
}
```

store during load 
```
d->linkedPdf = generator->linkedPdfFromXmp(popplerDoc);
```

### 2.3 Add dual-document controller

```
class DualDocumentController : public QObject {
    Q_OBJECT

public:
    void setPrimary(Okular::Document *doc);
    void loadLinked();

private:
    Okular::Document *primary = nullptr;
    Okular::Document *secondary = nullptr;
};
```
2.4 Load secondary PDF from metadata

```
void DualDocumentController::loadLinked()
{
    QString path = primary->linkedPdf();
    if (path.isEmpty())
        return;

    secondary = new Okular::Document(this);
    secondary->openDocument(path);
}
```
### 2.5 UI split view integration
📁 part.cpp

Replace single view with splitter:
```
QSplitter *splitter = new QSplitter(Qt::Horizontal, widget);

splitter->addWidget(primaryView);
splitter->addWidget(secondaryView);

splitter->setStretchFactor(0, 1);
splitter->setStretchFactor(1, 1);
```
force equal size
```
splitter->setSizes({500, 500});
```

2.6 Sync view behavior
```
connect(primaryView, &PageView::zoomChanged,
        secondaryView, &PageView::setZoom);

connect(primaryView, &PageView::scrollPositionChanged,
        secondaryView, &PageView::setScrollPosition);
```


## 3. Build instructions

### install dependencies

```
sudo apt install \
  cmake extra-cmake-modules \
  qtbase5-dev qtdeclarative5-dev \
  libkf5kio-dev libkf5parts-dev \
  libpoppler-qt5-dev

```

### Configure build

cmake -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=OFF

3.4 Compile
cmake --build build -j$(nproc)



#$4 test XMP

exiftool -XMP-okular:LinkedPDF=second.pdf main.pdf
odular main.pdf

 Result behavior
Left pane → main.pdf
Right pane → second.pdf (from XMP)
Both:
same zoom
same page scaling
optionally synchronized scrolling

6 Optional Lazy loading
only open secondary when first page is rendered
