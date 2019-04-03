#ifndef __MARKET_MAKER_H_
#define __MARKET_MAKER_H_

#include <iostream>
#include <thread>
#include <atomic>
#include <memory>

#include "protos/service.pb.h"
#include "protos/exchange.pb.h"

namespace kvt {
    class MarketMaker {
        public:
            MarketMaker()
                : val_{0}
            {
                thread_.reset(new std::thread(&MarketMaker::body, this));
            }

            int handle_update(void) {
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
