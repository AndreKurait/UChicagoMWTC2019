#!/usr/bin/env python
# coding: utf-8

# In[21]:


import numpy as np
import matplotlib.pyplot as plt
from cvxopt import matrix
from cvxopt.blas import dot
from cvxopt.solvers import qp
import pickle
from sklearn.linear_model import LinearRegression
from sklearn import metrics
import statsmodels.api as sm
from sklearn import decomposition
from sklearn import preprocessing
import pandas as pd
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import StandardScaler
from numpy import linalg
import statsmodels
import math


# In[2]:


def load_object(file_name):
    "load the pickled object"
    with open(file_name, 'rb') as f:
        return pickle.load(f)


def view_data(data_path):
    data = load_object(data_path)
    prices = data['prices']
    names = data['features']['names']
    features = data['features']['values']
    print(prices.shape)
    print(names)
    print(features.shape)
    return prices, features, names


# In[3]:


prices, features, names = view_data('C3_train.pkl')


# In[4]:


n_assets = len(prices[0])

n_obs = m = len(prices[:756,])


# In[5]:


#Creating a returns vector using prices. Loses one DF
df = pd.DataFrame(prices)
returns_df = df.pct_change()
returns_df.drop(0, axis = 0, inplace = True)


# In[6]:


#returns_df


# In[7]:


plt.plot(returns_df.T, alpha=.4);
plt.xlabel('time')
plt.ylabel('returns')


# In[8]:


def rand_weights(n):
    ''' Produces n random weights that sum to 1 '''
    k = np.random.rand(n)
    return k / sum(k)

# print(rand_weights(n_assets))
# print(rand_weights(n_assets))


# In[9]:


def random_portfolio(returns):
    ''' 
    Returns the mean and standard deviation of returns for a random portfolio
    '''

    p = np.asmatrix(np.mean(returns, axis=1))
    w = np.asmatrix(rand_weights(returns.shape[0]))
    C = np.asmatrix(np.cov(returns))
    
    mu = w * p.T
    sigma = np.sqrt(w * C * w.T)
    
    # This recursion reduces outliers to keep plots pretty
    if sigma > 2:
        return random_portfolio(returns)
    return mu, sigma


# In[10]:


n_portfolios = 500
means, stds = np.column_stack([
    random_portfolio(returns_df) 
    for _ in range(n_portfolios)
])


# In[11]:


plt.plot(stds, means, 'o', markersize=5)
plt.xlabel('std')
plt.ylabel('mean')
plt.title('Mean and standard deviation of returns of randomly generated portfolios')


# In[16]:


def optimal_portfolio(returns):
    n = len(returns)
    returns = np.asmatrix(returns)
    
    N = 100
    mus = [10**(5.0 * t/N - 1.0) for t in range(N)]
    
    # Convert to cvxopt matrices
    S = opt.matrix(np.cov(returns))
    pbar = opt.matrix(np.mean(returns, axis=1))
    
    # Create constraint matrices
    G = -opt.matrix(np.eye(n))   # negative n x n identity matrix
    h = opt.matrix(0.0, (n ,1))
    A = opt.matrix(1.0, (1, n))
    b = opt.matrix(1.0)
    
    # Calculate efficient frontier weights using quadratic programming
    portfolios = [solvers.qp(mu*S, -pbar, G, h, A, b)['x'] 
                  for mu in mus]
    ## CALCULATE RISKS AND RETURNS FOR FRONTIER
    returns = [blas.dot(pbar, x) for x in portfolios]
    risks = [np.sqrt(blas.dot(x, S*x)) for x in portfolios]
    ## CALCULATE THE 2ND DEGREE POLYNOMIAL OF THE FRONTIER CURVE
    m1 = np.polyfit(returns, risks, 2)
    x1 = np.sqrt(m1[2] / m1[0])
    # CALCULATE THE OPTIMAL PORTFOLIO
    wt = solvers.qp(opt.matrix(x1 * S), -pbar, G, h, A, b)['x']
    return np.asarray(wt), returns, risks

weights, returns, risks = optimal_portfolio(returns_df.T)

plt.plot(stds, means, 'o')
plt.ylabel('mean')
plt.xlabel('std')
plt.plot(risks, returns, 'y-o')


# In[29]:


np.array(returns).mean() * math.sqrt(254) / np.array(risks).mean()


# In[21]:


# weights


# # Forecasting

# In[12]:


#S0 to F0

s0 = returns_df.iloc[-50:-1,0] 
f0 = features[-50:-1,0]
f1squared = np.square(f0)
f0 = np.concatenate([f0,f1squared], axis = 1)


