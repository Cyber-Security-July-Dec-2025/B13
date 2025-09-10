#include "lamport.h"


#include <cryptopp/sha.h>
#include <cryptopp/cryptlib.h>

QByteArray sha256CryptoPP(const QByteArray &in) {
    CryptoPP::SHA256 hash;
    QByteArray out(32, 0);
    hash.CalculateDigest(
        reinterpret_cast<CryptoPP::byte*>(out.data()),
        reinterpret_cast<const CryptoPP::byte*>(in.constData()),
        static_cast<size_t>(in.size())
        );
    return out;
}

bool HashChain::build(const QByteArray &h0, int n) {
    if (n <= 0 || h0.isEmpty())
        return false;

    n_ = n;

   
    QVector<QByteArray> v;
    v.reserve(n_ + 1);

    v.push_back(h0);

    for (int i = 1; i <= n_; ++i) {
        v.push_back(sha256CryptoPP(v.back()));
    }

    chain_.swap(v);
    return true;
}

QByteArray HashChain::responseForChallenge(int c) const {
    if (c <= 0 || c > n_ || chain_.size() != n_ + 1)
        return QByteArray();

    const int idx = n_ - c;
    return chain_.at(idx);
}
