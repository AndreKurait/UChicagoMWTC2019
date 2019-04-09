import argparse
import random

from client.exchange_service.client import BaseExchangeServerClient
from protos.order_book_pb2 import Order
from protos.service_pb2 import PlaceOrderResponse
import pickle
import time
import numpy as np
import math

class Case1(BaseExchangeServerClient):
    """A simple market making bot - shows the basics of subscribing
    to market updates and sending orders"""
    
    def __init__(self, *args, **kwargs):
        BaseExchangeServerClient.__init__(self, *args, **kwargs)
        self.open_orderids = set([])
        self._count = 0
        self.k_array = []
        self.m_array = []
        self.n_array = []
        self.q_array = []
        self.u_array = []
        self.v_array = []

        self._symbol_dict = {'K': 0,
                             'M': 1,
                             'N': 2,
                             'Q': 3,
                             'U': 4,
                             'V': 5}
        self.order_id_dict = {}
        for x in ['K', 'M', 'N', 'Q', 'U', 'V']:
            self.order_id_dict[x + '_short'] = 0
            self.order_id_dict[x + '_long'] = 0

        self._weights = []
        for x in range(0,50):
            self._weights.append(math.e**-x)

    def _make_order(self, asset_code, quantity, base_price, spread, bid=True):
        base_price = float(format(base_price, '.2f'))
        return Order(asset_code = asset_code, quantity=quantity,
                     order_type = Order.ORDER_LMT,
                     price = base_price,
                     competitor_identifier = self._comp_id)

    def order_update_handle(self, order):
        if type(order) != PlaceOrderResponse:
            pass
        else:
            return order.order_id
    
    def order_creation(self, mean_list_a, mean_list_b, symbol, price_array):
        val = self._symbol_dict[symbol]
        s_short = symbol + '_short'
        s_long = symbol + '_long'
        if mean_list_a[val] + self._wiggle < price_array[-1][0]:
            k = self._make_order(symbol, -10, mean_list_a[val] + self._wiggle / 2, .001)
            if self.order_id_dict[s_short] == 0:
                order = self.place_order(k)
                self.order_id_dict[s_short] = self.order_update_handle(order)
            else:
                order = self.modify_order(self.order_id_dict[s_short], k)
                self.order_id_dict[s_short] = self.order_update_handle(order)
        else:
            if self.order_id_dict[s_short] != 0:
                self.cancel_order(self.order_id_dict[s_short])
                self.order_id_dict[s_short] = 0
        
        if mean_list_b[val] - self._wiggle > price_array[-1][1]:
            k = self._make_order(symbol, 10, mean_list_b[val] - self._wiggle / 2, .001)
            if self.order_id_dict[s_long] == 0:
                order = self.place_order(k)
                self.order_id_dict[s_long] = self.order_update_handle(order)
            else:
                order = self.modify_order(self.order_id_dict[s_long], k)
                self.order_id_dict[s_long] = self.order_update_handle(order)
        else:
            if self.order_id_dict[s_long] != 0:
                self.cancel_order(self.order_id_dict[s_long])
                self.order_id_dict[s_long] = 0

    def handle_exchange_update(self, exchange_update_response):
        #Data Handle
        
        print(exchange_update_response.competitor_metadata)
        market = []
        for z in exchange_update_response.market_updates:
            code = z.asset.asset_code
            bids = z.bids
            asks = z.asks
            mid_market = z.mid_market_price
            market.append(mid_market)
            bids_price = []
            asks_price = []
            #generators dont seem to work. must be the protos datatype
            for x in bids:
                bids_price.append(x.price)
            #cant iterate over the bids array. throws an error
            bid = np.log(max(bids_price))
            for y in asks:
                asks_price.append(y.price)
            ask = np.log(min(asks_price))
            
            if code == 'K':
                self.k_array.append(np.asarray([bid,ask]))
            elif code == 'M':
                self.m_array.append(np.asarray([bid,ask]))
            elif code == 'N':
                self.n_array.append(np.asarray([bid,ask])) 
            elif code == 'Q':
                self.q_array.append(np.asarray([bid,ask]))
            elif code == 'U':
                self.u_array.append(np.asarray([bid,ask]))
            elif code == 'V':
                self.v_array.append(np.asarray([bid,ask]))
            else:
                pass
        
        print(self._count)
        if self._count > 51:
            self._wiggle = .001
        
            #This is ensure that we have a decent sample size
            K_mean_b = np.average(np.stack(self.k_array)[-51:-1][:,0], weights = np.array(self._weights), axis = 0)
            M_mean_b = np.average(np.stack(self.m_array)[-51:-1][:,0], weights = np.array(self._weights), axis = 0) 
            N_mean_b = np.average(np.stack(self.n_array)[-51:-1][:,0], weights = np.array(self._weights), axis = 0)
            Q_mean_b = np.average(np.stack(self.q_array)[-51:-1][:,0], weights = np.array(self._weights), axis = 0)
            U_mean_b = np.average(np.stack(self.u_array)[-51:-1][:,0], weights = np.array(self._weights), axis = 0)
            V_mean_b = np.average(np.stack(self.v_array)[-51:-1][:,0], weights = np.array(self._weights), axis = 0)
            
            K_mean_a = np.average(np.stack(self.k_array)[-51:-1][:,1], weights = np.array(self._weights), axis = 0)
            M_mean_a = np.average(np.stack(self.m_array)[-51:-1][:,1], weights = np.array(self._weights), axis = 0)
            N_mean_a = np.average(np.stack(self.n_array)[-51:-1][:,1], weights = np.array(self._weights), axis = 0)
            Q_mean_a = np.average(np.stack(self.q_array)[-51:-1][:,1], weights = np.array(self._weights), axis = 0)
            U_mean_a = np.average(np.stack(self.u_array)[-51:-1][:,1], weights = np.array(self._weights), axis = 0)
            V_mean_a = np.average(np.stack(self.v_array)[-51:-1][:,1], weights = np.array(self._weights), axis = 0)

            mean_k_a = np.polyfit([1,2,3,4,5,6], [K_mean_a, M_mean_a, N_mean_a, Q_mean_a, U_mean_a, V_mean_a] , deg = 5)
            mean_k_b = np.polyfit([1,2,3,4,5,6], [K_mean_b, M_mean_b, N_mean_b, Q_mean_b, U_mean_b, V_mean_b] , deg = 5)
            
            #_make_order(self, asset_code, quantity, base_price, spread, bid=True):
            self.order_creation(mean_list_a = mean_k_a, mean_list_b = mean_k_b, symbol = 'K', price_array = self.k_array )
            self.order_creation(mean_list_a = mean_k_a, mean_list_b = mean_k_b, symbol = 'M', price_array = self.m_array )
            self.order_creation(mean_list_a = mean_k_a, mean_list_b = mean_k_b, symbol = 'N', price_array = self.n_array )
            self.order_creation(mean_list_a = mean_k_a, mean_list_b = mean_k_b, symbol = 'Q', price_array = self.q_array )
            self.order_creation(mean_list_a = mean_k_a, mean_list_b = mean_k_b, symbol = 'U', price_array = self.u_array )
            self.order_creation(mean_list_a = mean_k_a, mean_list_b = mean_k_b, symbol = 'V', price_array = self.v_array )
        
        self._count+=1
        print(self.order_id_dict)
    
        

    


            #Good code that I may need later
            # for x in bids:
            #     bids_price.append(x.price)
            #     bids_size.append(x.size)
            # for y in asks:
            #     asks_price.append(y.price)
            #     asks_size.append(y.size)
            # bids_array = np.asarray([(np.asarray(bids_price)).reshape(1,-1), (np.asarray(bids_size).reshape(1,-1))])
            # asks_array = np.asarray([(np.asarray(asks_price)).reshape(1,-1), (np.asarray(asks_size).reshape(1,-1))])
            # self._barray.append(bids_array)
            # self._aarray.append(asks_array) 
            # print(self._barray)
            # time.sleep(5)
            # print('nice')
        #print(exchange_update_response.market_updates)
        # self._count+=1
        # if self._count == 20:
        #     with open('scarped.pkl', 'wb') as handle:
        #         pickle.dump(self._data_scrape, handle, protocol=pickle.HIGHEST_PROTOCOL)
        #print(exchange_update_response)

        
        # # 10% of the time, cancel two arbitrary orders
        # if random.random() < 0.10 and len(self._orderids) > 0:
        #     self.cancel_order(self._orderids.pop())
        #     self.cancel_order(self._orderids.pop())

        # only trade 5% of the time
        # if random.random() > 0.05:
        #     return

        # # place a bid and an ask for each asset
        # for i, asset_code in enumerate(["K", "M", "N"]):
        #     quantity = random.randrange(1, 10)
        #     base_price = random.randrange((i + 1) * 100, (i+1) * 150)
        #     spread = random.randrange(5, 10)

        #     bid_resp = self.place_order(self._make_order(asset_code, quantity,
        #         base_price, spread, True))
        #     ask_resp = self.place_order(self._make_order(asset_code, quantity,
        #         base_price, spread, False))

        #     if type(bid_resp) != PlaceOrderResponse:
        #         print(bid_resp)
        #     else:
        #         self._orderids.add(bid_resp.order_id)

        #     if type(ask_resp) != PlaceOrderResponse:
        #         print(ask_resp)
        #     else:
        #         self._orderids.add(ask_resp.order_id)

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

    client = Case1(host, port, client_id, client_pk, websocket_port)
    client.start_updates()
