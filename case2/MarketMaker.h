#ifndef __MARKET_MAKER_H_
#define __MARKET_MAKER_H_

#include <iostream>
#include <thread>
#include <atomic>
#include <memory>
#include <string>

extern "C" double black(double F, double K, double sigma, double T, double q);

namespace kvt {
    struct Order {
        enum class OrderType {
            Market,
            Limit
        };

        std::string asset;
        int qty;
        OrderType type;
        float price;
        // competitor identifier
        std::string order_id;
    };

    struct Fill {

    };

    struct Asset {
        std::string asset_code;
        // min_tick_size
        // max_position
    };

    struct PriceLevel {
        int size;
        float price;
    };

    struct MarketUpdate {
        Asset asset;
        float mid_market_price;
        std::vector<PriceLevel> bids;
        std::vector<PriceLevel> asks;
    };

    struct Update {
        std::vector<Fill> fills;
        std::vector<MarketUpdate> market_updates;
    };

    class MarketMaker {
        public:
            static double black_scholes_merton(int flag, double S, double K, double t,
                    double r, double sigma, double q) {
                int binary_flag = (flag == 'c') ? 1 : -1;
                S = S * std::exp((r-q)*t);
                auto p = black(S, K, sigma, t, binary_flag);
                auto conversion_factor = std::exp(-r*t);
                return p * conversion_factor;
            }

            static double black_scholes(int flag, double S, double K, double t,
                    double r, double sigma) {
                int binary_flag = (flag == 'c') ? 1 : -1;
                double discount_factor = std::exp(-r*t);
                double F = S / discount_factor;
                return black(F, K, sigma, t, binary_flag) * discount_factor;
            }

            static double delta(int flag, double S, double K, double t, double r,
                    double sigma) {
                double dS = .01;
                return (black_scholes(flag, S + dS, K, t, r, sigma)
                      - black_scholes(flag, S - dS, K, t, r, sigma))
                      / (2 * dS);
            }

            static double vega(int flag, double S, double K, double t, double r,
                    double sigma) {
                double dSigma = .01;
                return (black_scholes(flag, S, K, t, r, sigma + dSigma)
                      - black_scholes(flag, S, K, t, r, sigma - dSigma))
                      / (2 * dSigma);
            }

            MarketMaker()
                : val_{0}
            {
                thread_.reset(new std::thread(&MarketMaker::body, this));
            }

            int handle_update(Update const& update) {
                for(auto& u : update.market_updates) {
                    for(auto& bid : u.bids) {
                        std::cout << "price: " << bid.price << std::endl;
                    }
                }
                int ret = val_.load();
                val_.store(0);
                return ret;
            }

            void body() {
                while(true) {
                    ++val_;
                }
            }

        private:
            std::atomic<int> val_;
            std::unique_ptr<std::thread> thread_;
    };
}
#endif
