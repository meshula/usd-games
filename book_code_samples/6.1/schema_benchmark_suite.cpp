/**
 * schema_benchmark_suite.cpp
 * 
 * Referenced in Chapter 6.1: Schema Resolution and Performance Analysis
 * 
 * A comprehensive benchmark suite for USD schema operations that helps
 * teams establish performance baselines. This tool enables developers
 * to track changes in schema performance over time, compare different
 * schema approaches, and measure the impact of optimizations across
 * various configurations and platforms.
 */

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/tf/type.h>
#include <pxr/base/tf/diagnostic.h>
#include <pxr/base/tf/stopwatch.h>

#include <chrono>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <functional>
#include <random>
#include <thread>

PXR_NAMESPACE_USING_DIRECTIVE

/**
 * BenchmarkType
 * 
 * Enum defining different types of benchmarks to run
 */
enum class BenchmarkType {
    PropertyAccess,       // Attribute getters performance
    PropertySetting,      // Attribute setters performance
    TypeChecking,         // IsA and HasAPI performance
    Traversal,            // Stage traversal performance
    Composition,          // Composition performance
    SchemaInstantiation,  // Schema object creation performance
    MemoryUsage,          // Memory usage statistics
};

/**
 * PlatformInfo
 * 
 * Structure containing information about the current platform
 */
struct PlatformInfo {
    std::string platformName;
    std::string cpuInfo;
    std::string memoryInfo;
    std::string osInfo;
    std::string buildConfig;
    
    static PlatformInfo GetCurrentPlatform() {
        PlatformInfo info;
        
        // Platform name (simple detection)
#ifdef _WIN32
        info.platformName = "Windows";
#elif __APPLE__
        info.platformName = "macOS";
#elif __linux__
        info.platformName = "Linux";
#else
        info.platformName = "Unknown";
#endif

        // OS version and other details would be platform-specific
        // This is a simplified version
        info.osInfo = info.platformName;
        
        // For a real implementation, use platform-specific APIs to get
        // detailed CPU info, memory size, etc.
        info.cpuInfo = "CPU information would be detected here";
        info.memoryInfo = "Memory information would be detected here";
        
        // Build configuration
#ifdef NDEBUG
        info.buildConfig = "Release";
#else
        info.buildConfig = "Debug";
#endif
        
        return info;
    }
};

/**
 * BenchmarkConfig
 * 
 * Configuration settings for benchmark runs
 */
struct BenchmarkConfig {
    int iterations = 10;              // How many times to run each test
    int warmupRuns = 2;               // Warm-up runs before timing
    bool measureMemory = true;        // Whether to measure memory usage
    bool detailedTimings = true;      // Whether to record individual timings
    std::string outputFile = "";      // Where to save results (empty for no save)
    BenchmarkType benchmarkType = BenchmarkType::PropertyAccess;
};

/**
 * BenchmarkResult
 * 
 * Results from a benchmark run
 */
struct BenchmarkResult {
    std::string benchmarkName;
    BenchmarkType benchmarkType;
    double averageTimeMs;
    double minTimeMs;
    double maxTimeMs;
    double medianTimeMs;
    double stdDeviation;
    size_t memoryUsageStart;
    size_t memoryUsagePeak;
    size_t memoryUsageEnd;
    std::vector<double> individualTimingsMs;  // If detailed timings requested
    
    // For property access benchmarks
    int64_t propertyAccessCount = 0;
    double timePerPropertyAccessUs = 0.0;
    
    // For traversal benchmarks
    int64_t primCount = 0;
    double timePerPrimUs = 0.0;
};

/**
 * BenchmarkSuite
 * 
 * Main class for running USD schema benchmarks
 */
class BenchmarkSuite {
public:
    /**
     * Constructor
     * 
     * @param usdStage The USD stage to benchmark
     */
    BenchmarkSuite(UsdStageRefPtr usdStage) : m_stage(usdStage) {
        // Initialize platform info
        m_platformInfo = PlatformInfo::GetCurrentPlatform();
        
        // Initialize random generator for random access patterns
        std::random_device rd;
        m_randomGenerator = std::mt19937(rd());
    }
    
