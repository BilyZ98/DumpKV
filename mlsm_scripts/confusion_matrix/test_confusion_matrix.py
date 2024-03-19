from sklearn.metrics import confusion_matrix
import numpy as np

# y_true is your actual test set class labels
# y_pred is your predicted class labels
y_true = [2, 0, 2, 2, 0, 1]
y_pred = [0, 0, 2, 2, 0, 2]

# Get the confusion matrix
cnf_matrix = confusion_matrix(y_true, y_pred)

# Calculate TP, TN, FP, FN for each class
FP = cnf_matrix.sum(axis=0) - np.diag(cnf_matrix)
FN = cnf_matrix.sum(axis=1) - np.diag(cnf_matrix)
TP = np.diag(cnf_matrix)
TN = cnf_matrix.sum() - (FP + FN + TP)

print("FP: ", FP)
print("FN: ", FN)
print("TP: ", TP)
print("TN: ", TN)

