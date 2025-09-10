#include "mainwindow.h"
#include <QFileInfo>
#include <QCoreApplication>

#include <QTextStream>

#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonDocument>
#include <QHostAddress>

#include <QtNetwork>
#include <QSettings>

#include "lamport.h"

// ---------- Utility ----------
static QByteArray hexToBytes(const QString &hex) {
    QByteArray h = hex.trimmed().toLatin1();
    return QByteArray::fromHex(h);
}
static QString bytesToHex(const QByteArray &b) {
    return QString::fromLatin1(b.toHex());
}

// ---------- MainWindow ----------
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    // UI
    auto *central = new QWidget(this);
    setCentralWidget(central);

    logView_ = new QTextEdit(this);
    logView_->setReadOnly(true);

    btnConnect_ = new QPushButton("Connect", this);
    btnStart_ = new QPushButton("Start", this);
    btnStop_ = new QPushButton("Stop", this);
    btnDisconnect_ = new QPushButton("Disconnect", this);

    lblStatus_ = new QLabel("Disconnected", this);
    lblRole_ = new QLabel(this);

    auto *btnRow = new QHBoxLayout();
    btnRow->addWidget(btnConnect_);
    btnRow->addWidget(btnStart_);
    btnRow->addWidget(btnStop_);
    btnRow->addWidget(btnDisconnect_);
    btnRow->addStretch();
    btnRow->addWidget(new QLabel("Role:",this));
    btnRow->addWidget(lblRole_);
    btnRow->addWidget(new QLabel("Status:",this));
    btnRow->addWidget(lblStatus_);

    auto *lyt = new QVBoxLayout(central);
    lyt->addLayout(btnRow);
    lyt->addWidget(logView_);

    setWindowTitle("Lamport Authentication (Qt + Crypto++)");
    resize(900, 560);

    // Signals
    connect(btnConnect_, &QPushButton::clicked, this, &MainWindow::onConnect);
    connect(btnStart_, &QPushButton::clicked, this, &MainWindow::onStart);
    connect(btnStop_, &QPushButton::clicked, this, &MainWindow::onStop);
    connect(btnDisconnect_, &QPushButton::clicked, this, &MainWindow::onDisconnect);
    connect(&tick_, &QTimer::timeout, this, &MainWindow::onTick);

    loadConfig();
    setUiState(false);
}

MainWindow::~MainWindow() {
    onDisconnect();
}

void MainWindow::loadConfig() {
    // Always read app.ini from the executable directory
    const QString iniPath = QCoreApplication::applicationDirPath() + "/app.ini";
    QSettings s(iniPath, QSettings::IniFormat);

    // Diagnostics
    s.sync();
    log("Using config: " + QFileInfo(s.fileName()).absoluteFilePath());
    if (s.status() != QSettings::NoError) {
        log(QString("QSettings status=%1 (0=OK)").arg(int(s.status())));
    }
    const QStringList keys = s.allKeys();
    log("Config keys: " + (keys.isEmpty() ? QString("<none>") : keys.join(", ")));

    // Case-tolerant readers
    auto readStr = [&](const QString &k, const QString &def = QString()) {
        auto tryKey = [&](QString key) {
            if (s.contains(key)) return s.value(key).toString().trimmed();
            return QString();
        };
        QString v = tryKey(k);
        if (v.isEmpty()) v = tryKey(QString(k).replace("general/", "General/"));
        if (v.isEmpty()) v = tryKey(QString(k).replace("network/", "Network/"));
        if (v.isEmpty()) v = tryKey(QString(k).replace("values/",  "Values/"));
        return v.isEmpty() ? def : v;
    };
    auto readInt = [&](const QString &k, int def) {
        bool ok = false; int x = readStr(k).toInt(&ok);
        return ok ? x : def;
    };
    auto readHex = [&](const QString &k) {
        return QByteArray::fromHex(readStr(k).toLatin1());
    };

    // Load values
    role_      = readStr("general/role", "Alice");
    n_         = readInt("general/n", 100);
    sleepMs_   = readInt("general/sleep_ms", 1000);

    listenIp_  = readStr("network/listen_ip", "0.0.0.0");
    listenPort_= quint16(readInt("network/listen_port", 0));
    peerIp_    = readStr("network/peer_ip", "127.0.0.1");
    peerPort_  = quint16(readInt("network/peer_port", 0));

    h0_        = readHex("values/h0");
    hn_        = readHex("values/hn");

    // Optional: single-PC convenience override based on folder name
    const QString exeDir = QCoreApplication::applicationDirPath();
    if (exeDir.endsWith("/Bob", Qt::CaseInsensitive) || exeDir.endsWith("\\Bob", Qt::CaseInsensitive)) {
        if (!role_.compare("Alice", Qt::CaseInsensitive))
            log("Note: overriding role to 'Bob' based on folder name.");
        role_ = "Bob";
    } else if (exeDir.endsWith("/Alice", Qt::CaseInsensitive) || exeDir.endsWith("\\Alice", Qt::CaseInsensitive)) {
        if (role_.compare("Alice", Qt::CaseInsensitive))
            log("Note: overriding role to 'Alice' based on folder name.");
        role_ = "Alice";
    }

    // Echo parsed config
    log(QString("Parsed role='%1', n=%2, sleep_ms=%3").arg(role_).arg(n_).arg(sleepMs_));
    log(QString("listen=%1:%2, peer=%3:%4").arg(listenIp_).arg(listenPort_).arg(peerIp_).arg(peerPort_));
    if (!h0_.isEmpty()) log("h0 present (" + QString::number(h0_.size()) + " bytes)");
    if (!hn_.isEmpty()) log("hn present (" + QString::number(hn_.size()) + " bytes)");

    lblRole_->setText(role_);

    // Initialize per-role state
    if (role_.compare("Alice", Qt::CaseInsensitive) == 0) {
        theta_ = hn_;
        if (theta_.isEmpty()) {
            log("ERROR: Alice requires hn in app.ini");
        }
    } else if (role_.compare("Bob", Qt::CaseInsensitive) == 0) {
        if (h0_.isEmpty()) {
            log("ERROR: Bob requires h0 in app.ini");
        } else if (!chain_.build(h0_, n_)) {
            log("ERROR: Bob failed to build hash chain");
        } else {
            // ➜ NEW: print Bob’s computed hn so you can paste it into Alice’s app.ini
            const QByteArray hn_calc = chain_.chain().isEmpty() ? QByteArray() : chain_.chain().back();
            if (!hn_calc.isEmpty()) {
                log("Bob computed hn=" + QString::fromLatin1(hn_calc.toHex()));
                log("Copy this hn into Alice's app.ini under [values]/hn and re-run.");
            }
        }
    } else {
        log("ERROR: Unknown role. Use 'Alice' or 'Bob'.");
    }
}

