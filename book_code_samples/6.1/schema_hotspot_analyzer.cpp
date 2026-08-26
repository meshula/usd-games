/**
 * schema_hotspot_analyzer.cpp
 * 
 * Referenced in Chapter 6.1: Schema Resolution and Performance Analysis
 * 
 * This tool identifies performance hotspots in USD schema usage, helping
 * developers focus optimization efforts on the most impactful areas.
 * It analyzes schema access patterns, identifies expensive operations,
 * and recommends specific optimizations based on observed usage.
 */

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/tf/type.h>
#include <pxr/base/tf/diagnostic.h>
#include <pxr/base/tf/hashmap.h>

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

PXR_NAMESPACE_USING_DIRECTIVE

/**
 * HotspotType
 * 
 * Enum defining different types of schema access hotspots
 */
enum class HotspotType {
    RepeatedStringLookup,       // Multiple lookups using the same string
    UncachedSchemaTraversal,    // Schema traversal without caching
    ExcessiveTypeChecking,      // Repeated type checking of the same prim
    DeepCompositionChain,       // Deep composition arc chains
    InefficientAccessPattern,   // Non-optimal property access patterns
    ComplexTypeInheritance,     // Complex schema inheritance hierarchy
    HighSchemaCardinality,      // Many schemas applied to a single prim
    LargePropertyCount,         // Prims with many properties
    ExpensiveDataTypes,         // Use of expensive data types
    IndirectRelationships       // Relationship resolution requiring traversal
};

/**
 * HotspotInfo
 * 
 * Struct to store information about a detected hotspot
 */
struct HotspotInfo {
    HotspotType type;
    Sdf::Path primPath;
    std::string description;
    double impactScore;         // Relative impact on performance (0-100)
    std::string optimizationSuggestion;
    
    HotspotInfo(HotspotType t, const Sdf::Path& p, const std::string& d, 
                double i, const std::string& opt) 
        : type(t), primPath(p), description(d), impactScore(i), 
          optimizationSuggestion(opt) {}
    
    // For sorting by impact score
    bool operator<(const HotspotInfo& other) const {
        return impactScore > other.impactScore;  // Sort in descending order
    }
};

/**
 * AccessPattern
 * 
 * Struct to track property access patterns
 */
struct AccessPattern {
    Tf::Token propertyName;
    int accessCount;
    int uniquePrims;
    double averageTime;         // Average time in microseconds
    bool hasCache;              // Whether observed accesses use caching
    
    AccessPattern() : accessCount(0), uniquePrims(0), averageTime(0.0), hasCache(false) {}
};

/**
 * SchemaHotspotAnalyzer
 * 
 * Main class for analyzing and reporting schema performance hotspots
 */
class SchemaHotspotAnalyzer {
public:
    /**
     * Constructor
     */
    SchemaHotspotAnalyzer() {}
    
    /**
     * Analyze a USD stage for performance hotspots
     * 
     * @param stage The USD stage to analyze
     * @param forceTraversal Whether to force full traversal 
     */
    void analyzeStage(UsdStageRefPtr stage, bool forceTraversal = false) {
        m_hotspots.clear();
        m_stringLookups.clear();
        m_typeChecks.clear();
        m_primAccessCounts.clear();
        
        if (!stage) {
            TF_CODING_ERROR("Invalid stage provided to SchemaHotspotAnalyzer");
            return;
        }
        
        // Perform initial analysis
        _analyzeStageStructure(stage);
        
        // Perform schema resolution analysis if requested
        if (forceTraversal) {
            _analyzeSchemaResolution(stage);
        }
        
        // Analyze access patterns
        _analyzeAccessPatterns();
        
        // Sort hotspots by impact score
        std::sort(m_hotspots.begin(), m_hotspots.end());
    }
    
    /**
     * Get hotspots of a specific type
     * 
     * @param type The type of hotspot to get
     * @return A vector of hotspots of the specified type
     */
    std::vector<HotspotInfo> getHotspotsByType(HotspotType type) const {
        std::vector<HotspotInfo> result;
        
        for (const auto& hotspot : m_hotspots) {
            if (hotspot.type == type) {
                result.push_back(hotspot);
            }
        }
        
        return result;
    }
    
