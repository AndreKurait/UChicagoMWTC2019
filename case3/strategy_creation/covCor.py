import numpy as np
def covCor(x, shrink=-1):

    # de-mean returns
    (t, n) = x.shape
    x = np.transpose(x)
    for i in range(n):
        meanx = x[i].mean()
        x[i] = x[i] - meanx
    x = np.transpose(x)


    #compute sample covariance matrix
    sample=(1/t)*(np.matmul(np.transpose(x),x))

    #compuute prior
    var = np.diag(sample)
    sqrtvar = np.sqrt(var)
    # print(sampl e is np.transpose(sample))
    rBar = (np.sum(sample / (np.repeat(sqrtvar, n, axis=0).reshape(n,n) * np.transpose(np.repeat(sqrtvar, n, axis=0).reshape(n,n)))) -n)/(n*(n-1))
    prior = rBar * (np.repeat(sqrtvar, n, axis=0).reshape(n,n) * np.transpose(np.repeat(sqrtvar, n, axis=0).reshape(n,n)))
    np.fill_diagonal(prior, var)
    if (shrink == -1):    # compute shrinkage parameters and constants
        # what we call pi-hat
        y = np.multiply(x,x)
        phiMat = np.matmul(np.transpose(y), y)/t - np.multiply((2*(np.matmul(np.transpose(x),x))), sample/t)+ np.multiply(sample,sample)
        phi = np.sum(phiMat)

        # what we call rho-hat
        term1 = np.matmul(np.transpose(np.multiply(x,np.multiply(x,x))), x) / t
        help = np.matmul(np.transpose(x),x) / t
        helpDiag = np.diag(help)
        
        term2 = np.multiply(np.repeat(helpDiag, n, axis=0).reshape(n,n),sample)
        term3 = np.multiply(help, np.repeat(var, n, axis=0).reshape(n,n))
        term4 = np.multiply(np.repeat(var, n, axis=0).reshape(n,n), sample)
        thetaMat = term1 - term2 - term3 + term4
        np.fill_diagonal(thetaMat, 0)
        rho = np.sum(np.diag(phiMat)) + rBar * np.sum(np.multiply((1.0/sqrtvar)*np.transpose(sqrtvar) , thetaMat))
        
        # what we call gamma-hat
        gamma = np.linalg.norm(sample - prior, 'fro')  ** 2

        # compute shrinkage constant
        kappa = (phi - rho) / gamma
        shrinkage = max(0, min(1, kappa / t))

    else:    # use specified constant
        shrinkage = shrink

    # compute the estimator
    sigma = shrinkage * prior + (1 - shrinkage) * sample
    return sigma, shrinkage
