


import lightgbm as lgb
# from sklearn.datasets import load_boston
from sklearn.datasets import fetch_california_housing
from sklearn.metrics import mean_squared_error
from sklearn.metrics import classification_report
from sklearn.metrics import precision_score
from sklearn.model_selection import train_test_split

import numpy as np
import pandas as pd
import sys
import os 
# Load data
# boston = load_boston()
# data_file_path = "../mlsm_scripts/internal_key_lifetime.txt"
output_dir = sys.argv[1]
data_file_path = sys.argv[2]
output_model_name = sys.argv[3]

if output_model_name == "":
    print('output_model_name is empty')
    exit(1)

delta_count = 10
delta_prefix = 'delta_'
edc_prefix = 'edc_'
edc_count = 10
edc_columns = [edc_prefix + str(i) for i in range(0, edc_count)]
delta_columns = [delta_prefix + str(i) for i in range(0, delta_count)]

feature_columns = edc_columns + delta_columns



def train_model(data_file_path):
    if data_file_path == "":
        print('data_file_path is empty')
        return
    data = pd.read_csv(data_file_path,  header=0)


    # housing = fetch_california_housing()
    # X_train, X_test, y_train, y_test = train_test_split(housing.data, housing.target, test_size=0.2, random_state=0)

    

    features = data.loc[:, feature_columns]

    # astype('category')
    # labels = data.iloc[:, 8]
    labels = data.loc[:, 'lifetime'].shift(-1)
    print('pre len(labels): ', len(labels))
    labels = labels.dropna(subset=['lifetime'])
    print('after len(labels): ', len(labels))


    # lbaels = labels.astype(int)
    std_dev = np.std(labels)
    print('std_dev: ', std_dev)
    labels = pd.to_numeric(labels)
    x_train, x_test, y_train, y_test = train_test_split(features, labels, test_size=0.2, random_state=0)

    # Create dataset for LightGBM
    lgb_train = lgb.Dataset(x_train, y_train)
    lgb_eval = lgb.Dataset(x_test, y_test, reference=lgb_train)

    # Specify your configurations as a dict
    # params = {
    #     'boosting_type': 'gbdt',
    #     'objective': 'multiclass',
    #     # 'metric': {'auc', },
    #     'metric': {'multi_logloss'},
    #     'num_leaves': 31,
    #     'learning_rate': 0.05,
    #     'num_class': 4,
    #     'feature_fraction': 0.9,
    #     'verbose': 10

    # }
    params = {
        'boosting_type': 'gbdt',
        'objective': 'binary',
        'metric': {'auc' },
        # 'metric': {'multi_logloss'},
        'num_leaves': 31,
        'learning_rate': 0.05,
        # 'num_class': 4,
        'feature_fraction': 0.9,
        'verbose': 10

    }
    # Train the model
    print('Starting training...')
    gbm = lgb.train(params,
                    lgb_train,
                    num_boost_round=100,
                    valid_sets=lgb_eval,
                    )

    # Predict and evaluate
    print('Starting predicting...')

    save_model_name = ""
    if params['objective'] == 'multiclass':
        y_pred = gbm.predict(x_test, num_iteration=gbm.best_iteration)
        y_pred = np.argmax(y_pred, axis=1)  
        print(classification_report(y_test, y_pred))
        print(precision_score(y_test, y_pred, average='macro'))
        # save_model_name = "op_keys_multi_lifetime_lightgbm_classification_key_range_model.txt"
        save_model_name = output_model_name + "_multi.txt"
    else:
        
        y_pred = gbm.predict(x_test, num_iteration=gbm.best_iteration)
        print(classification_report(y_test, y_pred.round() ))
        # y_pred = np.argmax(y_pred, axis=1)  
        # print(classification_report(y_test, y_pred))
        print(precision_score(y_test, y_pred.round(), average='macro'))
        save_model_name = output_model_name + "_binary.txt"

    # print('The rmse of prediction is:', mean_squared_error(y_test, y_pred) ** 0.5)

    gbm.save_model(save_model_name)
    # gbm.save_model("op_keys_binary_lifetime_lightgbm_classification_key_range_model.txt")



if __name__ == '__main__':
    cur_push_dir = os.getcwd()
    os.chdir(output_dir)
    train_model(data_file_path)
    os.chdir(cur_push_dir)