    /**
     * Get all detected hotspots
     * 
     * @return A vector of all hotspots
     */
    const std::vector<HotspotInfo>& getAllHotspots() const {
        return m_hotspots;
    }
    
    /**
     * Get top hotspots by impact score
     * 
     * @param count Maximum number of hotspots to return
     * @return A vector of the top hotspots
     */
    std::vector<HotspotInfo> getTopHotspots(size_t count) const {
        std::vector<HotspotInfo> result;
        
        size_t numToReturn = std::min(count, m_hotspots.size());
        result.insert(result.end(), m_hotspots.begin(), m_hotspots.begin() + numToReturn);
        
        return result;
    }
    
    /**
     * Generate a detailed report on the detected hotspots
     * 
     * @return A string containing the report
     */
    std::string generateReport() const {
        std::stringstream report;
        
        report << "===== Schema Hotspot Analysis Report =====" << std::endl << std::endl;
        
        // Overall statistics
        report << "Overall Statistics:" << std::endl;
        report << "-----------------" << std::endl;
        report << "Total prims analyzed: " << m_primAccessCounts.size() << std::endl;
        report << "Total hotspots detected: " << m_hotspots.size() << std::endl;
        report << std::endl;
        
        // Top hotspots
        report << "Top 10 Performance Hotspots:" << std::endl;
        report << "-------------------------" << std::endl;
        
        auto topHotspots = getTopHotspots(10);
        for (size_t i = 0; i < topHotspots.size(); ++i) {
            const auto& hotspot = topHotspots[i];
            
            report << (i + 1) << ". " << _getHotspotTypeName(hotspot.type) << std::endl;
            report << "   Path: " << hotspot.primPath << std::endl;
            report << "   Impact Score: " << std::fixed << std::setprecision(1) << hotspot.impactScore << std::endl;
            report << "   Description: " << hotspot.description << std::endl;
            report << "   Suggestion: " << hotspot.optimizationSuggestion << std::endl;
            report << std::endl;
        }
        
        // Hotspots by type
        report << "Hotspots by Type:" << std::endl;
        report << "----------------" << std::endl;
        
        std::unordered_map<HotspotType, int> hotspotCounts;
        for (const auto& hotspot : m_hotspots) {
            hotspotCounts[hotspot.type]++;
        }
        
        for (int type = static_cast<int>(HotspotType::RepeatedStringLookup);
             type <= static_cast<int>(HotspotType::IndirectRelationships);
             ++type) {
            HotspotType hotspotType = static_cast<HotspotType>(type);
            int count = hotspotCounts[hotspotType];
            
            report << _getHotspotTypeName(hotspotType) << ": " << count << std::endl;
        }
        
        report << std::endl;
        
        // General recommendations
        report << "General Recommendations:" << std::endl;
        report << "-----------------------" << std::endl;
        
        // Add recommendations based on the detected hotspots
        if (!getHotspotsByType(HotspotType::RepeatedStringLookup).empty()) {
            report << "* Implement token caching for frequently accessed properties" << std::endl;
        }
        
        if (!getHotspotsByType(HotspotType::UncachedSchemaTraversal).empty()) {
            report << "* Add caching for schema traversal results" << std::endl;
        }
        
        if (!getHotspotsByType(HotspotType::ExcessiveTypeChecking).empty()) {
            report << "* Reduce redundant type checking by caching type information" << std::endl;
        }
        
        if (!getHotspotsByType(HotspotType::InefficientAccessPattern).empty()) {
            report << "* Optimize property access patterns to reduce property lookups" << std::endl;
        }
        
        report << "* Implement component-based property caching for game entities" << std::endl;
        report << "* Consider custom schema resolution strategies for performance-critical paths" << std::endl;
        
        return report.str();
    }
    
    /**
     * Save report to a file
     * 
     * @param filePath Path to save the report
     * @return Whether the save was successful
     */
    bool saveReportToFile(const std::string& filePath) const {
        try {
            std::ofstream outFile(filePath);
            if (!outFile.is_open()) {
                return false;
            }
            
            outFile << generateReport();
            outFile.close();
            
            return true;
        } catch (std::exception& e) {
            TF_WARN("Failed to save report: %s", e.what());
            return false;
        }
    }
    
private:
    // Detected hotspots
    std::vector<HotspotInfo> m_hotspots;
    
