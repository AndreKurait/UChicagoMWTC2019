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
    namespace Asset {
        enum Type {
            IDXPHX,

            C98PHX,
            C99PHX,
            C100PHX,
            C101PHX,
            C102PHX,

            P98PHX,
            P99PHX,
            P100PHX,
            P101PHX,
            P102PHX,

            Size
        };

        std::string to_str(Type t) {
            switch(t) {
                case IDXPHX:  return "IDX#PHX";
                case C98PHX:  return "C98PHX";
                case C99PHX:  return "C99PHX";
                case C100PHX: return "C100PHX";
                case C101PHX: return "C101PHX";
                case C102PHX: return "C102PHX";
                case P98PHX:  return "P98PHX";
                case P99PHX:  return "P99PHX";
                case P100PHX: return "P100PHX";
                case P101PHX: return "P101PHX";
                case P102PHX: return "P102PHX";
            }
            assert(false);
            return "";
        }

        Type parse(std::string str) {
            if(str == "IDX#PHX") {
                return IDXPHX;
            } else if(str == "C98PHX") {
                return C98PHX;
            } else if(str == "C99PHX") {
                return C99PHX;
            } else if(str == "C100PHX") {
                return C100PHX;
            } else if(str == "C101PHX") {
                return C101PHX;
            } else if(str == "C102PHX") {
                return C102PHX;
            } else if(str == "P98PHX") {
                return P98PHX;
            } else if(str == "P99PHX") {
                return P99PHX;
            } else if(str == "P100PHX") {
                return P100PHX;
            } else if(str == "P101PHX") {
                return P101PHX;
            } else if(str == "P102PHX") {
                return P102PHX;
            }
            assert(false);
            return IDXPHX;
        }

        int get_strike(Type t) {
            if(t == C98PHX) {
                return 98;
            } else if(t == C99PHX) {
                return 99;
            } else if(t == C100PHX) {
                return 100;
            } else if(t == C101PHX) {
                return 101;
            } else if(t == C102PHX) {
                return 102;
            } else if(t == P98PHX) {
                return 98;
            } else if(t == P99PHX) {
                return 99;
            } else if(t == P100PHX) {
                return 100;
            } else if(t == P101PHX) {
                return 101;
            } else if(t == P102PHX) {
                return 102;
            }
            assert(false);
            return 0;
        }

        char get_bs_type(Type t) {
            switch(t) {
                case C98PHX:
                case C99PHX:
                case C100PHX:
                case C101PHX:
                case C102PHX:
                    return 'c';
                case P98PHX:
                case P99PHX:
                case P100PHX:
                case P101PHX:
                case P102PHX:
                    return 'p';
            }
            assert(false);
            return 0;
        }
    }

    struct Spread;

    struct Competitor {
        std::string id;
    };

    struct Order {
        enum class Strategy {
            MarketMaking,
            Hedge
        };

        enum class OrderType {
            Market,
            Limit
        };

        Strategy strategy;

        Asset::Type asset;
        int size;
        int remaining;
        OrderType type;
        double price;
        double spread;
        bool bid;
        Competitor comp;
        std::string order_id;

        // Keep track of where this order is between python calls
        Order* self;
    };

    struct Fill {
        std::string order_id;
        Competitor comp;
        int filled;
        double fill_price;
    };


    struct PriceLevel {
        int size;
        double price;
    };

    struct MarketUpdate {
        Asset::Type asset;
        double mid_market_price;
        std::vector<PriceLevel> bids;
        std::vector<PriceLevel> asks;
    };

    struct Update {
        std::vector<Fill> fills;
        std::vector<MarketUpdate> market_updates;
    };

    struct Spread {
        Spread(Asset::Type asset, double spread, int size)
            : asset(asset)
            , spread(spread)
            , size(size)
            , bid(nullptr)
            , ask(nullptr)
        {}

        Asset::Type asset;
        double spread;
        int size;

        std::unique_ptr<Order> bid;
        std::unique_ptr<Order> ask;
        // std::list<Order>::iterator bid;
        // std::list<Order>::iterator ask;
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

            MarketMaker() {
                for(int asset = Asset::Type::IDXPHX; asset < Asset::Type::Size; ++asset) {
                    spreads_.emplace_back(static_cast<Asset::Type>(asset), 0.2, 10);
                    //spreads_.emplace_back(asset, 0.03, 10, std::end(orders_), std::end(orders_));
                }

                thread_.reset(new std::thread(&MarketMaker::process_orders, this));
            }

            void handle_update(Update const& update) {
                { // mutex scope
                    std::lock_guard<std::mutex> lk(lastMidPriceMut_);
                    for(auto const& mu : update.market_updates) {
                        // This normalization of the double is to avoid order rejections
                        // due to minimum tick size
                        lastMidPrice_[mu.asset] =
                            static_cast<double>(round(mu.mid_market_price * 100)) / 100.0;
                    }
                }

                newMarket_.store(true);
            }

            void handle_fill(Fill const& fill) {
                std::lock_guard<std::mutex> lk(fillsMut_);
                fills_.push_back(fill);
            }

            void place_order(Order const& order, std::string order_id) {
                std::lock_guard<std::mutex> lk(ordersMut_);
                orders_[order_id] = order.self;
                /*
                std::lock_guard<std::mutex> lk(ordersMapMut_);
                    auto order_itr = std::end(orders_);
                if(order.strategy == Order::Strategy::MarketMaking) {
                    if(order.bid) {
                        order_itr = order.spreadStrat->bid;
                    } else {
                        order_itr = order.spreadStrat->ask;
                    }
                    order_itr->order_id = order_id;
                    ordersMap_[order_id] = order_itr;
                } else {
                    std::lock_guard<std::mutex> lk(hedgeMut_);
                    hedge_.push_back(order);
                }
                */
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
                if(!existing_order || existing_order->remaining <= 0) {
                    lastMidPriceMut_.lock();
                    auto last_price = lastMidPrice_[spread.asset];
                    lastMidPriceMut_.unlock();
                    if(last_price > 0.01) {
                        existing_order              = std::make_unique<Order>();
                        existing_order->asset       = spread.asset;
                        existing_order->strategy    = Order::Strategy::MarketMaking;
                        existing_order->price       = last_price;
                        existing_order->spread      = spread.spread;
                        existing_order->size        = spread.size;
                        existing_order->remaining   = spread.size;
                        existing_order->type        = Order::OrderType::Limit;
                        existing_order->bid         = bid;
                        existing_order->self        = existing_order.get();
                        std::lock_guard<std::mutex> lc(pendingOrdersMut_);
                        pendingOrders_.push_back(existing_order.get());
                    }
                }
            }

            void process_fills() {
                std::lock_guard<std::mutex> lk(fillsMut_);
                for(auto const& fill : fills_) {
                    auto* order = orders_[fill.order_id];
                    if(!order) {
                        continue;
                    }
                    if(order->bid) {
                        portfolio_[order->asset] += fill.filled;
                    } else {
                        portfolio_[order->asset] -= fill.filled;
                    }
                    order->remaining -= fill.filled;
                    /*
                    auto search = ordersMap_.find(fill.order_id);
                    if(search == std::end(ordersMap_)) {
                        continue;
                    }
                    auto order = search->second;
                    order->remaining -= fill.filled;
                    if(order->remaining == 0) {
                        if(order->bid) {
                            order->spreadStrat->bid = std::end(orders_);
                        } else {
                            order->spreadStrat->ask = std::end(orders_);
                        }
                        orders_.erase(order);
                    }
                }
                */
                }
                fills_.clear();
            }

            void process_orders() {
                while(true) {
                    if(!newMarket_.load()) {
                        continue;
                    }

                    process_fills();

                    // maintain spreads
                    for(auto& spread : spreads_) {
                        populate_spreads(spread, true);
                        populate_spreads(spread, false);
                    }

                    // manage greeks
                    double delta_portfolio = 0;
                    std::cout << "PHX#IDX: " << portfolio_[0] << std::endl;
                    for(int i = Asset::IDXPHX + 1; i < Asset::Size; ++i) {
                        delta_portfolio += portfolio_[i] * delta(
                            Asset::get_bs_type(static_cast<Asset::Type>(i)),
                            lastMidPrice_[Asset::IDXPHX],
                            Asset::get_strike(static_cast<Asset::Type>(i)),
                            0.1666666,
                            0,
                            0.2
                        );
                        std::cout << Asset::to_str(static_cast<Asset::Type>(i)) << ": " << portfolio_[i] << std::endl;
                    }
                    delta_portfolio += portfolio_[Asset::IDXPHX];

                    std::cout << "delta: " << delta_portfolio << std::endl;

                    if(delta_portfolio > 1 || delta_portfolio < -1) {
                        Order* hedge    = new Order;
                        hedge->asset    = Asset::IDXPHX;
                        hedge->strategy = Order::Strategy::Hedge;
                        hedge->size     = abs(static_cast<int>(delta_portfolio));
                        hedge->type     = Order::OrderType::Market;
                        hedge->price    = 1;
                        hedge->bid      = delta_portfolio < 0;
                        std::lock_guard<std::mutex> lk(pendingOrdersMut_);
                        pendingOrders_.push_back(hedge);
                    }
                    newMarket_.store(false);
                }
            }

        private:
            std::vector<std::string> allAssets_;

            std::atomic<bool> newMarket_;

            // hash table of last recorded market price of an asset
            std::mutex lastMidPriceMut_;
            double lastMidPrice_[Asset::Type::Size];

            int portfolio_[Asset::Type::Size];

            std::mutex ordersMut_;
            std::unordered_map<std::string, Order*> orders_;

            std::mutex fillsMut_;
            std::vector<Fill> fills_;

            // orders generated in processing thread which we are waiting to place
            std::mutex pendingOrdersMut_;
            std::vector<Order*> pendingOrders_;

            // what spreads are we trading?
            std::vector<Spread> spreads_;

            // processing thread
            std::unique_ptr<std::thread> thread_;
    };
}
#endif
