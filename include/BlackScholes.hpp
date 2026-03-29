#ifndef BLACKSCHOLES_HPP
#define BLACKSCHOLES_HPP

class BlackScholes {
public:
    static double callPrice (
        double S, //stock price
        double K, //strike price
        double T, //time to maturity
        double r, // risk-free interest rate
        double sigma //volatility
    );
};

#endif