    // String lookup tracking
    std::unordered_map<std::string, int> m_stringLookups;
    
    // Type checking tracking
    std::unordered_map<Sdf::Path, std::unordered_map<std::string, int>, SdfPath::Hash> m_typeChecks;
    
    // Prim access counts
    std::unordered_map<Sdf::Path, int, SdfPath::Hash> m_primAccessCounts;
    
    // Property access patterns
    std::unordered_map<std::string, AccessPattern> m_accessPatterns;
    
    /**
     * Analyze the structure of the USD stage
     * 
     * @param stage The USD stage to analyze
     */
    void _analyzeStageStructure(UsdStageRefPtr stage) {
        // Analyze stage structure
        for (const UsdPrim& prim : stage->TraverseAll()) {
            _analyzePrimStructure(prim);
        }
    }
    
    /**
     * Analyze the structure of a USD prim
     * 
     * @param prim The USD prim to analyze
     */
    void _analyzePrimStructure(const UsdPrim& prim) {
        // Skip invalid or abstract prims
        if (!prim.IsValid() || prim.IsAbstract()) {
            return;
        }
        
        // Analyze schema inheritance
        _analyzeSchemaInheritance(prim);
        
        // Analyze API schema count
        _analyzeApiSchemaCount(prim);
        
        // Analyze property count
        _analyzePropertyCount(prim);
        
        // Analyze data types
        _analyzeDataTypes(prim);
        
        // Analyze relationships
        _analyzeRelationships(prim);
    }
    
    /**
     * Analyze schema inheritance for a prim
     * 
     * @param prim The USD prim to analyze
     */
    void _analyzeSchemaInheritance(const UsdPrim& prim) {
        // Get inheritance depth through type hierarchy
        TfType type = prim.GetPrimTypeInfo().GetSchemaType();
        if (!type) {
            return;
        }
        
        std::vector<TfType> baseTypes;
        type.GetAllAncestorTypes(&baseTypes);
        
        // Check for deep inheritance hierarchies
        if (baseTypes.size() > 5) {  // Arbitrary threshold
            std::stringstream ss;
            ss << "Prim has a deep schema inheritance hierarchy with " 
               << baseTypes.size() << " ancestor types";
            
            double impactScore = 40.0 + (baseTypes.size() - 5) * 5.0;  // Scale impact with depth
            
            m_hotspots.emplace_back(
                HotspotType::ComplexTypeInheritance,
                prim.GetPath(),
                ss.str(),
                impactScore,
                "Consider flattening schema hierarchy or using component-based approach"
            );
        }
    }
    
    /**
     * Analyze API schema count for a prim
     * 
     * @param prim The USD prim to analyze
     */
    void _analyzeApiSchemaCount(const UsdPrim& prim) {
        std::vector<std::string> apiSchemas;
        prim.GetAppliedSchemas(&apiSchemas);
        
        // Check for high schema cardinality
        if (apiSchemas.size() > 8) {  // Arbitrary threshold
            std::stringstream ss;
            ss << "Prim has " << apiSchemas.size() 
               << " API schemas applied, which may impact resolution performance";
            
            double impactScore = 30.0 + (apiSchemas.size() - 8) * 5.0;  // Scale impact with count
            
            m_hotspots.emplace_back(
                HotspotType::HighSchemaCardinality,
                prim.GetPath(),
                ss.str(),
                impactScore,
                "Consider consolidating functionality into fewer API schemas"
            );
        }
    }
    
    /**
     * Analyze property count for a prim
     * 
     * @param prim The USD prim to analyze
     */
    void _analyzePropertyCount(const UsdPrim& prim) {
        size_t attrCount = prim.GetAttributes().size();
        size_t relCount = prim.GetRelationships().size();
        size_t totalProps = attrCount + relCount;
        
        // Check for large property counts
        if (totalProps > 30) {  // Arbitrary threshold
            std::stringstream ss;
            ss << "Prim has " << totalProps << " properties (" 
               << attrCount << " attributes, " << relCount 
               << " relationships), which may impact access performance";
            
            double impactScore = 20.0 + (totalProps - 30) * 1.0;  // Scale impact with count
            
            m_hotspots.emplace_back(
                HotspotType::LargePropertyCount,
                prim.GetPath(),
                ss.str(),
                impactScore,
                "Consider grouping related properties or using more efficient data structures"
            );
        }
    }
    
