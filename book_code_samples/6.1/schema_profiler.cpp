/**
 * schema_profiler.cpp
 * 
 * A performance profiler for USD schema operations in game engines.
 * This tool helps identify performance bottlenecks in schema access patterns.
 */

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/tf/type.h>

#include <chrono>
#include <unordered_map>
#include <string>
#include <vector>
#include <algorithm>
#include <numeric>
#include <iostream>
#include <iomanip>

PXR_NAMESPACE_USING_DIRECTIVE

/**
 * SchemaProfilingCategory
 * 
 * Enum defining the different categories of schema operations to profile.
 */
enum class SchemaProfilingCategory {
    TypeChecking,       // IsA and HasAPI checks
    PropertyAccess,     // Getting property values
    PropertySetting,    // Setting property values
    Traversal,          // Stage traversal operations
    Composition,        // Composition operations
};

/**
 * SchemaProfiler
 * 
 * A utility class for profiling USD schema operations in a game engine context.
 */
class SchemaProfiler {
public:
    /**
     * Constructor
     */
    SchemaProfiler() {
        // Reserve space to avoid reallocations
        for (int i = 0; i < static_cast<int>(SchemaProfilingCategory::Composition) + 1; ++i) {
            m_timings[static_cast<SchemaProfilingCategory>(i)].reserve(1000);
        }
    }

    /**
     * Start profiling an operation
     */
    void startOperation() {
        m_startTime = std::chrono::high_resolution_clock::now();
    }

    /**
     * End profiling an operation
     * 
     * @param category The category of operation being profiled
     * @param operationName Optional name for the specific operation
     */
    void endOperation(SchemaProfilingCategory category, const std::string& operationName = "") {
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - m_startTime).count();
        
        // Store timing data
        m_timings[category].push_back(duration);
        
        // Store named operation if provided
        if (!operationName.empty()) {
            m_namedTimings[operationName].push_back(duration);
        }
    }

    /**
     * Get a summary of profiling results
     * 
     * @return A formatted string with profiling statistics
     */
    std::string getSummary() const {
        std::stringstream ss;
        
        ss << "===== Schema Profiling Summary =====" << std::endl;
        
        // Report category summaries
        ss << "----- Category Summaries -----" << std::endl;
        reportCategorySummary(ss, SchemaProfilingCategory::TypeChecking, "Type Checking");
        reportCategorySummary(ss, SchemaProfilingCategory::PropertyAccess, "Property Access");
        reportCategorySummary(ss, SchemaProfilingCategory::PropertySetting, "Property Setting");
        reportCategorySummary(ss, SchemaProfilingCategory::Traversal, "Traversal");
        reportCategorySummary(ss, SchemaProfilingCategory::Composition, "Composition");
        
        // Report named operation summaries
        if (!m_namedTimings.empty()) {
            ss << std::endl << "----- Named Operations -----" << std::endl;
            
            // Sort operations by average time (descending)
            std::vector<std::pair<std::string, double>> sortedOps;
            for (const auto& pair : m_namedTimings) {
                double average = calculateAverage(pair.second);
                sortedOps.push_back(std::make_pair(pair.first, average));
            }
            
            std::sort(sortedOps.begin(), sortedOps.end(), 
                    [](const auto& a, const auto& b) { return a.second > b.second; });
            
            // Report top operations
            for (const auto& op : sortedOps) {
                const auto& samples = m_namedTimings.at(op.first);
                ss << std::left << std::setw(30) << op.first << ": ";
                ss << "Avg: " << std::fixed << std::setprecision(2) << (op.second / 1000.0) << " µs, ";
                ss << "Count: " << samples.size() << ", ";
                ss << "Min: " << std::fixed << std::setprecision(2) 
                   << (*std::min_element(samples.begin(), samples.end()) / 1000.0) << " µs, ";
                ss << "Max: " << std::fixed << std::setprecision(2) 
                   << (*std::max_element(samples.begin(), samples.end()) / 1000.0) << " µs" << std::endl;
            }
        }
        
        return ss.str();
    }

    /**
     * Reset all profiling data
     */
    void reset() {
        for (auto& pair : m_timings) {
            pair.second.clear();
        }
        m_namedTimings.clear();
    }

    /**
     * Profile type checking for a prim
     * 
     * @param prim The prim to check
     * @param typeToCheck The type to check against
     * @return Whether the prim is of the specified type
     */
    bool profileIsA(const UsdPrim& prim, const TfType& typeToCheck) {
        startOperation();
        bool result = prim.IsA(typeToCheck);
        endOperation(SchemaProfilingCategory::TypeChecking, "IsA");
        return result;
    }

    /**
     * Profile API schema checking for a prim
     * 
     * @param prim The prim to check
     * @param apiSchemaType The API schema type to check for
     * @return Whether the prim has the specified API schema
     */
    bool profileHasAPI(const UsdPrim& prim, const TfType& apiSchemaType) {
        startOperation();
        bool result = prim.HasAPI(apiSchemaType);
        endOperation(SchemaProfilingCategory::TypeChecking, "HasAPI");
        return result;
    }

    /**
     * Profile getting an attribute from a prim
     * 
     * @param prim The prim to get the attribute from
     * @param attrName The name of the attribute
     * @return The attribute
     */
    UsdAttribute profileGetAttribute(const UsdPrim& prim, const TfToken& attrName) {
        startOperation();
        UsdAttribute attr = prim.GetAttribute(attrName);
        endOperation(SchemaProfilingCategory::PropertyAccess, "GetAttribute");
        return attr;
    }

    /**
     * Profile getting a float value from an attribute
     * 
     * @param attr The attribute to get the value from
     * @param value Output parameter for the value
     * @return Whether the value was successfully retrieved
     */
    bool profileGetFloat(const UsdAttribute& attr, float* value) {
        startOperation();
        bool result = attr.Get(value);
        endOperation(SchemaProfilingCategory::PropertyAccess, "GetFloat");
        return result;
    }
    
    /**
     * Profile setting a float value on an attribute
     * 
     * @param attr The attribute to set the value on
     * @param value The value to set
     * @return Whether the value was successfully set
     */
    bool profileSetFloat(UsdAttribute& attr, float value) {
        startOperation();
        bool result = attr.Set(value);
        endOperation(SchemaProfilingCategory::PropertySetting, "SetFloat");
        return result;
    }
    
    /**
     * Profile stage traversal
     * 
     * @param stage The stage to traverse
     * @return Number of prims visited
     */
    int profileTraverseStage(const UsdStageRefPtr& stage) {
        startOperation();
        int count = 0;
        for (const UsdPrim& prim : stage->TraverseAll()) {
            count++;
        }
        endOperation(SchemaProfilingCategory::Traversal, "TraverseAll");
        return count;
    }

