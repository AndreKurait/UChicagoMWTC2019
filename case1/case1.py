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
        self._lastGuess = []
        self._lastMarket = []
        self._lastPolly = []
        #This is used to relate the symbol to the index of the arrays
        self._symbol_dict = {'K': 0,
                             'M': 1,
                             'N': 2,
                             'Q': 3,
                             'U': 4,
                             'V': 5}
        
        #some arbitrary price calculation. probably will do something w/ the standard deviation that makes it better
        self._max_wiggle =  6
        self._modify_wiggle=0.05
        self._each_wiggle=0.02
        self._quant = 4
        self._pnl_fix = 0
        self._dumpLim = 10
        self._dumped = False
        

        
        '''
        order_id_dict holds the ids for the limit orders around the calcualted theo
        reversion dict holds the ids for the orders that calculate the mean reversion
        _price_dict is not implemented but will hold the prices if needed
        '''
        self.order_id_dict = {}
        self.market_data = {}
        self.owned_shares = {}
        for key, _ in self._symbol_dict.items():
            self.order_id_dict[key + '_short'] = [(0, 0),(0, 0),(0, 0),(0, 0),(0, 0),(0, 0)]
            self.order_id_dict[key + '_long'] = [(0, 0),(0, 0),(0, 0),(0, 0),(0, 0),(0, 0)]
            self.market_data[key] = []
            self.owned_shares[key] = 0
            
        #This is used to calculated the exponential weights needed for the MA time series
        self._weights = []
        for x in range(0,29):
            self._weights.append(math.e**-x)
            
    def _make_order(self, asset_code, quantity, base_price=None, order_type = Order.ORDER_LMT, bid=True):
        # time.sleep(.03)
        if base_price != None:
            base_price = float(format(base_price, '.2f'))
            if(order_type == Order.ORDER_LMT):
                # print("make order: quant:",quantity, " price:",base_price)
                return Order(asset_code = asset_code, quantity=quantity,
                            order_type = order_type,
                            price = base_price,
                            competitor_identifier = self._comp_id)
        return Order(asset_code = asset_code, quantity=quantity,
                        order_type = order_type,
                        competitor_identifier = self._comp_id)

    def order_update_handle(self, order):
        #Don't want to hold failed orders
        if type(order) != PlaceOrderResponse:
            return 0
        else:
            return order.order_id
    
    def order_creation(self, mean_list):
        for symbol, _ in self._symbol_dict.items():
            symbol_idx = self._symbol_dict[symbol]
            s_short = symbol + '_short'
            s_long = symbol + '_long'

            #This creates a limit order for a short right above the calculated theo + wiggle
            #We want to place a new order if the old one was filled or modify it if it's not a good price anymore

            quant = self._quant
            for spreadIdx in range(0, self._max_wiggle):
                # print("wiggle this b")
                wiggle = spreadIdx*self._each_wiggle + self._each_wiggle
                k_short_price = mean_list[symbol_idx] + wiggle
                k_short = self._make_order(symbol, -1*quant, k_short_price)
                k_long_price = mean_list[symbol_idx] - wiggle
                k_long = self._make_order(symbol, quant, k_long_price)

                #If the value is not in the range specified by the second list in the order_id_dict, that means it's not longer good, and we want to modify it
                if self.order_id_dict[s_short][spreadIdx][0] != 0:
                    if abs(self.order_id_dict[s_short][spreadIdx][1] - mean_list[symbol_idx]) > self._modify_wiggle:
                        order = self.modify_order(self.order_id_dict[s_short][spreadIdx][0], k_short)
                        if not self.order_update_handle(order) is 0:
                            self.order_id_dict[s_short][spreadIdx] = (self.order_update_handle(order), mean_list[symbol_idx])
                if self.order_id_dict[s_long][spreadIdx][0] != 0:
                    #If the value is not in the range specified by the second list in the order_id_dict, that means it's not longer good, and we want to modify it
                    if abs(self.order_id_dict[s_long][spreadIdx][1] - mean_list[symbol_idx]) > self._modify_wiggle:
                        order = self.modify_order(self.order_id_dict[s_long][spreadIdx][0], k_long)
                        if not self.order_update_handle(order) is 0:
                            self.order_id_dict[s_long][spreadIdx] = (self.order_update_handle(order), mean_list[symbol_idx])
                    
                #If the first value is 0, this means there is not a current order for this spread
                if self.order_id_dict[s_short][spreadIdx][0] == 0:
                    order = self.place_order(k_short)
                    if not self.order_update_handle(order) is 0:
                        self.order_id_dict[s_short][spreadIdx] = (self.order_update_handle(order), mean_list[symbol_idx])

                if self.order_id_dict[s_long][spreadIdx][0] == 0:
                    order = self.place_order(k_long)
                    if not self.order_update_handle(order) is 0:
                        self.order_id_dict[s_long][spreadIdx] = (self.order_update_handle(order), mean_list[symbol_idx])


    def hedging(self, mean_list):
        #the idea here is that if the mean reverts we sell what we have
        #may want to impliment order cancelling
        exposure = 0
        mid_exposure = 0
        for key, values in self.owned_shares.items():
            exposure += values
            #Generally these are hedges
            if key != 'K' and key != 'V':
                mid_exposure += values
        # print("exposure: ", exposure)
        # print("midexposure: ", mid_exposure)
        if abs(exposure - mid_exposure) >= 4:
            quant =  math.trunc((exposure - mid_exposure) / -2)
            # print('HEDGING: ', quant)
            k = self._make_order(asset_code = 'K', quantity = quant, order_type = Order.ORDER_MKT)
            self.place_order(k)
            v = self._make_order(asset_code = 'V', quantity = quant,  order_type = Order.ORDER_MKT)
            self.place_order(v)
            # k = self._make_order(asset_code = 'M', quantity = quant , base_price=mean_list[self._symbol_dict['M']], order_type = Order.ORDER_LMT)
            # self.place_order(k)
            # v = self._make_order(asset_code = 'N', quantity = quant, base_price=mean_list[self._symbol_dict['N']], order_type = Order.ORDER_LMT)
            # self.place_order(v)
            # k = self._make_order(asset_code = 'Q', quantity = quant , base_price=mean_list[self._symbol_dict['Q']], order_type = Order.ORDER_LMT)
            # self.place_order(k)
            # v = self._make_order(asset_code = 'U', quantity = quant, base_price=mean_list[self._symbol_dict['U']], order_type = Order.ORDER_LMT)
            # self.place_order(v)
    def dump(self):
        # while sum(values for key, values in tempDict.items()) > 1:
        for key, values in self.owned_shares.items():
                    # print('HEDGING: ', quant)
                    if abs(values) > 0:
                        quant =  math.floor(-1*values)
                        k = self._make_order(asset_code = key, quantity = quant , order_type = Order.ORDER_MKT)
                        self.place_order(k)
                        # tempDict[key] -= quant
        for key, _ in self._symbol_dict.items():
            self.order_id_dict[key + '_short'] = [(0, 0),(0, 0),(0, 0),(0, 0),(0, 0),(0, 0)]
            self.order_id_dict[key + '_long'] = [(0, 0),(0, 0),(0, 0),(0, 0),(0, 0),(0, 0)]

    def handle_exchange_update(self, exchange_update_response):
        #Data Handle
        if len(exchange_update_response.market_updates) == 0:
            return
        print(exchange_update_response.competitor_metadata)
        curpnl = exchange_update_response.competitor_metadata.pnl
        if self._dumped:
            self._pnl_fix = curpnl
            if curpnl > 100:
                self._quant = 1
        # print(exchange_update_response.fills)
        # print(exchange_update_response.fills)
        print(self._count)
        #Iterate over the fills that we've recieved to store them and our position size
        # print("FILLS",exchange_update_response.fills)
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
                for idx, values in self.order_id_dict.items():
                    for spreadIdx in range(0, self._max_wiggle):
                        value = values[spreadIdx]
                        if value[0] == q.order.order_id:
                            old_id = value[0]
                            # print(self.order_id_dict[order_string][spreadIdx])
                            # time.sleep(10)
                            self.order_id_dict[order_string][spreadIdx] = (0,0)
                self.owned_shares[order_key] += q.order.quantity 
                #just in case
                # self.cancel_order(old_id)
            except KeyboardInterrupt:
                break
            #except:
                 #print('Failure')
                 #probably want this in order to recover from ids not in the list somehow.   
                 #can add into an arbitrary quantity and symbol dict to figure out what to do
        curMarket = []
        for z in exchange_update_response.market_updates:
            code = z.asset.asset_code
            #bids = z.bids
            #asks = z.asks
            mid_market = z.mid_market_price 
            curMarket.append(mid_market)           
            self.market_data[code].append(mid_market)
            #I guess we also want the spread
        # print(curMarket)
        if self._dumped:
            self._dumped = False
            for i in range(0,5):
                for z in exchange_update_response.market_updates:
                    code = z.asset.asset_code
                    #bids = z.bids
                    #asks = z.asks
                    mid_market = z.mid_market_price         
                    self.market_data[code].append(mid_market)
        backLook = self._count
        if self._count > 5:
            #This is ensure that we have a decent sample size. We are using a exponential decay to mimic a moving average time series
            backLook=5
        if self._count > 4:
            K_mean = np.average(self.market_data['K'][-1*backLook:-1], weights = np.array(self._weights)[0:backLook-1])
            M_mean = np.average(self.market_data['M'][-1*backLook:-1], weights = np.array(self._weights)[0:backLook-1])
            N_mean = np.average(self.market_data['N'][-1*backLook:-1], weights = np.array(self._weights)[0:backLook-1])
            Q_mean = np.average(self.market_data['Q'][-1*backLook:-1], weights = np.array(self._weights)[0:backLook-1])
            U_mean = np.average(self.market_data['U'][-1*backLook:-1], weights = np.array(self._weights)[0:backLook-1])
            V_mean = np.average(self.market_data['V'][-1*backLook:-1], weights = np.array(self._weights)[0:backLook-1])
            

            #we fit a quintic polynomail to hopefully 
            polycoef = np.polyfit([1,2,3,4,5,6], [K_mean, M_mean, N_mean, Q_mean, U_mean, V_mean] , deg = 5)
            p = np.poly1d(polycoef)
            meanpoly = [p(i+1) for i in range(len(polycoef))]
            mean_k = [K_mean, M_mean, N_mean, Q_mean, U_mean, V_mean]
            stocks = 0
            for key, values in self.owned_shares.items():
                stocks += abs(values)
            if curpnl - self._pnl_fix > self._dumpLim:
                self.dump()
                self._dumped = True
            else:
                self.order_creation(mean_k)
                self.hedging(mean_k)
            # print("MeanK: ",mean_k)
            # print("MidMarket: ",curMarket)
            # # print("MeanPoly: ",meanpoly)
            # print("predicted difference: ", [a - b for a, b in zip(curMarket, self._lastGuess)] )
            # # print("polly difference: ", [a - b for a, b in zip(curMarket, self._lastPolly)] )
            # print("actual difference: ", [a - b for a, b in zip(curMarket, self._lastMarket)] )
            self._lastMarket = curMarket
            # self._lastGuess = mean_k
            # self._lastPolly = meanpoly
        print(self.owned_shares)
        # print(self.order_id_dict)
        # self._lastMarket = curMarket
        self._count+=1
    
        

    


    #         #Good code that I may need later
    #         # for x in bids:
    #         #     bids_price.append(x.price)
    #         #     bids_size.append(x.size)
    #         # for y in asks:
    #         #     asks_price.append(y.price)
    #         #     asks_size.append(y.size)
    #         # bids_array = np.asarray([(np.asarray(bids_price)).reshape(1,-1), (np.asarray(bids_size).reshape(1,-1))])
    #         # asks_array = np.asarray([(np.asarray(asks_price)).reshape(1,-1), (np.asarray(asks_size).reshape(1,-1))])
    #         # self._barray.append(bids_array)
    #         # self._aarray.append(asks_array) 
    #         # print(self._barray)
    #         # time.sleep(5)
    #     #print(exchange_update_response.market_updates)
    #     # self._count+=1
    #     # if self._count == 20:
    #     #     with open('scarped.pkl', 'wb') as handle:
    #     #         pickle.dump(self._data_scrape, handle, protocol=pickle.HIGHEST_PROTOCOL)
    #     #print(exchange_update_response)

        
    #     # # 10% of the time, cancel two arbitrary orders
    #     # if random.random() < 0.10 and len(self._orderids) > 0:
    #     #     self.cancel_order(self._orderids.pop())
    #     #     self.cancel_order(self._orderids.pop())


    #     # # place a bid and an ask for each asset
    #     # for i, asset_code in enumerate(["K", "M", "N"]):
    #     #     quantity = random.randrange(1, 10)
    #     #     base_price = random.randrange((i + 1) * 100, (i+1) * 150)
    #     #     spread = random.randrange(5, 10)

    #     #     bid_resp = self.place_order(self._make_order(asset_code, quantity,
    #     #         base_price, spread, True))
    #     #     ask_resp = self.place_order(self._make_order(asset_code, quantity,
    #     #         base_price, spread, False))

    #     #     if type(bid_resp) != PlaceOrderResponse:
    #     #         print(bid_resp)
    #     #     else:
    #     #         self._orderids.add(bid_resp.order_id)

    #     #     if type(ask_resp) != PlaceOrderResponse:
    #     #         print(ask_resp)
    #     #     else:
    #     #         self._orderids.add(ask_resp.order_id)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Run the exchange client')
    parser.add_argument("--server_host", type=str, default="localhost")
    parser.add_argument("--server_port", type=str, default="50052")
    parser.add_argument("--client_id", type=str, default="kansas")
    parser.add_argument("--client_private_key", type=str)
    parser.add_argument("--websocket_port", type=int, default=5678)

    args = parser.parse_args()
    host, port, client_id, client_pk, websocket_port = (args.server_host, args.server_port,
                                        args.client_id, args.client_private_key,
                                        args.websocket_port)

    client = Case1(host, port, client_id, client_pk, websocket_port)
    client.start_updates()