    /**
     * Analyze data types used in prim attributes
     * 
     * @param prim The USD prim to analyze
     */
    void _analyzeDataTypes(const UsdPrim& prim) {
        int expensiveTypes = 0;
        std::vector<std::string> expensiveTypeNames;
        
        for (const UsdAttribute& attr : prim.GetAttributes()) {
            SdfValueTypeName typeName = attr.GetTypeName();
            
            // Check for expensive data types
            if (typeName == SdfValueTypeNames->Matrix4d ||
                typeName == SdfValueTypeNames->String ||
                typeName == SdfValueTypeNames->Asset ||
                typeName == SdfValueTypeNames->Dictionary) {
                
                expensiveTypes++;
                expensiveTypeNames.push_back(typeName.GetAsToken().GetString());
            }
        }
        
        // Check for many expensive data types
        if (expensiveTypes > 5) {  // Arbitrary threshold
            std::stringstream ss;
            ss << "Prim has " << expensiveTypes 
               << " attributes with expensive data types, which may impact memory usage and performance";
            
            double impactScore = 25.0 + (expensiveTypes - 5) * 5.0;  // Scale impact with count
            
            std::string suggestion = "Consider using more efficient data types where possible: ";
            suggestion += std::accumulate(
                std::next(expensiveTypeNames.begin()), 
                expensiveTypeNames.end(),
                expensiveTypeNames[0],
                [](const std::string& a, const std::string& b) {
                    return a + ", " + b;
                }
            );
            
            m_hotspots.emplace_back(
                HotspotType::ExpensiveDataTypes,
                prim.GetPath(),
                ss.str(),
                impactScore,
                suggestion
            );
        }
    }
    
    /**
     * Analyze relationships on a prim
     * 
     * @param prim The USD prim to analyze
     */
    void _analyzeRelationships(const UsdPrim& prim) {
        for (const UsdRelationship& rel : prim.GetRelationships()) {
            SdfPathVector targets;
            rel.GetTargets(&targets);
            
            // Check for many relationship targets
            if (targets.size() > 10) {  // Arbitrary threshold
                std::stringstream ss;
                ss << "Relationship '" << rel.GetName() << "' has " << targets.size() 
                   << " targets, which may impact resolution performance";
                
                double impactScore = 15.0 + (targets.size() - 10) * 1.0;  // Scale impact with count
                
                m_hotspots.emplace_back(
                    HotspotType::IndirectRelationships,
                    prim.GetPath(),
                    ss.str(),
                    impactScore,
                    "Consider using more direct references or optimizing relationship resolution"
                );
            }
            
            // Check for targets outside the stage
            for (const SdfPath& target : targets) {
                if (target.IsAbsolutePath() && !prim.GetStage()->GetPrimAtPath(target)) {
                    std::stringstream ss;
                    ss << "Relationship '" << rel.GetName() << "' targets path '" 
                       << target << "' which may not exist, causing resolution overhead";
                    
                    m_hotspots.emplace_back(
                        HotspotType::IndirectRelationships,
                        prim.GetPath(),
                        ss.str(),
                        45.0,  // High impact due to potential resolution failures
                        "Verify relationship targets exist or implement target validation"
                    );
                }
            }
        }
    }
    
