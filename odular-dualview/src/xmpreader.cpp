#include "xmpreader.h"

#include <QXmlStreamReader>
#include <poppler-qt5.h>

namespace XmpReader
{

QString linkedPdfFromXmpPacket(const QByteArray &xmpPacket)
{
    if (xmpPacket.isEmpty()) {
        return {};
    }

    QXmlStreamReader xml(xmpPacket);

    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::StartElement) {
            // Match on local name so this works regardless of which
            // namespace prefix ("okular:", "ok:", etc.) the producer used.
            if (xml.name().compare(QLatin1String("LinkedPDF"), Qt::CaseInsensitive) == 0) {
                const QString text = xml.readElementText(QXmlStreamReader::IncludeChildElements).trimmed();
                if (!text.isEmpty()) {
                    return text;
                }
            }
        }
    }

    return {};
}

QString linkedPdfFromDocument(Poppler::Document *doc)
{
    if (!doc) {
        return {};
    }

    const QByteArray xmp = doc->metadata().toUtf8();
    return linkedPdfFromXmpPacket(xmp);
}

}