void MainWindow::setUiState(bool connected) {
    btnConnect_->setEnabled(!connected);
    btnDisconnect_->setEnabled(connected);
    btnStart_->setEnabled(connected && role_.compare("Alice", Qt::CaseInsensitive) == 0);
    btnStop_->setEnabled(connected);
    lblStatus_->setText(connected ? "Connected" : "Disconnected");
}

void MainWindow::log(const QString &line) {
    auto ts = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    logView_->append(QString("[%1] %2").arg(ts, line));
}

// ---------------- Network control ----------------

void MainWindow::onConnect() {
    if (role_.compare("Alice", Qt::CaseInsensitive) == 0) {
        // Server
        if (server_) { server_->deleteLater(); server_ = nullptr; }
        server_ = new QTcpServer(this);
        connect(server_, &QTcpServer::newConnection, this, &MainWindow::onServerNewConnection);
        QHostAddress addr(listenIp_);
        if (!server_->listen(addr, listenPort_)) {
            log("ERROR: Server listen failed on " + listenIp_ + ":" + QString::number(listenPort_));
            return;
        }
        log(QString("Alice listening on %1:%2 ...").arg(listenIp_).arg(listenPort_));
    } else {
        // Client
        if (sock_) { sock_->deleteLater(); sock_ = nullptr; }
        sock_ = new QTcpSocket(this);
        connect(sock_, &QTcpSocket::connected, this, &MainWindow::onSocketConnected);
        connect(sock_, &QTcpSocket::disconnected, this, &MainWindow::onSocketDisconnected);
        connect(sock_, &QTcpSocket::readyRead, this, &MainWindow::onSocketReadyRead);

        log(QString("Bob connecting to %1:%2 ...").arg(peerIp_).arg(peerPort_));
        sock_->connectToHost(peerIp_, peerPort_);
    }
}

void MainWindow::onServerNewConnection() {
    if (!server_) return;
    auto *s = server_->nextPendingConnection();
    if (!s) return;
    if (sock_) { sock_->deleteLater(); sock_ = nullptr; } // single connection only
    sock_ = s;
    connect(sock_, &QTcpSocket::disconnected, this, &MainWindow::onSocketDisconnected);
    connect(sock_, &QTcpSocket::readyRead, this, &MainWindow::onSocketReadyRead);
    log("Incoming connection accepted.");
    setUiState(true);
}

void MainWindow::onSocketConnected() {
    log("Connected.");
    setUiState(true);
}

void MainWindow::onSocketDisconnected() {
    log("Disconnected.");
    running_ = false;
    waitingResponse_ = false;
    tick_.stop();
    setUiState(false);
    if (sock_) { sock_->deleteLater(); sock_ = nullptr; }
}

void MainWindow::onDisconnect() {
    running_ = false;
    waitingResponse_ = false;
    tick_.stop();
    if (sock_) { sock_->disconnectFromHost(); }
    if (server_) { server_->close(); server_->deleteLater(); server_ = nullptr; }
}