    /**
     * Run all available benchmarks
     * 
     * @param config Benchmark configuration
     * @return Map of benchmark results
     */
    std::unordered_map<std::string, BenchmarkResult> RunAllBenchmarks(const BenchmarkConfig& config) {
        std::unordered_map<std::string, BenchmarkResult> results;
        
        // Run property access benchmarks
        results.insert(RunPropertyAccessBenchmarks(config).begin(), RunPropertyAccessBenchmarks(config).end());
        
        // Run type checking benchmarks
        results.insert(RunTypeCheckingBenchmarks(config).begin(), RunTypeCheckingBenchmarks(config).end());
        
        // Run traversal benchmarks
        results.insert(RunTraversalBenchmarks(config).begin(), RunTraversalBenchmarks(config).end());
        
        // Run composition benchmarks
        results.insert(RunCompositionBenchmarks(config).begin(), RunCompositionBenchmarks(config).end());
        
        // Write results if output file specified
        if (!config.outputFile.empty()) {
            WriteResultsToFile(results, config.outputFile);
        }
        
        return results;
    }
    
    /**
     * Run property access benchmarks
     * 
     * @param config Benchmark configuration
     * @return Map of benchmark results
     */
    std::unordered_map<std::string, BenchmarkResult> RunPropertyAccessBenchmarks(const BenchmarkConfig& config) {
        std::unordered_map<std::string, BenchmarkResult> results;
        
        // Setup common tokens for property access tests
        static const TfToken healthToken("sparkle:health:current");
        static const TfToken damageToken("sparkle:combat:damage");
        static const TfToken speedToken("sparkle:movement:speed");
        
        // Collect all prims with these properties for benchmark
        std::vector<UsdPrim> healthPrims;
        std::vector<UsdPrim> damagePrims;
        std::vector<UsdPrim> speedPrims;
        
        for (const UsdPrim& prim : m_stage->TraverseAll()) {
            if (prim.GetAttribute(healthToken)) {
                healthPrims.push_back(prim);
            }
            if (prim.GetAttribute(damageToken)) {
                damagePrims.push_back(prim);
            }
            if (prim.GetAttribute(speedToken)) {
                speedPrims.push_back(prim);
            }
        }
        
        // Sequential access benchmark - health property
        if (!healthPrims.empty()) {
            results["SequentialHealthAccess"] = RunBenchmark(
                [&healthPrims]() {
                    float totalHealth = 0.0f;
                    for (const UsdPrim& prim : healthPrims) {
                        UsdAttribute attr = prim.GetAttribute(TfToken("sparkle:health:current"));
                        float value = 0.0f;
                        attr.Get(&value);
                        totalHealth += value;
                    }
                    return healthPrims.size();  // Return access count
                },
                "Sequential Health Property Access",
                BenchmarkType::PropertyAccess,
                config
            );
        }
        
        // Sequential access benchmark - damage property
        if (!damagePrims.empty()) {
            results["SequentialDamageAccess"] = RunBenchmark(
                [&damagePrims]() {
                    float totalDamage = 0.0f;
                    for (const UsdPrim& prim : damagePrims) {
                        UsdAttribute attr = prim.GetAttribute(TfToken("sparkle:combat:damage"));
                        float value = 0.0f;
                        attr.Get(&value);
                        totalDamage += value;
                    }
                    return damagePrims.size();  // Return access count
                },
                "Sequential Damage Property Access",
                BenchmarkType::PropertyAccess,
                config
            );
        }
        
        // Sequential access benchmark - speed property
        if (!speedPrims.empty()) {
            results["SequentialSpeedAccess"] = RunBenchmark(
                [&speedPrims]() {
                    float totalSpeed = 0.0f;
                    for (const UsdPrim& prim : speedPrims) {
                        UsdAttribute attr = prim.GetAttribute(TfToken("sparkle:movement:speed"));
                        float value = 0.0f;
                        attr.Get(&value);
                        totalSpeed += value;
                    }
                    return speedPrims.size();  // Return access count
                },
                "Sequential Speed Property Access",
                BenchmarkType::PropertyAccess,
                config
            );
        }
        
        // Token caching benchmark
        if (!healthPrims.empty()) {
            results["TokenCachedHealthAccess"] = RunBenchmark(
                [&healthPrims]() {
                    static const TfToken cachedToken("sparkle:health:current");
                    float totalHealth = 0.0f;
                    for (const UsdPrim& prim : healthPrims) {
                        UsdAttribute attr = prim.GetAttribute(cachedToken);
                        float value = 0.0f;
                        attr.Get(&value);
                        totalHealth += value;
                    }
                    return healthPrims.size();  // Return access count
                },
                "Token-Cached Health Property Access",
                BenchmarkType::PropertyAccess,
                config
            );
        }
        
        // Attribute handle caching benchmark
        if (!healthPrims.empty()) {
            results["AttributeCachedHealthAccess"] = RunBenchmark(
                [&healthPrims]() {
                    static const TfToken cachedToken("sparkle:health:current");
                    float totalHealth = 0.0f;
                    
                    // Pre-cache attribute handles
                    std::vector<UsdAttribute> cachedAttrs;
                    cachedAttrs.reserve(healthPrims.size());
                    for (const UsdPrim& prim : healthPrims) {
                        cachedAttrs.push_back(prim.GetAttribute(cachedToken));
                    }
                    
                    // Access using cached attributes
                    for (const UsdAttribute& attr : cachedAttrs) {
                        float value = 0.0f;
                        attr.Get(&value);
                        totalHealth += value;
                    }
                    
                    return healthPrims.size();  // Return access count
                },
                "Attribute-Cached Health Property Access",
                BenchmarkType::PropertyAccess,
                config
            );
        }
        
        // Random access benchmark
        if (!healthPrims.empty() && healthPrims.size() > 10) {
            results["RandomHealthAccess"] = RunBenchmark(
                [this, &healthPrims]() {
                    static const TfToken cachedToken("sparkle:health:current");
                    float totalHealth = 0.0f;
                    
                    // Create a vector of indices for random access
                    std::vector<size_t> indices(healthPrims.size());
                    for (size_t i = 0; i < indices.size(); ++i) {
                        indices[i] = i;
                    }
                    std::shuffle(indices.begin(), indices.end(), m_randomGenerator);
                    
                    // Access properties in random order
                    for (size_t idx : indices) {
                        UsdAttribute attr = healthPrims[idx].GetAttribute(cachedToken);
                        float value = 0.0f;
                        attr.Get(&value);
                        totalHealth += value;
                    }
                    
                    return healthPrims.size();  // Return access count
                },
                "Random Health Property Access",
                BenchmarkType::PropertyAccess,
                config
            );
        }
        
        return results;
    }
    