private:
    // Start time for the current operation
    std::chrono::time_point<std::chrono::high_resolution_clock> m_startTime;
    
    // Timings grouped by category (in nanoseconds)
    std::unordered_map<SchemaProfilingCategory, std::vector<int64_t>> m_timings;
    
    // Timings for named operations (in nanoseconds)
    std::unordered_map<std::string, std::vector<int64_t>> m_namedTimings;
    
    /**
     * Calculate the average of a vector of timing samples
     * 
     * @param samples The timing samples
     * @return The average time
     */
    static double calculateAverage(const std::vector<int64_t>& samples) {
        if (samples.empty()) return 0.0;
        return static_cast<double>(std::accumulate(samples.begin(), samples.end(), 0.0)) / samples.size();
    }
    
    /**
     * Report summary statistics for a profiling category
     * 
     * @param ss The stringstream to write to
     * @param category The category to report
     * @param categoryName The human-readable name of the category
     */
    void reportCategorySummary(std::stringstream& ss, SchemaProfilingCategory category, 
                              const std::string& categoryName) const {
        const auto& samples = m_timings.at(category);
        
        if (samples.empty()) {
            ss << categoryName << ": No samples" << std::endl;
            return;
        }
        
        double average = calculateAverage(samples);
        double total = std::accumulate(samples.begin(), samples.end(), 0.0);
        auto minMax = std::minmax_element(samples.begin(), samples.end());
        
        ss << std::left << std::setw(20) << categoryName << ": ";
        ss << "Avg: " << std::fixed << std::setprecision(2) << (average / 1000.0) << " µs, ";
        ss << "Count: " << samples.size() << ", ";
        ss << "Total: " << std::fixed << std::setprecision(2) << (total / 1000000.0) << " ms, ";
        ss << "Min: " << std::fixed << std::setprecision(2) << (*minMax.first / 1000.0) << " µs, ";
        ss << "Max: " << std::fixed << std::setprecision(2) << (*minMax.second / 1000.0) << " µs" << std::endl;
    }
};

