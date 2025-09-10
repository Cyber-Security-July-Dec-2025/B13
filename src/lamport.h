#pragma once
#include <QByteArray>
#include <QVector>
#include <QString>

struct ChainConfig {
    int n = 100;
    QByteArray h0;  // only Bob needs this
    QByteArray hn;  // only Alice needs this
};

QByteArray sha256CryptoPP(const QByteArray &in);

// Holds a precomputed chain if h0 is known (Bob’s side)
class HashChain {
public:
    HashChain() = default;

    // Build h[0]=h0, h[1]=H(h0), ..., h[n]=H^n(h0)
    bool build(const QByteArray &h0, int n);

    // Returns r = H^{n-c}(h0) quickly as chain[n - c]
    QByteArray responseForChallenge(int c) const;

    // Accessors
    const QVector<QByteArray>& chain() const { return chain_; }
    int length() const { return n_; }

private:
    QVector<QByteArray> chain_;
    int n_ = 0;
};
