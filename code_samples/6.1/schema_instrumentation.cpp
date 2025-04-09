/**
 * schema_instrumentation.cpp
 * 
 * Referenced in Chapter 6.1: Schema Resolution and Performance Analysis
 * 
 * This library demonstrates how to instrument schema code for detailed performance
 * analysis. It provides tools for adding lightweight performance instrumentation
 * to schema operations, enabling developers to precisely measure timing and
 * resource usage in different contexts.
 */

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/tf/type.h>
#include <pxr/base/tf/diagnostic.h>
#include <pxr/base/tf/stopwatch.h>
#include <pxr/base/tf/envSetting.h>

#include <chrono>
#include <string>
#include <vector>
#include <unordered_map>
#include <stack>
#include <fstream>
#include <mutex>
#include <thread>
#include <atomic>
#include <iomanip>
#include <iostream>
#include <functional>

PXR_NAMESPACE_USING_DIRECTIVE

// Environment settings to enable/disable instrumentation
TF_DEFINE_ENV_SETTING(ENABLE_SCHEMA_INSTRUMENTATION, false, 
                    "Enable schema performance instrumentation");
TF_DEFINE_ENV_SETTING(SCHEMA_INSTRUMENTATION_LOG_FILE, "",
                    "Log file for schema instrumentation data");
TF_DEFINE_ENV_SETTING(SCHEMA_INSTRUMENTATION_THRESHOLD_US, 100,
                    "Threshold in microseconds for logging schema operations");
TF_DEFINE_ENV_SETTING(SCHEMA_INSTRUMENTATION_SAMPLING_RATE, 1.0,
                    "Sampling rate for instrumentation (0.0-1.0)");

/**
 * SchemaInstrumentationCategory
 * 
 * Categories for instrumented schema operations
 */
enum class SchemaInstrumentationCategory {
    PropertyAccess,          // Property get operations
    PropertySetting,         // Property set operations
    TypeChecking,            // IsA and HasAPI checks
    Traversal,               // Stage traversal operations
    APISchemaApplication,    // API schema application
    Schema_Creation,         // Schema object creation
    Composition,             // Composition operations
    Other                    // Miscellaneous operations
};

/**
 * SchemaInstrumentationRecord
 * 
 * Records performance data for a single instrumented operation
 */
struct SchemaInstrumentationRecord {
    std::string operation;                   // Name of the operation
    SchemaInstrumentationCategory category;  // Category of the operation
    std::string primPath;                    // Path of the prim being operated on
    std::string propertyPath;                // Path of the property (if applicable)
    std::chrono::nanoseconds duration;       // Duration of the operation
    std::thread::id threadId;                // Thread ID the operation ran on
    int64_t timestamp;                       // Timestamp when operation started
    std::string stackTrace;                  // Optional stack trace
    
    SchemaInstrumentationRecord() 
        : category(SchemaInstrumentationCategory::Other)
        , duration(std::chrono::nanoseconds(0))
        , threadId(std::this_thread::get_id())
        , timestamp(std::chrono::duration_cast<std::chrono::microseconds>(
              std::chrono::steady_clock::now().time_since_epoch()).count()) 
    {}
};

/**
 * SchemaInstrumentationManager
 * 
 * Singleton class that manages schema performance instrumentation
 */
class SchemaInstrumentationManager {
public:
    /**
     * Get singleton instance
     */
    static SchemaInstrumentationManager& GetInstance() {
        static SchemaInstrumentationManager instance;
        return instance;
    }
    
    /**
     * Check if instrumentation is enabled
     */
    bool IsEnabled() const {
        return m_enabled;
    }
    
