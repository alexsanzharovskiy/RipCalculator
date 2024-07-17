#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <algorithm>
#include <stdexcept>
#include <filesystem>
#include <chrono>
#include "include/json.hpp"

using json = nlohmann::json;
using namespace std;

struct WinRange {
    double payout;
    vector<double> ranges;
};

class RtpCalculator {
public:
    static const string FILE_NAME;
    double rtp;
    vector<WinRange> win_ranges;
    string filePath;
    vector<double> probabilities;
     std::string savePath;

    RtpCalculator(double rtp, const std::string& exePath) {
        this->rtp = rtp / 100.0;  // Convert RTP to a decimal
         savePath = exePath + "/storage/" + FILE_NAME;
            createStorageDirectory(exePath + "/storage");
    }

    void createStorageDirectory(const std::string& path) {
        std::filesystem::create_directories(path);
    }

    bool generate(const string &win_ranges_json_path) {
        try {
            ifstream file(win_ranges_json_path);
            if (!file.is_open()) {
                throw runtime_error("Could not open file");
            }
            json j;
            file >> j;

            // Parse the JSON manually
            for (const auto& item : j) {
                WinRange wr;
                wr.payout = item["payout"];
                wr.ranges = item["ranges"].get<vector<double>>();
                win_ranges.push_back(wr);
            }

            // Определение пути к директории storage рядом с исполняемым файлом
            string exePath = filesystem::path(__FILE__).parent_path().string();


            probabilities = geneticAlgorithm();

            ofstream outFile(savePath);
            if (!outFile.is_open()) {
                throw runtime_error("Could not write to file");
            }
            json outJson = probabilities;
            outFile << outJson.dump(4);

            return true;
        } catch (const exception &e) {
            cerr << "Error: " << e.what() << endl;
            return false;
        }
    }

    string getFilePath() const {
        return filePath;
    }

private:
    vector<double> geneticAlgorithm() {
        const int populationSize = 500;
        const int numGenerations = 5000;
        const double mutationRate = 0.01;

        vector<Individual> population;
        for (int i = 0; i < populationSize; ++i) {
            population.push_back(initializeIndividual());
        }

        for (int generation = 0; generation < numGenerations; ++generation) {
            population = evaluateFitness(population);
            population = selection(population);
            population = crossover(population);
            population = mutate(population, mutationRate);
        }

        population = evaluateFitness(population);
        sort(population.begin(), population.end(), [](const Individual &a, const Individual &b) {
            return b.fitness < a.fitness;
        });

        return population[0].probabilities;
    }

    struct Individual {
        vector<double> probabilities;
        double fitness;
    };

    Individual initializeIndividual() {
        int numCombinations = win_ranges.size();
        vector<double> probabilities(numCombinations, 0.0);
        double totalProbability = 0.0;

        random_device rd;
        mt19937 gen(rd());
        uniform_real_distribution<> dis(0.0, 1.0);

        for (int i = 0; i < numCombinations; ++i) {
            probabilities[i] = dis(gen);
            totalProbability += probabilities[i];
        }

        for (int i = 0; i < numCombinations; ++i) {
            probabilities[i] /= totalProbability;
        }

        return {probabilities, 0.0};
    }

    vector<Individual> evaluateFitness(vector<Individual> &population) {
        for (auto &individual : population) {
            individual.fitness = calculateRTP(individual.probabilities);
        }
        return population;
    }

    double calculateRTP(const vector<double> &probabilities) {
        double expectedPayout = 0.0;
        for (size_t i = 0; i < win_ranges.size(); ++i) {
            expectedPayout += probabilities[i] * win_ranges[i].payout;
        }
        return 1.0 - abs(rtp - expectedPayout);
    }

    vector<Individual> selection(vector<Individual> &population) {
        sort(population.begin(), population.end(), [](const Individual &a, const Individual &b) {
            return b.fitness < a.fitness;
        });
        population.resize(population.size() / 2);
        return population;
    }

    vector<Individual> crossover(const vector<Individual> &population) {
        vector<Individual> newPopulation = population;
        int numCombinations = win_ranges.size();

        random_device rd;
        mt19937 gen(rd());

        while (newPopulation.size() < 100) {
            uniform_int_distribution<> dis(0, population.size() - 1);
            const Individual &parent1 = population[dis(gen)];
            const Individual &parent2 = population[dis(gen)];
            uniform_int_distribution<> crossDis(1, numCombinations - 1);
            int crossoverPoint = crossDis(gen);

            vector<double> child1Probabilities(parent1.probabilities.begin(), parent1.probabilities.begin() + crossoverPoint);
            child1Probabilities.insert(child1Probabilities.end(), parent2.probabilities.begin() + crossoverPoint, parent2.probabilities.end());

            vector<double> child2Probabilities(parent2.probabilities.begin(), parent2.probabilities.begin() + crossoverPoint);
            child2Probabilities.insert(child2Probabilities.end(), parent1.probabilities.begin() + crossoverPoint, parent1.probabilities.end());

            newPopulation.push_back({normalize(child1Probabilities), 0.0});
            newPopulation.push_back({normalize(child2Probabilities), 0.0});
        }

        return newPopulation;
    }

    vector<Individual> mutate(vector<Individual> &population, double mutationRate) {
        random_device rd;
        mt19937 gen(rd());
        uniform_real_distribution<> dis(0.0, 1.0);

        for (auto &individual : population) {
            if (dis(gen) < mutationRate) {
                uniform_int_distribution<> indexDis(0, individual.probabilities.size() - 1);
                int mutationIndex = indexDis(gen);
                individual.probabilities[mutationIndex] = dis(gen);
                individual.probabilities = normalize(individual.probabilities);
            }
        }

        return population;
    }

    vector<double> normalize(vector<double> probabilities) {
        double totalProbability = accumulate(probabilities.begin(), probabilities.end(), 0.0);
        for (auto &prob : probabilities) {
            prob /= totalProbability;
        }
        return probabilities;
    }
};

const string RtpCalculator::FILE_NAME = "rtp_probabilities.json";

int main(int argc, char *argv[]) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <rtp> <win_ranges.json>" << endl;
        return 1;
    }

    double rtp = stod(argv[1]);
    string winRangesFile = argv[2];

    std::string exePath(argv[0]);
    exePath = exePath.substr(0, exePath.find_last_of("/"));

    auto start = chrono::high_resolution_clock::now();

    RtpCalculator calculator(rtp, exePath);
    if (calculator.generate(winRangesFile)) {
        cout << "Probabilities saved to " << calculator.getFilePath() << endl;
    } else {
        cerr << "Failed to generate probabilities." << endl;
        return 1;
    }

    auto end = chrono::high_resolution_clock::now();
    chrono::duration<double> duration = end - start;
    cout << "Execution time: " << duration.count() << " seconds" << endl;

    return 0;
}
