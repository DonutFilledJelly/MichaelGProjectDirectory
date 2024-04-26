#include "stdio.h"
#include "iostream"
#include "random
#include <windows.h>
#include <vector>
#include <numeric>
#include <algorithm>
#include <fstream>

using namespace std;

vector<vector<bool>> stabilitypattern;

int timeRandomizer(){
    int random = rand();
    if(random%2 == 0){
        return 1;
    }
    else{
        return -1;
    }
};


//generate one pattern, with 100 neurons in it
vector<double> generateOne(){
    vector<double> currentVec;
    currentVec.reserve(100);
    for(int i = 0; i < 100; i++){
        currentVec.push_back(timeRandomizer());
    }
    return currentVec;
}
//generate 50 patterns
vector<vector<double>> generatefifty(){
    vector<vector<double>> fifty;
    fifty.reserve(50);
    for(int i = 0; i < 50; i++){
        fifty.push_back(generateOne());
    }
    return fifty;
};


//finds the weight 
vector<vector<double>> findWeightFormula(vector<vector<double>> Weight, vector<vector<double>> nums, int patterncount){
        
    for(int i = 0; i < 100; i++){
        vector<double> tmpvector;
        Weight.push_back(tmpvector);
        Weight[i].reserve(100);
        for(int j = 0; j < 100; j++){
            double W = 0;
            if(i != j){
                for(int k = 0; k <= patterncount; k++){
                    W += nums[k][i] * nums[k][j];

                }
            }
            W = W/100;
            Weight[i].push_back(W);
        }
    }
    return Weight;
}



bool stabilityTest(const std::vector<std::vector<double>>& weights, const std::vector<double>& pattern) {
    // Number of neurons in the network
    int N = weights.size();
    // Compute the updated state of neurons
    std::vector<double> Sprime(N);
    for (int i = 0; i < N; ++i) {
        double dot_product = 0.0;
        for (int j = 0; j < N; ++j) {
            dot_product += weights[i][j] * pattern[j];
        }
        Sprime[i] = (dot_product >= 0) ? 1 : -1;
    }
    // Check if the updated state matches the original pattern
    return std::equal(Sprime.begin(), Sprime.end(), pattern.begin());
}


int main(){
    srand((unsigned) time(NULL));
    vector<vector<double>> nums;
    nums = generatefifty();
    vector<vector<double>> Weight;
    int stopat = 0;
    vector<vector<double>> network;
    for(int i = 0; i < 50; i++){

        stopat++;
        network.clear();
        for(int k = 0; k <= i; k++){
            network.push_back(nums[k]);
        }
        Weight.clear();
        Weight = findWeightFormula(Weight, nums, i);
        bool stability = true;
        vector<bool> tmp;
        stabilitypattern.push_back(tmp);
        for(int k = 0; k <= i; k++){
            stability = stabilityTest(Weight, network[k]);
            stabilitypattern[i].push_back(stability);
        }
        
    }
    string filename = "test.csv";
    ofstream outputFile(filename, std::ios_base::app);

    if (!outputFile.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return 0;
    }
    outputFile << "Current Pattern Count" << endl;
    for (int i = 0; i < stabilitypattern.size(); i++) {
        outputFile << i << ",";
        
        for (size_t k = 0; k < stabilitypattern[i].size(); ++k) {
            outputFile << stabilitypattern[i][k];
            if (k != stabilitypattern[i].size() - 1) {
                outputFile << ",";
            }
        }
        outputFile << endl;
    }

    outputFile.close();
    return 0;

}