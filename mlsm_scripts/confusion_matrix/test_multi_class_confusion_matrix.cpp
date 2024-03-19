#include <vector>
#include <iostream>
#include <map>

void printConfusionMatrix(const std::vector<int>& y_true, const std::vector<int>& y_pred, int num_classes) {
    std::map<std::pair<int, int>, int> confusionMatrix;

    for (int i = 0; i < y_true.size(); i++) {
        confusionMatrix[std::make_pair(y_true[i], y_pred[i])]++;
    }

    std::cout << "Confusion Matrix: \n";
    for (int i = 0; i < num_classes; i++) {
        for (int j = 0; j < num_classes; j++) {
            std::cout << confusionMatrix[std::make_pair(i, j)] << "\t";
        }
        std::cout << "\n";
    }
}

int main() {
    std::vector<int> y_true = {0, 1, 2, 0, 1, 2, 0, 1, 2};
    std::vector<int> y_pred = {0, 2, 1, 0, 2, 1, 0, 1, 2};
    int num_classes = 3;

    printConfusionMatrix(y_true, y_pred, num_classes);

    return 0;
}

