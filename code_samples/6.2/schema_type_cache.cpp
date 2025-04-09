/**
 * schema_type_cache.h
 * 
 * Referenced in Chapter 6.2: Caching and Optimization Strategies
 * 
 * An efficient schema type caching system that improves type checking performance
 * for USD schemas. This system pre-caches TfType objects, inheritance relationships,
 * and prim-to-schema-type mappings for fast type checking and API schema lookup.
 */

#pragma once

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/schemaRegistry.h>
#include <pxr/usd/usd/apiSchemaBase.h>
#include <pxr/base/tf/type.h>
#include <pxr/base/tf/singleton.h>
#include <pxr/base/tf/hash.h>

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <memory>
#include <string_view>
#include <atomic>
#include <shared_mutex>
#include <typeindex>

PXR_NAMESPACE_USING_DIRECTIVE

/**
 * TypeInfo
 * 
 * Cached information about a schema type
 */
struct TypeInfo {
    TfType type;                               // The TfType object
    std::string typeName;                      // Name of the type
    bool isAPISchema = false;                  // Whether this is an API schema
    bool isMultipleApply = false;              // Whether this is a multiple-apply API schema
    bool isAbstract = false;                   // Whether this is an abstract type
    std::vector<TfType> baseTypes;             // Direct base types
    std::vector<TfType> allAncestorTypes;      // All ancestor types
    std::unordered_set<TfType, TfHash> derivedTypes; // Types that derive from this type
    
    TypeInfo() = default;
    
    explicit TypeInfo(const TfType& t) : type(t) {
        if (t) {
            typeName = t.GetTypeName();
            
            // Check if this is an API schema
            TfType apiSchemaType = TfType::FindByName("UsdAPISchemaBase");
            isAPISchema = (t != apiSchemaType) && t.IsA(apiSchemaType);
            
            // Check if this is a multiple-apply API schema
            if (isAPISchema) {
                isMultipleApply = UsdSchemaRegistry::GetInstance().IsMultipleApplyAPISchema(t);
            }
            
            // Check if this is an abstract type
            TfType::TypeId typeId = t.GetTypeid();
            isAbstract = (typeId == typeid(void));
            
            // Get base types
            t.GetDirectlyDerivedTypes(&derivedTypes);
            
            // Get all base types
            t.GetAllAncestorTypes(&allAncestorTypes);
            
            // Get direct base types
            t.GetDirectlyDerivedTypes(&derivedTypes);
        }
    }
};

/**
 * SchemaTypeCache
 * 
 * A cache for schema type information to improve type checking performance
 */
class SchemaTypeCache {
public:
    /**
     * Get the singleton instance
     */
    static SchemaTypeCache& GetInstance() {
        static SchemaTypeCache instance;
        return instance;
    }
    
    /**
     * Get type info for a schema type
     * 
     * @param typeName Name of the schema type
     * @return Pointer to the type info, or nullptr if type not found
     */
    const TypeInfo* GetTypeInfo(const std::string& typeName) {
        // Check if type info is already cached
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            auto it = m_typeInfos.find(typeName);
            if (it != m_typeInfos.end()) {
                return &it->second;
            }
        }
        
        // If not cached, find the type and cache it
        TfType type = TfType::FindByName(typeName);
        if (!type) {
            return nullptr;  // Type not found
        }
        