    /**
     * Start timing an operation
     * 
     * @param operation Name of the operation
     * @param category Category of the operation
     * @param primPath Path of the prim being operated on
     * @param propertyPath Path of the property (if applicable)
     * @return A handle to be passed to EndOperation
     */
    int64_t StartOperation(const std::string& operation,
                         SchemaInstrumentationCategory category,
                         const std::string& primPath = "",
                         const std::string& propertyPath = "") {
        // Quick check if instrumentation is disabled
        if (!m_enabled) {
            return -1;
        }
        
        // Check sampling rate
        if (m_samplingRate < 1.0) {
            static std::random_device rd;
            static std::mt19937 gen(rd());
            static std::uniform_real_distribution<> dis(0.0, 1.0);
            
            if (dis(gen) > m_samplingRate) {
                return -1;  // Skip this operation based on sampling
            }
        }
        
        // Create a new operation record
        SchemaInstrumentationRecord record;
        record.operation = operation;
        record.category = category;
        record.primPath = primPath;
        record.propertyPath = propertyPath;
        record.timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        
        // Record stack trace if enabled
        if (m_captureStackTraces) {
            record.stackTrace = GetStackTrace();
        }
        
        // Store the record and return its ID
        std::lock_guard<std::mutex> lock(m_mutex);
        int64_t id = m_nextOperationId++;
        m_pendingOperations[id] = record;
        return id;
    }
    
    /**
     * End timing an operation
     * 
     * @param operationId Handle returned by StartOperation
     */
    void EndOperation(int64_t operationId) {
        if (operationId < 0 || !m_enabled) {
            return;  // Invalid ID or instrumentation disabled
        }
        
        auto endTime = std::chrono::steady_clock::now();
        auto endTimestamp = std::chrono::duration_cast<std::chrono::microseconds>(
            endTime.time_since_epoch()).count();
        
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_pendingOperations.find(operationId);
        if (it == m_pendingOperations.end()) {
            return;  // Operation not found
        }
        
        // Calculate duration
        SchemaInstrumentationRecord& record = it->second;
        int64_t startTimestamp = record.timestamp;
        int64_t durationUs = endTimestamp - startTimestamp;
        record.duration = std::chrono::nanoseconds(durationUs * 1000);
        
        // Only log operations that exceed the threshold
        if (durationUs >= m_thresholdUs) {
            m_completedOperations.push_back(record);
            
            // Log to file if enabled
            if (m_logFile.is_open()) {
                WriteRecordToLog(record);
            }
            
            // Log to console if real-time logging is enabled
            if (m_realtimeLogging) {
                LogRecordToConsole(record);
            }
        }
        
        // Remove from pending operations
        m_pendingOperations.erase(it);
        
        // Flush log periodically
        if (m_logFile.is_open() && m_completedOperations.size() % 1000 == 0) {
            m_logFile.flush();
        }
    }
    
