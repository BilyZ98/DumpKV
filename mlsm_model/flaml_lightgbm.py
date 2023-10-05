from flaml import AutoML
import numpy as np
import pandas as pd
import lightgbm as lgb
import matplotlib.pyplot as plt
# from sklearn.datasets import load_boston
from sklearn.datasets import fetch_california_housing
from sklearn.metrics import mean_squared_error
from sklearn.metrics import classification_report
from sklearn.model_selection import train_test_split


automl = AutoML()

data_file_path = "/mnt/nvme0n1/mlsm/test_blob/with_gc_1.0_0.8/internal_key_lifetime.txt"
data = pd.read_csv(data_file_path, sep=' ', header=0)


# housing = fetch_california_housing()
# X_train, X_test, y_train, y_test = train_test_split(housing.data, housing.target, test_size=0.2, random_state=0)


features = data.iloc[:, [0,2,7,9]]
features['key'] = pd.to_numeric(features['key']) 
features['insert_time'] = pd.to_numeric(features['insert_time'])
features['period_num_writes'] = pd.to_numeric(features['period_num_writes'])
features['key_range_idx'] = pd.to_numeric(features['key_range_idx'])
# features['key'] = features['key'].apply(lambda x: int(x,16))
# astype('category')
labels = data.iloc[:, 8]
std_dev = np.std(labels)
print('std_dev: ', std_dev)
labels = pd.to_numeric(labels)
x_train, x_test, y_train, y_test = train_test_split(features, labels, test_size=0.2, random_state=0)





automl_settings = {
    "time_budget": 200,  # total running time in seconds
    "metric": 'accuracy',
    "task": 'classification',
    "log_file_name": "key_lifetime.txt",  # flaml log file
     "estimator_list": ['lgbm', 'xgboost', 'catboost', 'rf'],
    }



automl.fit(X_train=x_train, y_train=y_train, **automl_settings)
y_pred = automl.predict(x_test)
print(classification_report(y_test, y_pred ))
print(automl.model.estimator)
plt.barh(automl.model.estimator.feature_name_, automl.model.estimator.feature_importances_)
plt.savefig('feature_importance.png')
# automl.model.estimator.save_model('automl_lightgbm_model.txt')
print(automl.best_estimator)
# lgbm
print(automl.best_config)

from flaml.automl.data import get_output_from_log

time_history, best_valid_loss_history, valid_loss_history, config_history, metric_history = get_output_from_log(filename=automl_settings["log_file_name"], time_budget=120)


import numpy as np

plt.figure()
plt.title("Learning Curve")
plt.xlabel("Wall Clock Time (s)")
plt.ylabel("Validation Accuracy")
plt.step(time_history, 1 - np.array(best_valid_loss_history), where="post")
plt.savefig('learning_curve.png')
