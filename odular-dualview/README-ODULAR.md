# odular 0.0.1 — dual-view addition

This directory is a **pure addition** to the odular/okular repository — it
does not modify any existing repo file. It implements the feature
described in `odular-dev.md` ("Okular dual-view build using XMP metadata /
Linked PDF feature") as a standalone, independently-buildable application
named `odular`.

## What it does

- Opens a PDF.
- Reads its embedded XMP metadata looking for an `okular:LinkedPDF` tag.
- If found, opens the referenced PDF (resolved relative to the primary
  file's directory) in a second pane.
- Displays both documents side-by-side in a 50/50 `QSplitter`.
- Keeps zoom level and scroll position synchronized from the primary pane
  to the secondary pane.

## Layout (new files only)

```
odular-dualview/
├── CMakeLists.txt
├── README-ODULAR.md
└── src/
    ├── main.cpp
    ├── xmpreader.h / xmpreader.cpp
    ├── pdfpageview.h / pdfpageview.cpp
    └── dualviewwindow.h / dualviewwindow.cpp
```

## Dependencies

```
sudo apt install cmake qtbase6-dev libqt
5xml5 libpoppler-qt6-dev pkg-config
```

## Build (produces the `odular` executable)

```
cd odular-dualview
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

The resulting binary is at `odular-dualview/build/odular`.

## Test

```
exiftool -XMP-okular:LinkedPDF=second.pdf main.pdf
./odular-dualview/build/odular main.pdf
```

Left pane shows `main.pdf`; right pane automatically loads `second.pdf` if
present alongside it, with synchronized zoom/scroll.
