<?php

class RtpCalculator {

    public const FILE_NAME = 'rtp_probabilities.json';

    private $rtp;

    private array $win_ranges;

    private string $filePath;

    private array $probabilities;

    public function __construct($rtp) {
        $this->rtp = $rtp / 100;  // Convert RTP to a decimal
    }

    /**
     * @return bool
     */
    public function generate(string $win_ranges_json_path): bool
    {
        try {
            $readFile = file_get_contents($win_ranges_json_path);

            $this->win_ranges = json_decode($readFile, true);

            $storageDir = __DIR__ . '/storage';

            if (!is_dir($storageDir)) {
                mkdir($storageDir, 0777, true);
            }

            $this->probabilities = $this->geneticAlgorithm();

            $this->filePath = $storageDir . '/' . self::FILE_NAME;

            $save = file_put_contents($this->filePath, json_encode($this->getProbabilities()));

            return $save !== false;
        } catch (Exception $exception) {
            return false;
        }
    }

    /**
     * @return string
     */
    public function getFilePath() : string
    {
        return $this->filePath;
    }


    private function geneticAlgorithm() {
        $populationSize = 100;
        $numGenerations = 1000;
        $mutationRate = 0.01;

        // Initialize population
        $population = [];
        for ($i = 0; $i < $populationSize; $i++) {
            $individual = $this->initializeIndividual();
            $population[] = $individual;
        }

        // Evolve population
        for ($generation = 0; $generation < $numGenerations; $generation++) {
            $population = $this->evaluateFitness($population);
            $population = $this->selection($population);
            $population = $this->crossover($population);
            $population = $this->mutate($population, $mutationRate);
        }

        // Return the best individual
        $population = $this->evaluateFitness($population);
        usort($population, function($a, $b) {
            return $b['fitness'] <=> $a['fitness'];
        });

        return $population[0]['probabilities'];
    }

    private function initializeIndividual() {
        $numCombinations = count($this->win_ranges);
        $probabilities = array_fill(0, $numCombinations, 0);
        $totalProbability = 0;

        // Assign random probabilities
        for ($i = 0; $i < $numCombinations; $i++) {
            $probabilities[$i] = mt_rand() / mt_getrandmax();
            $totalProbability += $probabilities[$i];
        }

        // Normalize probabilities to sum up to 1
        for ($i = 0; $i < $numCombinations; $i++) {
            $probabilities[$i] /= $totalProbability;
        }

        return ['probabilities' => $probabilities, 'fitness' => 0];
    }

    private function evaluateFitness($population) {
        foreach ($population as &$individual) {
            $individual['fitness'] = $this->calculateRTP($individual['probabilities']);
        }
        return $population;
    }

    private function calculateRTP($probabilities) {
        $expectedPayout = 0;
        for ($i = 0; $i < count($this->win_ranges); $i++) {
            $expectedPayout += $probabilities[$i] * $this->win_ranges[$i]['payout'];
        }
        return 1 - abs($this->rtp - $expectedPayout); // Fitness is inversely proportional to the difference from target RTP
    }

    private function selection($population) {
        // Select top 50% individuals
        usort($population, function($a, $b) {
            return $b['fitness'] <=> $a['fitness'];
        });
        return array_slice($population, 0, count($population) / 2);
    }

    private function crossover($population) {
        $newPopulation = $population;
        $numCombinations = count($this->win_ranges);

        // Crossover top 50% individuals to create new individuals
        while (count($newPopulation) < 100) {
            $parent1 = $population[array_rand($population)];
            $parent2 = $population[array_rand($population)];
            $crossoverPoint = rand(1, $numCombinations - 1);

            $child1Probabilities = array_merge(
                array_slice($parent1['probabilities'], 0, $crossoverPoint),
                array_slice($parent2['probabilities'], $crossoverPoint)
            );

            $child2Probabilities = array_merge(
                array_slice($parent2['probabilities'], 0, $crossoverPoint),
                array_slice($parent1['probabilities'], $crossoverPoint)
            );

            $newPopulation[] = ['probabilities' => $this->normalize($child1Probabilities), 'fitness' => 0];
            $newPopulation[] = ['probabilities' => $this->normalize($child2Probabilities), 'fitness' => 0];
        }

        return $newPopulation;
    }

    private function mutate($population, $mutationRate) {
        foreach ($population as &$individual) {
            if (mt_rand() / mt_getrandmax() < $mutationRate) {
                $mutationIndex = array_rand($individual['probabilities']);
                $individual['probabilities'][$mutationIndex] = mt_rand() / mt_getrandmax();
                $individual['probabilities'] = $this->normalize($individual['probabilities']);
            }
        }
        return $population;
    }

    private function normalize($probabilities) {
        $totalProbability = array_sum($probabilities);
        for ($i = 0; $i < count($probabilities); $i++) {
            $probabilities[$i] /= $totalProbability;
        }
        return $probabilities;
    }

    public function getProbabilities(): array
    {
        return $this->probabilities;
    }

    public function getWinRanges(): array
    {
        return $this->win_ranges;
    }
}