/**
 * Example usage of the schema profiler
 */
void demonstrateProfiler(const UsdStageRefPtr& stage) {
    // Create profiler instance
    SchemaProfiler profiler;
    
    // Example: Profile traversal and type checking
    std::cout << "Profiling stage traversal..." << std::endl;
    profiler.profileTraverseStage(stage);
    
    // Find all prims with a specific schema type
    std::cout << "Profiling type checking..." << std::endl;
    TfType myEntityType = TfType::FindByName("SparkleGameEntity");
    std::vector<UsdPrim> gameEntities;
    
    for (const UsdPrim& prim : stage->TraverseAll()) {
        if (profiler.profileIsA(prim, myEntityType)) {
            gameEntities.push_back(prim);
        }
    }
    
    // Profile property access
    std::cout << "Profiling property access..." << std::endl;
    TfToken healthAttrName("sparkle:health:current");
    
    for (const UsdPrim& prim : gameEntities) {
        UsdAttribute healthAttr = profiler.profileGetAttribute(prim, healthAttrName);
        if (healthAttr) {
            float health = 0.0f;
            profiler.profileGetFloat(healthAttr, &health);
        }
    }
    
    // Print profiling results
    std::cout << profiler.getSummary() << std::endl;
}

/**
 * Example performance optimization based on profiling results
 */
void optimizeSchemaAccess(const UsdStageRefPtr& stage) {
    SchemaProfiler profiler;
    
    // First attempt: Standard property access
    std::cout << "Before optimization:" << std::endl;
    
    profiler.startOperation();
    float totalHealth = 0.0f;
    int entityCount = 0;
    
    TfToken healthToken("sparkle:health:current");
    TfType entityType = TfType::FindByName("SparkleGameEntity");
    
    for (const UsdPrim& prim : stage->TraverseAll()) {
        if (prim.IsA(entityType)) {
            UsdAttribute healthAttr = prim.GetAttribute(healthToken);
            if (healthAttr) {
                float health = 0.0f;
                healthAttr.Get(&health);
                totalHealth += health;
                entityCount++;
            }
        }
    }
    
    profiler.endOperation(SchemaProfilingCategory::PropertyAccess, "Unoptimized");
    
    // Second attempt: Optimized access with token caching and fewer type checks
    profiler.startOperation();
    float optimizedTotal = 0.0f;
    int optimizedCount = 0;
    
    // Pre-cache tokens
    static const TfToken cachedHealthToken("sparkle:health:current");
    
    // Only get entities once
    std::vector<UsdPrim> gameEntities;
    for (const UsdPrim& prim : stage->TraverseAll()) {
        if (prim.IsA(entityType)) {
            gameEntities.push_back(prim);
        }
    }
    
    // Access properties with cached token
    for (const UsdPrim& prim : gameEntities) {
        UsdAttribute healthAttr = prim.GetAttribute(cachedHealthToken);
        if (healthAttr) {
            float health = 0.0f;
            healthAttr.Get(&health);
            optimizedTotal += health;
            optimizedCount++;
        }
    }
    
    profiler.endOperation(SchemaProfilingCategory::PropertyAccess, "Optimized");
    
    // Compare results
    std::cout << profiler.getSummary() << std::endl;
    std::cout << "Unoptimized: " << entityCount << " entities, total health: " << totalHealth << std::endl;
    std::cout << "Optimized: " << optimizedCount << " entities, total health: " << optimizedTotal << std::endl;
}

/**
 * Main entry point for the schema profiler example
 */
int main(int argc, char* argv[]) {
    // Check if a USD file was provided
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <usd_file>" << std::endl;
        return 1;
    }
    
    // Open the USD stage
    std::string usdFile = argv[1];
    UsdStageRefPtr stage = UsdStage::Open(usdFile);
    
    if (!stage) {
        std::cerr << "Failed to open USD stage: " << usdFile << std::endl;
        return 1;
    }
    
    // Run profiling demonstration
    demonstrateProfiler(stage);
    
    // Run optimization example
    optimizeSchemaAccess(stage);
    
    return 0;
}
