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

## 5. Minimal-change build strategy for odular

The goal: **never edit okular's own files in place**. All odular-specific
code lives in a single new subtree, okular's own CMake/build path stays
byte-for-byte identical when the overlay isn't requested, and there's a
concrete way to prove that.

### 5.1 Design

```
<okular-dir>/
├── CMakeLists.txt          # okular's own root — gets exactly ONE new,
│                           # default-OFF block added (see 5.2)
├── core/ generators/ part/ ...   # untouched okular sources
└── odular/                 # NEW — everything odular-specific lives here
    ├── overlay.cmake       # copies files below onto the tree, gated
    │                       # behind BUILD_ODULAR_OVERLAY
    ├── manifest.txt        # list of repo-relative paths this overlay
    │                       # replaces or adds — used both by overlay.cmake
    │                       # and by the validation script in 5.4
    ├── core/document_p.h              # replaces core/document_p.h
    ├── core/document.cpp              # replaces core/document.cpp
    ├── generators/poppler/generator_pdf.h
    ├── generators/poppler/generator_pdf.cpp
    ├── part/part.cpp
    ├── part/pageview.h
    ├── part/pageview.cpp
    ├── part/dualdocumentcontroller.h  # new file, not a replacement
    └── part/dualdocumentcontroller.cpp
```

`odular/` mirrors the paths of the okular files it changes or adds, so
`manifest.txt` is just:

```
core/document_p.h
core/document.cpp
generators/poppler/generator_pdf.h
generators/poppler/generator_pdf.cpp
part/part.cpp
part/pageview.h
part/pageview.cpp
part/dualdocumentcontroller.h
part/dualdocumentcontroller.cpp
```

`odular/overlay.cmake`:

```cmake
# odular/overlay.cmake — only included when BUILD_ODULAR_OVERLAY is ON
file(STRINGS "${CMAKE_SOURCE_DIR}/odular/manifest.txt" _odular_files)

foreach(_rel ${_odular_files})
    set(_src "${CMAKE_SOURCE_DIR}/odular/${_rel}")
    set(_dst "${CMAKE_SOURCE_DIR}/${_rel}")
    if(EXISTS ${_src})
        message(STATUS "odular overlay: ${_rel}")
        configure_file(${_src} ${_dst} COPYONLY)
    else()
        message(FATAL_ERROR "odular overlay: missing ${_src} listed in manifest.txt")
    endif()
endforeach()

# part/dualdocumentcontroller.{h,cpp} are new sources, not replacements —
# they need to be added to okularpart's source list explicitly.
# In part/CMakeLists.txt this is one added line, guarded the same way:
#   if(BUILD_ODULAR_OVERLAY)
#       target_sources(okularpart PRIVATE dualdocumentcontroller.cpp)
#   endif()
```

The **only** edit okular's own root `CMakeLists.txt` needs is this, added
once, near the top:

```cmake
option(BUILD_ODULAR_OVERLAY "Apply odular/ source overlay (dual-view build)" OFF)
if(BUILD_ODULAR_OVERLAY)
    include(odular/overlay.cmake)
endif()
```