        return CacheType(type);
    }
    
    /**
     * Get type info for a schema type
     * 
     * @param type The schema type
     * @return Pointer to the type info, or nullptr if type not found
     */
    const TypeInfo* GetTypeInfo(const TfType& type) {
        if (!type) {
            return nullptr;
        }
        
        std::string typeName = type.GetTypeName();
        
        // Check if type info is already cached
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            auto it = m_typeInfos.find(typeName);
            if (it != m_typeInfos.end()) {
                return &it->second;
            }
        }
        
        return CacheType(type);
    }
    
    /**
     * Check if a type is or inherits from another type
     * 
     * @param type The type to check
     * @param baseType The base type to check against
     * @return Whether the type inherits from the base type
     */
    bool IsA(const TfType& type, const TfType& baseType) {
        if (!type || !baseType) {
            return false;
        }
        
        // Fast path for same type
        if (type == baseType) {
            return true;
        }
        
        // Get type info
        const TypeInfo* info = GetTypeInfo(type);
        if (!info) {
            return false;
        }
        
        // Check against all ancestor types
        for (const TfType& ancestor : info->allAncestorTypes) {
            if (ancestor == baseType) {
                return true;
            }
        }
        
        return false;
    }
    
    /**
     * Check if a prim has a specific schema applied
     * 
     * @param prim The USD prim to check
     * @param schemaType The schema type to check for
     * @return Whether the prim has the schema applied
     */
    bool HasSchema(const UsdPrim& prim, const TfType& schemaType) {
        if (!prim.IsValid() || !schemaType) {
            return false;
        }
        
        // Get type info
        const TypeInfo* typeInfo = GetTypeInfo(schemaType);
        if (!typeInfo) {
            return false;
        }
        
        // Check prim type cache
        SdfPath path = prim.GetPath();
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            auto primIt = m_primTypeCache.find(path);
            if (primIt != m_primTypeCache.end()) {
                const auto& typeMap = primIt->second;
                auto typeIt = typeMap.find(schemaType);
                if (typeIt != typeMap.end()) {
                    return typeIt->second;  // Return cached result
                }
            }
        }
        
        // Determine result based on whether this is an API schema or not
        bool result;
        if (typeInfo->isAPISchema) {
            result = prim.HasAPI(schemaType);
        } else {
            result = prim.IsA(schemaType);
        }
        
        // Cache the result
        {
            std::unique_lock<std::shared_mutex> lock(m_mutex);
            m_primTypeCache[path][schemaType] = result;
        }
        
        return result;
    }
    
    /**
     * Check if a prim has a specific schema applied by name
     * 
     * @param prim The USD prim to check
     * @param schemaTypeName The name of the schema type to check for
     * @return Whether the prim has the schema applied
     */
    bool HasSchema(const UsdPrim& prim, const std::string& schemaTypeName) {
        TfType schemaType = TfType::FindByName(schemaTypeName);
        if (!schemaType) {
            return false;
        }
        
        return HasSchema(prim, schemaType);
    }
    
    /**
     * Get all schema types applied to a prim
     * 
     * @param prim The USD prim to check
     * @return A vector of applied schema types
     */
    std::vector<TfType> GetAppliedSchemas(const UsdPrim& prim) {
        if (!prim.IsValid()) {
            return {};
        }
        
        std::vector<TfType> result;
        
        // Check cache
        SdfPath path = prim.GetPath();
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            auto it = m_primAppliedSchemas.find(path);
            if (it != m_primAppliedSchemas.end()) {
                return it->second;  // Return cached result
            }
        }
        
        // Get prim schema type
        TfType primType = prim.GetPrimTypeInfo().GetSchemaType();
        if (primType) {
            result.push_back(primType);
            
            // Add ancestor types
            const TypeInfo* typeInfo = GetTypeInfo(primType);
            if (typeInfo) {
                for (const TfType& ancestor : typeInfo->allAncestorTypes) {
                    result.push_back(ancestor);
                }
            }
        }
        
        // Get applied API schemas
        std::vector<std::string> apiSchemas;
        prim.GetAppliedSchemas(&apiSchemas);
        
        for (const std::string& schemaName : apiSchemas) {
            TfType apiType = TfType::FindByName(schemaName);
            if (apiType) {
                result.push_back(apiType);
            }
        }
        
        // Cache the result
        {
            std::unique_lock<std::shared_mutex> lock(m_mutex);
            m_primAppliedSchemas[path] = result;
        }
        
        return result;
    }
    
    /**
     * Invalidate cached information for a prim
     * 
     * @param prim The USD prim to invalidate
     */
    void InvalidatePrim(const UsdPrim& prim) {
        if (!prim.IsValid()) {
            return;
        }
        
        SdfPath path = prim.GetPath();
        
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_primTypeCache.erase(path);
        m_primAppliedSchemas.erase(path);
    }
    
    /**
     * Clear all cached information
     */
    void ClearAll() {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_typeInfos.clear();
        m_primTypeCache.clear();
        m_primAppliedSchemas.clear();
        m_cachedTypes.clear();
    }
    
    /**
     * Pre-cache common schema types
     */
    void PreCacheCommonTypes() {
        // Pre-cache USD base types
        std::vector<std::string> baseTypes = {
            "UsdSchemaBase",
            "UsdTyped",
            "UsdGeomXformable",
            "UsdGeomGprim",
            "UsdGeomMesh",
            "UsdGeomXform",
            "UsdLuxLight",
            "UsdShadeMaterial",
            "UsdShadeShader",
            "UsdAPISchemaBase"
        };
        
        for (const std::string& typeName : baseTypes) {
            TfType type = TfType::FindByName(typeName);
            if (type) {
                CacheType(type);
            }
        }
        
        // Pre-cache game-specific types
        std::vector<std::string> gameTypes = {
            "SparkleGameEntity",
            "SparkleEnemyCarrot",
            "SparklePlayer",
            "SparklePickup",
            "SparkleHealthAPI",
            "SparkleCombatAPI",
            "SparkleMovementAPI",
            "SparkleAIAPI",
            "SparkleTeamAPI",
            "SparkleLootAPI"
        };
        
        for (const std::string& typeName : gameTypes) {
            TfType type = TfType::FindByName(typeName);
            if (type) {
                CacheType(type);
            }
        }
    }
    