#Pull data
X_train, X_test, y_train, y_test = train_test_split(f1, s1, test_size=0.2, random_state=0) 

#Standardize
sc = StandardScaler()  
X_train = sc.fit_transform(X_train)  
X_test = sc.transform(X_test)  

#PCA on 6 components
pca = decomposition.PCA(n_components = 3)  
X_train = pca.fit_transform(X_train)
X_trainsquare = np.square(X_train)
X_train = np.concatenate([X_train,X_trainsquare], axis = 1)
X_test = pca.transform(X_test)
X_testsquare = np.square(X_test)
X_test = np.concatenate([X_test,X_testsquare], axis = 1)

model = LinearRegression()

explained_variance = pca.explained_variance_ratio_  
print(explained_variance)


# In[86]:


reg1 = model.fit(X_train, y_train)
(reg1.coef_)
print(reg1.score(X_test, y_test))


# In[87]:


# F0 to S1

s1 = prices[-50:-1,0] 
f0 = features[-51:-2,0]
f1squared = np.square(f0)
f0 = np.concatenate([f0,f1squared], axis = 1)


#Pull data
X_train, X_test, y_train, y_test = train_test_split(f0, s1, test_size=0.2, random_state=0) 

#Standardize
sc = StandardScaler()  
X_train = sc.fit_transform(X_train)  
X_test = sc.transform(X_test)  

#PCA on 6 components
pca = decomposition.PCA(n_components = 3)  
X_train = pca.fit_transform(X_train)
X_trainsquare = np.square(X_train)
X_train = np.concatenate([X_train,X_trainsquare], axis = 1)
X_test = pca.transform(X_test)
X_testsquare = np.square(X_test)
X_test = np.concatenate([X_test,X_testsquare], axis = 1)

model = LinearRegression()

explained_variance = pca.explained_variance_ratio_  
print(explained_variance)


# In[88]:


reg1 = model.fit(X_train, y_train)
print(reg1.coef_)
print(reg1.score(X_test, y_test))


# In[101]:


#S0 to F0

s0 = returns_df.iloc[-50:-1,0] 
f0 = features[-50:-1,0]
#f1squared = np.square(f0)
#f0 = np.concatenate([f0,f1squared], axis = 1)


#Pull data
#X_train, X_test, y_train, y_test = train_test_split(f1, s1, test_size=0.2, random_state=0) 

#Standardize
sc = StandardScaler()  
X_train = sc.fit_transform(f0)  
#X_test = sc.transform(X_test)  

#PCA on 6 components
pca = decomposition.PCA(n_components = 5)  
X_train = pca.fit_transform(X_train)
#X_trainsquare = np.square(X_train)
#X_train = np.concatenate([X_train,X_trainsquare], axis = 1)
#X_test = pca.transform(X_test)
#X_testsquare = np.square(X_test)
#X_test = np.concatenate([X_test,X_testsquare], axis = 1)

model = LinearRegression()

explained_variance = pca.explained_variance_ratio_  
print(explained_variance)


# In[102]:


reg1 = model.fit(X_train, s0)
print(reg1.coef_)
print(reg1.score(X_train, s0))


# # Application

# In[13]:


#Say we are given some new price/factors vector
org_prices, org_features, names = view_data('C3_train.pkl')

#Just take the first 500 features and the 501 prices, Just like train data
prices = org_prices[0:501]
features = org_features[0:500]



#Log the price vector
df = pd.DataFrame(prices)
returns_df = df.pct_change()
returns_df.drop(0, axis = 0, inplace = True)


# In[14]:


#S0 to F0 for stock 0
pca = decomposition.PCA(n_components = 3)
model = LinearRegression()
sc = StandardScaler()

s0 = returns_df.iloc[-51:-2,0] 
f0 = features[:,0].copy()
df = pd.DataFrame(f0)
featureschange_df = df.pct_change()
featureschange_df.drop(0, axis = 0, inplace = True)
f0 = featureschange_df.T.iloc[:,-51:-2]



#Standardize
 
#f0 = sc.fit_transform(f0)  


#PCA on 5 components
f0 = pca.fit_transform(f0.T)


#Fit
reg1 = model.fit(f0, s0)
coef = reg1.coef_

s1 = (prices[-1][0]  - prices[-2][0]) / prices[-2][0] 
predicted_f1 = coef.T * s1


######################################################################


# F1 to S2