    /**
     * Run type checking benchmarks
     * 
     * @param config Benchmark configuration
     * @return Map of benchmark results
     */
    std::unordered_map<std::string, BenchmarkResult> RunTypeCheckingBenchmarks(const BenchmarkConfig& config) {
        std::unordered_map<std::string, BenchmarkResult> results;
        
        // Prepare types to check
        std::vector<std::pair<std::string, TfType>> typesToCheck = {
            {"SparkleGameEntity", TfType::FindByName("SparkleGameEntity")},
            {"SparkleEnemyCarrot", TfType::FindByName("SparkleEnemyCarrot")},
            {"SparklePlayer", TfType::FindByName("SparklePlayer")},
            {"SparklePickup", TfType::FindByName("SparklePickup")}
        };
        
        // Prepare API schemas to check
        std::vector<std::pair<std::string, TfType>> apisToCheck = {
            {"SparkleHealthAPI", TfType::FindByName("SparkleHealthAPI")},
            {"SparkleCombatAPI", TfType::FindByName("SparkleCombatAPI")},
            {"SparkleMovementAPI", TfType::FindByName("SparkleMovementAPI")},
            {"SparkleAIAPI", TfType::FindByName("SparkleAIAPI")}
        };
        
        // Get all prims for testing
        std::vector<UsdPrim> allPrims;
        for (const UsdPrim& prim : m_stage->TraverseAll()) {
            if (prim.IsValid() && !prim.IsAbstract()) {
                allPrims.push_back(prim);
            }
        }
        
        // Skip if no prims to check
        if (allPrims.empty()) {
            return results;
        }
        
        // IsA type checking benchmark
        for (const auto& typePair : typesToCheck) {
            const std::string& typeName = typePair.first;
            const TfType& type = typePair.second;
            
            // Skip if type not found
            if (!type) {
                continue;
            }
            
            std::string benchmarkName = "IsA_" + typeName;
            results[benchmarkName] = RunBenchmark(
                [&allPrims, &type]() {
                    int matchCount = 0;
                    for (const UsdPrim& prim : allPrims) {
                        if (prim.IsA(type)) {
                            matchCount++;
                        }
                    }
                    return allPrims.size();  // Return check count
                },
                "Type Checking IsA(" + typeName + ")",
                BenchmarkType::TypeChecking,
                config
            );
        }
        
        // HasAPI type checking benchmark
        for (const auto& apiPair : apisToCheck) {
            const std::string& apiName = apiPair.first;
            const TfType& apiType = apiPair.second;
            
            // Skip if API type not found
            if (!apiType) {
                continue;
            }
            
            std::string benchmarkName = "HasAPI_" + apiName;
            results[benchmarkName] = RunBenchmark(
                [&allPrims, &apiType]() {
                    int matchCount = 0;
                    for (const UsdPrim& prim : allPrims) {
                        if (prim.HasAPI(apiType)) {
                            matchCount++;
                        }
                    }
                    return allPrims.size();  // Return check count
                },
                "API Schema Check HasAPI(" + apiName + ")",
                BenchmarkType::TypeChecking,
                config
            );
        }
        
        // Type checking with caching benchmark
        results["TypeCheckingWithCache"] = RunBenchmark(
            [&allPrims, &typesToCheck]() {
                int matchCount = 0;
                
                // Create a cache of prim-to-type results
                std::unordered_map<SdfPath, std::unordered_map<TfType, bool>, SdfPath::Hash> typeCache;
                
                for (const UsdPrim& prim : allPrims) {
                    const SdfPath& path = prim.GetPath();
                    
                    for (const auto& typePair : typesToCheck) {
                        const TfType& type = typePair.second;
                        
                        // Check if we've already tested this prim and type
                        auto primIt = typeCache.find(path);
                        if (primIt != typeCache.end()) {
                            auto typeIt = primIt->second.find(type);
                            if (typeIt != primIt->second.end()) {
                                if (typeIt->second) {
                                    matchCount++;
                                }
                                continue;
                            }
                        }
                        
                        // If not in cache, perform check and cache result
                        bool isType = prim.IsA(type);
                        typeCache[path][type] = isType;
                        
                        if (isType) {
                            matchCount++;
                        }
                    }
                }
                
                return allPrims.size() * typesToCheck.size();  // Return check count
            },
            "Type Checking with Caching",
            BenchmarkType::TypeChecking,
            config
        );
        
        return results;
    }
    