    /**
     * Get all completed operation records
     */
    std::vector<SchemaInstrumentationRecord> GetCompletedOperations() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_completedOperations;
    }
    
    /**
     * Clear all operation records
     */
    void ClearOperations() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pendingOperations.clear();
        m_completedOperations.clear();
    }
    
    /**
     * Set real-time logging to console
     */
    void SetRealtimeLogging(bool enabled) {
        m_realtimeLogging = enabled;
    }
    
    /**
     * Set stack trace capture
     */
    void SetStackTraceCapture(bool enabled) {
        m_captureStackTraces = enabled;
    }
    
    /**
     * Get category name
     */
    static std::string GetCategoryName(SchemaInstrumentationCategory category) {
        switch (category) {
            case SchemaInstrumentationCategory::PropertyAccess:
                return "PropertyAccess";
            case SchemaInstrumentationCategory::PropertySetting:
                return "PropertySetting";
            case SchemaInstrumentationCategory::TypeChecking:
                return "TypeChecking";
            case SchemaInstrumentationCategory::Traversal:
                return "Traversal";
            case SchemaInstrumentationCategory::APISchemaApplication:
                return "APISchemaApplication";
            case SchemaInstrumentationCategory::Schema_Creation:
                return "Schema_Creation";
            case SchemaInstrumentationCategory::Composition:
                return "Composition";
            case SchemaInstrumentationCategory::Other:
            default:
                return "Other";
        }
    }
    
    /**
     * Create a report of all recorded operations
     */
    std::string GenerateReport() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (m_completedOperations.empty()) {
            return "No instrumentation data recorded.\n";
        }
        
        std::stringstream ss;
        ss << "Schema Instrumentation Report\n";
        ss << "=============================\n\n";
        
        // Group by category
        std::unordered_map<SchemaInstrumentationCategory, std::vector<const SchemaInstrumentationRecord*>> categorized;
        for (const auto& record : m_completedOperations) {
            categorized[record.category].push_back(&record);
        }
        
        // Print summary per category
        ss << "Summary by Category:\n";
        ss << "-------------------\n";
        
        for (int i = 0; i < static_cast<int>(SchemaInstrumentationCategory::Other) + 1; ++i) {
            auto category = static_cast<SchemaInstrumentationCategory>(i);
            auto it = categorized.find(category);
            
            if (it != categorized.end() && !it->second.empty()) {
                const auto& records = it->second;
                
                // Calculate total and average duration
                std::chrono::nanoseconds totalDuration(0);
                for (const auto* record : records) {
                    totalDuration += record->duration;
                }
                
                double averageDurationUs = std::chrono::duration<double, std::micro>(totalDuration).count() / records.size();
                double totalDurationMs = std::chrono::duration<double, std::milli>(totalDuration).count();
                
                ss << GetCategoryName(category) << ": "
                   << records.size() << " operations, "
                   << "total: " << std::fixed << std::setprecision(3) << totalDurationMs << " ms, "
                   << "avg: " << std::fixed << std::setprecision(3) << averageDurationUs << " μs\n";
            }
        }
        
        ss << "\nSlowest Operations by Category:\n";
        ss << "------------------------------\n";
        
        // Find and print slowest operations per category
        for (int i = 0; i < static_cast<int>(SchemaInstrumentationCategory::Other) + 1; ++i) {
            auto category = static_cast<SchemaInstrumentationCategory>(i);
            auto it = categorized.find(category);
            
            if (it != categorized.end() && !it->second.empty()) {
                const auto& records = it->second;
                
                // Sort by duration (descending)
                std::vector<const SchemaInstrumentationRecord*> sorted = records;
                std::sort(sorted.begin(), sorted.end(),
                         [](const SchemaInstrumentationRecord* a, const SchemaInstrumentationRecord* b) {
                             return a->duration > b->duration;
                         });
                
                // Print category header
                ss << "\n" << GetCategoryName(category) << ":\n";
                
                // Print top 5 slowest operations or all if fewer
                size_t count = std::min(size_t(5), sorted.size());
                for (size_t j = 0; j < count; ++j) {
                    const auto* record = sorted[j];
                    double durationUs = std::chrono::duration<double, std::micro>(record->duration).count();
                    
                    ss << "  " << (j + 1) << ". " << record->operation
                       << " (" << std::fixed << std::setprecision(3) << durationUs << " μs): ";
                    
                    if (!record->primPath.empty()) {
                        ss << "prim=" << record->primPath;
                    }
                    
                    if (!record->propertyPath.empty()) {
                        ss << ", property=" << record->propertyPath;
                    }
                    
                    ss << "\n";
                    
                    // Print stack trace if available
                    if (!record->stackTrace.empty()) {
                        ss << "     Stack trace:\n";
                        std::istringstream traceStream(record->stackTrace);
                        std::string line;
                        while (std::getline(traceStream, line)) {
                            ss << "     " << line << "\n";
                        }
                    }
                }
            }
        }
        
        return ss.str();
    }
    
    /**
     * Write report to file
     */
    bool WriteReportToFile(const std::string& filename) const {
        std::ofstream reportFile(filename);
        if (!reportFile.is_open()) {
            return false;
        }
        
        reportFile << GenerateReport();
        reportFile.close();
        return true;
    }
    
    /**
     * Destructor - close log file if open
     */
    ~SchemaInstrumentationManager() {
        if (m_logFile.is_open()) {
            m_logFile.close();
        }
    }
    