s1 = returns_df.iloc[-50:-1,0] 
f0 = features[:,0].copy()
df = pd.DataFrame(f0)
featureschange_df = df.pct_change()
featureschange_df.drop(0, axis = 0, inplace = True)
f0 = featureschange_df.T.iloc[:,-51:-2]

f0 = pca.fit_transform(f0.T)
#f0_sqr = np.square(f0)
#f0 = np.concatenate([f0, f0_sqr], axis = 1)


reg2 = model.fit(f0, s1)
predicted_f1
#predicted_f1squared = np.square(predicted_f1)
#predicted_f1 = np.concatenate([predicted_f1, predicted_f1squared])

x = reg2.predict(predicted_f1.reshape(1,-1))


# In[ ]:





# In[246]:


#S0 to F0 for stock 1
pca = decomposition.PCA(n_components = 3)
model = LinearRegression()
sc = StandardScaler()

s0 = returns_df.iloc[-51:-2,1] 
f0 = features[:,1].copy()
df = pd.DataFrame(f0)
featureschange_df = df.pct_change()
featureschange_df.drop(0, axis = 0, inplace = True)
f0 = featureschange_df.T.iloc[:,-51:-2]



#Standardize
 
#f0 = sc.fit_transform(f0)  


#PCA on 5 components
f0 = pca.fit_transform(f0.T)


#Fit
reg1 = model.fit(f0, s0)
coef = reg1.coef_

s1 = (prices[-1][1]  - prices[-2][1]) / prices[-2][1] 
predicted_f1 = coef.T * s1


######################################################################


# F1 to S2

s1 = returns_df.iloc[-50:-1,1] 
f0 = features[:,1].copy()
df = pd.DataFrame(f0)
featureschange_df = df.pct_change()
featureschange_df.drop(0, axis = 0, inplace = True)
f0 = featureschange_df.T.iloc[:,-51:-2]

f0 = pca.fit_transform(f0.T)
#f0_sqr = np.square(f0)
#f0 = np.concatenate([f0, f0_sqr], axis = 1)


reg2 = model.fit(f0, s1)
predicted_f1
#predicted_f1squared = np.square(predicted_f1)
#predicted_f1 = np.concatenate([predicted_f1, predicted_f1squared])

x = reg2.predict(predicted_f1.reshape(1,-1))


# In[252]:


#S0 to F0 for stock 2
pca = decomposition.PCA(n_components = 3)
model = LinearRegression()
sc = StandardScaler()

s0 = returns_df.iloc[-51:-2,2] 
f0 = features[:,2].copy()
df = pd.DataFrame(f0)
featureschange_df = df.pct_change()
featureschange_df.drop(0, axis = 0, inplace = True)
f0 = featureschange_df.T.iloc[:,-51:-2]



#Standardize
 
#f0 = sc.fit_transform(f0)  


#PCA on 5 components
f0 = pca.fit_transform(f0.T)


#Fit
reg1 = model.fit(f0, s0)
coef = reg1.coef_

s1 = (prices[-1][2]  - prices[-2][2]) / prices[-2][2] 
predicted_f1 = coef.T * s1


######################################################################


# F1 to S2

s1 = returns_df.iloc[-50:-1,2] 
f0 = features[:,2].copy()
df = pd.DataFrame(f0)
featureschange_df = df.pct_change()
featureschange_df.drop(0, axis = 0, inplace = True)
f0 = featureschange_df.T.iloc[:,-51:-2]

f0 = pca.fit_transform(f0.T)
#f0_sqr = np.square(f0)
#f0 = np.concatenate([f0, f0_sqr], axis = 1)


reg2 = model.fit(f0, s1)
predicted_f1
#predicted_f1squared = np.square(predicted_f1)
#predicted_f1 = np.concatenate([predicted_f1, predicted_f1squared])

x = reg2.predict(predicted_f1.reshape(1,-1))


# # Arbitrary Stock Iteration

# In[15]:


