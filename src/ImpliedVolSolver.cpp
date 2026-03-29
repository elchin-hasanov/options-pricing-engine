// solves for implied volatility

// BS(sigma) - market_price = 0
// newton-raphson Xn+1 = Xn - f(Xn)/f'(Xn) : sigma_n+1 = sigma_n - (BS(sigma) - market_price) / vega

#include "BlackScholes.hpp"
#include "ImpliedVolSolver.hpp"
#include <cmath>

double vegaCalc(double S, double K, double T, double r, double sigma) {
    double numerator = log(S / K) + (r + 0.5 * sigma * sigma) * T;
    double exponent = -(numerator * numerator) / (2 * sigma * sigma * T);

    return S * sqrt(T / (2 * M_PI)) * exp(exponent);
}

double ImpliedVolSolver::impliedVolatility(
    double S,
    double K,
    double T,
    double r,
    double marketPrice,
    double initialSigma,
    double tolerance,
    int maxIterations
) {
    double sigma = initialSigma;

    for (int i = 0; i < maxIterations; i++) {
        double vega = vegaCalc(S, K, T, r, sigma);
        double error = BlackScholes::callPrice(S, K, T, r, sigma) - marketPrice;

        if (fabs(error) < tolerance) {
            return sigma;
        }

        if (fabs(vega) < 1e-8) {
            break;
        }

        sigma = sigma - error / vega;

        if (sigma <= 0) {
            sigma = 1e-4;
        }
    }

    return sigma;
}