Default is `OFF`, so a normal `cmake -B build` behaves exactly as upstream
okular always has. `configure_file(... COPYONLY)` runs at CMake-configure
time and re-runs automatically (via CMake's own dependency tracking) if a
file under `odular/` changes, so there's no separate "copy step" to remember.

### 5.2 Build sequence

```bash
# Stage 1 — build stock okular, completely unmodified, as a baseline.
cmake -B build-okular -DCMAKE_BUILD_TYPE=Debug
cmake --build build-okular -j"$(nproc)"

# Stage 2 — build odular: same tree, overlay applied on top.
cmake -B build-odular -DCMAKE_BUILD_TYPE=Debug -DBUILD_ODULAR_OVERLAY=ON
cmake --build build-odular -j"$(nproc)"
```

Both builds configure from the *same* source directory. Stage 1 never
touches `odular/`, so it is exactly the upstream okular build chain. Stage 2
copies the files in `manifest.txt` over their originals before compiling —
this is destructive to the working tree (the copied-over files are now
modified on disk), so run Stage 1 first, or use a separate `git worktree` /
clone per stage if you want to keep both buildable side-by-side without
re-checking-out files in between:

```bash
git worktree add ../odular-stock odular
git worktree add ../odular-overlay odular
# build stock in ../odular-stock, overlay build in ../odular-overlay
```

### 5.3 Validating okular's own build chain is unchanged

Confirm the root `CMakeLists.txt` diff against upstream is exactly the
5-line `option()`/`if()` block above and nothing else:

```bash
git diff upstream/master -- CMakeLists.txt
```

Confirm a default configure+build (no `-DBUILD_ODULAR_OVERLAY`) produces an
identical object list and identical binary hash to a build from a checkout
that has no `odular/` directory at all:

```bash
cmake -B build-baseline -DCMAKE_BUILD_TYPE=Debug   # BUILD_ODULAR_OVERLAY defaults OFF
cmake --build build-baseline -j"$(nproc)"
sha256sum build-baseline/bin/okular build-okular/bin/okular   # should match
```

### 5.4 Validating the odular build is "original okular files + changes"

```bash
# Every file the overlay touched should now match odular/, and every
# other file in the tree should still match upstream.
while read -r rel; do
    diff -q "odular/${rel}" "${rel}" || echo "MISMATCH: ${rel}"
done < odular/manifest.txt

# And confirm nothing *outside* the manifest changed:
git status --porcelain | grep -v -F -f <(sed 's/^/ M /' odular/manifest.txt)
# (should print nothing)
```

If both checks come back clean, the odular build is provably "upstream
okular, plus exactly the files listed in `manifest.txt`, and nothing else."

### 5.5 Dropping translations (English-only interface) without touching source

`i18n()` / `i18nc()` calls stay in the code as-is — ripping them out of every
call site across the tree would violate the minimize-changes goal, and isn't
necessary: KI18n's `i18n()` already falls back to the untranslated source
string (which is English) whenever no translation catalog is loaded. So the
practical way to get an English-only interface without editing any `.cpp`
file is to **stop building/installing the translation catalogs**, which is
two lines in the same overlay-gated block in the root `CMakeLists.txt`:

```cmake
if(NOT BUILD_ODULAR_OVERLAY)
    ki18n_install(po)
    if(KF6DocTools_FOUND)
        kdoctools_install(po)
    endif()
endif()
```

`find_package(KF6 ... COMPONENTS ... I18n ...)` still has to stay — the
`I18n` component is a required, non-optional entry in okular's own
`find_package(KF6 ...)` call, and `i18n()` itself is a compile-time macro
from that library, not something layered on top. Removing the component
would break the build; skipping catalog installation just means there's
nothing for it to translate *to* at runtime.

### 5.6 Script: strip unneeded `.po` catalogs

The `po/` directory ships ~1,069 `.po` files across 83 locale subdirectories.
If odular never installs them (5.5), they cost nothing at build time, but
if you also want them gone from the working tree/repo entirely:

```bash
#!/usr/bin/env bash
# odular/scripts/strip-translations.sh
# Deletes every po/<locale> directory, keeping only the source (English)
# strings baked into the .cpp/.ui/.rc files. Safe to re-run; it's a no-op
# once already stripped.
set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel)"
PO_DIR="${REPO_ROOT}/po"

# Locales to keep, if any (space-separated, e.g. KEEP_LOCALES="en_GB").
KEEP_LOCALES="${KEEP_LOCALES:-}"

if [[ ! -d "${PO_DIR}" ]]; then
    echo "No po/ directory found at ${PO_DIR}, nothing to do."
    exit 0
fi

shopt -s nullglob
removed=0
for locale_dir in "${PO_DIR}"/*/; do
    locale="$(basename "${locale_dir}")"
    keep=false
    for k in ${KEEP_LOCALES}; do
        [[ "${locale}" == "${k}" ]] && keep=true
    done
    if [[ "${keep}" == false ]]; then
        rm -rf -- "${locale_dir}"
        removed=$((removed + 1))
    fi
done

echo "Removed ${removed} locale director$( [[ ${removed} -eq 1 ]] && echo y || echo ies ) from po/."
[[ -n "${KEEP_LOCALES}" ]] && echo "Kept: ${KEEP_LOCALES}"
```

Usage:

```bash
chmod +x odular/scripts/strip-translations.sh
./odular/scripts/strip-translations.sh           # deletes all 83 locale dirs
KEEP_LOCALES="en_GB" ./odular/scripts/strip-translations.sh   # keep one
```

Since no locale directory contains "en" (US English is the untranslated
source string, not a `.po` file), the default run with no `KEEP_LOCALES`
already leaves the interface English-only — there's nothing to "keep,"
only locales to remove.
