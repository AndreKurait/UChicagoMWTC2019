#ifndef __PORTFOLIO_H_
#define __PORTFOLIO_H_

#include "black_scholes.h"

#define SQRT_252 15.8745078664

namespace kvt {
    template<int Size>
    class Portfolio {
    public:
        Portfolio()
            : assets_{0}
            , pending_{0}
            , market_{0}
            , strike_{0}
            , call_{0}
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

        void compute_greeks() {
            vega_ = 0;
            delta_ = assets_[0] + pending_[0];
            for(int i = 1; i < Size; ++i) {
                double annualized_vol = SQRT_252 * bs::implied_volatility(
                    market_[i],
                    market_[0],
                    strike_[i],
                    0.25,
                    0,
                    call_[i]
                );
                asset_delta_[i] = bs::delta(
                    call_[i],
                    market_[0],
                    strike_[i],
                    0.25,
                    0,
                    annualized_vol
                );
                asset_vega_[i] = bs::vega(
                    call_[i],
                    market_[0],
                    strike_[i],
                    0.25,
                    0,
                    annualized_vol
                ) / 100.0;

                delta_ += (pending_[i] + assets_[i]) * asset_delta_[i];
                vega_ += (pending_[i] + assets_[i]) * asset_vega_[i];
            }
        }

        double get_asset_vega(int asset) const { return asset_vega_[asset]; }

        double get_vega() const { return vega_; }
        double get_delta() const { return delta_; }

    private:
        /*
         * index 0 is always the underlying
         * index >0 is an option
         */
        int assets_[Size];
        int pending_[Size];
        double market_[Size];
        int strike_[Size];
        char call_[Size];

        double asset_delta_[Size];
        double asset_vega_[Size];

        double vega_;
        double delta_;
    };
}

#endif
