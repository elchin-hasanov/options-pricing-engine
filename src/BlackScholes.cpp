//computes option prices based on S, K, r, T, sigma

#include "BlackScholes.hpp";

#include <cmath>

static double normalCDF(double value) {
    return 0.5 * erfc(-value * M_SQRT1_2);
}

double BlackScholes::callPrice(
        double S,
        double K,
        double T,
        double r,
        double sigma
) {
    double d1 {(log(S/K) + (r + sigma * sigma / 2) * T) / (sigma * sqrt(T))};
    double d2 {d1 - sigma * sqrt(T)};
    
    return S * normalCDF(d1) - K * exp(-r * T) * normalCDF(d2);
}