private:
    /**
     * Cache a type and its information
     * 
     * @param type The type to cache
     * @return Pointer to the cached type info
     */
    const TypeInfo* CacheType(const TfType& type) {
        if (!type) {
            return nullptr;
        }
        
        std::string typeName = type.GetTypeName();
        
        // Add to cache
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        auto it = m_typeInfos.find(typeName);
        if (it != m_typeInfos.end()) {
            return &it->second;  // Already cached
        }
        
        // Create and cache type info
        TypeInfo info(type);
        auto result = m_typeInfos.emplace(typeName, std::move(info));
        
        // Add to cached types set
        m_cachedTypes.insert(type);
        
        return &result.first->second;
    }
    
    // Private constructor for singleton
    SchemaTypeCache() {
        // Initialize cache with common types
        PreCacheCommonTypes();
    }
    
    // Prevent copying or moving
    SchemaTypeCache(const SchemaTypeCache&) = delete;
    SchemaTypeCache& operator=(const SchemaTypeCache&) = delete;
    SchemaTypeCache(SchemaTypeCache&&) = delete;
    SchemaTypeCache& operator=(SchemaTypeCache&&) = delete;
    
    // Member variables
    mutable std::shared_mutex m_mutex;
    std::unordered_map<std::string, TypeInfo> m_typeInfos;
    std::unordered_set<TfType, TfHash> m_cachedTypes;
    std::unordered_map<SdfPath, std::unordered_map<TfType, bool, TfHash>, SdfPath::Hash> m_primTypeCache;
    std::unordered_map<SdfPath, std::vector<TfType>, SdfPath::Hash> m_primAppliedSchemas;
};

/**
 * OptimizedTypeChecker
 * 
 * A helper class for efficiently checking prim types with caching
 */
class OptimizedTypeChecker {
public:
    /**
     * Constructor
     * 
     * @param typeName Name of the schema type to check for
     */
    explicit OptimizedTypeChecker(const std::string& typeName)
        : m_type(TfType::FindByName(typeName))
        , m_typeName(typeName)
    {
        if (m_type) {
            m_typeInfo = SchemaTypeCache::GetInstance().GetTypeInfo(m_type);
        }
    }
    
