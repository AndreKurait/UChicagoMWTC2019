import argparse
import random

import kvt

from client.exchange_service.client import BaseExchangeServerClient
from protos.order_book_pb2 import Order
from protos.service_pb2 import PlaceOrderResponse
from protos.service_pb2 import ModifyOrderResponse

from collections import deque
import time

class ExampleMarketMaker(BaseExchangeServerClient):
    def __init__(self, *args, **kwargs):
        BaseExchangeServerClient.__init__(self, *args, **kwargs)

        self.symbols = ["C98PHX", "C99PHX", "C100PHX", "C101PHX", "C102PHX",
                        "P98PHX", "P99PHX", "P100PHX", "P101PHX", "P102PHX",
                        "IDX#PHX"];

        self.kernel = kvt.MarketMaker(10,   # straddle size
                                      10,  # max position size
                                      5,   # liquidity depth
                                      1, # reprice threshold
                                      1.0) # delta max

        self.mid_market_price = {}
        self.orders    = {}
        self.portfolio = {}
        for sym in self.symbols:
            self.mid_market_price[sym] = 0
            self.orders[sym] = deque()
            self.portfolio[sym] = 0

    def _fix_price(self, p, bid):
        price = int(p * 100) / 100.0
        if not bid:
            price += 0.01
        return price

    def _make_lmt_order(self, asset_code, quantity, base_price, spread, bid=True):
        return Order(asset_code = asset_code, quantity=quantity if bid else -1*quantity,
                     order_type = Order.ORDER_LMT,
                     price = self._fix_price(base_price-spread/2 if bid else base_price+spread/2, bid),
                     competitor_identifier = self._comp_id)

    def _make_mkt_order(self, asset_code, quantity, bid=True):
        return Order(asset_code = asset_code, quantity=quantity if bid else -1*quantity,
                     order_type = Order.ORDER_MKT,
                     competitor_identifier = self._comp_id)

    def convert_competitor(self, comp):
        comp_kvt = kvt.Competitor()
        comp_kvt.id = comp.competitor_id
        return comp_kvt

    def convert_order(self, order):
        order_kvt = kvt.Order()
        order_kvt.asset = kvt.asset_parse(order.asset_code)
        order_kvt.size = abs(order.quantity)
        order_kvt.bid = True if order.quantity > 0 else False
        order_kvt.type = kvt.OrderType.Market if order.order_type == Order.ORDER_MKT else kvt.OrderType.Limit
        order_kvt.price = order.price
        order_kvt.comp = self.convert_competitor(order.competitor_identifier)
        order_kvt.order_id = order.order_id
        return order_kvt

    def convert_fill(self, fill):
        fill_kvt = kvt.Fill()
        fill_kvt.order = self.convert_order(fill.order);
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
        for fill in exchange_update_response.fills:
            fill_kvt = self.convert_fill(fill)
            self.kernel.handle_fill(fill_kvt)

        # hand-over market data to processing kernel
        update = kvt.Update()
        for market_update in exchange_update_response.market_updates:
            if not kvt.acceptable(market_update.asset.asset_code):
                continue
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

        #self.kernel.new_market()
        self.kernel.process_orders()

        # send orders
        orders_to_send = self.kernel.get_and_clear_orders()
        for order in orders_to_send:
            order_obj = {}
            if order.type == kvt.OrderType.Limit:
                order_obj = self._make_lmt_order(kvt.asset_to_string(order.asset), order.size, order.price,
                                         order.spread, order.bid)
            else:
                order_obj = self._make_mkt_order(kvt.asset_to_string(order.asset), order.size, order.bid)
            order_id = self.place_order(order_obj)
            #print("new order!")
            if type(order_id) != PlaceOrderResponse:
                print(order_id)
                print(order_obj)
                #quit()
                self.kernel.order_failed(order)
            else:
                self.kernel.place_order(order, order_id.order_id)

        # modify orders
        modifies_to_send = self.kernel.get_and_clear_modifies()
        for modify in modifies_to_send:
            #print("modify order!", modify.order_id)
            modify_obj = self._make_lmt_order(kvt.asset_to_string(modify.asset), modify.size, modify.price,
                                              modify.spread, modify.bid)
            '''
            self.cancel_order(modify.order_id)
            order_id = self.place_order(modify_obj)
            if type(order_id) != PlaceOrderResponse:
                print(order_id)
            else:
                self.kernel.modify_order(modify, order_id.order_id)
            '''
            modify_obj.order_id = modify.order_id
            modify_obj.remaining_quantity = modify.remaining
            order_id = self.modify_order(modify.order_id, modify_obj)
            if type(order_id) != ModifyOrderResponse:
                #print(order_id)
                #print(modify_obj)
                #print("modify failed!")
                quit()
                print("MODIFY BOUNCED")
                self.kernel.order_failed(modify)
            else:
                self.kernel.modify_order(modify, order_id.order_id)



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
