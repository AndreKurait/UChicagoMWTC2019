#include <cmath>

#include "black_scholes.h"

namespace kvt {
    namespace bs {
        double black_scholes_merton(int flag, double S, double K, double t,
                double r, double sigma, double q) {
            int binary_flag = (flag == 'c') ? 1 : -1;
            S = S * std::exp((r-q)*t);
            auto p = black(S, K, sigma, t, binary_flag);
            auto conversion_factor = std::exp(-r*t);
            return p * conversion_factor;
        }

        double black_scholes(int flag, double S, double K, double t,
                double r, double sigma) {
            int binary_flag = (flag == 'c') ? 1 : -1;
            double discount_factor = std::exp(-r*t);
            double F = S / discount_factor;
            return black(F, K, sigma, t, binary_flag) * discount_factor;
        }

        double delta(int flag, double S, double K, double t, double r,
                double sigma) {
            double dS = .01;
            return (black_scholes(flag, S + dS, K, t, r, sigma)
                  - black_scholes(flag, S - dS, K, t, r, sigma))
                  / (2 * dS);
        }

        double vega(int flag, double S, double K, double t, double r,
                double sigma) {
            double dSigma = .01;
            return (black_scholes(flag, S, K, t, r, sigma + dSigma)
                  - black_scholes(flag, S, K, t, r, sigma - dSigma))
                  / (2 * dSigma);
        }

        double implied_volatility(double price, double S, double K,
                double t, double r, char flag) {
            double adjusted_price = price / std::exp(-r*t);
            double forward_price  = S / std::exp(-r * t);
            int binary_flag = (flag == 'c') ? 1 : -1;
            return implied_volatility_from_a_transformed_rational_guess(
                adjusted_price,
                forward_price,
                K,
                t,
                binary_flag
            );
        }
    }
}