    /**
     * Constructor
     * 
     * @param type The schema type to check for
     */
    explicit OptimizedTypeChecker(const TfType& type)
        : m_type(type)
        , m_typeName(type ? type.GetTypeName() : "")
    {
        if (m_type) {
            m_typeInfo = SchemaTypeCache::GetInstance().GetTypeInfo(m_type);
        }
    }
    
    /**
     * Check if a prim is or has this type
     * 
     * @param prim The USD prim to check
     * @return Whether the prim is or has this type
     */
    bool Check(const UsdPrim& prim) const {
        if (!m_type || !prim.IsValid()) {
            return false;
        }
        
        return SchemaTypeCache::GetInstance().HasSchema(prim, m_type);
    }
    
    /**
     * Get the schema type
     */
    const TfType& GetType() const {
        return m_type;
    }
    
    /**
     * Get the type name
     */
    const std::string& GetTypeName() const {
        return m_typeName;
    }
    
    /**
     * Check if the checker is valid
     */
    bool IsValid() const {
        return m_type && m_typeInfo;
    }
    
    /**
     * Check if this is an API schema
     */
    bool IsAPISchema() const {
        return m_typeInfo && m_typeInfo->isAPISchema;
    }
    
    /**
     * Check if this is a multiple-apply API schema
     */
    bool IsMultipleApply() const {
        return m_typeInfo && m_typeInfo->isMultipleApply;
    }

private:
    TfType m_type;
    std::string m_typeName;
    const TypeInfo* m_typeInfo = nullptr;
};

/**
 * SchemaCompatibilityChecker
 * 
 * Checks compatibility between schema versions
 */
class SchemaCompatibilityChecker {
public:
    /**
     * Constructor
     */
    SchemaCompatibilityChecker() = default;
    
    /**
     * Register a schema version
     * 
     * @param schemaName Name of the schema
     * @param majorVersion Major version
     * @param minorVersion Minor version
     * @param patchVersion Patch version
     */
    void RegisterSchemaVersion(const std::string& schemaName, 
                              int majorVersion, 
                              int minorVersion,
                              int patchVersion) {
        VersionInfo info{majorVersion, minorVersion, patchVersion};
        m_schemaVersions[schemaName] = info;
    }
    
    /**
     * Check if a schema version is compatible with another version
     * 
     * @param schemaName Name of the schema
     * @param requiredMajor Required major version
     * @param requiredMinor Required minor version
     * @param requiredPatch Required patch version
     * @return Whether the versions are compatible
     */
    bool IsCompatible(const std::string& schemaName,
                     int requiredMajor,
                     int requiredMinor,
                     int requiredPatch) const {
        auto it = m_schemaVersions.find(schemaName);
        if (it == m_schemaVersions.end()) {
            return false;  // Schema not found
        }
        
        const VersionInfo& version = it->second;
        
        // Check compatibility (major version must match, minor and patch must be >= required)
        return (version.majorVersion == requiredMajor) && 
               ((version.minorVersion > requiredMinor) || 
                (version.minorVersion == requiredMinor && version.patchVersion >= requiredPatch));
    }
    
    /**
     * Extract version information from a stage
     * 
     * @param stage The USD stage to extract versions from
     */
    void ExtractVersionsFromStage(UsdStageRefPtr stage) {
        if (!stage) {
            return;
        }
        
        // Get global customData
        UsdPrim globalPrim = stage->GetPrimAtPath(SdfPath("/GLOBAL"));
        if (!globalPrim.IsValid()) {
            return;
        }
        
        // Extract version info
        VtDictionary customData;
        if (globalPrim.GetMetadata(SdfFieldKeys->CustomData, &customData)) {
            VtValue versionInfoValue = customData.GetValueAtPath("versionInfo");
            if (versionInfoValue.IsHolding<VtDictionary>()) {
                VtDictionary versionInfo = versionInfoValue.Get<VtDictionary>();
                
                // Extract individual version components
                VtValue libraryNameValue = customData.GetValueAtPath("libraryName");
                if (libraryNameValue.IsHolding<std::string>()) {
                    std::string libraryName = libraryNameValue.Get<std::string>();
                    
                    // Extract version numbers
                    int major = 0, minor = 0, patch = 0;
                    ExtractVersionNumber(versionInfo, "majorVersion", major);
                    ExtractVersionNumber(versionInfo, "minorVersion", minor);
                    ExtractVersionNumber(versionInfo, "patchVersion", patch);
                    
                    // Register version
                    RegisterSchemaVersion(libraryName, major, minor, patch);
                }
            }
        }
    }

private:
    struct VersionInfo {
        int majorVersion = 0;
        int minorVersion = 0;
        int patchVersion = 0;
    };
    
