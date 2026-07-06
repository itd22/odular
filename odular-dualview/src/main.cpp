#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>

#include "dualviewwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("odular"));
    QApplication::setApplicationVersion(QStringLiteral("0.0.1"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("odular - Okular dual-view build using XMP metadata (Linked PDF feature)"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("file"),
                                  QStringLiteral("PDF document to open (may declare an okular:LinkedPDF)"));
    parser.process(app);

    DualViewWindow window;
    window.show();

    const QStringList args = parser.positionalArguments();
    if (!args.isEmpty()) {
        window.openPrimaryDocument(args.first());
    }

    return QApplication::exec();
}
