#ifndef EAUTHSERVICE_H
#define EAUTHSERVICE_H

#include <QObject>
#include <QDebug>
#include <QtNetwork/QTcpSocket>

#include <clientsettings.h>
#include <connectionmanager.h>

class ClientSettings;
class ConnectionManager;

class EAuthService : public QObject {
    Q_OBJECT

public:
    explicit EAuthService(QObject *parent = 0);
    ~EAuthService();

    void init(QString, QString);
    void initSession(QString, QString);
    void resetSession();

private:
    ClientSettings* settings;
    QTcpSocket *tcpSocket;
    QByteArray buffer;
    ConnectionManager *connectionManager;

    QString key;
    QString user;

    QString gameId;

    QString errorMessage;

    char* sge_encrypt_password(char *passwd, char *hash);
    void negotiateSession(QByteArray);
    QByteArray extractValue(QByteArray);

signals:
     void sessionRetrieved(QString, QString, QString);
     void addCharacter(QString, QString);
     void selectGame();
     void connectionError(QString);
     void authError();

public slots:
    void retrieveSessionKey(QString);
    void gameSelected(QString id);

private slots:
    void socketReadyRead();
    void socketError(QAbstractSocket::SocketError error);
    void startSession();
};

#endif // EAUTHSERVICE_H