    /**
     * Simulate schema resolution to analyze runtime behavior
     * 
     * @param stage The USD stage to analyze
     */
    void _analyzeSchemaResolution(UsdStageRefPtr stage) {
        // Track string lookups
        std::unordered_map<std::string, int> propertyLookups;
        
        // Track prim access counts
        std::unordered_map<Sdf::Path, int, SdfPath::Hash> primAccesses;
        
        // Track type checks
        std::unordered_map<Sdf::Path, std::unordered_map<std::string, int>, SdfPath::Hash> typeChecks;
        
        // Common property names to simulate access
        std::vector<std::string> commonProps = {
            "sparkle:health:current",
            "sparkle:health:maximum",
            "sparkle:combat:damage",
            "sparkle:movement:speed",
            "sparkle:ai:behavior"
        };
        
        // Common schema types to check
        std::vector<std::string> commonTypes = {
            "SparkleGameEntity",
            "SparkleEnemyCarrot",
            "SparklePlayer",
            "SparklePickup"
        };
        
        // Simulate common access patterns
        for (const UsdPrim& prim : stage->TraverseAll()) {
            if (!prim.IsValid() || prim.IsAbstract()) {
                continue;
            }
            
            // Simulate property access
            for (const std::string& propName : commonProps) {
                auto attr = prim.GetAttribute(TfToken(propName));
                propertyLookups[propName]++;
                
                if (attr) {
                    primAccesses[prim.GetPath()]++;
                    
                    // Track access pattern
                    _recordAccessPattern(propName, prim);
                }
            }
            
            // Simulate type checking
            for (const std::string& typeName : commonTypes) {
                bool isType = prim.IsA(TfType::FindByName(typeName));
                typeChecks[prim.GetPath()][typeName]++;
                
                if (isType) {
                    primAccesses[prim.GetPath()]++;
                }
            }
        }
        
        // Store tracking data for analysis
        m_stringLookups = propertyLookups;
        m_typeChecks = typeChecks;
        m_primAccessCounts = primAccesses;
        
        // Analyze for hotspots
        _analyzeRepeatedStringLookups(propertyLookups);
        _analyzeExcessiveTypeChecking(typeChecks);
    }
    
    /**
     * Record an access pattern for a property
     * 
     * @param propName The property name
     * @param prim The prim being accessed
     */
    void _recordAccessPattern(const std::string& propName, const UsdPrim& prim) {
        // Record access in our pattern tracker
        AccessPattern& pattern = m_accessPatterns[propName];
        pattern.propertyName = TfToken(propName);
        pattern.accessCount++;
        
        // Track unique prims
        static std::unordered_map<std::string, std::unordered_set<Sdf::Path, SdfPath::Hash>> uniquePrims;
        uniquePrims[propName].insert(prim.GetPath());
        pattern.uniquePrims = uniquePrims[propName].size();
    }
    
    /**
     * Analyze access patterns for inefficiencies
     */
    void _analyzeAccessPatterns() {
        // Identify common access patterns
        for (const auto& pair : m_accessPatterns) {
            const std::string& propName = pair.first;
            const AccessPattern& pattern = pair.second;
            
            // Check for frequent access with many unique prims
            if (pattern.accessCount > 100 && pattern.uniquePrims > 20) {
                std::stringstream ss;
                ss << "Property '" << propName << "' is accessed " << pattern.accessCount 
                   << " times across " << pattern.uniquePrims 
                   << " prims, suggesting a common access pattern";
                
                double impactScore = 35.0 + (pattern.accessCount / 100.0);
                
                m_hotspots.emplace_back(
                    HotspotType::InefficientAccessPattern,
                    Sdf::Path("/"),  // Global hotspot
                    ss.str(),
                    impactScore,
                    "Consider implementing property caching for frequently accessed properties"
                );
            }
        }
    }
    
    /**
     * Analyze for repeated string lookups
     * 
     * @param lookups Map of property names to lookup counts
     */
    void _analyzeRepeatedStringLookups(const std::unordered_map<std::string, int>& lookups) {
        for (const auto& pair : lookups) {
            const std::string& propName = pair.first;
            int count = pair.second;
            
            // Check for frequently looked up properties
            if (count > 50) {  // Arbitrary threshold
                std::stringstream ss;
                ss << "Property '" << propName << "' is looked up " << count 
                   << " times, potentially creating TfToken overhead";
                
                double impactScore = 40.0 + (count / 50.0);  // Scale impact with count
                
                m_hotspots.emplace_back(
                    HotspotType::RepeatedStringLookup,
                    Sdf::Path("/"),  // Global hotspot
                    ss.str(),
                    impactScore,
                    "Cache TfToken objects for frequently accessed properties"
                );
            }
        }
    }
    
