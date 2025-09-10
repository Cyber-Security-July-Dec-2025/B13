#pragma once
#include <QMainWindow>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QSettings>
#include <QJsonObject>   // added to match declarations using QJsonObject
#include "lamport.h"

class QTextEdit;
class QPushButton;
class QLabel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent=nullptr);
    ~MainWindow() override;

private slots:
    void onConnect();
    void onStart();
    void onStop();
    void onDisconnect();

    void onServerNewConnection();
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketReadyRead();

    void onTick();  // 1-second pacing for challenges (Alice only)

private:
    void loadConfig();
    void log(const QString &line);
    void setUiState(bool connected);
    void sendJson(const QJsonObject &obj);
    void handleJson(const QJsonObject &obj);

    // Protocol helpers
    void sendChallenge();                     // Alice
    void handleChallenge(int c);              // Bob
    void handleResponse(const QByteArray &r); // Alice (param is raw bytes; we hex-decode before calling)
    void handleAck(bool ok);                  // Bob

    // Verification (Alice): θ starts at h_n
    bool verifyAndUpdateTheta(const QByteArray &r);

private:
    // UI
    QTextEdit *logView_ = nullptr;
    QPushButton *btnConnect_ = nullptr;
    QPushButton *btnStart_ = nullptr;
    QPushButton *btnStop_ = nullptr;
    QPushButton *btnDisconnect_ = nullptr;
    QLabel *lblStatus_ = nullptr;
    QLabel *lblRole_ = nullptr;

    // Network
    QTcpServer *server_ = nullptr;
    QTcpSocket *sock_ = nullptr;
    QByteArray inBuffer_;

    // Config
    QString role_;       // "Alice" or "Bob"
    int n_ = 100;
    int sleepMs_ = 1000;
    QString listenIp_;
    quint16 listenPort_ = 0;
    QString peerIp_;
    quint16 peerPort_ = 0;
    QByteArray h0_;      // Bob
    QByteArray hn_;      // Alice

    // Lamport state
    HashChain chain_;    // Bob
    QByteArray theta_;   // Alice: θ, starts at h_n
    int c_ = 1;          // Alice's counter
    bool waitingResponse_ = false;
    bool running_ = false;

    QTimer tick_;        // pacing
};
