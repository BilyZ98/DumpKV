#include <vector>
#include <iostream>

void printConfusionMatrix(const std::vector<int>& y_true, const std::vector<int>& y_pred) {
    int TP = 0, FP = 0, TN = 0, FN = 0;

    for (int i = 0; i < y_true.size(); i++) {
        if (y_true[i] == 1 && y_pred[i] == 1)
            TP++;
        else if (y_true[i] == 0 && y_pred[i] == 1)
            FP++;
        else if (y_true[i] == 0 && y_pred[i] == 0)
            TN++;
        else if (y_true[i] == 1 && y_pred[i] == 0)
            FN++;
    }

    std::cout << "Confusion Matrix: \n";
    std::cout << "TP: " << TP << ", FP: " << FP << "\n";
    std::cout << "FN: " << FN << ", TN: " << TN << "\n";
}

int main() {
    std::vector<int> y_true = {1, 0, 1, 1, 0, 1, 0, 0, 1, 0};
    std::vector<int> y_pred = {1, 0, 0, 1, 0, 1, 0, 0, 1, 1};

    printConfusionMatrix(y_true, y_pred);

    return 0;
}