private:
    // Private constructor for singleton
    SchemaInstrumentationManager() 
        : m_enabled(TfGetEnvSetting(ENABLE_SCHEMA_INSTRUMENTATION))
        , m_nextOperationId(0)
        , m_thresholdUs(TfGetEnvSetting(SCHEMA_INSTRUMENTATION_THRESHOLD_US))
        , m_samplingRate(TfGetEnvSetting(SCHEMA_INSTRUMENTATION_SAMPLING_RATE))
        , m_realtimeLogging(false)
        , m_captureStackTraces(false)
    {
        // Open log file if specified
        std::string logFilePath = TfGetEnvSetting(SCHEMA_INSTRUMENTATION_LOG_FILE);
        if (!logFilePath.empty()) {
            m_logFile.open(logFilePath, std::ios::out | std::ios::app);
            if (m_logFile.is_open()) {
                // Write header if file is new (empty)
                if (m_logFile.tellp() == 0) {
                    m_logFile << "Timestamp,ThreadId,Category,Operation,Duration,PrimPath,PropertyPath\n";
                }
            } else {
                TF_WARN("Failed to open schema instrumentation log file: %s", logFilePath.c_str());
            }
        }
        
        // Clamp sampling rate to valid range
        m_samplingRate = std::max(0.0, std::min(1.0, m_samplingRate));
    }
    
    /**
     * Get a simplified stack trace
     */
    std::string GetStackTrace() const {
        // This is a placeholder. In a real implementation, this would use
        // platform-specific APIs to get a stack trace.
        // For example, on Linux you might use backtrace() and backtrace_symbols().
        return "Stack trace not available in this sample";
    }
    
    /**
     * Write a record to the log file
     */
    void WriteRecordToLog(const SchemaInstrumentationRecord& record) {
        if (!m_logFile.is_open()) {
            return;
        }
        
        double durationUs = std::chrono::duration<double, std::micro>(record.duration).count();
        
        m_logFile << record.timestamp << ","
                 << record.threadId << ","
                 << GetCategoryName(record.category) << ","
                 << record.operation << ","
                 << std::fixed << std::setprecision(3) << durationUs << ","
                 << record.primPath << ","
                 << record.propertyPath << "\n";
    }
    
    /**
     * Log a record to the console
     */
    void LogRecordToConsole(const SchemaInstrumentationRecord& record) {
        double durationUs = std::chrono::duration<double, std::micro>(record.duration).count();
        
        std::cout << "[SCHEMA_INSTR] "
                 << GetCategoryName(record.category) << " "
                 << record.operation << ": "
                 << std::fixed << std::setprecision(3) << durationUs << " μs";
        
        if (!record.primPath.empty()) {
            std::cout << " | prim=" << record.primPath;
        }
        
        if (!record.propertyPath.empty()) {
            std::cout << " | property=" << record.propertyPath;
        }
        
        std::cout << std::endl;
    }
    
    // Member variables
    bool m_enabled;
    std::atomic<int64_t> m_nextOperationId;
    int m_thresholdUs;
    double m_samplingRate;
    bool m_realtimeLogging;
    bool m_captureStackTraces;
    mutable std::mutex m_mutex;
    std::unordered_map<int64_t, SchemaInstrumentationRecord> m_pendingOperations;
    std::vector<SchemaInstrumentationRecord> m_completedOperations;
    std::ofstream m_logFile;
};

/**
 * SchemaInstrumentationScope
 * 
 * RAII helper class for timing operations
 */
class SchemaInstrumentationScope {
public:
    /**
     * Constructor - starts timing an operation
     */
    SchemaInstrumentationScope(const std::string& operation,
                              SchemaInstrumentationCategory category,
                              const std::string& primPath = "",
                              const std::string& propertyPath = "")
        : m_operationId(-1)
    {
        auto& manager = SchemaInstrumentationManager::GetInstance();
        if (manager.IsEnabled()) {
            m_operationId = manager.StartOperation(operation, category, primPath, propertyPath);
        }
    }
    
    /**
     * Destructor - ends timing the operation
     */
    ~SchemaInstrumentationScope() {
        if (m_operationId >= 0) {
            SchemaInstrumentationManager::GetInstance().EndOperation(m_operationId);
        }
    }
    
