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
        self._mid_market = []
        self.k_array = []
        self.m_array = []
        self.n_array = []
        self.q_array = []
        self.u_array = []
        self.v_array = []
        #some arbitrary price calculation. probably will do something w/ the standard deviation that makes it better
        self._wiggle = .5
        
        #This is used to relate the symbol to the index of the arrays
        self._symbol_dict = {'K': 0,
                             'M': 1,
                             'N': 2,
                             'Q': 3,
                             'U': 4,
                             'V': 5}

        
        #order_id_dict holds the ids for the limit orders around the calcualted theo
        #reversion dict holds the ids for the orders that calculate the mean reversion
        #_price_dict is not implemented but will hold the prices if needed
        self.order_id_dict = {}
        self.reversion_dict = {}
        self.price_dict = {}
        self.market_data = {}
        for x in ['K', 'M', 'N', 'Q', 'U', 'V']:
            self.order_id_dict[x + '_short'] = [0, [0]]
            self.order_id_dict[x + '_long'] = [0, [0]]
            self.price_dict[x + '_short'] = [0, [0]]
            self.price_dict[x + '_long'] = [0, [0]]
            self.reversion_dict[x + '_short'] = [0, [0]]
            self.reversion_dict[x + '_long'] = [0, [0]]
            self.market_data[x] = []
        #This is used to calculated the exponential weights needed for the MA time series
        self._weights = []
        for x in range(0,29):
            self._weights.append(math.e**-x)
        
        #This is used to know what amount of shares we have
        self.owned_shares = {}
        for x in ['K', 'M', 'N', 'Q', 'U', 'V']:
            self.owned_shares[x] = 0


    def _make_order(self, asset_code, quantity, base_price, order_type = Order.ORDER_LMT, bid=True):
        base_price = float(format(base_price, '.2f'))
        return Order(asset_code = asset_code, quantity=quantity,
                     order_type = order_type,
                     price = base_price,
                     competitor_identifier = self._comp_id)

    def order_update_handle(self, order):
        #Don't want to hold failed orders
        if type(order) != PlaceOrderResponse:
            return 0
        else:
            return order.order_id
    
    def order_creation(self, mean_list, symbol):
        val = self._symbol_dict[symbol]
        s_short = symbol + '_short'
        s_long = symbol + '_long'


        #This creates a limit order for a short right above the calculated theo + wiggle
        #We want to place a new order if the old one was filled or modify it if it's not a good price anymore
        k = self._make_order(symbol, -1, mean_list[val] + self._wiggle)
        #If the first value is 0, this means there is not a current order for this spread
        if self.order_id_dict[s_short][0] == 0:
            order = self.place_order(k)
            self.order_id_dict[s_short][0] = self.order_update_handle(order)
            self.order_id_dict[s_short][1] = [mean_list[val] + self._wiggle / 2, mean_list[val] + self._wiggle * 1.5]
        print(self.order_id_dict[s_short][1])
        print(type(self.order_id_dict[s_short][1][1]))
        time.sleep(500)
        #If the value is not in the range specified by the second list in the order_id_dict, that means it's not longer good, and we want to modify it
        if mean_list[val] < self.order_id_dict[s_short][1][0] or mean_list[s_short] > self.order_id_dict[s_short][1][1]:
            order = self.modify_order(self.order_id_dict[s_short], k)
            self.order_id_dict[s_short][0] = self.order_update_handle(order)
            self.order_id_dict[s_short][1] = [mean_list[val] + self._wiggle / 2, mean_list[val] + self._wiggle * 1.5]
    
        #This creates a limit order for a long right below the calculated theo - wiggle
        #We want to place a new order if the old one was filled or modify it if it's not a good price anymore
        k = self._make_order(symbol, 1, mean_list[val] - self._wiggle)
        if self.order_id_dict[s_long][0] == 0:
            order = self.place_order(k)
            self.order_id_dict[s_long][0] = self.order_update_handle(order)
            self.order_id_dict[s_long][1] = [mean_list[val] - self._wiggle / 2, mean_list[val] - self._wiggle * 1.5]
        #If the value is not in the range specified by the second list in the order_id_dict, that means it's not longer good, and we want to modify it
        if mean_list[val] > self.order_id_dict[s_long][1][0] or mean_list[s_long] < self.order_id_dict[s_long][1][1]:
            order = self.modify_order(self.order_id_dict[s_long], k)
            self.order_id_dict[s_long] = self.order_update_handle(order)
        




    def hedging(self, mean_list):
        #the idea here is that if the mean reverts we sell what we have
        #may want to impliment order cancelling
        exposure = 0
        mid_exposure = 0
        for key, values in self.owned_shares.items():
            exposure += values
            #Generally these are hedges
            if key != 'K' or key != 'V':
                mid_exposure += values
        if abs(exposure) > 10 and abs(mid_exposure) > 10:
            quant =  mid_exposure / -2
            k = self._make_order(asset_code = 'K', quantity = quant, base_price = mean_list[0], order_type = Order.ORDER_MKT)
            order1 = self.place_order(k)
            v = self._make_order(asset_code = 'V', quantity = quant, base_price = mean_list[5], order_type = Order.ORDER_MKT)
            order = self.place_order(v)
            if self.order_update_handle(order1) != 0:
                self.owned_shares['K']+= quant
            if self.order_update_handle(order) != 0:
                self.owned_shares['V']+= quant

    def handle_exchange_update(self, exchange_update_response):
        #Data Handle
        if len(exchange_update_response.market_updates) == 0:
            return
        print(exchange_update_response.competitor_metadata)
        print(self._count)
        #Iterate over the fills that we've recieved to store them and our position size
        print(self.owned_shares)
        for q in exchange_update_response.fills:
            try:
                #need to fix that fills may come from the mean reversion and to fix that
                order_key = q.order.asset_code
                old_id = ''
                if q.order.quantity > 0:
                    order_string = order_key + '_long'
                elif q.order.quantity < 0:
                    order_string = order_key + '_short'
                #fix the iteration
                for values, ids in self.order_id_dict.items():
                    if ids[0] == q.order.order_id:
                        old_id = self.order_id_dict[order_string][0]
                        self.order_id_dict[order_string][0] = 0
                self.owned_shares[order_key] += q.order.quantity 
                #just in case
                self.cancel_order(old_id)
            except KeyboardInterrupt:
                break
            #except:
                 #print('Failure')
                 #probably want this in order to recover from ids not in the list somehow.   
                 #can add into an arbitrary quantity and symbol dict to figure out what to do

        for z in exchange_update_response.market_updates:
            code = z.asset.asset_code
            #bids = z.bids
            #asks = z.asks
            mid_market = z.mid_market_price            
            self.market_data[code].append(mid_market)
            #I guess we also want the spread

        if self._count > 30:
            #This is ensure that we have a decent sample size. We are using a exponential decay to mimic a moving average time series
            K_mean = np.average(self.market_data['K'][-30:-1], weights = np.array(self._weights))
            M_mean = np.average(self.market_data['M'][-30:-1], weights = np.array(self._weights))
            N_mean = np.average(self.market_data['N'][-30:-1], weights = np.array(self._weights))
            Q_mean = np.average(self.market_data['Q'][-30:-1], weights = np.array(self._weights))
            U_mean = np.average(self.market_data['U'][-30:-1], weights = np.array(self._weights))
            V_mean = np.average(self.market_data['V'][-30:-1], weights = np.array(self._weights))
            

            #we fit a quintic polynomail to hopefully 
            mean_k = np.polyfit([1,2,3,4,5,6], [K_mean, M_mean, N_mean, Q_mean, U_mean, V_mean] , deg = 5)
             
            for x in ['K', 'M', 'N', 'Q', 'U', 'V']:
                self.order_creation(mean_list = mean_k, symbol = x)

            self.hedging(mean_k)

        self._count+=1
    
        

    


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