    // Helper to extract version numbers from dictionary
    void ExtractVersionNumber(const VtDictionary& dict, const std::string& key, int& value) const {
        VtValue vtValue = dict.GetValueAtPath(key);
        if (vtValue.IsHolding<std::string>()) {
            std::string strValue = vtValue.Get<std::string>();
            try {
                value = std::stoi(strValue);
            }
            catch (const std::exception&) {
                // Handle conversion error
                value = 0;
            }
        }
        else if (vtValue.IsHolding<int>()) {
            value = vtValue.Get<int>();
        }
    }
    
    // Member variables
    std::unordered_map<std::string, VersionInfo> m_schemaVersions;
};

/**
 * Example of how to use the schema type caching system
 */
void SchemaTypeCacheExample() {
    // Create a stage with some test prims
    UsdStageRefPtr stage = UsdStage::CreateInMemory();
    
    // Define entity type prim
    UsdPrim entityPrim = stage->DefinePrim(SdfPath("/Game/Entity"), TfToken("SparkleGameEntity"));
    
    // Define enemy prim
    UsdPrim enemyPrim = stage->DefinePrim(SdfPath("/Game/Enemy"), TfToken("SparkleEnemyCarrot"));
    
    // Define xform and apply API schema
    UsdPrim xformPrim = stage->DefinePrim(SdfPath("/Game/Object"), TfToken("Xform"));
    xformPrim.ApplyAPI(TfType::FindByName("SparkleHealthAPI"));
    
    // Create optimized type checkers
    OptimizedTypeChecker entityChecker("SparkleGameEntity");
    OptimizedTypeChecker healthAPIChecker("SparkleHealthAPI");
    
    // Use cached type checking
    bool isEntity = entityChecker.Check(entityPrim);       // Should be true
    bool isEnemy = entityChecker.Check(enemyPrim);         // Should be true (inheritance)
    bool hasHealthAPI = healthAPIChecker.Check(xformPrim); // Should be true
    
    // Get all schemas for a prim
    std::vector<TfType> enemySchemas = SchemaTypeCache::GetInstance().GetAppliedSchemas(enemyPrim);
    
    // Print results
    std::cout << "Entity prim is SparkleGameEntity: " << (isEntity ? "Yes" : "No") << std::endl;
    std::cout << "Enemy prim is SparkleGameEntity: " << (isEnemy ? "Yes" : "No") << std::endl;
    std::cout << "Xform prim has SparkleHealthAPI: " << (hasHealthAPI ? "Yes" : "No") << std::endl;
    
    std::cout << "Enemy prim has " << enemySchemas.size() << " schemas:" << std::endl;
    for (const TfType& type : enemySchemas) {
        std::cout << "  - " << type.GetTypeName() << std::endl;
    }
    
    // Check schema compatibility
    SchemaCompatibilityChecker compatChecker;
    compatChecker.RegisterSchemaVersion("sparkleGame", 1, 2, 5);
    
    bool isCompatible = compatChecker.IsCompatible("sparkleGame", 1, 2, 0); // Should be true
    std::cout << "Schema version 1.2.5 is compatible with required 1.2.0: " 
              << (isCompatible ? "Yes" : "No") << std::endl;
    
    // Clean up cache
    SchemaTypeCache::GetInstance().ClearAll();
}
