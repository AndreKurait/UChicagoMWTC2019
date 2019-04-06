#ifndef __MARKET_MAKER_H_
#define __MARKET_MAKER_H_

#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include <memory>
#include <string>

extern "C" double black(double F, double K, double sigma, double T, double q);

namespace kvt {
    struct Spread;

    struct Order {
        enum class OrderType {
            Market,
            Limit
        };

        std::string asset;
        int qty;
        OrderType type;
        double price;
        double spread;
        bool bid;
        // competitor identifier
        std::string order_id;

        // the spread which this order was generated for
        Spread* spreadStrat;
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
        double price;
    };

    struct MarketUpdate {
        Asset asset;
        double mid_market_price;
        std::vector<PriceLevel> bids;
        std::vector<PriceLevel> asks;
    };

    struct Update {
        std::vector<Fill> fills;
        std::vector<MarketUpdate> market_updates;
    };

    struct Spread {
        Spread(std::string asset, double spread, int size)
            : asset(asset)
            , spread(spread)
            , size(size)
            , bid(nullptr)
            , ask(nullptr)
        {}

        std::string asset;
        double spread;
        int size;
        Order* bid;
        Order* ask;
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
                : allAssets_{"C98PHX", "C99PHX", "C100PHX", "C101PHX", "C102PHX",
                             "P98PHX", "P99PHX", "P100PHX", "P101PHX", "P102PHX",
                             "IDX#PHX"}
                , val_{0}
            {
                holdingOrder_.reset(new Order);

                for(auto const& asset : allAssets_) {
                    spreads_.emplace_back(asset, 0.01, 10);
                    spreads_.emplace_back(asset, 0.03, 10);
                }

                thread_.reset(new std::thread(&MarketMaker::body, this));
            }

            int handle_update(Update const& update) {
                { // mutex scope
                    std::lock_guard<std::mutex> lk(lastMidPriceMut_);
                    for(auto const& mu : update.market_updates) {
                        // This normalization of the double is to avoid order rejections
                        // due to minimum tick size
                        lastMidPrice_[mu.asset.asset_code] =
                            static_cast<double>(round(mu.mid_market_price * 100)) / 100.0;
                    }
                }

                int ret = val_.load();
                val_.store(0);
                return ret;
            }

            void place_order(Order order, std::string order_id) {
                order.order_id = order_id;
                std::lock_guard<std::mutex> lk(placedOrdersQueueMut_);
                placedOrdersQueue_.push_back(order);
            }

            std::vector<Order> get_and_clear_orders() {
                std::lock_guard<std::mutex> lk(pendingOrdersMut_);
                std::vector<Order> ret(pendingOrders_);
                pendingOrders_.clear();
                return ret;
            }

            void body() {
                while(true) {
                    ++val_;
                    // make sure spreads are populated
                    for(auto& spread : spreads_) {
                        if(spread.bid == nullptr) {
                            lastMidPriceMut_.lock();
                            auto last_price = lastMidPrice_[spread.asset];
                            lastMidPriceMut_.unlock();
                            if(last_price > 0.01) {
                                std::lock_guard<std::mutex> lc(pendingOrdersMut_);
                                // mark this as taken
                                spread.bid = holdingOrder_.get();

                                Order o;
                                o.asset = spread.asset;
                                o.price = last_price;
                                o.qty   = spread.size;
                                o.type  = Order::OrderType::Limit;
                                o.bid   = true;
                                o.spreadStrat = &spread;
                                pendingOrders_.push_back(o);
                            }
                        }

                    if(spread.ask == nullptr) {
                            lastMidPriceMut_.lock();
                            auto last_price = lastMidPrice_[spread.asset];
                            lastMidPriceMut_.unlock();
                            // prevent premature orders
                            if(last_price > 0.01) {
                                std::lock_guard<std::mutex> lc(pendingOrdersMut_);
                                // mark this as taken
                                spread.ask = holdingOrder_.get();

                                Order o;
                                o.asset = spread.asset;
                                o.price = last_price;
                                o.qty   = spread.size;
                                o.type  = Order::OrderType::Limit;
                                o.bid   = false;
                                o.spreadStrat = &spread;
                                pendingOrders_.push_back(o);
                            }
                        }
                    }
                }
            }

        private:
            std::vector<std::string> allAssets_;

            // holding variable for when we have sent an order to an exchange
            // for a spread strategy, but it hasn't been confirmed
            std::unique_ptr<Order> holdingOrder_;

            // hash table of last recorded market price of an asset
            std::mutex lastMidPriceMut_;
            std::unordered_map<std::string, double> lastMidPrice_;

            // orders generated in processing thread which we are waiting to place
            std::mutex pendingOrdersMut_;
            std::vector<Order> pendingOrders_;

            // orders that we have placed in the exchange
            std::mutex placedOrdersQueueMut_;
            std::vector<Order> placedOrdersQueue_;

            // what spreads are we trading?
            std::vector<Spread> spreads_;

            // This is just for testing for now, to see how fast the processing thread
            // actually is
            std::atomic<int> val_;

            // processing thread
            std::unique_ptr<std::thread> thread_;
    };
}
#endif