    // Prevent copying
    SchemaInstrumentationScope(const SchemaInstrumentationScope&) = delete;
    SchemaInstrumentationScope& operator=(const SchemaInstrumentationScope&) = delete;
    
private:
    int64_t m_operationId;
};

/**
 * Instrumented versions of common USD schema operations
 */
namespace SchemaInstrumentation {

/**
 * Instrumented version of UsdPrim::IsA
 */
template<typename SchemaType>
bool IsA(const UsdPrim& prim) {
    SchemaInstrumentationScope scope("IsA",
                                   SchemaInstrumentationCategory::TypeChecking,
                                   prim.GetPath().GetString());
    return prim.IsA<SchemaType>();
}

/**
 * Instrumented version of UsdPrim::HasAPI
 */
template<typename APISchemaType>
bool HasAPI(const UsdPrim& prim) {
    SchemaInstrumentationScope scope("HasAPI",
                                   SchemaInstrumentationCategory::TypeChecking,
                                   prim.GetPath().GetString());
    return prim.HasAPI<APISchemaType>();
}

/**
 * Instrumented version of UsdPrim::GetAttribute
 */
UsdAttribute GetAttribute(const UsdPrim& prim, const TfToken& attrName) {
    SchemaInstrumentationScope scope("GetAttribute",
                                   SchemaInstrumentationCategory::PropertyAccess,
                                   prim.GetPath().GetString(),
                                   attrName.GetString());
    return prim.GetAttribute(attrName);
}

/**
 * Instrumented version of UsdAttribute::Get
 */
template<typename T>
bool GetAttributeValue(const UsdAttribute& attr, T* value) {
    UsdPrim prim = attr.GetPrim();
    SchemaInstrumentationScope scope("Get",
                                   SchemaInstrumentationCategory::PropertyAccess,
                                   prim.GetPath().GetString(),
                                   attr.GetPath().GetString());
    return attr.Get(value);
}

/**
 * Instrumented version of UsdAttribute::Set
 */
template<typename T>
bool SetAttributeValue(UsdAttribute& attr, const T& value) {
    UsdPrim prim = attr.GetPrim();
    SchemaInstrumentationScope scope("Set",
                                   SchemaInstrumentationCategory::PropertySetting,
                                   prim.GetPath().GetString(),
                                   attr.GetPath().GetString());
    return attr.Set(value);
}

/**
 * Instrumented traversal of a USD stage
 */
template<typename Func>
void TraverseStage(const UsdStageRefPtr& stage, Func callback) {
    SchemaInstrumentationScope scope("TraverseStage",
                                   SchemaInstrumentationCategory::Traversal);
    for (const UsdPrim& prim : stage->TraverseAll()) {
        callback(prim);
    }
}

/**
 * Instrumented API schema application
 */
template<typename APISchemaType>
bool ApplyAPISchema(UsdPrim& prim) {
    SchemaInstrumentationScope scope("ApplyAPI",
                                   SchemaInstrumentationCategory::APISchemaApplication,
                                   prim.GetPath().GetString());
    return prim.ApplyAPI<APISchemaType>();
}

/**
 * Instrumented schema creation
 */
template<typename SchemaType>
SchemaType CreateSchema(const UsdPrim& prim) {
    SchemaInstrumentationScope scope("CreateSchema",
                                   SchemaInstrumentationCategory::Schema_Creation,
                                   prim.GetPath().GetString());
    return SchemaType(prim);
}

} // namespace SchemaInstrumentation

/**
 * Example usage of schema instrumentation
 */
