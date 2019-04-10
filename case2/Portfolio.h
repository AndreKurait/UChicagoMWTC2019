#ifndef __PORTFOLIO_H_
#define __PORTFOLIO_H_

#include <numeric>
#include <deque>

#include "black_scholes.h"

#define SQRT_252 15.8745078664

namespace kvt {
    template<int Size>
    class Portfolio {
    public:
        Portfolio()
            : assets_{}
            , pending_{}
            , bs_price_{}
            , market_{}
            , strike_{}
            , call_{}
            , time_{0}
            , implied_vol{0.25}
            , asset_delta_{}
            , asset_vega_{}
            , vega_{0}
            , delta_{0}
        {
            asset_delta_[0] = 1;
            asset_vega_[0]  = 0;
        }

        void set_strike(int asset, int strike) {
            assert(0 < asset);
            assert(asset < Size);
            strike_[asset] = strike;
        }

        void set_call(int asset, char call) {
            assert(0 < asset);
            assert(asset < Size);
            call_[asset] = call;
        }

        void update_market(int asset, double price) {
            assert(0 <= asset);
            assert(asset < Size);
            market_[asset] = price;
        }

        void add_pending(int asset, int amt) {
            assert(0 <= asset);
            assert(asset < Size);
            pending_[asset] += amt;
            delta_ += amt * asset_delta_[asset];
            vega_ += amt * asset_vega_[asset];
        }

        int& operator[](int asset) {
            assert(0 <= asset);
            assert(asset < Size);
            return assets_[asset];
        }

        void set_time(int time) {
            time_ = time;
        }

        double option_price(int asset) const {
            return bs_price_[asset];
        }

        void compute_greeks() {
            vega_ = 0;
            delta_ = assets_[0] + pending_[0];

            for(int i = 1; i < Size; ++i) {
                implied_vol = bs::implied_volatility(
                    market_[i],
                    market_[0],
                    strike_[i],
                    ((1-time_/900.0) * 0.25) + (time_/900.0)*0.16666667,
                    0,
                    call_[i]
                );
                /*
                vol_hist_[i].push_back(implied_vol);
                if(vol_hist_[i].size() >= 40) {
                    vol_hist_[i].pop_front();
                }

                double vol = vol_hist_[i].empty() ? 0.245 :
                    std::accumulate(std::begin(vol_hist_[i]), std::end(vol_hist_[i]), 0.0, std::plus<double>()) / vol_hist_[i].size();
*/
                bs_price_[i] = bs::black_scholes(
                    call_[i],
                    market_[0],
                    strike_[i],
                    ((1-time_/900.0) * 0.25) + (time_/900.0)*0.16666667,
                    0,
                    0.25 // implied_vo
                );

                if(abs(0.25 - implied_vol) > 0.005) {
                    std::cout << "vol dif!!!!!!" << std::endl << std::endl;
                }

                asset_delta_[i] = bs::delta(
                    call_[i],
                    market_[0],
                    strike_[i],
                    ((1-time_/900.0) * 0.25) + (time_/900.0)*0.16666667,
                    0,
                    implied_vol
                );
                asset_vega_[i] = bs::vega(
                    call_[i],
                    market_[0],
                    strike_[i],
                    ((1-time_/900.0) * 0.25) + (time_/900.0)*0.16666667,
                    0,
                    implied_vol
                ) / 100.0;

                delta_ += (pending_[i] + assets_[i]) * asset_delta_[i];
                vega_ += (pending_[i] + assets_[i]) * asset_vega_[i];
            }
        }

        double get_asset_vega(int asset) const { return asset_vega_[asset]; }

        double get_vega() const { return vega_; }
        double get_delta() const { return delta_; }

        double get_delta_exact() const {
            double exact_delta = 0;
            for(int i = 0; i < Size; ++i) {
                exact_delta += assets_[i] * asset_delta_[i];
            }
            return exact_delta;
        }

        double get_vega_exact() const {
            double exact_vega = 0;
            for(int i = 0; i < Size; ++i) {
                exact_vega += assets_[i] * asset_vega_[i];
            }
            return exact_vega;
        }

    private:
        /*
         * index 0 is always the underlying
         * index >0 is an option
         */
        int assets_[Size];
        int pending_[Size];
        double bs_price_[Size];
        double market_[Size];
        int strike_[Size];
        char call_[Size];

        std::deque<double> vol_hist_[Size];

        int time_;

        double implied_vol;
        double asset_delta_[Size];
        double asset_vega_[Size];

        double vega_;
        double delta_;
    };
}

#endif
