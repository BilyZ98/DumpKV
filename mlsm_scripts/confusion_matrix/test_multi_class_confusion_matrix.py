from sklearn.metrics import confusion_matrix
import numpy as np

def print_confusion_matrix(y_true, y_pred):
    cm = confusion_matrix(y_true, y_pred)
    print('Confusion Matrix:')
    print(cm)

y_true = [0, 1, 2, 0, 1, 2, 0, 1, 2]
y_pred = [0, 2, 1, 0, 2, 1, 0, 1, 2]

print_confusion_matrix(y_true, y_pred)
