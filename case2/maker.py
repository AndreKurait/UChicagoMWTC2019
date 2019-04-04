import argparse
import random

import kvt

from client.exchange_service.client import BaseExchangeServerClient
from protos.order_book_pb2 import Order
from protos.service_pb2 import PlaceOrderResponse

from collections import deque

class ExampleMarketMaker(BaseExchangeServerClient):
    """A simple market making bot - shows the basics of subscribing
    to market updates and sending orders"""

    def __init__(self, *args, **kwargs):
        BaseExchangeServerClient.__init__(self, *args, **kwargs)

        self.symbols = ["C98PHX", "C99PHX", "C100PHX", "C101PHX", "C102PHX",
                        "P98PHX", "P99PHX", "P100PHX", "P101PHX", "P102PHX",
                        "IDX#PHX"];

        self.kernel = kvt.MarketMaker()

        self.mid_market_price = {}
        self.orders    = {}
        self.portfolio = {}
        for sym in self.symbols:
            self.mid_market_price[sym] = 0
            self.orders[sym] = deque()
            self.portfolio[sym] = deque()

    def _make_order(self, asset_code, quantity, base_price, spread, bid=True):
        return Order(asset_code = asset_code, quantity=quantity if bid else -1*quantity,
                     order_type = Order.ORDER_LMT,
                     price = base_price-spread/2 if bid else base_price+spread/2,
                     competitor_identifier = self._comp_id)

    def handle_exchange_update(self, exchange_update_response):
        update = kvt.Update()
        for market_update in exchange_update_response.market_updates:
            market_up = kvt.MarketUpdate()
            market_up.asset.asset_code = market_update.asset.asset_code
            market_up.mid_market_price = market_update.mid_market_price
            for bid in market_update.bids:
                level = kvt.PriceLevel()
                level.price = bid.price
                level.size = bid.size
                market_up.bids.append(level)
            for ask in market_update.asks:
                level = kvt.PriceLevel()
                level.price = ask.price
                level.size = ask.size
                market_up.asks.append(level)
            update.market_updates.append(market_up)
        a = self.kernel.handle_update(update)
        print(a)


        return
        for market_update in exchange_update_response.market_updates:
            sym = market_update.asset.asset_code
            self.mid_market_price[sym] = market_update.mid_market_price
            if len(self.orders[sym]) == 0:
                order_bid = self._make_order(sym, 10, self.mid_market_price[sym], 0.02, true)
                order_ask = self._make_order(sym, 10, self.mid_market_price[sym], 0.02, false)
                order_bid_code = self.place_order(order_bid)
                order_ask_code = self.place_order(order_ask)
                if type(order_bid_code) != placeorderresponse:
                    print(order_bid_code)
                else:
                    self.orders[sym].append([order_bid_code, order_bid])

                if type(order_ask_code) != PlaceOrderResponse:
                    print(order_ask_code)
                else:
                    self.orders[sym].append([order_ask_code, order_ask])

        # make a market around the pegged price
        # if the price starts moving, cancel orders outside of the range
        # when we get a fill back, calculate our greeks and fix our hedge

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Run the exchange client')
    parser.add_argument("--server_host", type=str, default="localhost")
    parser.add_argument("--server_port", type=str, default="50052")
    parser.add_argument("--client_id", type=str)
    parser.add_argument("--client_private_key", type=str)
    parser.add_argument("--websocket_port", type=int, default=5678)

    args = parser.parse_args()
    host, port, client_id, client_pk, websocket_port = (args.server_host, args.server_port,
                                        args.client_id, args.client_private_key,
                                        args.websocket_port)

    client = ExampleMarketMaker(host, port, client_id, client_pk, websocket_port)
    client.start_updates()