    /**
     * Run traversal benchmarks
     * 
     * @param config Benchmark configuration
     * @return Map of benchmark results
     */
    std::unordered_map<std::string, BenchmarkResult> RunTraversalBenchmarks(const BenchmarkConfig& config) {
        std::unordered_map<std::string, BenchmarkResult> results;
        
        // Full stage traversal benchmark
        results["FullStageTraversal"] = RunBenchmark(
            [this]() {
                int primCount = 0;
                for (const UsdPrim& prim : m_stage->TraverseAll()) {
                    primCount++;
                }
                return primCount;  // Return prim count
            },
            "Full Stage Traversal",
            BenchmarkType::Traversal,
            config
        );
        
        // Filtered traversal by type
        TfType entityType = TfType::FindByName("SparkleGameEntity");
        if (entityType) {
            results["FilteredTraversalByType"] = RunBenchmark(
                [this, entityType]() {
                    int primCount = 0;
                    for (const UsdPrim& prim : m_stage->TraverseAll()) {
                        if (prim.IsA(entityType)) {
                            primCount++;
                        }
                    }
                    return primCount;  // Return matching prim count
                },
                "Filtered Traversal by Type (SparkleGameEntity)",
                BenchmarkType::Traversal,
                config
            );
        }
        
        // Cached traversal benchmark
        results["CachedTraversal"] = RunBenchmark(
            [this]() {
                static std::vector<UsdPrim> cachedPrims;
                if (cachedPrims.empty()) {
                    // First time, populate cache
                    for (const UsdPrim& prim : m_stage->TraverseAll()) {
                        cachedPrims.push_back(prim);
                    }
                }
                
                // Use cached prims
                int primCount = 0;
                for (const UsdPrim& prim : cachedPrims) {
                    if (prim) {
                        primCount++;
                    }
                }
                
                return primCount;  // Return prim count
            },
            "Cached Traversal",
            BenchmarkType::Traversal,
            config
        );
        
        return results;
    }
    
