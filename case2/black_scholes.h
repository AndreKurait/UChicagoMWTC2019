#ifndef __BLACK_SCHOLES_H_
#define __BLACK_SCHOLES_H_

extern "C" double black(double F, double K, double sigma, double T, double q);
extern "C" double implied_volatility_from_a_transformed_rational_guess(double price, double F, double K, double T, double q /* q=±1 */);

namespace kvt {
    namespace bs {
        double black_scholes_merton(int flag, double S, double K, double t,
                double r, double sigma, double q);

        double black_scholes(int flag, double S, double K, double t,
                double r, double sigma);

        double delta(int flag, double S, double K, double t, double r,
                double sigma);

        double vega(int flag, double S, double K, double t, double r,
                double sigma);

        double implied_volatility(double price, double S, double K,
                double t, double r, char flag);
    }
}

#endif
