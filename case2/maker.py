import argparse
import random

import kvt

from client.exchange_service.client import BaseExchangeServerClient
from protos.order_book_pb2 import Order
from protos.service_pb2 import PlaceOrderResponse

from collections import deque

class ExampleMarketMaker(BaseExchangeServerClient):
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

    def convert_competitor(self, comp):
        comp_kvt = kvt.Competitor()
        comp_kvt.id = comp.competitor_id
        return comp_kvt

    def convert_order(self, order):
        order_kvt = kvt.Order()
        order_kvt.asset = order.asset_code
        order_kvt.qty = order.quantity
        order_kvt.type = kvt.OrderType.Market if order.order_type == Order.ORDER_MKT else kvt.OrderType.Limit
        order_kvt.price = order.price
        order_kvt.comp = self.convert_competitor(order.competitor_identifier)
        order_kvt.order_id = order.order_id
        return order_kvt

    def convert_fill(self, fill):
        fill_kvt = kvt.Fill()
        fill_kvt.order_id = fill.order.order_id;
        fill_kvt.comp = self.convert_competitor(fill.trader)
        fill_kvt.filled = fill.filled_quantity
        fill_kvt.fill_price = fill.fill_price
        return fill_kvt

    def handle_exchange_update(self, exchange_update_response):
        print("----------------")
        print("PNL: ", exchange_update_response.competitor_metadata.pnl)
        print("FINES: ", exchange_update_response.competitor_metadata.fines)
        print("COMMISSIONS: ", exchange_update_response.competitor_metadata.commissions)
        print("----------------")

        # send orders
        orders_to_send = self.kernel.get_and_clear_orders()
        for order in orders_to_send:
            order_obj = self._make_order(kvt.asset_to_string(order.asset), order.size, order.price,
                                         order.spread, order.bid)
            order_id = self.place_order(order_obj)
            if type(order_id) != PlaceOrderResponse:
                print(order_id)
            else:
                #print("ORDER PLACED: ")
                self.kernel.place_order(order, order_id.order_id)

        for fill in exchange_update_response.fills:
            fill_kvt = self.convert_fill(fill)
            self.kernel.handle_fill(fill_kvt)

        # hand-over market data to processing kernel
        update = kvt.Update()
        for market_update in exchange_update_response.market_updates:
            market_up = kvt.MarketUpdate()
            market_up.asset = kvt.asset_parse(market_update.asset.asset_code)
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
