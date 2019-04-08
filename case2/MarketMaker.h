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

#define SQRT_252 15.8745078664

extern "C" double black(double F, double K, double sigma, double T, double q);
extern "C" double implied_volatility_from_a_transformed_rational_guess(double price, double F, double K, double T, double q /* q=±1 */);

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

        Type get_asset(int price, bool call) {
            switch(price) {
                case 98:  return call ? C98PHX  : P98PHX;
                case 99:  return call ? C99PHX  : P99PHX;
                case 100: return call ? C100PHX : P100PHX;
                case 101: return call ? C101PHX : P101PHX;
                case 102: return call ? C102PHX : P102PHX;
            }
            return C98PHX;
        }

        bool acceptable(std::string str) {
            if(str == "IDX#PHX") {
                return true;
            } else if(str == "C98PHX") {
                return true;
            } else if(str == "C99PHX") {
                return true;
            } else if(str == "C100PHX") {
                return true;
            } else if(str == "C101PHX") {
                return true;
            } else if(str == "C102PHX") {
                return true;
            } else if(str == "P98PHX") {
                return true;
            } else if(str == "P99PHX") {
                return true;
            } else if(str == "P100PHX") {
                return true;
            } else if(str == "P101PHX") {
                return true;
            } else if(str == "P102PHX") {
                return true;
            }
            return false;
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

        // in case the order fails to go through
        bool failed;

        // Keep track of where this order is between python calls
        Order* self;
    };

    struct Fill {
        Order order;
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

            static double implied_volatility(double price, double S, double K,
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

            MarketMaker(int straddle_size, int position_max, int spread_liquidity,
                    double reprice_thresh, double delta_limit)
                : straddle_size_{straddle_size}
                , position_max_{position_max}
                , spread_liquidity_{spread_liquidity}
                , reprice_thresh_{reprice_thresh}
                , delta_limit_{delta_limit}
                , greeks{0}
                , pendingHedge_{0}
            {
                portfolio_[0] = 0;
                lastMidPrice_[0] = 0;
                marketSpread_[0] = 0;
                for(int asset = Asset::Type::IDXPHX + 1; asset < Asset::Type::Size; ++asset) {
                    spreads_.emplace_back(static_cast<Asset::Type>(asset), 0.6, straddle_size);
                    portfolio_[asset] = 0;
                    lastMidPrice_[asset] = 0;
                    marketSpread_[asset] = 0;
                }

                thread_.reset(new std::thread(&MarketMaker::process_orders, this));
            }

            void handle_update(Update const& update) {
                { // mutex scope
                    std::lock_guard<std::mutex> lk(lastMidPriceMut_);
                    std::lock_guard<std::mutex> lc(marketSpreadMut_);
                    for(auto const& mu : update.market_updates) {
                        // This normalization of the double is to avoid order rejections
                        // due to minimum tick size
                        lastMidPrice_[mu.asset] =
                            static_cast<double>(round(mu.mid_market_price * 100)) / 100.0;

                        double best_bid = INT_MIN;
                        double best_ask = INT_MAX;
                        for(auto const& bid : mu.bids) {
                            if(best_bid < bid.price && bid.size >= spread_liquidity_) {
                                best_bid = bid.price;
                            }
                        }
                        for(auto const& ask : mu.asks) {
                            if(best_ask > ask.price && ask.size >= spread_liquidity_) {
                                best_ask = ask.price;
                            }
                        }
                        marketSpread_[mu.asset] = best_ask - best_bid;
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
                order.self->order_id = order_id;
                orders_[order_id] = order.self;
            }

            void modify_order(Order const& order, std::string order_id) {
                std::lock_guard<std::mutex> lk(ordersMut_);
                //orders_[order.self->order_id] = nullptr;
                order.self->order_id = order_id;
                orders_[order_id] = order.self;
            }

            void order_failed(Order const& order) {
                order.self->failed = true;
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

            std::vector<Order> get_and_clear_modifies() {
                std::lock_guard<std::mutex> lk(modifyOrdersMut_);
                std::vector<Order> orders;
                std::transform(std::begin(modifyOrders_),
                               std::end(modifyOrders_),
                               std::back_inserter(orders),
                               [] (auto* order) {
                                    return *order;
                               });
                modifyOrders_.clear();
                return orders;
            }

            void populate_spreads(Spread& spread, bool bid) {
                auto& existing_order = (bid) ? spread.bid : spread.ask;
                if(!existing_order || existing_order->remaining <= 0 || existing_order->failed) {
                    if((portfolio_[spread.asset] > position_max_ && bid)
                            || (portfolio_[spread.asset] < (-1 * position_max_) && !bid)) {
                        //existing_order.reset(nullptr);
                        //return;
                    }
                    lastMidPriceMut_.lock();
                    auto last_price = lastMidPrice_[spread.asset];
                    lastMidPriceMut_.unlock();
                    if(last_price > 0.01) {
                        existing_order.reset(new Order());
                        existing_order->asset       = spread.asset;
                        existing_order->strategy    = Order::Strategy::MarketMaking;
                        existing_order->price       = last_price;
                        existing_order->spread      = spread.spread;
                        existing_order->size        = spread.size;
                        existing_order->remaining   = spread.size;
                        existing_order->type        = Order::OrderType::Limit;
                        existing_order->bid         = bid;
                        existing_order->failed      = false;
                        existing_order->self        = existing_order.get();
                        std::lock_guard<std::mutex> lc(pendingOrdersMut_);
                        pendingOrders_.push_back(existing_order.get());
                    }
                } else {
                    bool modify = false;
                    std::lock_guard<std::mutex> lk(marketSpreadMut_);
                    std::lock_guard<std::mutex> lc(lastMidPriceMut_);

                    if((portfolio_[spread.asset] > position_max_ && bid)
                            || (portfolio_[spread.asset] < (-1 * position_max_) && !bid)) {
                        std::lock_guard<std::mutex> ll(cancelOrdersMut_);
                        cancelOrders_.push_back(existing_order->order_id);
                        existing_order.reset(nullptr);
                        return;
                    }

                    if(marketSpread_[spread.asset] + 0.01 <= spread.spread) {
                        modify                    = true;
                        spread.spread             = static_cast<double>(round(marketSpread_[spread.asset] * 100)) / 100.0;
                        existing_order->spread    = spread.spread;
                        existing_order->size      = existing_order->remaining;
                        existing_order->remaining = 0;
                        existing_order->price     = static_cast<double>(round(lastMidPrice_[spread.asset] * 100)) / 100.0;
                    } else if(bid
                            && (lastMidPrice_[spread.asset] - existing_order->price > (spread.spread/2.0 - reprice_thresh_))
                            && (lastMidPrice_[spread.asset] - existing_order->price < (spread.spread/2.0 + reprice_thresh_))) {
                        modify                    = true;
                        existing_order->price     = static_cast<double>(round(lastMidPrice_[spread.asset] * 100)) / 100.0;
                        existing_order->size      = existing_order->remaining;
                        existing_order->remaining = 0;
                    } else if(!bid
                            && (existing_order->price - lastMidPrice_[spread.asset] > (spread.spread/2.0 - reprice_thresh_))
                            && (existing_order->price - lastMidPrice_[spread.asset] < (spread.spread/2.0 + reprice_thresh_))) {
                        modify                    = true;
                        existing_order->price     = static_cast<double>(round(lastMidPrice_[spread.asset] * 100)) / 100.0;
                        existing_order->size      = existing_order->remaining;
                        existing_order->remaining = 0;
                    }

                    if(modify) {
                        std::lock_guard<std::mutex> ll(modifyOrdersMut_);
                        modifyOrders_.push_back(existing_order.get());
                    }
                }
            }

            void process_fills() {
                std::lock_guard<std::mutex> lk(fillsMut_);
                for(auto const& fill : fills_) {
                    if(fill.order.bid) {
                        portfolio_[fill.order.asset] += fill.filled;
                        if(fill.order.asset == Asset::IDXPHX) {
                            pendingHedge_ -= fill.filled;
                        }
                    } else {
                        portfolio_[fill.order.asset] -= fill.filled;
                        if(fill.order.asset == Asset::IDXPHX) {
                            pendingHedge_ += fill.filled;
                        }
                    }
                    orders_[fill.order.order_id]->remaining -= fill.filled;
                    if(orders_[fill.order.order_id]->remaining <= 0
                            && orders_[fill.order.order_id]->strategy == Order::Strategy::Hedge) {
                        std::cout << "kill hedge!" << std::endl;
                        delete orders_[fill.order.order_id];
                    }
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
                    //std::cout << "SPREAD:---------------------" << std::endl;
                    for(auto& spread : spreads_) {
                        if(spread.bid && spread.ask) {
                        /*std::cout << Asset::to_str(spread.asset)
                            << ": "
                            << spread.bid->price - spread.bid->spread/2.0
                            << " - "
                            << lastMidPrice_[spread.asset]
                            << " - "
                            << spread.ask->price + spread.bid->spread/2.0
                            << std::endl;*/
                        }

                        populate_spreads(spread, true);
                        populate_spreads(spread, false);
                    }
                    //std::cout << "SPREAD:---------------------" << std::endl;

                    /*
                    // manage position sizes (if we get over 50 units, sell 25
                    for(int i = Asset::IDXPHX + 1; i < Asset::Size; ++i) {
                        if(portfolio_[i] >= 50 || portfolio_[i] <= -50) {
                            Order* rebalance    = new Order;
                            rebalance->asset    = static_cast<Asset::Type>(i);
                            rebalance->strategy = Order::Strategy::Hedge;
                            rebalance->size     = 10;
                            rebalance->type     = Order::OrderType::Market;
                            rebalance->price    = 0;
                            rebalance->bid      = portfolio_[i] < 0;
                            rebalance->failed   = false;
                            rebalance->self     = rebalance;
                            portfolio_[i]      += (rebalance->bid ? rebalance->size : -1 * rebalance->size);
                            std::lock_guard<std::mutex> lk(pendingOrdersMut_);
                            pendingOrders_.push_back(rebalance);
                        }
                    }
                    */
                    double delta_portfolio = 0;
                    double vega_portfolio = 0;

                    for(int i = Asset::IDXPHX + 1; i < Asset::Size; ++i) {
                        double annualized_vol = SQRT_252 * implied_volatility(
                            lastMidPrice_[i],
                            lastMidPrice_[static_cast<int>(Asset::IDXPHX)],
                            Asset::get_strike(static_cast<Asset::Type>(i)),
                            0.16666666,
                            0,
                            Asset::get_bs_type(static_cast<Asset::Type>(i))
                        );

                        vega_portfolio += portfolio_[i] * vega(
                            Asset::get_bs_type(static_cast<Asset::Type>(i)),
                            lastMidPrice_[Asset::IDXPHX],
                            Asset::get_strike(static_cast<Asset::Type>(i)),
                            0.1666666,
                            0,
                            annualized_vol
                        );
                    }


                    double vega_hedged_portfolio[Asset::Size] = {0};
                    if(greeks++ % 6 == 0 && abs(vega_portfolio) > 300) {
                        int total_exposure = 0;
                        for(int i = 1; i < Asset::Size; ++i) {
                            if((vega_portfolio > 0 && portfolio_[i] > 0)
                                || (vega_portfolio < 0 && portfolio_[i] < 0)) {
                                total_exposure += portfolio_[i];
                            }
                        }
                        for(int i = 1; i < Asset::Size; ++i) {
                            if((vega_portfolio > 0 && portfolio_[i] < 0)
                                || (vega_portfolio < 0 && portfolio_[i] > 0)) {
                                continue;
                            }
                            double vega_of_hedge = vega(
                                Asset::get_bs_type(static_cast<Asset::Type>(i)),
                                lastMidPrice_[Asset::IDXPHX],
                                Asset::get_strike(static_cast<Asset::Type>(i)),
                                0.166666666,
                                0,
                                SQRT_252 * implied_volatility(
                                    lastMidPrice_[static_cast<Asset::Type>(i)],
                                    lastMidPrice_[static_cast<int>(Asset::IDXPHX)],
                                    Asset::get_strike(static_cast<Asset::Type>(i)),
                                    0.16666666,
                                    0,
                                    Asset::get_bs_type(static_cast<Asset::Type>(i))
                                )
                            );
                            auto vega_hedge_order = new Order();
                            vega_hedge_order->asset = static_cast<Asset::Type>(i);
                            vega_hedge_order->strategy = Order::Strategy::Hedge;
                            vega_hedge_order->bid = vega_portfolio < 0;
                            double rat = static_cast<double>(portfolio_[i]) / total_exposure;
                            vega_hedge_order->size = abs(round(rat * (vega_portfolio / vega_of_hedge)));
                            if(vega_hedge_order->size == 0) {
                                continue;
                            }
                            vega_hedged_portfolio[i] = vega_hedge_order->size * (vega_hedge_order->bid ? 1 : -1);
                            vega_hedge_order->type = Order::OrderType::Market;
                            vega_hedge_order->price = 0;
                            vega_hedge_order->failed = 0;
                            vega_hedge_order->self = vega_hedge_order;
                            /*std::cout << "VEGA HEDGE: "
                                      << vega_hedge_order->size
                                      << " OF "
                                      << Asset::to_str(static_cast<Asset::Type>(i))
                                      << std::endl;*/
                            {
                                std::lock_guard<std::mutex> lk(pendingOrdersMut_);
                                pendingOrders_.push_back(vega_hedge_order);
                            }
                        }
                    }
                    std::cout << "PHX#IDX: " << portfolio_[0] << std::endl;
                    for(int i = Asset::IDXPHX + 1; i < Asset::Size; ++i) {
                        std::lock_guard<std::mutex> lk(lastMidPriceMut_);
                        double annualized_vol = SQRT_252 * implied_volatility(
                            lastMidPrice_[i],
                            lastMidPrice_[static_cast<int>(Asset::IDXPHX)],
                            Asset::get_strike(static_cast<Asset::Type>(i)),
                            0.16666666,
                            0,
                            Asset::get_bs_type(static_cast<Asset::Type>(i))
                        );
                        delta_portfolio += (portfolio_[i] + vega_hedged_portfolio[i]) * delta(
                            Asset::get_bs_type(static_cast<Asset::Type>(i)),
                            lastMidPrice_[Asset::IDXPHX],
                            Asset::get_strike(static_cast<Asset::Type>(i)),
                            0.1666666,
                            0,
                            annualized_vol
                        );
                        std::cout << Asset::to_str(static_cast<Asset::Type>(i))
                                  << " "
                                  << portfolio_[i]
                                  << std::endl;
                    }
                    delta_portfolio += portfolio_[Asset::IDXPHX];
                    delta_portfolio += pendingHedge_;

                    std::cout << "delta: " << delta_portfolio << std::endl;
                    std::cout << "vega: " << vega_portfolio/100.0 << std::endl;

                    // hedge delta
                    if(delta_portfolio > delta_limit_ || delta_portfolio < (-1 * delta_limit_)) {
                        Order* hedge    = new Order();
                        hedge->asset    = Asset::IDXPHX;
                        hedge->strategy = Order::Strategy::Hedge;
                        hedge->size     = abs(static_cast<int>(delta_portfolio));
                        hedge->type     = Order::OrderType::Market;
                        hedge->price    = 1;
                        hedge->bid      = delta_portfolio < 0;
                        hedge->failed   = false;
                        hedge->self     = hedge;
                        std::lock_guard<std::mutex> lk(pendingOrdersMut_);
                        std::lock_guard<std::mutex> lc(pendingHedgeMut_);
                        pendingOrders_.push_back(hedge);
                        pendingHedge_ += hedge->size * (hedge->bid ? 1 : -1);
                    }
                    newMarket_.store(false);
                }
            }

        private:
            int straddle_size_;
            int position_max_;
            int spread_liquidity_;
            double reprice_thresh_;
            double delta_limit_;

            int greeks;

            std::atomic<bool> newMarket_;

            // hash table of last recorded market price of an asset
            std::mutex lastMidPriceMut_;
            double lastMidPrice_[Asset::Type::Size];

            std::mutex marketSpreadMut_;
            double marketSpread_[Asset::Type::Size];

            int portfolio_[Asset::Type::Size];

            std::mutex pendingHedgeMut_;
            int pendingHedge_;

            std::mutex ordersMut_;
            std::unordered_map<std::string, Order*> orders_;

            std::mutex fillsMut_;
            std::vector<Fill> fills_;

            // orders generated in processing thread which we are waiting to place
            std::mutex pendingOrdersMut_;
            std::vector<Order*> pendingOrders_;

            // orders to be modified
            std::mutex modifyOrdersMut_;
            std::vector<Order*> modifyOrders_;

            // orders to be cancelled
            std::mutex cancelOrdersMut_;
            std::vector<std::string> cancelOrders_;

            // what spreads are we trading?
            std::vector<Spread> spreads_;

            // processing thread
            std::unique_ptr<std::thread> thread_;
    };
}
#endif
