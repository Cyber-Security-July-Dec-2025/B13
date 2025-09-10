#pragma once
#include <QByteArray>
#include <QVector>
#include <QString>

struct ChainConfig {
    int n = 100;
    QByteArray h0; 
    QByteArray hn;  
};

QByteArray sha256CryptoPP(const QByteArray &in);

class HashChain {
public:
    HashChain() = default;

    bool build(const QByteArray &h0, int n);

    QByteArray responseForChallenge(int c) const;

    const QVector<QByteArray>& chain() const { return chain_; }
    int length() const { return n_; }

private:
    QVector<QByteArray> chain_;
    int n_ = 0;
};
