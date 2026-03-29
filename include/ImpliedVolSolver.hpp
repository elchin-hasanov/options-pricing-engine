#ifndef IMPLIEDVOLSOLVER_HPP
#define IMPLIEDVOLSOLVER_HPP

class ImpliedVolSolver{
public:
    static double impliedVolatility(
        double S,
        double K,
        double T,
        double r,
        double marketPrice,
        double initialSigma,
        double tolerance,
        int maxIterations
    );
};




#endif