void InstrumentationExample(const UsdStageRefPtr& stage) {
    // Enable instrumentation
    setenv("ENABLE_SCHEMA_INSTRUMENTATION", "true", 1);
    
    // Configure real-time logging
    auto& manager = SchemaInstrumentationManager::GetInstance();
    manager.SetRealtimeLogging(true);
    
    std::cout << "Running instrumentation example..." << std::endl;
    
    // Example 1: Instrumented traversal counting entities
    int entityCount = 0;
    TfType sparkleEntityType = TfType::FindByName("SparkleGameEntity");
    
    SchemaInstrumentation::TraverseStage(stage, [&](const UsdPrim& prim) {
        if (SchemaInstrumentation::IsA<TfType>(prim)) {
            entityCount++;
        }
    });
    
    std::cout << "Found " << entityCount << " game entities" << std::endl;
    
    // Example 2: Instrumented property access
    TfToken healthToken("sparkle:health:current");
    float totalHealth = 0.0f;
    
    SchemaInstrumentation::TraverseStage(stage, [&](const UsdPrim& prim) {
        UsdAttribute attr = SchemaInstrumentation::GetAttribute(prim, healthToken);
        if (attr) {
            float health = 0.0f;
            if (SchemaInstrumentation::GetAttributeValue(attr, &health)) {
                totalHealth += health;
            }
        }
    });
    
    std::cout << "Total health across all entities: " << totalHealth << std::endl;
    
    // Example 3: Instrumented API schema application
    SchemaInstrumentation::TraverseStage(stage, [&](const UsdPrim& prim) {
        // Check if entity is a player
        if (prim.GetName().GetString().find("Player") != std::string::npos) {
            // Apply health API if not already present
            TfType healthAPI = TfType::FindByName("SparkleHealthAPI");
            if (healthAPI && !SchemaInstrumentation::HasAPI<TfType>(prim)) {
                UsdPrim mutablePrim = const_cast<UsdPrim&>(prim);
                SchemaInstrumentation::ApplyAPISchema<TfType>(mutablePrim);
                
                // Set default health values
                UsdAttribute healthAttr = SchemaInstrumentation::GetAttribute(mutablePrim, healthToken);
                if (healthAttr) {
                    SchemaInstrumentation::SetAttributeValue(healthAttr, 100.0f);
                }
            }
        }
    });
    
    // Generate and print report
    std::cout << "\nInstrumentation Report:\n" << manager.GenerateReport() << std::endl;
    
    // Write report to file
    manager.WriteReportToFile("schema_instrumentation_report.txt");
    
    // Clear instrumentation data
    manager.ClearOperations();
}

/**
 * Advanced instrumentation example showing deeper analysis
 */
void AdvancedInstrumentationExample(const UsdStageRefPtr& stage) {
    // Enable instrumentation with stack trace capture
    setenv("ENABLE_SCHEMA_INSTRUMENTATION", "true", 1);
    
    auto& manager = SchemaInstrumentationManager::GetInstance();
    manager.SetStackTraceCapture(true);
    
    std::cout << "Running advanced instrumentation example..." << std::endl;
    
    // Simulate different workloads for analysis
    
    // Example 1: Heavy property access pattern
    {
        SchemaInstrumentationScope scope("HeavyPropertyAccess",
                                       SchemaInstrumentationCategory::Other);
        
        // Collect attributes to access
        std::vector<UsdAttribute> healthAttrs;
        std::vector<UsdAttribute> damageAttrs;
        
        for (const UsdPrim& prim : stage->TraverseAll()) {
            if (UsdAttribute attr = prim.GetAttribute(TfToken("sparkle:health:current"))) {
                healthAttrs.push_back(attr);
            }
            if (UsdAttribute attr = prim.GetAttribute(TfToken("sparkle:combat:damage"))) {
                damageAttrs.push_back(attr);
            }
        }
        
        // Perform repeated accesses to simulate game loop
        for (int i = 0; i < 10; ++i) {
            float totalHealth = 0.0f;
            float totalDamage = 0.0f;
            
            for (const UsdAttribute& attr : healthAttrs) {
                float value = 0.0f;
                SchemaInstrumentation::GetAttributeValue(attr, &value);
                totalHealth += value;
            }
            
            for (const UsdAttribute& attr : damageAttrs) {
                float value = 0.0f;
                SchemaInstrumentation::GetAttributeValue(attr, &value);
                totalDamage += value;
            }
        }
    }
    
    // Example 2: Heavy type checking pattern
    {
        SchemaInstrumentationScope scope("HeavyTypeChecking",
                                       SchemaInstrumentationCategory::Other);
        
        std::vector<UsdPrim> allPrims;
        for (const UsdPrim& prim : stage->TraverseAll()) {
            allPrims.push_back(prim);
        }
        
        // Create a list of types to check
        std::vector<TfType> typesToCheck = {
            TfType::FindByName("SparkleGameEntity"),
            TfType::FindByName("SparkleEnemyCarrot"),
            TfType::FindByName("SparklePlayer"),
            TfType::FindByName("SparklePickup")
        };
        
        // Perform repeated type checks to simulate game queries
        for (int i = 0; i < 5; ++i) {
            for (const TfType& type : typesToCheck) {
                if (!type) continue;
                
                for (const UsdPrim& prim : allPrims) {
                    SchemaInstrumentation::IsA<TfType>(prim);
                }
            }
        }
    }
    
    // Generate and write detailed report
    manager.WriteReportToFile("advanced_instrumentation_report.txt");
    std::cout << "Advanced instrumentation report written to 'advanced_instrumentation_report.txt'" << std::endl;
    
    // Clear instrumentation data
    manager.ClearOperations();
}