    /**
     * Run composition benchmarks
     * 
     * @param config Benchmark configuration
     * @return Map of benchmark results
     */
    std::unordered_map<std::string, BenchmarkResult> RunCompositionBenchmarks(const BenchmarkConfig& config) {
        std::unordered_map<std::string, BenchmarkResult> results;
        
        // Stage open benchmark
        results["StageOpen"] = RunBenchmark(
            [this]() {
                UsdStageRefPtr newStage = UsdStage::Open(m_stage->GetRootLayer()->GetIdentifier());
                return newStage ? 1 : 0;  // Return success count
            },
            "Stage Open",
            BenchmarkType::Composition,
            config
        );
        
        // Property composition benchmark
        results["PropertyComposition"] = RunBenchmark(
            [this]() {
                int propertyCount = 0;
                
                // Get commonly used health properties
                static const TfToken healthToken("sparkle:health:current");
                
                for (const UsdPrim& prim : m_stage->TraverseAll()) {
                    UsdAttribute attr = prim.GetAttribute(healthToken);
                    if (attr) {
                        // Get composed value
                        float value = 0.0f;
                        attr.Get(&value);
                        propertyCount++;
                    }
                }
                
                return propertyCount;  // Return property count
            },
            "Property Composition",
            BenchmarkType::Composition,
            config
        );
        
        return results;
    }
    
    /**
     * Run a single benchmark
     * 
     * @param benchmarkFunc Function to benchmark, returns operation count
     * @param benchmarkName Human-readable name of the benchmark
     * @param benchmarkType Type of benchmark
     * @param config Benchmark configuration
     * @return Benchmark results
     */
    template<typename Func>
    BenchmarkResult RunBenchmark(Func benchmarkFunc, const std::string& benchmarkName, 
                              BenchmarkType benchmarkType, const BenchmarkConfig& config) {
        BenchmarkResult result;
        result.benchmarkName = benchmarkName;
        result.benchmarkType = benchmarkType;
        
        std::cout << "Running benchmark: " << benchmarkName << std::endl;
        
        // Warm-up runs
        for (int i = 0; i < config.warmupRuns; ++i) {
            benchmarkFunc();
        }
        
        // Memory usage tracking
        auto getMemoryUsage = []() -> size_t {
            // This is a placeholder - actual implementation would use
            // platform-specific APIs to get process memory usage
            return 0;
        };
        
        // Collect individual timings
        std::vector<double> timingsMs;
        std::vector<int64_t> opCounts;
        
        result.memoryUsageStart = getMemoryUsage();
        result.memoryUsagePeak = result.memoryUsageStart;
        
        for (int i = 0; i < config.iterations; ++i) {
            auto start = std::chrono::high_resolution_clock::now();
            
            // Run the benchmark function
            int64_t opCount = benchmarkFunc();
            opCounts.push_back(opCount);
            
            auto end = std::chrono::high_resolution_clock::now();
            
            // Calculate elapsed time in milliseconds
            double elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();
            timingsMs.push_back(elapsedMs);
            
            // Track memory usage
            if (config.measureMemory) {
                size_t currentMemory = getMemoryUsage();
                result.memoryUsagePeak = std::max(result.memoryUsagePeak, currentMemory);
            }
            
            // Add timing to detailed results if requested
            if (config.detailedTimings) {
                result.individualTimingsMs.push_back(elapsedMs);
            }
        }
        
        result.memoryUsageEnd = getMemoryUsage();
        
        // Sort timings for percentile calculations
        std::sort(timingsMs.begin(), timingsMs.end());
        
        // Calculate statistics
        result.minTimeMs = timingsMs.front();
        result.maxTimeMs = timingsMs.back();
        result.medianTimeMs = timingsMs[timingsMs.size() / 2];
        
        // Calculate average
        double sum = std::accumulate(timingsMs.begin(), timingsMs.end(), 0.0);
        result.averageTimeMs = sum / timingsMs.size();
        
        // Calculate standard deviation
        double sqSum = std::accumulate(timingsMs.begin(), timingsMs.end(), 0.0,
                                     [&result](double acc, double x) {
                                         double diff = x - result.averageTimeMs;
                                         return acc + diff * diff;
                                     });
        result.stdDeviation = std::sqrt(sqSum / timingsMs.size());
        
        // Calculate average operation count
        int64_t totalOps = std::accumulate(opCounts.begin(), opCounts.end(), int64_t(0));
        int64_t avgOps = totalOps / opCounts.size();
        result.propertyAccessCount = avgOps;
        
        // Calculate time per operation
        if (avgOps > 0) {
            result.timePerPropertyAccessUs = (result.averageTimeMs * 1000.0) / avgOps;
            result.primCount = avgOps;
            result.timePerPrimUs = (result.averageTimeMs * 1000.0) / avgOps;
        }
        
        // Log results
        std::cout << "  Average: " << std::fixed << std::setprecision(3) << result.averageTimeMs << " ms";
        if (avgOps > 0) {
            std::cout << " (" << std::fixed << std::setprecision(3) << result.timePerPropertyAccessUs 
                     << " μs per operation, " << avgOps << " ops)";
        }
        std::cout << std::endl;
        
        return result;
    }
    
