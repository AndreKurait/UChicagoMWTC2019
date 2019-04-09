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

#include <fstream>

#include "Portfolio.h"

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
                case Size:    return "Size";
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

                case IDXPHX:
                case Size:
                    assert(false);
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
        union {
            Spread* spread;
            int hedgeId;
        } loc;
        bool pending;

        Asset::Type asset;
        int size;
        int remaining;
        OrderType type;
        double price;
        double spread;
        bool bid;
        Competitor comp;
        std::string order_id;
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
            , pendingBid(false)
            , bid(nullptr)
            , pendingAsk(false)
            , ask(nullptr)
        {}

        Asset::Type asset;
        double spread;
        int size;

        bool pendingBid;
        std::unique_ptr<Order> bid;

        bool pendingAsk;
        std::unique_ptr<Order> ask;
    };

    class MarketMaker {
        public:
            MarketMaker(int straddle_size, int position_max, int spread_liquidity,
                    double reprice_thresh, double delta_limit)
                : straddle_size_{straddle_size}
                , position_max_{position_max}
                , spread_liquidity_{spread_liquidity}
                , reprice_thresh_{reprice_thresh}
                , delta_limit_{delta_limit}
                , greeks{0}
                , lastHedgeId{0}
            {
                portfolio_[0] = 0;
                lastMidPrice_[0] = 0;
                marketSpread_[0] = 0;
                for(int asset = Asset::Type::IDXPHX + 1; asset < Asset::Type::Size; ++asset) {
                    spreads_.emplace_back(static_cast<Asset::Type>(asset), 0.6, straddle_size);
                    portfolio_[asset] = 0;
                    lastMidPrice_[asset] = 0;
                    marketSpread_[asset] = 0;

                    port_.set_strike(asset, Asset::get_strike(static_cast<Asset::Type>(asset)));
                    port_.set_call(asset, Asset::get_bs_type(static_cast<Asset::Type>(asset)));
                }

                thread_.reset(new std::thread(&MarketMaker::process_orders, this));
                file.open("prices.txt");
            }


            int time = 0;
            std::ofstream file;
            void handle_update(Update const& update) {
                { // mutex scope
                    std::lock_guard<std::mutex> lk(lastMidPriceMut_);
                    std::lock_guard<std::mutex> lc(marketSpreadMut_);
                    std::lock_guard<std::mutex> lr(portfolioMut_);
                    for(auto const& mu : update.market_updates) {
                        // This normalization of the double is to avoid order rejections
                        // due to minimum tick size
                        port_.update_market(mu.asset, mu.mid_market_price);
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
                if(Order::Strategy::MarketMaking == order.strategy) {
                    if(order.bid) {
                        orders_[order_id] = order.loc.spread->bid.get();
                        order.loc.spread->pendingBid = false;
                    } else {
                        orders_[order_id] = order.loc.spread->ask.get();
                        order.loc.spread->pendingAsk = false;
                    }
                } else {
                    orders_[order_id] = hedgeOrders_[order.loc.hedgeId].get();
                }
            }

            void modify_order(Order const& order, std::string order_id) {
                //std::lock_guard<std::mutex> lk(ordersMut_);
                //orders_[order.self->order_id] = nullptr;
                //order.self->order_id = order_id;
                //orders_[order_id] = order.self;
            }

            void order_failed(Order const& order) {
                std::lock_guard<std::mutex> lk(ordersMut_);
                if(Order::Strategy::MarketMaking == order.strategy) {
                    if(order.bid) {
                        orders_[order.order_id] = nullptr;
                        order.loc.spread->bid.reset(nullptr);
                    } else {
                        orders_[order.order_id] = nullptr;
                        order.loc.spread->ask.reset(nullptr);
                    }
                } else {
                    orders_[order.order_id] = nullptr;
                    hedgeOrders_.erase(order.loc.hedgeId);
                }
            }

            std::vector<Order*> get_and_clear_orders() {
                std::lock_guard<std::mutex> lk(pendingOrdersMut_);
                std::vector<Order*> orders;
                orders.swap(pendingOrders_);
                return orders;
            }

            std::vector<Order*> get_and_clear_modifies() {
                std::lock_guard<std::mutex> lk(modifyOrdersMut_);
                std::vector<Order*> orders;
                orders.swap(modifyOrders_);
                return orders;
            }

            void populate_spreads(Spread& spread, bool bid) {
                auto& existing_order = (bid) ? spread.bid : spread.ask;

                if(!existing_order || existing_order->remaining <= 0) {
                    if((bid && port_[spread.asset] >= 100) || (!bid && port_[spread.asset] <= -100)) {
                        return;
                    }

                    if((portfolio_[spread.asset] > position_max_ && bid)
                            || (portfolio_[spread.asset] < (-1 * position_max_) && !bid)) {
                        //existing_order.reset(nullptr);
                        //return;
                    }
                    lastMidPriceMut_.lock();
                    auto last_price = lastMidPrice_[spread.asset];
                    lastMidPriceMut_.unlock();
                    if(last_price > 0.01) {
                        if(existing_order) {
                            std::lock_guard<std::mutex> ll(ordersMut_);
                            orders_[existing_order->order_id] = nullptr;
                        }
                        existing_order.reset(new Order());
                        existing_order->asset       = spread.asset;
                        existing_order->strategy    = Order::Strategy::MarketMaking;
                        existing_order->loc.spread  = &spread;
                        existing_order->price       = last_price;
                        existing_order->spread      = spread.spread;
                        existing_order->size        = spread.size;
                        existing_order->remaining   = spread.size;
                        existing_order->type        = Order::OrderType::Limit;
                        existing_order->bid         = bid;
                        std::lock_guard<std::mutex> lc(pendingOrdersMut_);
                        pendingOrders_.push_back(existing_order.get());
                    }
                } else {
                    bool modify = false;
                    std::lock_guard<std::mutex> lk(marketSpreadMut_);
                    std::lock_guard<std::mutex> lc(lastMidPriceMut_);

                    // Tighten or loosen our spread
                    if((marketSpread_[spread.asset] + 0.02 <= spread.spread)
                            || (marketSpread_[spread.asset] >= spread.spread + 0.02)) {
                        modify                    = true;
                        spread.spread             = marketSpread_[spread.asset];
                        existing_order->spread    = spread.spread;
                        existing_order->size      = existing_order->remaining;
                        existing_order->remaining = 0;
                        existing_order->price     = lastMidPrice_[spread.asset];
                    } else if(bid && (
                                (lastMidPrice_[spread.asset] - existing_order->price > (spread.spread/2.0 - reprice_thresh_))
                            || (lastMidPrice_[spread.asset] - existing_order->price < (spread.spread/2.0 + reprice_thresh_)))) {
                        modify                    = true;
                        existing_order->price     = lastMidPrice_[spread.asset];
                        existing_order->size      = existing_order->remaining;
                        existing_order->remaining = 0;
                    } else if(!bid && (
                               (existing_order->price - lastMidPrice_[spread.asset] > (spread.spread/2.0 - reprice_thresh_))
                            || (existing_order->price - lastMidPrice_[spread.asset] < (spread.spread/2.0 + reprice_thresh_)))) {
                        modify                    = true;
                        existing_order->price     = lastMidPrice_[spread.asset];
                        existing_order->size      = existing_order->remaining;
                        existing_order->remaining = 0;
                    }

                    if(false && modify) {
                        std::lock_guard<std::mutex> ll(modifyOrdersMut_);
                        modifyOrders_.push_back(existing_order.get());
                    }
                }
            }

            void process_fills() {
                std::lock_guard<std::mutex> lk(fillsMut_);
                std::lock_guard<std::mutex> lc(ordersMut_);
                std::lock_guard<std::mutex> lr(portfolioMut_);
                for(auto const& fill : fills_) {
                    if(fill.order.bid) {
                        portfolio_[fill.order.asset] += fill.filled;
                        port_[fill.order.asset] += fill.filled;
                        if(fill.order.strategy == Order::Strategy::Hedge) {
                            port_.add_pending(fill.order.asset, -1 * fill.filled);
                        }
                    } else {
                        portfolio_[fill.order.asset] -= fill.filled;
                        port_[fill.order.asset] -= fill.filled;
                        if(fill.order.strategy == Order::Strategy::Hedge) {
                            port_.add_pending(fill.order.asset, fill.filled);
                        }
                    }
                    if(orders_[fill.order.order_id] == nullptr) {
                        continue;
                    }
                    orders_[fill.order.order_id]->remaining -= fill.filled;
                    if(orders_[fill.order.order_id]->remaining <= 0
                            && orders_[fill.order.order_id]->strategy == Order::Strategy::Hedge) {
                        //std::cout << "kill hedge!" << std::endl;
                        //delete orders_[fill.order.order_id];
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

                    for(auto& spread : spreads_) {
                        populate_spreads(spread, true);
                        populate_spreads(spread, false);
                    }

                    std::lock_guard<std::mutex> lr(portfolioMut_);
                    for(int i = 0; i < Asset::Size; ++i) {
                        //std::cout << i << " " << port_[i] << std::endl;
                    }
                    port_.compute_greeks();
                    //std::cout << "vega: " << port_.get_vega() << std::endl;
                    //std::cout << "delta: " << port_.get_delta() << std::endl;

                    if(port_.get_vega() > 0.5 || port_.get_vega() < -0.5) {
                    int total_exposure = 0;
                    for(int i = 1; i < Asset::Size; ++i) {
                        if((port_.get_vega() > 0 && port_[i] > 0)
                            || (port_.get_vega() < 0 && port_[i] < 0)) {
                            total_exposure += port_[i];
                        }
                    }
                    for(int i = 1; i < Asset::Size; ++i) {
                        if((port_.get_vega() > 0 && port_[i] < 0)
                            || (port_.get_vega() < 0 && port_[i] > 0)) {
                            continue;
                        }
                        auto vega_hedge_order = std::make_unique<Order>();
                        vega_hedge_order->asset = static_cast<Asset::Type>(i);
                        vega_hedge_order->strategy = Order::Strategy::Hedge;
                        vega_hedge_order->bid = port_.get_vega() < 0;
                        double rat = static_cast<double>(port_[i]) / total_exposure;
                        vega_hedge_order->size = abs(round(rat * (port_.get_vega() / port_.get_asset_vega(i))));
                        /*
                        std::cout << "vega hedge: " << vega_hedge_order->size
                            << " | exposure: " << port_[i]
                            << " | total exposure: " << total_exposure
                            << " | instrument vega: " << port_.get_asset_vega(i)
                            << " | ratio: " << rat
                            << " | ration2: " << (port_.get_vega() / port_.get_asset_vega(i))
                            << std::endl;
                            */
                        if(vega_hedge_order->size <= 0 || vega_hedge_order->size > 500) {
                            continue;
                        }
                        vega_hedge_order->type = Order::OrderType::Market;
                        vega_hedge_order->price = 0;
                        vega_hedge_order->loc.hedgeId = ++lastHedgeId;
                        std::lock_guard<std::mutex> lk(pendingOrdersMut_);
                        pendingOrders_.push_back(vega_hedge_order.get());
                        port_.add_pending(i, vega_hedge_order->size * (vega_hedge_order->bid ? 1 : -1));
                        std::lock_guard<std::mutex> ll(hedgeOrdersMut_);
                        hedgeOrders_[vega_hedge_order->loc.hedgeId] = std::move(vega_hedge_order);
                    }
                    }
                    if(port_.get_delta() > delta_limit_ || port_.get_delta() < (-1 * delta_limit_)) {
                        if(abs(port_.get_delta()) <= 500) {
                            auto hedge    = std::make_unique<Order>();
                            hedge->asset    = Asset::IDXPHX;
                            hedge->strategy = Order::Strategy::Hedge;
                            hedge->size     = abs(static_cast<int>(port_.get_delta()));
                            hedge->type     = Order::OrderType::Market;
                            hedge->price    = 1;
                            hedge->bid      = port_.get_delta() < 0;
                            hedge->loc.hedgeId = ++lastHedgeId;
                            std::lock_guard<std::mutex> lc(pendingOrdersMut_);
                            pendingOrders_.push_back(hedge.get());
                            port_.add_pending(0, hedge->size * (hedge->bid ? 1 : -1));
                            std::lock_guard<std::mutex> ll(hedgeOrdersMut_);
                            hedgeOrders_[hedge->loc.hedgeId] = std::move(hedge);
                        }
                    }

                    //std::cout << "hedged vega: " << port_.get_vega() << std::endl;
                    //std::cout << "hedged delta: " << port_.get_delta() << std::endl;

                    /*
                    for(int i = Asset::IDXPHX + 1; i < Asset::Size; ++i) {
                        double annualized_vol = SQRT_252 * implied_volatility(
                            lastMidPrice_[i],
                            lastMidPrice_[static_cast<int>(Asset::IDXPHX)],
                            Asset::get_strike(static_cast<Asset::Type>(i)),
                            0.16666666,
                            0,
                            Asset::get_bs_type(static_cast<Asset::Type>(i))
                        );

                    }


                    double vega_hedged_portfolio[Asset::Size] = {0};
                    if(greeks++ % 4 == 0) {
                        if(vega_portfolio > 100) {
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
                            {
                                std::lock_guard<std::mutex> lk(pendingOrdersMut_);
                                pendingOrders_.push_back(*vega_hedge_order);
                            }
                        }
                        }

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
                    }
                    delta_portfolio += portfolio_[Asset::IDXPHX];

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
                        pendingOrders_.push_back(*hedge);
                    }
                    }
                */
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

            std::mutex portfolioMut_;
            Portfolio<Asset::Size> port_;

            std::atomic<bool> newMarket_;

            // hash table of last recorded market price of an asset
            std::mutex lastMidPriceMut_;
            double lastMidPrice_[Asset::Type::Size];

            std::mutex marketSpreadMut_;
            double marketSpread_[Asset::Type::Size];

            int portfolio_[Asset::Type::Size];

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

            // what spreads are we trading?
            std::vector<Spread> spreads_;

            std::mutex hedgeOrdersMut_;
            std::unordered_map<int, std::unique_ptr<Order>> hedgeOrders_;
            int lastHedgeId;

            // processing thread
            std::unique_ptr<std::thread> thread_;
    };
}
#endif