/**
 * Multi-threaded instrumentation example
 */
void ThreadedInstrumentationExample(const UsdStageRefPtr& stage) {
    // Enable instrumentation
    setenv("ENABLE_SCHEMA_INSTRUMENTATION", "true", 1);
    
    auto& manager = SchemaInstrumentationManager::GetInstance();
    
    std::cout << "Running multi-threaded instrumentation example..." << std::endl;
    
    // Collect all prims for testing
    std::vector<UsdPrim> allPrims;
    for (const UsdPrim& prim : stage->TraverseAll()) {
        if (prim.IsValid()) {
            allPrims.push_back(prim);
        }
    }
    
    if (allPrims.empty()) {
        std::cout << "No prims found in stage for threading test" << std::endl;
        return;
    }
    
    // Define a worker function
    auto workerFunc = [&allPrims](int threadId, int primStart, int primCount) {
        // Thread-specific processing
        TfToken healthToken("sparkle:health:current");
        TfToken damageToken("sparkle:combat:damage");
        TfType entityType = TfType::FindByName("SparkleGameEntity");
        
        for (int i = 0; i < primCount; ++i) {
            int primIndex = (primStart + i) % allPrims.size();
            const UsdPrim& prim = allPrims[primIndex];
            
            // Type checking
            {
                SchemaInstrumentationScope scope("ThreadedIsA",
                                               SchemaInstrumentationCategory::TypeChecking,
                                               prim.GetPath().GetString());
                bool isEntity = prim.IsA(entityType);
            }
            
            // Property access
            {
                SchemaInstrumentationScope scope("ThreadedGetAttribute",
                                               SchemaInstrumentationCategory::PropertyAccess,
                                               prim.GetPath().GetString(),
                                               healthToken.GetString());
                UsdAttribute attr = prim.GetAttribute(healthToken);
                if (attr) {
                    float value = 0.0f;
                    attr.Get(&value);
                }
            }
        }
    };
    
    // Create and run worker threads
    const int numThreads = 4;
    std::vector<std::thread> threads;
    
    int primsPerThread = std::max(1, static_cast<int>(allPrims.size()) / numThreads);
    
    for (int t = 0; t < numThreads; ++t) {
        int primStart = t * primsPerThread;
        threads.push_back(std::thread(workerFunc, t, primStart, primsPerThread));
    }
    
    // Wait for all threads to finish
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Generate and write report
    manager.WriteReportToFile("threaded_instrumentation_report.txt");
    std::cout << "Threaded instrumentation report written to 'threaded_instrumentation_report.txt'" << std::endl;
    
    // Clear instrumentation data
    manager.ClearOperations();
}

/**
 * Entry point for schema instrumentation examples
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
    
    // Run instrumentation examples
    InstrumentationExample(stage);
    AdvancedInstrumentationExample(stage);
    ThreadedInstrumentationExample(stage);
    
    return 0;
}