    /**
     * Write benchmark results to file
     * 
     * @param results Map of benchmark results
     * @param filename Output filename
     */
    void WriteResultsToFile(const std::unordered_map<std::string, BenchmarkResult>& results, 
                            const std::string& filename) {
        std::ofstream outFile(filename);
        if (!outFile.is_open()) {
            std::cerr << "Failed to open output file: " << filename << std::endl;
            return;
        }
        
        // Write header
        outFile << "Schema Benchmark Suite Results" << std::endl << std::endl;
        
        // Write platform info
        outFile << "Platform Information:" << std::endl;
        outFile << "  Platform: " << m_platformInfo.platformName << std::endl;
        outFile << "  OS: " << m_platformInfo.osInfo << std::endl;
        outFile << "  CPU: " << m_platformInfo.cpuInfo << std::endl;
        outFile << "  Memory: " << m_platformInfo.memoryInfo << std::endl;
        outFile << "  Build: " << m_platformInfo.buildConfig << std::endl;
        outFile << std::endl;
        
        // Write stage info
        outFile << "Stage Information:" << std::endl;
        outFile << "  Root Layer: " << m_stage->GetRootLayer()->GetIdentifier() << std::endl;
        int primCount = 0;
        for (auto _ : m_stage->TraverseAll()) { primCount++; }
        outFile << "  Prim Count: " << primCount << std::endl;
        outFile << std::endl;
        
        // Write results grouped by benchmark type
        std::unordered_map<BenchmarkType, std::vector<const BenchmarkResult*>> groupedResults;
        
        for (const auto& pair : results) {
            groupedResults[pair.second.benchmarkType].push_back(&pair.second);
        }
        
        // Define benchmark type names
        std::unordered_map<BenchmarkType, std::string> typeNames = {
            {BenchmarkType::PropertyAccess, "Property Access Benchmarks"},
            {BenchmarkType::PropertySetting, "Property Setting Benchmarks"},
            {BenchmarkType::TypeChecking, "Type Checking Benchmarks"},
            {BenchmarkType::Traversal, "Traversal Benchmarks"},
            {BenchmarkType::Composition, "Composition Benchmarks"},
            {BenchmarkType::SchemaInstantiation, "Schema Instantiation Benchmarks"},
            {BenchmarkType::MemoryUsage, "Memory Usage Benchmarks"},
        };
        
        // Write each group
        for (const auto& typePair : typeNames) {
            BenchmarkType type = typePair.first;
            const std::string& typeName = typePair.second;
            
            auto it = groupedResults.find(type);
            if (it == groupedResults.end() || it->second.empty()) {
                continue;  // Skip empty groups
            }
            
            outFile << typeName << ":" << std::endl;
            
            // Header for benchmark results
            outFile << std::left << std::setw(40) << "Benchmark"
                   << std::right << std::setw(12) << "Avg (ms)"
                   << std::setw(12) << "Min (ms)"
                   << std::setw(12) << "Max (ms)"
                   << std::setw(12) << "Median (ms)"
                   << std::setw(12) << "StdDev"
                   << std::setw(20) << "Time/Op (μs)"
                   << std::endl;
            
            outFile << std::string(120, '-') << std::endl;
            
            // Sort benchmarks by average time
            std::vector<const BenchmarkResult*> sortedResults = it->second;
            std::sort(sortedResults.begin(), sortedResults.end(),
                     [](const BenchmarkResult* a, const BenchmarkResult* b) {
                         return a->averageTimeMs < b->averageTimeMs;
                     });
            
            // Write benchmark results
            for (const BenchmarkResult* result : sortedResults) {
                outFile << std::left << std::setw(40) << result->benchmarkName
                       << std::fixed << std::setprecision(3)
                       << std::right << std::setw(12) << result->averageTimeMs
                       << std::setw(12) << result->minTimeMs
                       << std::setw(12) << result->maxTimeMs
                       << std::setw(12) << result->medianTimeMs
                       << std::setw(12) << result->stdDeviation;
                
                if (result->propertyAccessCount > 0) {
                    outFile << std::setw(20) << result->timePerPropertyAccessUs;
                } else {
                    outFile << std::setw(20) << "N/A";
                }
                
                outFile << std::endl;
            }
            
            outFile << std::endl;
        }
        
        outFile.close();
        std::cout << "Results written to: " << filename << std::endl;
    }
    