    /**
     * Analyze for excessive type checking
     * 
     * @param typeChecks Map of prims to type check counts
     */
    void _analyzeExcessiveTypeChecking(
        const std::unordered_map<Sdf::Path, std::unordered_map<std::string, int>, SdfPath::Hash>& typeChecks) {
        
        for (const auto& primPair : typeChecks) {
            const Sdf::Path& primPath = primPair.first;
            const auto& typesMap = primPair.second;
            
            int totalChecks = 0;
            for (const auto& typePair : typesMap) {
                totalChecks += typePair.second;
            }
            
            // Check for frequently type-checked prims
            if (totalChecks > 20) {  // Arbitrary threshold
                std::stringstream ss;
                ss << "Prim is checked for types " << totalChecks 
                   << " times, creating unnecessary overhead";
                
                double impactScore = 30.0 + (totalChecks / 5.0);  // Scale impact with count
                
                m_hotspots.emplace_back(
                    HotspotType::ExcessiveTypeChecking,
                    primPath,
                    ss.str(),
                    impactScore,
                    "Cache type check results to avoid repeated checking"
                );
            }
        }
    }
    
    /**
     * Get the name of a hotspot type
     * 
     * @param type The hotspot type
     * @return The name of the hotspot type
     */
    std::string _getHotspotTypeName(HotspotType type) const {
        switch (type) {
            case HotspotType::RepeatedStringLookup:
                return "Repeated String Lookup";
            case HotspotType::UncachedSchemaTraversal:
                return "Uncached Schema Traversal";
            case HotspotType::ExcessiveTypeChecking:
                return "Excessive Type Checking";
            case HotspotType::DeepCompositionChain:
                return "Deep Composition Chain";
            case HotspotType::InefficientAccessPattern:
                return "Inefficient Access Pattern";
            case HotspotType::ComplexTypeInheritance:
                return "Complex Type Inheritance";
            case HotspotType::HighSchemaCardinality:
                return "High Schema Cardinality";
            case HotspotType::LargePropertyCount:
                return "Large Property Count";
            case HotspotType::ExpensiveDataTypes:
                return "Expensive Data Types";
            case HotspotType::IndirectRelationships:
                return "Indirect Relationships";
            default:
                return "Unknown";
        }
    }
};

/**
 * Main function demonstrating how to use the SchemaHotspotAnalyzer
 */
int main(int argc, char* argv[]) {
    // Check for arguments
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <usd_file> [report_file]" << std::endl;
        return 1;
    }
    
    // Open USD stage
    std::string inputFile = argv[1];
    UsdStageRefPtr stage = UsdStage::Open(inputFile);
    
    if (!stage) {
        std::cerr << "Failed to open USD stage: " << inputFile << std::endl;
        return 1;
    }
    
    // Create analyzer and analyze stage
    SchemaHotspotAnalyzer analyzer;
    
    std::cout << "Analyzing stage: " << inputFile << std::endl;
    analyzer.analyzeStage(stage, true);
    
    // Get top hotspots
    auto topHotspots = analyzer.getTopHotspots(5);
    
    std::cout << "Top 5 performance hotspots:" << std::endl;
    for (size_t i = 0; i < topHotspots.size(); ++i) {
        const auto& hotspot = topHotspots[i];
        
        std::cout << (i + 1) << ". " << hotspot.description << std::endl;
        std::cout << "   Path: " << hotspot.primPath << std::endl;
        std::cout << "   Suggestion: " << hotspot.optimizationSuggestion << std::endl;
        std::cout << std::endl;
    }
    
    // Generate report
    if (argc >= 3) {
        std::string reportFile = argv[2];
        if (analyzer.saveReportToFile(reportFile)) {
            std::cout << "Saved detailed report to: " << reportFile << std::endl;
        } else {
            std::cerr << "Failed to save report to: " << reportFile << std::endl;
        }
    } else {
        // Print report to console
        std::cout << "\nDetailed Report:\n" << std::endl;
        std::cout << analyzer.generateReport() << std::endl;
    }
    
    return 0;
}
