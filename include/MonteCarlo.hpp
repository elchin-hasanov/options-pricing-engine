#ifndef MONTECARLO_HPP
#define MONTECARLO_HPP

double monteCarloCallPrice(
    double S0,
    double K,
    double T,
    double r,
    double sigma,
    int numSimulations
);

double monteCarloPutPrice(
    double S0,
    double K,
    double T,
    double r,
    double sigma,
    int numSimulations
);

#endif