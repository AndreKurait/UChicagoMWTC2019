import numpy as np
import pickle

import numpy as np
from cvxopt import matrix
from cvxopt.blas import dot
from cvxopt.solvers import qp
import pickle
from sklearn.linear_model import LinearRegression
from sklearn import decomposition
import pandas as pd
from numpy import linalg
import math
from sklearn.covariance import LedoitWolf
import cvxopt

def load_object(file_name):
    """load the pickled object"""
    with open(file_name, 'rb') as f:
        return pickle.load(f)


def view_data(data_path):
    data = load_object(data_path)
    prices = data['prices']
    names = data['features']['names']
    factors = data['features']['values']
    print(prices.shape)
    print(names)
    print(factors.shape)
    return prices, factors


class Strategy():
    def __init__(self):
        self.prices = []
        self.past_factors = []
        pass

    def handle_update(self, inx, price, factors):
        """Put your logic here
        Args:
            inx: zero-based inx in days
            price: [num_assets, ]
            factors: [num_assets, num_factors]
        Return:
            allocation: [num_assets, ]
        """
        print(inx)
        self.prices.append(price)
        self.past_factors.append(factors)

        if(inx >= 700):
            try:
                factors = np.array(self.past_factors)
                #Just take the first 500 features and the 501 self.prices, Just like train data

                #Log the price vector
                df = pd.DataFrame(self.prices)
                returns_df = df.pct_change()
                returns_df.drop(0, axis = 0, inplace = True)
                # returns_df.sub(returns_df.mean(axis=1), axis=0)

                # In[84]:

                q = []
                for k in range(0,len(self.prices[-1])):    
                    #S0 to F0
                    
                    backrange = inx - 4
                    if(inx > 54):
                        backrange = 50

                    pca = decomposition.PCA(n_components = 3)
                    model = LinearRegression()

                    s0 = returns_df.iloc[-1*backrange-1:-2,k] 
                    f0 = factors[:,k].copy()
                    f0 = f0[-1*backrange-2:-2]
                    df = pd.DataFrame(f0)
                    featureschange_df = df.pct_change()
                    f0 = featureschange_df.drop(0, axis = 0)
                    f0.fillna(0, inplace = True)


                    #PCA on 5 components
                    f0 = pca.fit_transform(f0)


                    #Fit
                    reg1 = model.fit(f0, s0)
                    coef = reg1.coef_

                    s1 = (self.prices[-1][k]  - self.prices[-2][k]) / self.prices[-2][k] 
                    predicted_f1 = coef.T * s1
                    # print(predicted_f1[0])

                    ######################################################################


                    # F0 to S1

                    s1 = returns_df.iloc[-1*backrange:-1,k] 

                    f0_sqr = np.square(f0)
                    f0 = np.concatenate([f0, f0_sqr], axis = 1)


                    reg2 = model.fit(f0, s1)
                    predicted_f1
                    predicted_f1squared = np.square(predicted_f1)
                    predicted_f1 = np.concatenate([predicted_f1, predicted_f1squared])

                    x = reg2.predict(predicted_f1.reshape(1,-1))
                    q.append(x)

                LedWolf = LedoitWolf().fit(self.prices)
                cov = LedWolf.covariance_


                cvxopt.solvers.options['show_progress'] = False

                from math import sqrt
                n = len(returns_df.T)
                S = matrix(cov)
                pbar = matrix(np.stack(q))
                G = matrix(0.0, (n,n))
                G[::n+1] = -1.0
                h = matrix(0.0, (n,1))
                A = matrix(1.0, (1,n))
                b = matrix(1.0)

                # Compute trade-off.
                N = 50
                mus = [ 10**(5.0*t/N-1.0) for t in range(N) ]
                #neg_mus = [ -10**(5.0*t/N-1.0) for t in range(N) ]
                #mus = neg_mus + mus
                portfolios = [ qp(mu*S, -pbar, G, h, A, b)['x'] for mu in mus ]
                returns = [ dot(pbar,x) for x in portfolios ]
                risks = [ sqrt(dot(x, S*x)) for x in portfolios ]

                sharpe = []
                for x, y in zip(returns, risks):
                    sharpe.append(x / np.square(y))
                import operator
                index, value = max(enumerate(sharpe), key=operator.itemgetter(1))

                return (np.array(portfolios[index])[:,0])
            except:
                print("Exception")
                return np.array([1.0] * price.shape[0])

        # assert portfolios[index].shape == factors.shape[0]
        return np.array([1.0] * price.shape[0])