    /**
     * Generate a comparison report between two sets of benchmark results
     * 
     * @param baseline Baseline benchmark results
     * @param current Current benchmark results
     * @param filename Output filename
     */
    static void GenerateComparisonReport(
        const std::unordered_map<std::string, BenchmarkResult>& baseline,
        const std::unordered_map<std::string, BenchmarkResult>& current,
        const std::string& filename)
    {
        std::ofstream outFile(filename);
        if (!outFile.is_open()) {
            std::cerr << "Failed to open comparison output file: " << filename << std::endl;
            return;
        }
        
        // Write header
        outFile << "Schema Benchmark Comparison Report" << std::endl << std::endl;
        
        // Header for comparison results
        outFile << std::left << std::setw(40) << "Benchmark"
               << std::right << std::setw(12) << "Baseline (ms)"
               << std::setw(12) << "Current (ms)"
               << std::setw(12) << "Diff (ms)"
               << std::setw(12) << "Diff (%)"
               << std::setw(12) << "Significance"
               << std::endl;
        
        outFile << std::string(100, '-') << std::endl;
        
        // Find all benchmark names
        std::set<std::string> benchmarkNames;
        for (const auto& pair : baseline) benchmarkNames.insert(pair.first);
        for (const auto& pair : current) benchmarkNames.insert(pair.first);
        
        // Calculate comparison data
        std::vector<std::tuple<std::string, double, double, double, double>> comparisons;
        
        for (const std::string& name : benchmarkNames) {
            auto baselineIt = baseline.find(name);
            auto currentIt = current.find(name);
            
            if (baselineIt == baseline.end() || currentIt == current.end()) {
                // Skip benchmarks not in both sets
                continue;
            }
            
            double baselineTime = baselineIt->second.averageTimeMs;
            double currentTime = currentIt->second.averageTimeMs;
            double diffMs = currentTime - baselineTime;
            double diffPct = (baselineTime > 0) ? (diffMs / baselineTime) * 100.0 : 0.0;
            
            comparisons.emplace_back(name, baselineTime, currentTime, diffMs, diffPct);
        }
        
        // Sort by relative performance change (largest regression first)
        std::sort(comparisons.begin(), comparisons.end(),
                 [](const auto& a, const auto& b) {
                     return std::get<4>(a) > std::get<4>(b);
                 });
        
        // Write comparison results
        for (const auto& comparison : comparisons) {
            const std::string& name = std::get<0>(comparison);
            double baselineTime = std::get<1>(comparison);
            double currentTime = std::get<2>(comparison);
            double diffMs = std::get<3>(comparison);
            double diffPct = std::get<4>(comparison);
            
            outFile << std::left << std::setw(40) << name
                   << std::fixed << std::setprecision(3)
                   << std::right << std::setw(12) << baselineTime
                   << std::setw(12) << currentTime
                   << std::setw(12) << diffMs
                   << std::fixed << std::setprecision(2)
                   << std::setw(12) << diffPct;
            
            // Indicate significance
            std::string significance;
            if (diffPct > 10.0) {
                significance = "SIGNIFICANT REGRESSION";
            } else if (diffPct < -10.0) {
                significance = "SIGNIFICANT IMPROVEMENT";
            } else if (diffPct > 5.0) {
                significance = "Minor Regression";
            } else if (diffPct < -5.0) {
                significance = "Minor Improvement";
            } else {
                significance = "No Significant Change";
            }
            
            outFile << std::setw(24) << significance << std::endl;
        }
        
        outFile.close();
        std::cout << "Comparison report written to: " << filename << std::endl;
    }
    
private:
    UsdStageRefPtr m_stage;
    PlatformInfo m_platformInfo;
    std::mt19937 m_randomGenerator;
};

/**
 * SchemaBenchmarkSuiteRunner
 * 
 * Helper class for setting up and running the benchmark suite with various configurations
 */
class SchemaBenchmarkSuiteRunner {
public:
    /**
     * Constructor
     * 
     * @param usdFile Path to the USD file to benchmark
     */
    SchemaBenchmarkSuiteRunner(const std::string& usdFile) {
        m_stage = UsdStage::Open(usdFile);
        if (!m_stage) {
            TF_FATAL_ERROR("Failed to open USD stage: %s", usdFile.c_str());
        }
        
        std::cout << "Opened stage: " << usdFile << std::endl;
        int primCount = 0;
        for (auto _ : m_stage->TraverseAll()) { primCount++; }
        std::cout << "  Prim count: " << primCount << std::endl;
    }
    
