#ifndef __MARKET_MAKER_H_
#define __MARKET_MAKER_H_

#include <algorithm>
#include <iostream>
#include <list>
#include <thread>
#include <mutex>
#include <atomic>
#include <memory>
#include <string>

extern "C" double black(double F, double K, double sigma, double T, double q);

namespace kvt {
    struct Spread;

    struct Competitor {
        std::string id;
    };

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
        Competitor comp;
        std::string order_id;

        // the spread which this order was generated for
        Spread* spreadStrat;
    };

    struct Fill {
        std::string order_id;
        Competitor comp;
        int filled;
        int remaining;
        double fill_price;
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
        Spread(std::string asset, double spread, int size,
                std::list<Order>::iterator bid, std::list<Order>::iterator ask)
            : asset(asset)
            , spread(spread)
            , size(size)
            , bid(bid)
            , ask(ask)
        {}

        std::string asset;
        double spread;
        int size;
        std::list<Order>::iterator bid;
        std::list<Order>::iterator ask;
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
                , ctr_{0}
            {
                for(auto const& asset : allAssets_) {
                    spreads_.emplace_back(asset, 0.2, 10, std::end(orders_), std::end(orders_));
                    //spreads_.emplace_back(asset, 0.03, 10, std::end(orders_), std::end(orders_));
                }

                thread_.reset(new std::thread(&MarketMaker::process_orders, this));
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

                //std::cout << "SIZE: " << orders_.size() << std::endl;

                int ret = ctr_.load();
                ctr_.store(0);
                return ret;
            }

            void handle_fill(Fill const& fill) {
                std::lock_guard<std::mutex> lk(fillsMut_);
                fills_.push_back(fill);
            }

            void place_order(Order const& order, std::string order_id) {
                auto order_itr = std::end(orders_);
                if(order.bid) {
                    order_itr = order.spreadStrat->bid;
                } else {
                    order_itr = order.spreadStrat->ask;
                }
                order_itr->order_id = order_id;
                std::lock_guard<std::mutex> lk(ordersMapMut_);
                ordersMap_[order_id] = order_itr;
                std::lock_guard<std::mutex> lc(placedOrdersQueueMut_);
                placedOrdersQueue_.push_back(&*order_itr);
            }

            std::vector<Order> get_and_clear_orders() {
                std::lock_guard<std::mutex> lk(pendingOrdersMut_);
                std::vector<Order> orders;
                std::transform(std::begin(pendingOrders_),
                               std::end(pendingOrders_),
                               std::back_inserter(orders),
                               [] (auto* order) {
                                    return *order;
                               });
                pendingOrders_.clear();
                return orders;
            }

            void populate_spreads(Spread& spread, bool bid) {
                auto& existing_order = (bid) ? spread.bid : spread.ask;
                if(existing_order == std::end(orders_)) {
                    lastMidPriceMut_.lock();
                    auto last_price = lastMidPrice_[spread.asset];
                    lastMidPriceMut_.unlock();
                    if(last_price > 0.01) {
                        std::lock_guard<std::mutex> lc(pendingOrdersMut_);
                        auto& o = orders_.emplace_back();
                        o.asset = spread.asset;
                        o.price = last_price;
                        o.spread = spread.spread;
                        o.qty   = spread.size;
                        o.type  = Order::OrderType::Limit;
                        o.bid   = bid;
                        o.spreadStrat = &spread;
                        existing_order = --std::end(orders_);
                        pendingOrders_.push_back(&o);
                    }
                }
            }

            void process_fills() {
                std::lock_guard<std::mutex> lk(fillsMut_);
                for(auto const& fill : fills_) {
                    //std::cout << "FILL: " << fill.fill_price
                    //    << " SIZE " << fill.filled << std::endl;
                    if(fill.remaining == 0) {
                        std::lock_guard<std::mutex> lk(ordersMapMut_);
                        auto search = ordersMap_.find(fill.order_id);
                        if(search == std::end(ordersMap_)) {
                            continue;
                        }
                        auto order = search->second;
                        if(order->bid) {
                            order->spreadStrat->bid = std::end(orders_);
                        } else {
                            order->spreadStrat->ask = std::end(orders_);
                        }
                        orders_.erase(order);

                    }
                }
                fills_.clear();
            }

            int get_strike(std::string asset) {
                if(asset == "C98PHX") {
                    return 98;
                } else if(asset == "C99PHX") {
                    return 99;
                } else if(asset == "C100PHX") {
                    return 100;
                } else if(asset == "C101PHX") {
                    return 101;
                } else if(asset == "C102PHX") {
                    return 102;
                } else if(asset == "P98PHX") {
                    return 98;
                } else if(asset == "P99PHX") {
                    return 99;
                } else if(asset == "P100PHX") {
                    return 100;
                } else if(asset == "P101PHX") {
                    return 101;
                } else if(asset == "P102PHX") {
                    return 102;
                }
            }

            void process_orders() {
                while(true) {
                    ++ctr_;

                    process_fills();

                    // maintain spreads
                    for(auto& spread : spreads_) {
                        populate_spreads(spread, true);
                        populate_spreads(spread, false);
                    }

                    // manage greeks
                    double delta_portfolio = 0;
                    for(auto const& order : orders_) {
                        // hasn't been accepted by exchange yet
                        if(order.order_id.empty()) {
                            continue;
                        }
                        delta_portfolio += delta((order.asset[0] == 'C') ? 'c' : 'p',
                                lastMidPrice_["IDX#PHX"],
                                get_strike(order.asset),
                                0.16666666,
                                0,
                                0.2);
                    }

                    delta_portfolio *= 100.0;
                    if(delta_portfolio > 1 || delta_portfolio < -1)
                    {
                        Order* hedge = new Order;
                        hedge->asset = "IDX#PHX";
                        hedge->qty   = abs(static_cast<int>(delta_portfolio));
                        hedge->type  = Order::OrderType::Market;
                        hedge->price = 1;
                        hedge->bid   = delta_portfolio > 0;
                        std::lock_guard<std::mutex> lk(pendingOrdersMut_);
                        pendingOrders_.push_back(hedge);
                    }

                }
            }

        private:
            std::vector<std::string> allAssets_;

            // hash table of last recorded market price of an asset
            std::mutex lastMidPriceMut_;
            std::unordered_map<std::string, double> lastMidPrice_;

            // object pool of every order that is live (has not been cancelled or
            // completely filled)
            std::list<Order> orders_;

            std::mutex fillsMut_;
            std::vector<Fill> fills_;

            // reference an order by its id
            std::mutex ordersMapMut_;
            std::unordered_map<std::string, decltype(orders_)::iterator> ordersMap_;

            // orders generated in processing thread which we are waiting to place
            std::mutex pendingOrdersMut_;
            std::vector<Order*> pendingOrders_;

            // orders that we have placed in the exchange
            std::mutex placedOrdersQueueMut_;
            std::vector<Order*> placedOrdersQueue_;

            // what spreads are we trading?
            std::vector<Spread> spreads_;

            // This is just for testing for now, to see how fast the processing thread
            // actually is
            std::atomic<int> ctr_;

            // processing thread
            std::unique_ptr<std::thread> thread_;
    };
}
#endif
