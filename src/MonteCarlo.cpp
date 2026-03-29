#include "MonteCarlo.hpp"
#include <random>
#include <cmath>
#include <algorithm>

double monteCarloCallPrice(
    double S0,
    double K,
    double T,
    double r,
    double sigma,
    int numSimulations
) {
    //mersenne twister (pseudo random num generator)
    // needs the random_device{}() seed, to generate it uniquely every time

    std::mt19937 gen(std::random_device{}()); 
    std::normal_distribution<double> dist(0.0, 1.0); //normal distribution

    double payoffSum = 0.0;

    for (int i = 0; i < numSimulations; i++) {
        double Z = dist(gen);

        double ST = S0 * std::exp(
            (r - 0.5 * sigma * sigma) * T
            + sigma * std::sqrt(T) * Z
        ); // future price = current price * (growth + randomness)

        double payoff = std::max(ST - K, 0.0);
        payoffSum += payoff;
    }

    double averagePayoff = payoffSum / numSimulations;
    return std::exp(-r * T) * averagePayoff;
}


// same but payoff is K-ST
double monteCarloPutPrice(
    double S0,
    double K,
    double T,
    double r,
    double sigma,
    int numSimulations
) {
    std::mt19937 gen(std::random_device{}());
    std::normal_distribution<double> dist(0.0, 1.0);

    double payoffSum = 0.0;

    for (int i = 0; i < numSimulations; i++) {
        double Z = dist(gen);

        double ST = S0 * std::exp(
            (r - 0.5 * sigma * sigma) * T
            + sigma * std::sqrt(T) * Z
        );

        double payoff = std::max(K - ST, 0.0);
        payoffSum += payoff;
    }

    double averagePayoff = payoffSum / numSimulations;
    return std::exp(-r * T) * averagePayoff;
}