    /**
     * Run property access benchmarks
     * 
     * @param outputFile Path to output results file
     */
    void RunPropertyAccessBenchmarks(const std::string& outputFile) {
        if (!m_stage) return;
        
        BenchmarkConfig config;
        config.iterations = 20;
        config.warmupRuns = 3;
        config.benchmarkType = BenchmarkType::PropertyAccess;
        config.outputFile = outputFile;
        
        std::cout << "Running property access benchmarks..." << std::endl;
        BenchmarkSuite suite(m_stage);
        auto results = suite.RunPropertyAccessBenchmarks(config);
        
        std::cout << "Property access benchmarks complete." << std::endl;
    }
    
    /**
     * Run type checking benchmarks
     * 
     * @param outputFile Path to output results file
     */
    void RunTypeCheckingBenchmarks(const std::string& outputFile) {
        if (!m_stage) return;
        
        BenchmarkConfig config;
        config.iterations = 20;
        config.warmupRuns = 3;
        config.benchmarkType = BenchmarkType::TypeChecking;
        config.outputFile = outputFile;
        
        std::cout << "Running type checking benchmarks..." << std::endl;
        BenchmarkSuite suite(m_stage);
        auto results = suite.RunTypeCheckingBenchmarks(config);
        
        std::cout << "Type checking benchmarks complete." << std::endl;
    }
    
    /**
     * Run all benchmarks with default configuration
     * 
     * @param outputFile Path to output results file
     */
    void RunAllBenchmarks(const std::string& outputFile) {
        if (!m_stage) return;
        
        BenchmarkConfig config;
        config.iterations = 10;
        config.warmupRuns = 2;
        config.outputFile = outputFile;
        
        std::cout << "Running all benchmarks..." << std::endl;
        BenchmarkSuite suite(m_stage);
        auto results = suite.RunAllBenchmarks(config);
        
        std::cout << "All benchmarks complete." << std::endl;
    }
    
    /**
     * Run comparison between two sets of benchmark results
     * 
     * @param baselineFile Path to baseline results file
     * @param currentFile Path to current results file
     * @param outputFile Path to output comparison file
     */
    static void RunComparison(const std::string& baselineFile, const std::string& currentFile, 
                             const std::string& outputFile) {
        // Note: In a real implementation, this would parse the benchmark results from files
        // This is a simplified placeholder
        std::cout << "Running comparison between " << baselineFile << " and " << currentFile << "..." << std::endl;
        
        // Placeholder benchmark results
        std::unordered_map<std::string, BenchmarkResult> baseline;
        std::unordered_map<std::string, BenchmarkResult> current;
        
        BenchmarkSuite::GenerateComparisonReport(baseline, current, outputFile);
        
        std::cout << "Comparison complete." << std::endl;
    }
    
private:
    UsdStageRefPtr m_stage;
};

/**
 * Entry point for the schema benchmark suite
 */
int main(int argc, char* argv[]) {
    // Parse command line arguments
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <usd_file> [output_file] [benchmark_type]" << std::endl;
        std::cerr << "  benchmark_type: all, property, type, traversal, composition" << std::endl;
        return 1;
    }
    
    std::string usdFile = argv[1];
    std::string outputFile = (argc >= 3) ? argv[2] : "benchmark_results.txt";
    std::string benchmarkType = (argc >= 4) ? argv[3] : "all";
    
    // Create and run benchmark suite
    try {
        SchemaBenchmarkSuiteRunner runner(usdFile);
        
        if (benchmarkType == "all") {
            runner.RunAllBenchmarks(outputFile);
        } else if (benchmarkType == "property") {
            runner.RunPropertyAccessBenchmarks(outputFile);
        } else if (benchmarkType == "type") {
            runner.RunTypeCheckingBenchmarks(outputFile);
        } else if (benchmarkType == "comparison" && argc >= 5) {
            // Special case for comparison mode
            std::string baselineFile = argv[2];
            std::string currentFile = argv[3];
            std::string comparisonFile = argv[4];
            SchemaBenchmarkSuiteRunner::RunComparison(baselineFile, currentFile, comparisonFile);
        } else {
            std::cerr << "Unknown benchmark type: " << benchmarkType << std::endl;
            return 1;
        }
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