// ---------------- Protocol I/O ----------------

void MainWindow::sendJson(const QJsonObject &obj) {
    if (!sock_) return;
    QByteArray line = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    line.append('\n');                          // newline-delimited JSON framing
    sock_->write(line);
    sock_->flush();
}

void MainWindow::onSocketReadyRead() {
    if (!sock_) return;
    inBuffer_.append(sock_->readAll());
    // consume complete lines
    while (true) {
        int idx = inBuffer_.indexOf('\n');
        if (idx < 0) break;
        QByteArray line = inBuffer_.left(idx);
        inBuffer_.remove(0, idx + 1);
        if (line.trimmed().isEmpty()) continue;

        QJsonParseError err{};
        auto doc = QJsonDocument::fromJson(line, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            log("ERROR: bad JSON message");
            continue;
        }
        handleJson(doc.object());
    }
}

void MainWindow::handleJson(const QJsonObject &obj) {
    const QString type = obj.value("type").toString();
    if (type == "challenge") {
        int c = obj.value("c").toInt();
        log(QString("RECV challenge c=%1").arg(c));
        handleChallenge(c);
    } else if (type == "response") {
        QString rHex = obj.value("r").toString();
        log(QString("RECV response r=%1...").arg(rHex.left(16)));
        handleResponse(QByteArray::fromHex(rHex.toLatin1()));
    } else if (type == "ack") {
        bool ok = obj.value("ok").toBool();
        log(QString("RECV ack ok=%1").arg(ok ? "1" : "0"));
        handleAck(ok);
    } else {
        log("ERROR: unknown message type: " + type);
    }
}

// ---------------- Buttons ----------------

void MainWindow::onStart() {
    if (!sock_) { log("Not connected."); return; }
    if (role_.compare("Alice", Qt::CaseInsensitive) != 0) {
        log("Start is only for Alice.");
        return;
    }
    c_ = 1;
    waitingResponse_ = false;
    running_ = true;
    tick_.start(sleepMs_);
    log(QString("Alice started. Will run up to %1 rounds (n-1).").arg(n_-1));
}

void MainWindow::onStop() {
    running_ = false;
    waitingResponse_ = false;
    tick_.stop();
    log("Stopped.");
}

// -------------- Ticker (Alice drives rounds) --------------

void MainWindow::onTick() {
    if (!running_) return;
    if (role_.compare("Alice", Qt::CaseInsensitive) != 0) return;

    if (c_ >= n_) {
        log("All rounds complete (n-1). Stopping.");
        onStop();
        return;
    }
    if (!waitingResponse_)
        sendChallenge();
}

void MainWindow::sendChallenge() {
    // Alice -> Bob
    QJsonObject msg;
    msg["type"] = "challenge";
    msg["c"] = c_;
    sendJson(msg);
    log(QString("SEND challenge c=%1").arg(c_));
    waitingResponse_ = true;
}

// ---------------- Bob handles challenge -------------------

void MainWindow::handleChallenge(int c) {
    if (role_.compare("Bob", Qt::CaseInsensitive) != 0) {
        log("Ignoring challenge (not Bob).");
        return;
    }
    if (c <= 0 || c > n_) {
        log("ERROR: invalid c");
        return;
    }
    // Compute r = H^{n-c}(h0). We precomputed chain.
    QByteArray r = chain_.responseForChallenge(c);
    QJsonObject msg;
    msg["type"] = "response";
    msg["r"] = bytesToHex(r);
    sendJson(msg);
    log(QString("SEND response for c=%1 (r=%2...)")
            .arg(c).arg(bytesToHex(r).left(16)));
}

// --------------- Alice verifies response ------------------

bool MainWindow::verifyAndUpdateTheta(const QByteArray &r) {
    // Accept iff Hash(r) == theta; then set theta = r
    QByteArray h = sha256CryptoPP(r);
    if (h == theta_) {
        theta_ = r;
        return true;
    }
    return false;
}

void MainWindow::handleResponse(const QByteArray &r) {
    if (role_.compare("Alice", Qt::CaseInsensitive) != 0) {
        log("Ignoring response (not Alice).");
        return;
    }
    bool ok = verifyAndUpdateTheta(r);
    // send ack
    QJsonObject msg;
    msg["type"] = "ack";
    msg["ok"] = ok;
    sendJson(msg);
    log(QString("SEND ack ok=%1").arg(ok ? "1" : "0"));

    if (!ok) {
        log("Verification FAILED. Stopping.");
        onStop();
        return;
    }
    waitingResponse_ = false;
    c_ += 1; // next round
}

// ------------------ Bob receives ack ----------------------

void MainWindow::handleAck(bool ok) {
    if (role_.compare("Bob", Qt::CaseInsensitive) != 0) {
        log("Ignoring ack (not Bob).");
        return;
    }
    if (!ok) {
        log("Alice rejected response.");
    }
}

// ----------------- Connect/Disconnect ---------------------

