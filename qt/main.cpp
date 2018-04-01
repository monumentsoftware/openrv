#include "mainwindow.h"

#include <libopenrv/libopenrv.h>

#include <QtWidgets/QApplication>
#include <QtCore/QCommandLineParser>
#include <QtGui/QSurfaceFormat>
#include <QtCore/QSettings>
#include <QtCore/QtDebug>
#include <QtCore/QFile>
#include <QtCore/QTimer>

struct Options
{
    bool mWantShowHelp = false;
    QString mHelpText;
    bool mWantShowVersion = false;
    QString mHostName = QLatin1String("localhost");
    int mPort = 5900;
    QString mPassword;
    bool mWantAutoConnect = false;
};
static bool parseCommandLineArguments(Options* options, QString* errorMessage);

int main(int argc, char** argv)
{
    QCoreApplication::setApplicationName(QObject::tr("OpenRVClient"));
    QCoreApplication::setOrganizationName("Monument-Software GmbH");
    QCoreApplication::setApplicationVersion(LIBOPENRV_VERSION_STRING); // NOTE: The qt application uses the same version number as the library
    QSettings::setDefaultFormat(QSettings::IniFormat); // native formats behave differently on different systems, so avoid them. (e.g. null QDateTime objects are not supported on mac, they load as "Jan 1, 2001")


    QSurfaceFormat format;
    format.setDepthBufferSize(0);
    format.setStencilBufferSize(0);
    // NOTE: OpenGL ES 3 matches roughly OpenGL 3.3
    // Also note: We want a core profile, so we need at least OpenGL >= 3.2.
    format.setVersion(3, 2);
    format.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);

    Options options;
    QString errorMessage;
    if (!parseCommandLineArguments(&options, &errorMessage)) {
        fprintf(stderr, "%s\n", qPrintable(errorMessage));
        fflush(stderr);
        return 1;
    }
    if (options.mWantShowHelp) {
        fprintf(stdout, "%s\n", qPrintable(options.mHelpText));
        fflush(stdout);
        return 0;
    }
    if (options.mWantShowVersion) {
        fprintf(stdout, "%s %s\n", qPrintable(QCoreApplication::applicationName()), qPrintable(QCoreApplication::applicationVersion()));
        fflush(stdout);
        return 0;
    }

    MainWindow mainWindow;
    mainWindow.resize(400, 300);

    mainWindow.setInitialHostName(options.mHostName);
    mainWindow.setInitialPort(options.mPort);
    if (!options.mPassword.isEmpty()) {
        mainWindow.setInitialPassword(options.mPassword);
        options.mPassword = QString();
    }
    mainWindow.show();
    if (options.mWantAutoConnect) {
        QTimer::singleShot(0, &mainWindow, SLOT(startConnect()));
    }
    return app.exec();
}

static bool parseCommandLineArguments(Options* options, QString* errorMessage)
{
    QCommandLineParser parser;
    QCommandLineOption helpOption = parser.addHelpOption();
    QCommandLineOption versionOption = parser.addVersionOption();
    QCommandLineOption hostnameOption = QCommandLineOption(QStringList() << "hostname",
            QObject::tr("Connect to the specified hostname on startup. This starts an immediate connect on startup."),
            QObject::tr("hostname"));
    parser.addOption(hostnameOption);
    QCommandLineOption portOption = QCommandLineOption(QStringList() << "p" << "port",
            QObject::tr("Use the specified port initially, instead of the default (5900) port."),
            QObject::tr("port"),
            QString::number(5900));
    parser.addOption(portOption);
    QCommandLineOption passwordFileOption = QCommandLineOption(QStringList() << "passwordfile",
            QObject::tr("Read the password from the specified file."),
            QObject::tr("file"));
    parser.addOption(passwordFileOption);
    if (!parser.parse(QCoreApplication::instance()->arguments())) {
        *errorMessage = parser.errorText();
        return false;
    }

    if (parser.isSet(helpOption)) {
        options->mWantShowHelp = true;
        options->mHelpText = parser.helpText();
    }
    if (parser.isSet(versionOption)) {
        options->mWantShowVersion = true;
    }
    if (parser.isSet(hostnameOption)) {
        options->mHostName = parser.value(hostnameOption);
        options->mWantAutoConnect = true;
    }
    if (parser.isSet(portOption)) {
        bool ok;
        options->mPort = parser.value(portOption).toInt(&ok);
        if (!ok) {
            *errorMessage = QObject::tr("The port argument must be an integer.");
            return false;
        }
        if (options->mPort <= 0) {
            *errorMessage = QObject::tr("The port argument must be greater than 0.");
            return false;
        }
        if (options->mPort > 65535) {
            *errorMessage = QObject::tr("The port argument must not be greater than 65535.");
            return false;
        }
    }
    else {
        options->mPort = portOption.defaultValues().at(0).toInt();
    }
    if (parser.isSet(passwordFileOption)) {
        QString fileName = parser.value(passwordFileOption);
        QFile file(fileName);
        if (!file.open(QFile::ReadOnly)) {
            *errorMessage = QObject::tr("Unable to open file '%1' for reading.").arg(fileName);
            return false;
        }
        options->mPassword = QString::fromUtf8(file.readAll());
        if (!options->mPassword.isEmpty() && options->mPassword.endsWith(QLatin1Char('\n'))) {
            options->mPassword = options->mPassword.left(options->mPassword.size() - 1);
        }
    }

    return true;
}


