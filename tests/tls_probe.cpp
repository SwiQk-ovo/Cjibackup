// TLS 诊断探针：验证打包后的 Qt 能不能正常走 HTTPS（更新检查依赖它）
#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslSocket>
#include <QTextStream>
#include <QTimer>
#include <QUrl>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    out << "supportsSsl=" << QSslSocket::supportsSsl()
        << " build=" << QSslSocket::sslLibraryBuildVersionString()
        << " runtime=" << QSslSocket::sslLibraryVersionString() << "\n";
    out.flush();

    QNetworkAccessManager net;
    QNetworkRequest request{
        QUrl(QStringLiteral("https://api.github.com/repos/SwiQk-ovo/Cjibackup/releases?per_page=1"))};
    request.setRawHeader("User-Agent", "KOPAL-TLS-Probe");
    request.setTransferTimeout(15000);

    QNetworkReply *reply = net.get(request);
    QObject::connect(reply, &QNetworkReply::finished, &app, [&]() {
        out << "error=" << int(reply->error()) << " " << reply->errorString() << "\n";
        out << "status="
            << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() << "\n";
        out << "bytes=" << reply->bytesAvailable() << "\n";
        out.flush();
        app.quit();
    });

    QTimer::singleShot(20000, &app, &QCoreApplication::quit);
    return app.exec();
}
