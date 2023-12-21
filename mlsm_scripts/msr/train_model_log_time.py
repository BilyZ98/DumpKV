
import pdb


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
    labels = data.loc[:, 'lifetime_next']
    # data[data == np.inf] = LARGE_FINITE_NUMBER
    de_duped_labels = labels.drop_duplicates()
    de_duped_labels = de_duped_labels.sort_values()
    sec_large = de_duped_labels.iloc[-2]
    LARGE_FINITE_NUMBER = 2 * sec_large
    labels = labels.replace([np.inf], LARGE_FINITE_NUMBER)

    log_labels = np.log1p(labels)
    filter_labels = labels.loc[labels < sec_large]
    print('std_dev of filter_labels: ', np.std(filter_labels))
    # pdb.set_trace()


    # lbaels = labels.astype(int)
    std_dev = np.std(labels)
    print('std_dev of labels: ', std_dev)
    std_dev_log = np.std(log_labels)
    print('std_dev of log labels: ', std_dev_log)

    

    
    # labels = pd.to_numeric(labels)
    x_train, x_test, y_train, y_test = train_test_split(features, log_labels, test_size=0.2, random_state=0)

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
        'objective': 'regression',
        'metric': {'l2' },
        # 'metric': {'multi_logloss'},
        'num_leaves': 31,
        'learning_rate': 0.05,
        'feature_fraction': 0.9,
        'bagging_fraction': 0.8,
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
    elif params['objective'] == 'binary':
        
        y_pred = gbm.predict(x_test, num_iteration=gbm.best_iteration)
        print(classification_report(y_test, y_pred.round() ))
        # y_pred = np.argmax(y_pred, axis=1)  
        # print(classification_report(y_test, y_pred))
        print(precision_score(y_test, y_pred.round(), average='macro'))
        save_model_name = output_model_name + "_binary.txt"
    elif params['objective'] == 'regression':
        y_pred = gbm.predict(x_test, num_iteration=gbm.best_iteration)
        print("mean_squared_error: ", mean_squared_error(y_test, y_pred) ** 0.5)
        y_pred_orig = np.expm1(y_pred)
        y_test_orig = np.expm1(y_test)
        print("mean_squared_error orig: ", mean_squared_error(y_test_orig, y_pred_orig) ** 0.5)
        y_test_orig = y_test_orig.reset_index(drop=True)

        print('mean y_test_orig: ', np.mean(y_test_orig))
        print('std_dev y_test_orig: ', np.std(y_test_orig))
        print('mean y_pred_orig: ', np.mean(y_pred_orig))
        print('std_dev y_pred_orig: ', np.std(y_pred_orig))



        save_model_name = output_model_name + "_regression.txt"
    else:
        print('objective is not supported')
        return

    # print('The rmse of prediction is:', mean_squared_error(y_test, y_pred) ** 0.5)

    gbm.save_model(save_model_name)
    # gbm.save_model("op_keys_binary_lifetime_lightgbm_classification_key_range_model.txt")



if __name__ == '__main__':
    cur_push_dir = os.getcwd()
    os.chdir(output_dir)
    train_model(data_file_path)
    os.chdir(cur_push_dir)


