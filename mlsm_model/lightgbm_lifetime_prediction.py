import lightgbm as lgb
# from sklearn.datasets import load_boston
from sklearn.datasets import fetch_california_housing
from sklearn.metrics import mean_squared_error
from sklearn.model_selection import train_test_split

import numpy as np
import pandas as pd
# Load data
# boston = load_boston()
data_file_path = "../mlsm_scripts/internal_key_lifetime.txt"
data = pd.read_csv(data_file_path, sep=' ', header=0)


# housing = fetch_california_housing()
# X_train, X_test, y_train, y_test = train_test_split(housing.data, housing.target, test_size=0.2, random_state=0)


features = data.iloc[:, [0,2]]
features['key'] = pd.to_numeric(features['key']) 
features['insert_time'] = pd.to_numeric(features['insert_time'])
# features['key'] = features['key'].apply(lambda x: int(x,16))
# astype('category')
labels = data.iloc[:, 5]
std_dev = np.std(labels)
print('std_dev: ', std_dev)
labels = pd.to_numeric(labels)
x_train, x_test, y_train, y_test = train_test_split(features, labels, test_size=0.2, random_state=0)

# Create dataset for LightGBM
lgb_train = lgb.Dataset(x_train, y_train)
lgb_eval = lgb.Dataset(x_test, y_test, reference=lgb_train)

# Specify your configurations as a dict
params = {
    'boosting_type': 'gbdt',
    'objective': 'regression',
    'metric': {'l2', 'l1'},
    'num_leaves': 31,
    'learning_rate': 0.05,
    'feature_fraction': 0.9
}

# Train the model
print('Starting training...')
gbm = lgb.train(params,
                lgb_train,
                num_boost_round=20,
                valid_sets=lgb_eval,
                )

# Predict and evaluate
print('Starting predicting...')
y_pred = gbm.predict(x_test, num_iteration=gbm.best_iteration)
print('The rmse of prediction is:', mean_squared_error(y_test, y_pred) ** 0.5)