prices = org_prices[0:501]
features = org_features[0:500]
q = []
for k in range(0,len(prices[-1])):    
    #S0 to F0
    pca = decomposition.PCA(n_components = 3)
    model = LinearRegression()
    sc = StandardScaler()

    s0 = returns_df.iloc[-51:-2,k] 
    f0 = features[:,k].copy()
    df = pd.DataFrame(f0)
    featureschange_df = df.pct_change()
    featureschange_df.drop(0, axis = 0, inplace = True)
    f0 = featureschange_df.T.iloc[:,-51:-2]



    #Standardize

    #f0 = sc.fit_transform(f0)  


    #PCA on 5 components
    f0 = pca.fit_transform(f0.T)


    #Fit
    reg1 = model.fit(f0, s0)
    coef = reg1.coef_

    s1 = (prices[-1][k]  - prices[-2][k]) / prices[-2][k] 
    predicted_f1 = coef.T * s1


    ######################################################################


    # F1 to S2

    s1 = returns_df.iloc[-50:-1,k] 
    f0 = features[:,k].copy()
    df = pd.DataFrame(f0)
    featureschange_df = df.pct_change()
    featureschange_df.drop(0, axis = 0, inplace = True)
    f0 = featureschange_df.T.iloc[:,-51:-2]

    f0 = pca.fit_transform(f0.T)
    #f0_sqr = np.square(f0)
    #f0 = np.concatenate([f0, f0_sqr], axis = 1)


    reg2 = model.fit(f0, s1)
    predicted_f1
    #predicted_f1squared = np.square(predicted_f1)
    #predicted_f1 = np.concatenate([predicted_f1, predicted_f1squared])

    x = reg2.predict(predicted_f1.reshape(1,-1))
    q.append(x)


# In[16]:


z = q.copy()
z = np.stack(z)


# In[417]:


#Predicted Prices
z


# In[418]:


def rand_weights(n):
    ''' Produces n random weights that sum to 1 '''
    k = np.random.randn(n)
    return k / sum(k)


# In[419]:


def random_portfolio(returns, cov):
    ''' 
    Returns the mean and standard deviation of returns for a random portfolio
    '''

    p = returns
    w = np.asmatrix(rand_weights(returns.shape[0]))
    C = cov
    
    mu = w * p
    sigma = np.sqrt(w * C * w.T)
    if m / (np.square(stds))
    # This recursion reduces outliers to keep plots pretty
    return mu, sigma


# In[483]:


#Make a bunch of random portfolios and pick the highest sharpe ratio one
#b/c this is hard jk I found a better way to do it so we are using that now


# In[94]:


# k = np.cov(returns_df.T)
# n_portfolios = 10000
# means, stds = np.column_stack([
#     random_portfolio(z, k) 
#     for _ in range(n_portfolios)
# ])


# In[408]:


# a = means / (np.square(stds))


# In[476]:


# p = z
# C = k
# portfolio = 0
# count_change = 0
# count_posmu = 0
# for x in range(0,50000):
#     w = np.asmatrix(rand_weights(returns_df.T.shape[0]))
#     mu = w * p
#     if mu > 0:
#         count_posmu+=1
#         sigma = np.sqrt(w * C * w.T)
#         sharpe = (mu / np.square(sigma))
#         if sharpe > portfolio:
#             weights = w
#             portfolio = sharpe
#             count_change+=1


# In[481]:


count_posmu


# In[480]:


count_change


# In[44]:


np.cov(features[-1].T).


# In[47]:


p = features[-1]
sigma = np.cov(features[-1].T)

cov_matrix = np.dot(np.dot(p, sigma), p.T)


# In[ ]:


G = -opt.matrix(np.eye(n))   # negative n x n identity matrix
h = opt.matrix(0.0, (n ,1))
A = opt.matrix(1.0, (1, n))
b = opt.matrix(1.0)
    
# Calculate efficient frontier weights using quadratic programming
portfolios = [solvers.qp(mu*S, -pbar, G, h, A, b)['x'] 
                 for mu in mus]


# In[57]:


np.cov(prices.T)


# In[59]:


cov_matrix


# In[75]:


from math import sqrt
n = len(returns_df.T)
S = matrix(np.cov(returns_df.T))
pbar = matrix(z)
G = matrix(0.0, (n,n))
G[::n+1] = -1.0
h = matrix(0.0, (n,1))
A = matrix(1.0, (1,n))
b = matrix(1.0)

# Compute trade-off.
N = 100
mus = [ 10**(5.0*t/N-1.0) for t in range(N) ]
#neg_mus = [ -10**(5.0*t/N-1.0) for t in range(N) ]
#mus = neg_mus + mus
portfolios = [ qp(mu*S, -pbar, G, h, A, b)['x'] for mu in mus ]
returns = [ dot(pbar,x) for x in portfolios ]
risks = [ sqrt(dot(x, S*x)) for x in portfolios ]


# In[93]:


sharpe = []
for x, y in zip(returns, risks):
    sharpe.append(x / np.square(y))
import operator
index, value = max(enumerate(sharpe), key=operator.itemgetter(1))
print(np.array(portfolios[index]))


# In[ ]:





# In[ ]:





# In[ ]:





# In[ ]:





# In[ ]:





# In[ ]:





# In[ ]:





# In[ ]:





# In[ ]:




