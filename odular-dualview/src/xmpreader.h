#ifndef ODULAR_XMPREADER_H
#define ODULAR_XMPREADER_H

#include <QString>

namespace poppler {
class document;
}

namespace Poppler {
class Document;
}

// Reads the custom <okular:LinkedPDF> tag out of a PDF's embedded XMP
// metadata packet. This lets a PDF declare a companion document that
// should be opened alongside it in a synchronized dual view.
namespace XmpReader
{
// Returns the linked PDF path/filename declared in doc's XMP metadata,
// or an empty string if no such tag is present (or doc has no XMP data).
QString linkedPdfFromDocument(Poppler::Document *doc);

// Lower-level helper: parses a raw XMP XML packet and returns the text
// content of the first <okular:LinkedPDF> element found, if any.
QString linkedPdfFromXmpPacket(const QByteArray &xmpPacket);
}

#endif // ODULAR_XMPREADER_H
