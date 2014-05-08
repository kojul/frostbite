#include "connectionmanager.h"

ConnectionManager::ConnectionManager(QObject *parent) : QObject(parent) {
    tcpSocket = new QTcpSocket(this);
    eAuth = new EAuthService(this);
    mainWindow = (MainWindow*)parent;
    windowManager = mainWindow->getWindowManager();
    settings = ClientSettings::Instance();

    debugLogger = new DebugLogger();

    if(tcpSocket) {
        connect(tcpSocket, SIGNAL(readyRead()), this, SLOT(socketReadyRead()));
        connect(tcpSocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(socketError(QAbstractSocket::SocketError)));
        connect(tcpSocket, SIGNAL(disconnected()), this, SLOT(disconnectedFromHost()));
        tcpSocket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    }    

    dataProcessThread = new DataProcessThread(parent);
    connect(this, SIGNAL(addToQueue(QByteArray)), dataProcessThread, SLOT(addData(QByteArray)));
    connect(this, SIGNAL(updateHighlighterSettings()), dataProcessThread, SLOT(updateHighlighterSettings()));

    if(MainWindow::DEBUG) {
        this->loadMockData();
    }
}

void ConnectionManager::updateSettings() {
    emit updateHighlighterSettings();
}

void ConnectionManager::loadMockData() {
    QFile file(MOCK_DATA_PATH);

    if(!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Unable to open mock data file!";
        return;
    }

    QByteArray mockData = file.readAll();

    emit addToQueue(mockData);

    if(!dataProcessThread->isRunning()) {
        dataProcessThread->start();
    }
}

void ConnectionManager::initEauthSession(QString host, QString port, QString user, QString password) {
    eAuth->init(user, password);
    eAuth->initSession(host, port);
}

void ConnectionManager::selectGame() {
    // eAuth -> connectWizard
    emit enableGameSelect();
}

void ConnectionManager::gameSelected(QString id) {
    // connectWizard -> eAuth
    emit eAuthGameSelected(id);
}

void ConnectionManager::resetEauthSession() {
    eAuth->resetSession();
}

void ConnectionManager::addCharacter(QString id, QString name) {
    emit characterFound(id, name);
}

void ConnectionManager::retrieveEauthSession(QString id) {
    emit retrieveSessionKey(id);
}

void ConnectionManager::eAuthSessionRetrieved(QString host, QString port, QString sessionKey) {
    emit sessionRetrieved(host, port, sessionKey);
}

void ConnectionManager::connectWizardError(QString errorMsg) {
    emit eAuthError(errorMsg);
}

void ConnectionManager::authError() {
    emit resetPassword();
}

void ConnectionManager::connectToHost(QString sessionHost, QString sessionPort, QString sessionKey) {
    windowManager->writeGameWindow("Connecting ...");

    waitForSettings = true;

    mainWindow->connectEnabled(false);

    tcpSocket->connectToHost(sessionHost, sessionPort.toInt());

    //if(tcpSocket->state() == QAbstractSocket::HostLookupState ||
    //   tcpSocket->state() == QAbstractSocket::ConnectedState) {

        tcpSocket->write("<c>" + sessionKey.toLocal8Bit() + "\n" +
                         "<c>/FE:STORMFRONT /VERSION:1.0.1.26 /P:WIN_XP /XML\n");
    //}
}

void ConnectionManager::disconnectedFromHost() {
    mainWindow->connectEnabled(true);
}

void ConnectionManager::setProxy(bool enabled, QString proxyHost, QString proxyPort) {
    if(enabled) {
        QNetworkProxy proxy;
        proxy.setType(QNetworkProxy::HttpProxy);
        proxy.setHostName(proxyHost);
        proxy.setPort(proxyPort.toInt());

        QNetworkProxy::setApplicationProxy(proxy);
    } else {
        QNetworkProxy proxy(QNetworkProxy::NoProxy);
        QNetworkProxy::setApplicationProxy(proxy);
    }
}

void ConnectionManager::socketReadyRead() {    
    buffer.append(tcpSocket->readAll());

    if(buffer.endsWith("\n")){
        if(waitForSettings) {
                this->writeCommand("");
                this->writeCommand("_STATE CHATMODE OFF");
                this->writeCommand("");
                this->writeCommand("_swclose sassess");
                waitForSettings = false;
        }
        //qDebug() << buffer;

        // process raw data
        emit addToQueue(buffer);
        if(!dataProcessThread->isRunning()) {
            dataProcessThread->start();
        }

        // log raw data
        if(settings->getParameter("Logging/debug", false).toBool()) {
            debugLogger->addText(buffer);
            if(!debugLogger->isRunning()) {
                debugLogger->start();
            }
        }

        buffer.clear();
    }
}

void ConnectionManager::writeCommand(QString cmd) {
    //qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz");
    tcpSocket->write("<c>" + cmd.append("\n").toLocal8Bit());
    tcpSocket->flush();
}

void ConnectionManager::socketError(QAbstractSocket::SocketError error) {
    if(error == QAbstractSocket::RemoteHostClosedError) {
        this->showError("Disconnected from server.");
    } else if (error == QAbstractSocket::ConnectionRefusedError) {
        this->showError("Unable to connect to server. Please check your internet connection and try again later.");
    } else if (error == QAbstractSocket::NetworkError) {
        this->showError("Connection timed out.");
    } else if (error == QAbstractSocket::HostNotFoundError) {
        this->showError("Unable to resolve game host.");
    }    
    mainWindow->connectEnabled(true);

    qDebug() << error;
}

void ConnectionManager::showError(QString message) {
    windowManager->writeGameWindow("<br><br>"
        "*<br>"
        "* " + message.toLocal8Bit() + "<br>"
        "*<br>"
        "<br><br>");
}

void ConnectionManager::disconnectFromServer() {
    if(tcpSocket && tcpSocket->state() == QAbstractSocket::ConnectedState) {
        showError("Disconnected from server.");
        this->writeCommand("quit");
    }
    tcpSocket->disconnectFromHost();
}

ConnectionManager::~ConnectionManager() {
    this->disconnectFromServer();

    delete debugLogger;
    delete tcpSocket;
    delete dataProcessThread;
    delete eAuth;    
}