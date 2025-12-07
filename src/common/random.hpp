/*
 * random.hpp
 *
 * thread-safe RNG with seeding for reproducible builds
 */

#ifndef MORPHECT_RANDOM_HPP
#define MORPHECT_RANDOM_HPP

#include <random>
#include <mutex>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <stdexcept>

namespace morphect {

class Random {
public:
    Random() : rng_(std::random_device{}()) {}
    explicit Random(uint64_t seed) : rng_(seed), seed_(seed) {}

    uint64_t getSeed() const { return seed_; }

    void seed(uint64_t new_seed) {
        std::lock_guard<std::mutex> lock(mutex_);
        seed_ = new_seed;
        rng_.seed(new_seed);
    }

    // [min, max] inclusive
    int nextInt(int min, int max) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::uniform_int_distribution<int> dist(min, max);
        return dist(rng_);
    }

    // [0, max) exclusive
    int nextInt(int max) {
        return nextInt(0, max - 1);
    }

    size_t nextSize(size_t max) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::uniform_int_distribution<size_t> dist(0, max - 1);
        return dist(rng_);
    }

    uint64_t nextUint64() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::uniform_int_distribution<uint64_t> dist;
        return dist(rng_);
    }

    // [0.0, 1.0)
    double nextDouble() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        return dist(rng_);
    }

    double nextDouble(double min, double max) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::uniform_real_distribution<double> dist(min, max);
        return dist(rng_);
    }

    // returns true with given probability
    bool decide(double probability) {
        if (probability <= 0.0) return false;
        if (probability >= 1.0) return true;
        return nextDouble() < probability;
    }

    template<typename T>
    const T& choose(const std::vector<T>& items) {
        if (items.empty()) {
            throw std::runtime_error("Cannot choose from empty vector");
        }
        return items[nextSize(items.size())];
    }

    // weighted selection - probabilities don't need to sum to 1
    template<typename T>
    size_t chooseWeighted(const std::vector<T>& items,
                          const std::vector<double>& probabilities) {
        if (items.empty()) {
            throw std::runtime_error("Cannot choose from empty vector");
        }

        if (probabilities.size() != items.size()) {
            return nextSize(items.size());
        }

        double total = 0.0;
        for (double p : probabilities) total += p;
        if (total <= 0.0) return nextSize(items.size());

        double rand_val = nextDouble() * total;
        double cumulative = 0.0;

        for (size_t i = 0; i < probabilities.size(); i++) {
            cumulative += probabilities[i];
            if (rand_val <= cumulative) {
                return i;
            }
        }

        return items.size() - 1;
    }

    template<typename T>
    const T& chooseWeightedItem(const std::vector<T>& items,
                                const std::vector<double>& probabilities) {
        return items[chooseWeighted(items, probabilities)];
    }

    template<typename T>
    void shuffle(std::vector<T>& items) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::shuffle(items.begin(), items.end(), rng_);
    }

    template<typename T>
    std::vector<T> shuffled(const std::vector<T>& items) {
        std::vector<T> result = items;
        shuffle(result);
        return result;
    }

    uint8_t nextByte() {
        return static_cast<uint8_t>(nextInt(0, 255));
    }

    uint8_t nextNonZeroByte() {
        return static_cast<uint8_t>(nextInt(1, 255));
    }

    std::vector<uint8_t> nextBytes(size_t count) {
        std::vector<uint8_t> result(count);
        for (size_t i = 0; i < count; i++) {
            result[i] = nextByte();
        }
        return result;
    }

private:
    std::mt19937_64 rng_;
    uint64_t seed_ = 0;
    std::mutex mutex_;
};

// global instance for convenience
class GlobalRandom {
public:
    static Random& get() {
        static Random instance;
        return instance;
    }

    static void setSeed(uint64_t seed) {
        get().seed(seed);
    }

    static int nextInt(int min, int max) { return get().nextInt(min, max); }
    static int nextInt(int max) { return get().nextInt(max); }
    static size_t nextSize(size_t max) { return get().nextSize(max); }
    static double nextDouble() { return get().nextDouble(); }
    static bool decide(double probability) { return get().decide(probability); }

    template<typename T>
    static const T& choose(const std::vector<T>& items) {
        return get().choose(items);
    }

    static size_t chooseWeightedIndex(const std::vector<double>& weights) {
        if (weights.empty()) return 0;

        double total = 0.0;
        for (double w : weights) total += w;
        if (total <= 0.0) return get().nextSize(weights.size());

        double rand_val = get().nextDouble() * total;
        double cumulative = 0.0;

        for (size_t i = 0; i < weights.size(); i++) {
            cumulative += weights[i];
            if (rand_val <= cumulative) return i;
        }
        return weights.size() - 1;
    }

    GlobalRandom(const GlobalRandom&) = delete;
    GlobalRandom& operator=(const GlobalRandom&) = delete;

private:
    GlobalRandom() = default;
};

} // namespace morphect

#endif // MORPHECT_RANDOM_HPP
