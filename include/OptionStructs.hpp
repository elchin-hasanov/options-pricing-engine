#ifndef OPTIONSTRUCTS_HPP
#define OPTIONSTRUCTS_HPP

#include <string>

struct RawOptionRecord {
    std::string contractSymbol;
    std::string type;
    std::string expiration;
    double strike;
    double closePrice;
    double volume;
    double openInterest;
    long expirationUnix;

    RawOptionRecord()
        : contractSymbol(""),
          type(""),
          expiration(""),
          strike(0.0),
          closePrice(0.0),
          volume(0.0),
          openInterest(0.0),
          expirationUnix(0) {
    }
};

struct OptionRecord {
    std::string contractSymbol;
    std::string type;
    std::string expiration;
    double strike;
    double maturity;
    double marketPrice;
    double volume;
    double openInterest;

    OptionRecord()
        : contractSymbol(""),
          type(""),
          expiration(""),
          strike(0.0),
          maturity(0.0),
          marketPrice(0.0),
          volume(0.0),
          openInterest(0.0) {
    }
};

struct VolPoint {
    std::string contractSymbol;
    std::string expiration;
    double strike;
    double maturity;
    double marketPrice;
    double impliedVol;

    VolPoint()
        : contractSymbol(""),
          expiration(""),
          strike(0.0),
          maturity(0.0),
          marketPrice(0.0),
          impliedVol(0.0) {
    }
};

#endif