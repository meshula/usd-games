/**
 * schema_registry_index.h
 * 
 * Referenced in Chapter 6.2: Caching and Optimization Strategies
 * 
 * A schema indexing system that creates efficient indices for schema types and properties,
 * enabling rapid lookup and improved traversal performance. This system helps find all prims
 * with particular schemas or properties without full stage traversal.
 */

#pragma once

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/tf/hash.h>
#include <pxr/base/tf/weakBase.h>

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <shared_mutex>
#include <memory>
#include <functional>
#include <optional>

PXR_NAMESPACE_USING_DIRECTIVE

/**
 * SchemaPathIndex
 * 
 * An index mapping schema types to prim paths
 */
class SchemaPathIndex {
public:
    /**
     * Add a prim to the index
     * 
     * @param prim The USD prim to add
     */
    void AddPrim(const UsdPrim& prim) {
        if (!prim.IsValid()) {
            return;
        }
        
        // Get prim schema type
        TfType type = prim.GetPrimTypeInfo().GetSchemaType();
        if (type) {
            std::string typeName = type.GetTypeName();
            SdfPath path = prim.GetPath();
            
            std::unique_lock<std::shared_mutex> lock(m_mutex);
            m_schemaTypeToPaths[typeName].insert(path);
            m_pathToSchemaTypes[path].insert(typeName);
        }
        
        // Get applied API schemas
        std::vector<std::string> apiSchemas;
        prim.GetAppliedSchemas(&apiSchemas);
        
        if (!apiSchemas.empty()) {
            SdfPath path = prim.GetPath();
            
            std::unique_lock<std::shared_mutex> lock(m_mutex);
            for (const std::string& schema : apiSchemas) {
                m_schemaTypeToPaths[schema].insert(path);
                m_pathToSchemaTypes[path].insert(schema);
            }
        }
    }
    
    /**
     * Remove a prim from the index
     * 
     * @param path The USD prim path to remove
     */
    void RemovePrim(const SdfPath& path) {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        
        // Get schema types for this path
        auto pathIt = m_pathToSchemaTypes.find(path);
        if (pathIt != m_pathToSchemaTypes.end()) {
            // Remove path from each schema type's set
            for (const std::string& typeName : pathIt->second) {
                auto typeIt = m_schemaTypeToPaths.find(typeName);
                if (typeIt != m_schemaTypeToPaths.end()) {
                    typeIt->second.erase(path);
                    
                    // Remove schema type entry if empty
                    if (typeIt->second.empty()) {
                        m_schemaTypeToPaths.erase(typeIt);
                    }
                }
            }
            
            // Remove path entry
            m_pathToSchemaTypes.erase(pathIt);
        }
    }
    
    /**
     * Clear the index
     */
    void Clear() {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_schemaTypeToPaths.clear();
        m_pathToSchemaTypes.clear();
    }
    
    /**
     * Find prims with a specific schema type
     * 
     * @param schemaType The schema type to search for
     * @return Set of prim paths with the schema type
     */
    std::unordered_set<SdfPath, SdfPath::Hash> FindPrimsBySchemaType(const std::string& schemaType) const {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        
        auto it = m_schemaTypeToPaths.find(schemaType);
        if (it != m_schemaTypeToPaths.end()) {
            return it->second;
        }
        
        return {};
    }
    
    /**
     * Get schema types for a prim
     * 
     * @param path The prim path
     * @return Set of schema types for the prim
     */
    std::unordered_set<std::string> GetSchemaTypesForPrim(const SdfPath& path) const {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        
        auto it = m_pathToSchemaTypes.find(path);
        if (it != m_pathToSchemaTypes.end()) {
            return it->second;
        }
        
        return {};
    }
    
    /**
     * Check if a prim has a specific schema type
     * 
     * @param path The prim path
     * @param schemaType The schema type to check for
     * @return Whether the prim has the schema type
     */
    bool HasSchemaType(const SdfPath& path, const std::string& schemaType) const {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        
        auto it = m_pathToSchemaTypes.find(path);
        if (it != m_pathToSchemaTypes.end()) {
            return it->second.find(schemaType) != it->second.end();
        }
        
        return false;
    }
    
    /**
     * Get all schema types in the index
     * 
     * @return Set of all schema types
     */
    std::unordered_set<std::string> GetAllSchemaTypes() const {
        std::unordered_set<std::string> types;
        
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        for (const auto& pair : m_schemaTypeToPaths) {
            types.insert(pair.first);
        }
        
        return types;
    }
    
    /**
     * Get the number of prims with a specific schema type
     * 
     * @param schemaType The schema type to count
     * @return The number of prims with the schema type
     */
    size_t GetSchemaTypeCount(const std::string& schemaType) const {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        
        auto it = m_schemaTypeToPaths.find(schemaType);
        if (it != m_schemaTypeToPaths.end()) {
            return it->second.size();
        }
        
        return 0;
    }

private:
    mutable std::shared_mutex m_mutex;
    std::unordered_map<std::string, std::unordered_set<SdfPath, SdfPath::Hash>> m_schemaTypeToPaths;
    std::unordered_map<SdfPath, std::unordered_set<std::string>, SdfPath::Hash> m_pathToSchemaTypes;
};

/**
 * PropertyPathIndex
 * 
 * An index mapping property names to prim paths
 */
class PropertyPathIndex {
public:
    /**
     * Add a prim's properties to the index
     * 
     * @param prim The USD prim to add
     */
    void AddPrim(const UsdPrim& prim) {
        if (!prim.IsValid()) {
            return;
        }
        
        SdfPath path = prim.GetPath();
        
        // Index attributes
        for (const UsdAttribute& attr : prim.GetAttributes()) {
            TfToken name = attr.GetName();
            
            std::unique_lock<std::shared_mutex> lock(m_mutex);
            m_propertyToPaths[name].insert(path);
            m_pathToProperties[path].insert(name);
        }
    }
    
    /**
     * Remove a prim from the index
     * 
     * @param path The USD prim path to remove
     */
    void RemovePrim(const SdfPath& path) {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        
        // Get properties for this path
        auto pathIt = m_pathToProperties.find(path);
        if (pathIt != m_pathToProperties.end()) {
            // Remove path from each property's set
            for (const TfToken& name : pathIt->second) {
                auto propIt = m_propertyToPaths.find(name);
                if (propIt != m_propertyToPaths.end()) {
                    propIt->second.erase(path);
                    
                    // Remove property entry if empty
                    if (propIt->second.empty()) {
                        m_propertyToPaths.erase(propIt);
                    }
                }
            }
            
            // Remove path entry
            m_pathToProperties.erase(pathIt);
        }
    }
    
    /**
     * Clear the index
     */
    void Clear() {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_propertyToPaths.clear();
        m_pathToProperties.clear();
    }
    
    /**
     * Find prims with a specific property
     * 
     * @param propertyName The property name to search for
     * @return Set of prim paths with the property
     */
    std::unordered_set<SdfPath, SdfPath::Hash> FindPrimsByProperty(const TfToken& propertyName) const {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        
        auto it = m_propertyToPaths.find(propertyName);
        if (it != m_propertyToPaths.end()) {
            return it->second;
        }
        
        return {};
    }
    
    /**
     * Find prims with properties matching a prefix
     * 
     * @param prefix The property name prefix to search for
     * @return Set of prim paths with matching properties
     */
    std::unordered_set<SdfPath, SdfPath::Hash> FindPrimsByPropertyPrefix(const std::string& prefix) const {
        std::unordered_set<SdfPath, SdfPath::Hash> result;
        
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        for (const auto& pair : m_propertyToPaths) {
            if (TfStringStartsWith(pair.first.GetString(), prefix)) {
                result.insert(pair.second.begin(), pair.second.end());
            }
        }
        
        return result;
    }
    
    /**
     * Get properties for a prim
     * 
     * @param path The prim path
     * @return Set of property names for the prim
     */
    std::unordered_set<TfToken, TfToken::HashFunctor> GetPropertiesForPrim(const SdfPath& path) const {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        
        auto it = m_pathToProperties.find(path);
        if (it != m_pathToProperties.end()) {
            return it->second;
        }
        
        return {};
    }
    
    /**
     * Check if a prim has a specific property
     * 
     * @param path The prim path
     * @param propertyName The property name to check for
     * @return Whether the prim has the property
     */
    bool HasProperty(const SdfPath& path, const TfToken& propertyName) const {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        
        auto it = m_pathToProperties.find(path);
        if (it != m_pathToProperties.end()) {
            return it->second.find(propertyName) != it->second.end();
        }
        
        return false;
    }
    
    /**
     * Get all property names in the index
     * 
     * @return Set of all property names
     */
    std::unordered_set<TfToken, TfToken::HashFunctor> GetAllPropertyNames() const {
        std::unordered_set<TfToken, TfToken::HashFunctor> names;
        
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        for (const auto& pair : m_propertyToPaths) {
            names.insert(pair.first);
        }
        
        return names;
    }
    
    /**
     * Get the number of prims with a specific property
     * 
     * @param propertyName The property name to count
     * @return The number of prims with the property
     */
    size_t GetPropertyCount(const TfToken& propertyName) const {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        
        auto it = m_propertyToPaths.find(propertyName);
        if (it != m_propertyToPaths.end()) {
            return it->second.size();
        }
        
        return 0;
    }

private:
    mutable std::shared_mutex m_mutex;
    std::unordered_map<TfToken, std::unordered_set<SdfPath, SdfPath::Hash>, TfToken::HashFunctor> m_propertyToPaths;
    std::unordered_map<SdfPath, std::unordered_set<TfToken, TfToken::HashFunctor>, SdfPath::Hash> m_pathToProperties;
};

/**
 * RelationshipTargetIndex
 * 
 * An index mapping relationship targets to source prims
 */
class RelationshipTargetIndex {
public:
    /**
     * Add a prim's relationships to the index
     * 
     * @param prim The USD prim to add
     */
    void AddPrim(const UsdPrim& prim) {
        if (!prim.IsValid()) {
            return;
        }
        
        SdfPath sourcePath = prim.GetPath();
        
        // Index relationships
        for (const UsdRelationship& rel : prim.GetRelationships()) {
            TfToken name = rel.GetName();
            
            // Get targets
            SdfPathVector targets;
            rel.GetTargets(&targets);
            
            if (!targets.empty()) {
                std::unique_lock<std::shared_mutex> lock(m_mutex);
                
                // Add relationship to index
                auto& relEntry = m_relationships[sourcePath][name];
                relEntry.insert(relEntry.end(), targets.begin(), targets.end());
                
                // Add reverse lookup entries
                for (const SdfPath& targetPath : targets) {
                    m_targetToSources[targetPath][name].insert(sourcePath);
                }
            }
        }
    }
    
    /**
     * Remove a prim from the index
     * 
     * @param path The USD prim path to remove
     */
    void RemovePrim(const SdfPath& path) {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        
        // Remove as source
        auto sourceIt = m_relationships.find(path);
        if (sourceIt != m_relationships.end()) {
            // Remove from reverse lookup
            for (const auto& relPair : sourceIt->second) {
                const TfToken& relName = relPair.first;
                const SdfPathVector& targets = relPair.second;
                
                for (const SdfPath& targetPath : targets) {
                    auto targetIt = m_targetToSources.find(targetPath);
                    if (targetIt != m_targetToSources.end()) {
                        auto nameIt = targetIt->second.find(relName);
                        if (nameIt != targetIt->second.end()) {
                            nameIt->second.erase(path);
                            
                            // Remove name entry if empty
                            if (nameIt->second.empty()) {
                                targetIt->second.erase(nameIt);
                            }
                        }
                        
                        // Remove target entry if empty
                        if (targetIt->second.empty()) {
                            m_targetToSources.erase(targetIt);
                        }
                    }
                }
            }
            
            // Remove source entry
            m_relationships.erase(sourceIt);
        }
        
        // Remove as target
        auto targetIt = m_targetToSources.find(path);
        if (targetIt != m_targetToSources.end()) {
            // For each relationship name targeting this prim
            for (const auto& relPair : targetIt->second) {
                const TfToken& relName = relPair.first;
                const auto& sources = relPair.second;
                
                // For each source prim
                for (const SdfPath& sourcePath : sources) {
                    auto sourceIt = m_relationships.find(sourcePath);
                    if (sourceIt != m_relationships.end()) {
                        auto nameIt = sourceIt->second.find(relName);
                        if (nameIt != sourceIt->second.end()) {
                            // Remove target from source's targets
                            SdfPathVector& targets = nameIt->second;
                            targets.erase(std::remove(targets.begin(), targets.end(), path), targets.end());
                            
                            // Remove name entry if empty
                            if (targets.empty()) {
                                sourceIt->second.erase(nameIt);
                            }
                        }
                        
                        // Remove source entry if empty
                        if (sourceIt->second.empty()) {
                            m_relationships.erase(sourceIt);
                        }
                    }
                }
            }
            
            // Remove target entry
            m_targetToSources.erase(targetIt);
        }
    }
    
    /**
     * Clear the index
     */
    void Clear() {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_relationships.clear();
        m_targetToSources.clear();
    }
    
    /**
     * Find targets for a prim's relationship
     * 
     * @param sourcePath The source prim path
     * @param relationshipName The relationship name
     * @return Vector of target paths, or empty if not found
     */
    SdfPathVector FindTargets(const SdfPath& sourcePath, const TfToken& relationshipName) const {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        
        auto sourceIt = m_relationships.find(sourcePath);
        if (sourceIt != m_relationships.end()) {
            auto nameIt = sourceIt->second.find(relationshipName);
            if (nameIt != sourceIt->second.end()) {
                return nameIt->second;
            }
        }
        
        return {};
    }
    
    /**
     * Find source prims with relationships targeting a prim
     * 
     * @param targetPath The target prim path
     * @param relationshipName The relationship name (optional)
     * @return Map of relationship names to source prim paths
     */
    std::unordered_map<TfToken, std::unordered_set<SdfPath, SdfPath::Hash>, TfToken::HashFunctor> 
    FindSources(const SdfPath& targetPath, const TfToken& relationshipName = TfToken()) const {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        
        auto targetIt = m_targetToSources.find(targetPath);
        if (targetIt != m_targetToSources.end()) {
            if (!relationshipName.IsEmpty()) {
                // Return specific relationship
                auto nameIt = targetIt->second.find(relationshipName);
                if (nameIt != targetIt->second.end()) {
                    return {{relationshipName, nameIt->second}};
                }
                return {};
            } else {
                // Return all relationships
                return targetIt->second;
            }
        }
        
        return {};
    }
    
    /**
     * Check if a prim has a specific relationship
     * 
     * @param sourcePath The source prim path
     * @param relationshipName The relationship name
     * @return Whether the prim has the relationship
     */
    bool HasRelationship(const SdfPath& sourcePath, const TfToken& relationshipName) const {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        
        auto sourceIt = m_relationships.find(sourcePath);
        if (sourceIt != m_relationships.end()) {
            return sourceIt->second.find(relationshipName) != sourceIt->second.end();
        }
        
        return false;
    }
    
    /**
     * Check if a prim is targeted by a specific relationship
     * 
     * @param targetPath The target prim path
     * @param relationshipName The relationship name (optional)
     * @return Whether the prim is targeted
     */
    bool IsTargeted(const SdfPath& targetPath, const TfToken& relationshipName = TfToken()) const {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        
        auto targetIt = m_targetToSources.find(targetPath);
        if (targetIt != m_targetToSources.end()) {
            if (!relationshipName.IsEmpty()) {
                return targetIt->second.find(relationshipName) != targetIt->second.end();
            }
            return true;
        }
        
        return false;
    }

private:
    mutable std::shared_mutex m_mutex;
    std::unordered_map<SdfPath, std::unordered_map<TfToken, SdfPathVector, TfToken::HashFunctor>, SdfPath::Hash> m_relationships;
    std::unordered_map<SdfPath, std::unordered_map<TfToken, std::unordered_set<SdfPath, SdfPath::Hash>, TfToken::HashFunctor>, SdfPath::Hash> m_targetToSources;
};

/**
 * SchemaRegistryIndex
 * 
 * A comprehensive index of USD schema types, properties, and relationships
 */
class SchemaRegistryIndex {
public:
    /**
     * Get the singleton instance
     */
    static SchemaRegistryIndex& GetInstance() {
        static SchemaRegistryIndex instance;
        return instance;
    }
    
    /**
     * Build indices for a USD stage
     * 
     * @param stage The USD stage to index
     */
    void BuildIndices(const UsdStageRefPtr& stage) {
        if (!stage) {
            return;
        }
        
        // Clear existing indices
        ClearIndices();
        
        // Set the indexed stage
        m_stage = stage;
        
        // Build indices for all prims
        for (const UsdPrim& prim : stage->TraverseAll()) {
            AddPrim(prim);
        }
    }
    
    /**
     * Add a prim to all indices
     * 
     * @param prim The USD prim to add
     */
    void AddPrim(const UsdPrim& prim) {
        if (!prim.IsValid()) {
            return;
        }
        
        // Add to schema type index
        m_schemaIndex.AddPrim(prim);
        
        // Add to property index
        m_propertyIndex.AddPrim(prim);
        
        // Add to relationship index
        m_relationshipIndex.AddPrim(prim);
    }
    
    /**
     * Remove a prim from all indices
     * 
     * @param path The USD prim path to remove
     */
    void RemovePrim(const SdfPath& path) {
        // Remove from schema type index
        m_schemaIndex.RemovePrim(path);
        
        // Remove from property index
        m_propertyIndex.RemovePrim(path);
        
        // Remove from relationship index
        m_relationshipIndex.RemovePrim(path);
    }
    
    /**
     * Clear all indices
     */
    void ClearIndices() {
        m_schemaIndex.Clear();
        m_propertyIndex.Clear();
        m_relationshipIndex.Clear();
        m_stage = nullptr;
    }
    
    /**
     * Find prims with a specific schema type
     * 
     * @param schemaType The schema type to search for
     * @return Vector of prims with the schema type
     */
    std::vector<UsdPrim> FindPrimsBySchemaType(const std::string& schemaType) const {
        std::vector<UsdPrim> result;
        
        if (!m_stage) {
            return result;
        }
        
        // Get paths from index
        std::unordered_set<SdfPath, SdfPath::Hash> paths = m_schemaIndex.FindPrimsBySchemaType(schemaType);
        
        // Convert paths to prims
        result.reserve(paths.size());
        for (const SdfPath& path : paths) {
            UsdPrim prim = m_stage->GetPrimAtPath(path);
            if (prim) {
                result.push_back(prim);
            }
        }
        
        return result;
    }
    
    /**
     * Find prims with a specific property
     * 
     * @param propertyName The property name to search for
     * @return Vector of prims with the property
     */
    std::vector<UsdPrim> FindPrimsByProperty(const TfToken& propertyName) const {
        std::vector<UsdPrim> result;
        
        if (!m_stage) {
            return result;
        }
        
        // Get paths from index
        std::unordered_set<SdfPath, SdfPath::Hash> paths = m_propertyIndex.FindPrimsByProperty(propertyName);
        
        // Convert paths to prims
        result.reserve(paths.size());
        for (const SdfPath& path : paths) {
            UsdPrim prim = m_stage->GetPrimAtPath(path);
            if (prim) {
                result.push_back(prim);
            }
        }
        
        return result;
    }
    
    /**
     * Find prims with properties matching a prefix
     * 
     * @param prefix The property name prefix to search for
     * @return Vector of prims with matching properties
     */
    std::vector<UsdPrim> FindPrimsByPropertyPrefix(const std::string& prefix) const {
        std::vector<UsdPrim> result;
        
        if (!m_stage) {
            return result;
        }
        
        // Get paths from index
        std::unordered_set<SdfPath, SdfPath::Hash> paths = m_propertyIndex.FindPrimsByPropertyPrefix(prefix);
        
        // Convert paths to prims
        result.reserve(paths.size());
        for (const SdfPath& path : paths) {
            UsdPrim prim = m_stage->GetPrimAtPath(path);
            if (prim) {
                result.push_back(prim);
            }
        }
        
        return result;
    }
    
    /**
     * Find game entities with specific component
     * 
     * @param componentType The component type (e.g., "health", "combat")
     * @return Vector of entity prims with the component
     */
    std::vector<UsdPrim> FindEntitiesByComponent(const std::string& componentType) const {
        std::string prefix = "sparkle:" + componentType + ":";
        return FindPrimsByPropertyPrefix(prefix);
    }
    
    /**
     * Find relationship targets
     * 
     * @param sourcePrim The source prim
     * @param relationshipName The relationship name
     * @return Vector of target prims
     */
    std::vector<UsdPrim> FindRelationshipTargets(const UsdPrim& sourcePrim, const TfToken& relationshipName) const {
        std::vector<UsdPrim> result;
        
        if (!m_stage || !sourcePrim) {
            return result;
        }
        
        // Get target paths from index
        SdfPathVector targetPaths = m_relationshipIndex.FindTargets(sourcePrim.GetPath(), relationshipName);
        
        // Convert paths to prims
        result.reserve(targetPaths.size());
        for (const SdfPath& path : targetPaths) {
            UsdPrim prim = m_stage->GetPrimAtPath(path);
            if (prim) {
                result.push_back(prim);
            }
        }
        
        return result;
    }
    
    /**
     * Find prims with relationships targeting a prim
     * 
     * @param targetPrim The target prim
     * @param relationshipName The relationship name (optional)
     * @return Map of relationship names to source prims
     */
    std::unordered_map<TfToken, std::vector<UsdPrim>, TfToken::HashFunctor> 
    FindRelationshipSources(const UsdPrim& targetPrim, const TfToken& relationshipName = TfToken()) const {
        std::unordered_map<TfToken, std::vector<UsdPrim>, TfToken::HashFunctor> result;
        
        if (!m_stage || !targetPrim) {
            return result;
        }
        
        // Get source paths from index
        auto sourcesMap = m_relationshipIndex.FindSources(targetPrim.GetPath(), relationshipName);
        
        // Convert paths to prims
        for (const auto& pair : sourcesMap) {
            const TfToken& relName = pair.first;
            const auto& sourcePaths = pair.second;
            
            std::vector<UsdPrim>& prims = result[relName];
            prims.reserve(sourcePaths.size());
            
            for (const SdfPath& path : sourcePaths) {
                UsdPrim prim = m_stage->GetPrimAtPath(path);
                if (prim) {
                    prims.push_back(prim);
                }
            }
        }
        
        return result;
    }
    
    /**
     * Get schema types for a prim
     * 
     * @param prim The USD prim
     * @return Set of schema types for the prim
     */
    std::unordered_set<std::string> GetSchemaTypesForPrim(const UsdPrim& prim) const {
        if (!prim) {
            return {};
        }
        
        return m_schemaIndex.GetSchemaTypesForPrim(prim.GetPath());
    }
    
    /**
     * Get properties for a prim
     * 
     * @param prim The USD prim
     * @return Set of property names for the prim
     */
    std::unordered_set<TfToken, TfToken::HashFunctor> GetPropertiesForPrim(const UsdPrim& prim) const {
        if (!prim) {
            return {};
        }
        
        return m_propertyIndex.GetPropertiesForPrim(prim.GetPath());
    }
    
    /**
     * Check if a prim has a specific schema type
     * 
     * @param prim The USD prim
     * @param schemaType The schema type to check for
     * @return Whether the prim has the schema type
     */
    bool HasSchemaType(const UsdPrim& prim, const std::string& schemaType) const {
        if (!prim) {
            return false;
        }
        
        return m_schemaIndex.HasSchemaType(prim.GetPath(), schemaType);
    }
    
    /**
     * Check if a prim has a specific property
     * 
     * @param prim The USD prim
     * @param propertyName The property name to check for
     * @return Whether the prim has the property
     */
    bool HasProperty(const UsdPrim& prim, const TfToken& propertyName) const {
        if (!prim) {
            return false;
        }
        
        return m_propertyIndex.HasProperty(prim.GetPath(), propertyName);
    }
    
    /**
     * Check if a prim has a specific relationship
     * 
     * @param prim The USD prim
     * @param relationshipName The relationship name to check for
     * @return Whether the prim has the relationship
     */
    bool HasRelationship(const UsdPrim& prim, const TfToken& relationshipName) const {
        if (!prim) {
            return false;
        }
        
        return m_relationshipIndex.HasRelationship(prim.GetPath(), relationshipName);
    }
    
    /**
     * Check if a prim is targeted by relationships
     * 
     * @param prim The USD prim
     * @param relationshipName The relationship name (optional)
     * @return Whether the prim is targeted
     */
    bool IsTargeted(const UsdPrim& prim, const TfToken& relationshipName = TfToken()) const {
        if (!prim) {
            return false;
        }
        
        return m_relationshipIndex.IsTargeted(prim.GetPath(), relationshipName);
    }
    
    /**
     * Find prims that satisfy multiple criteria
     * 
     * @param schemaType The schema type (optional)
     * @param propertyName The property name (optional)
     * @param propertyPrefix The property prefix (optional)
     * @return Vector of prims satisfying all criteria
     */
    std::vector<UsdPrim> FindPrimsWithCriteria(
        const std::string& schemaType = "",
        const TfToken& propertyName = TfToken(),
        const std::string& propertyPrefix = "") const {
        
        if (!m_stage) {
            return {};
        }
        
        // Determine the most restrictive criteria to start with
        std::unordered_set<SdfPath, SdfPath::Hash> candidatePaths;
        bool initialized = false;
        
        if (!schemaType.empty()) {
            candidatePaths = m_schemaIndex.FindPrimsBySchemaType(schemaType);
            initialized = true;
        }
        
        if (!propertyName.IsEmpty()) {
            std::unordered_set<SdfPath, SdfPath::Hash> propertyPaths = m_propertyIndex.FindPrimsByProperty(propertyName);
            
            if (!initialized) {
                candidatePaths = propertyPaths;
                initialized = true;
            } else {
                // Intersection with existing candidates
                std::unordered_set<SdfPath, SdfPath::Hash> intersection;
                for (const SdfPath& path : candidatePaths) {
                    if (propertyPaths.find(path) != propertyPaths.end()) {
                        intersection.insert(path);
                    }
                }
                candidatePaths = std::move(intersection);
            }
        }
        
        if (!propertyPrefix.empty()) {
            std::unordered_set<SdfPath, SdfPath::Hash> prefixPaths = m_propertyIndex.FindPrimsByPropertyPrefix(propertyPrefix);
            
            if (!initialized) {
                candidatePaths = prefixPaths;
                initialized = true;
            } else {
                // Intersection with existing candidates
                std::unordered_set<SdfPath, SdfPath::Hash> intersection;
                for (const SdfPath& path : candidatePaths) {
                    if (prefixPaths.find(path) != prefixPaths.end()) {
                        intersection.insert(path);
                    }
                }
                candidatePaths = std::move(intersection);
            }
        }
        
        // If no criteria specified, return empty result
        if (!initialized) {
            return {};
        }
        
        // Convert paths to prims
        std::vector<UsdPrim> result;
        result.reserve(candidatePaths.size());
        for (const SdfPath& path : candidatePaths) {
            UsdPrim prim = m_stage->GetPrimAtPath(path);
            if (prim) {
                result.push_back(prim);
            }
        }
        
        return result;
    }
    
    /**
     * Get the indexed stage
     */
    UsdStageRefPtr GetStage() const {
        return m_stage;
    }
    
    /**
     * Get access to the schema path index
     */
    const SchemaPathIndex& GetSchemaIndex() const {
        return m_schemaIndex;
    }
    
    /**
     * Get access to the property path index
     */
    const PropertyPathIndex& GetPropertyIndex() const {
        return m_propertyIndex;
    }
    
    /**
     * Get access to the relationship target index
     */
    const RelationshipTargetIndex& GetRelationshipIndex() const {
        return m_relationshipIndex;
    }

private:
    // Private constructor for singleton
    SchemaRegistryIndex() = default;
    
    // Prevent copying or moving
    SchemaRegistryIndex(const SchemaRegistryIndex&) = delete;
    SchemaRegistryIndex& operator=(const SchemaRegistryIndex&) = delete;
    SchemaRegistryIndex(SchemaRegistryIndex&&) = delete;
    SchemaRegistryIndex& operator=(SchemaRegistryIndex&&) = delete;
    
    // Member variables
    UsdStageRefPtr m_stage;
    SchemaPathIndex m_schemaIndex;
    PropertyPathIndex m_propertyIndex;
    RelationshipTargetIndex m_relationshipIndex;
};

/**
 * FilteredPrimRange
 * 
 * A class that provides filtered traversal of prims based on schema criteria
 */
class FilteredPrimRange {
public:
    class Iterator;
    
    /**
     * Constructor for schema type filter
     * 
     * @param stage The USD stage to traverse
     * @param schemaType The schema type to filter by
     */
    FilteredPrimRange(const UsdStageRefPtr& stage, const std::string& schemaType)
        : m_stage(stage)
        , m_schemaType(schemaType)
    {}
    
    /**
     * Constructor for property filter
     * 
     * @param stage The USD stage to traverse
     * @param propertyName The property name to filter by
     */
    FilteredPrimRange(const UsdStageRefPtr& stage, const TfToken& propertyName)
        : m_stage(stage)
        , m_propertyName(propertyName)
    {}
    
    /**
     * Constructor for property prefix filter
     * 
     * @param stage The USD stage to traverse
     * @param propertyPrefix The property prefix to filter by
     */
    FilteredPrimRange(const UsdStageRefPtr& stage, const std::string& propertyPrefix, bool isPrefix)
        : m_stage(stage)
        , m_propertyPrefix(propertyPrefix)
    {}
    
    /**
     * Constructor for combined filters
     * 
     * @param stage The USD stage to traverse
     * @param schemaType The schema type to filter by (optional)
     * @param propertyName The property name to filter by (optional)
     * @param propertyPrefix The property prefix to filter by (optional)
     */
    FilteredPrimRange(
        const UsdStageRefPtr& stage,
        const std::string& schemaType,
        const TfToken& propertyName,
        const std::string& propertyPrefix)
        : m_stage(stage)
        , m_schemaType(schemaType)
        , m_propertyName(propertyName)
        , m_propertyPrefix(propertyPrefix)
    {}
    
    /**
     * Get begin iterator
     */
    Iterator begin() const;
    
    /**
     * Get end iterator
     */
    Iterator end() const;
    
    /**
     * Iterator for FilteredPrimRange
     */
    class Iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = UsdPrim;
        using difference_type = std::ptrdiff_t;
        using pointer = UsdPrim*;
        using reference = UsdPrim&;
        
        /**
         * Default constructor (end iterator)
         */
        Iterator() = default;
        
        /**
         * Constructor
         * 
         * @param stage The USD stage
         * @param paths The filtered prim paths
         * @param index Current index
         */
        Iterator(UsdStageRefPtr stage, std::vector<SdfPath> paths, size_t index)
            : m_stage(stage)
            , m_paths(std::move(paths))
            , m_index(index)
        {
            // Advance to the first valid prim if needed
            if (m_index < m_paths.size()) {
                m_currentPrim = m_stage->GetPrimAtPath(m_paths[m_index]);
                if (!m_currentPrim) {
                    operator++();
                }
            }
        }
        
        /**
         * Dereference operator
         */
        UsdPrim operator*() const {
            return m_currentPrim;
        }
        
        /**
         * Arrow operator
         */
        UsdPrim* operator->() {
            return &m_currentPrim;
        }
        
        /**
         * Pre-increment operator
         */
        Iterator& operator++() {
            if (m_index < m_paths.size()) {
                ++m_index;
                while (m_index < m_paths.size()) {
                    m_currentPrim = m_stage->GetPrimAtPath(m_paths[m_index]);
                    if (m_currentPrim) {
                        break;
                    }
                    ++m_index;
                }
                if (m_index >= m_paths.size()) {
                    m_currentPrim = UsdPrim();
                }
            }
            return *this;
        }
        
        /**
         * Post-increment operator
         */
        Iterator operator++(int) {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }
        
        /**
         * Equality operator
         */
        bool operator==(const Iterator& other) const {
            if (m_index >= m_paths.size() && other.m_index >= other.m_paths.size()) {
                return true;  // Both at end
            }
            return m_stage == other.m_stage && m_index == other.m_index && m_paths == other.m_paths;
        }
        
        /**
         * Inequality operator
         */
        bool operator!=(const Iterator& other) const {
            return !(*this == other);
        }
        
    private:
        UsdStageRefPtr m_stage;
        std::vector<SdfPath> m_paths;
        size_t m_index = 0;
        UsdPrim m_currentPrim;
    };

private:
    UsdStageRefPtr m_stage;
    std::string m_schemaType;
    TfToken m_propertyName;
    std::string m_propertyPrefix;
};

/**
 * Implementation of FilteredPrimRange::begin()
 */
FilteredPrimRange::Iterator FilteredPrimRange::begin() const {
    if (!m_stage) {
        return end();
    }
    
    // Get filtered paths
    std::vector<SdfPath> paths;
    
    // Apply filters
    if (!m_schemaType.empty() && !m_propertyName.IsEmpty()) {
        // Combined schema type and property filter
        auto schemaRegistry = SchemaRegistryIndex::GetInstance();
        auto prims = schemaRegistry.FindPrimsWithCriteria(m_schemaType, m_propertyName, m_propertyPrefix);
        
        paths.reserve(prims.size());
        for (const UsdPrim& prim : prims) {
            paths.push_back(prim.GetPath());
        }
    } else if (!m_schemaType.empty()) {
        // Schema type filter
        auto schemaRegistry = SchemaRegistryIndex::GetInstance();
        auto schemaIndex = schemaRegistry.GetSchemaIndex();
        auto pathSet = schemaIndex.FindPrimsBySchemaType(m_schemaType);
        
        paths.reserve(pathSet.size());
        for (const SdfPath& path : pathSet) {
            paths.push_back(path);
        }
    } else if (!m_propertyName.IsEmpty()) {
        // Property filter
        auto schemaRegistry = SchemaRegistryIndex::GetInstance();
        auto propertyIndex = schemaRegistry.GetPropertyIndex();
        auto pathSet = propertyIndex.FindPrimsByProperty(m_propertyName);
        
        paths.reserve(pathSet.size());
        for (const SdfPath& path : pathSet) {
            paths.push_back(path);
        }
    } else if (!m_propertyPrefix.empty()) {
        // Property prefix filter
        auto schemaRegistry = SchemaRegistryIndex::GetInstance();
        auto propertyIndex = schemaRegistry.GetPropertyIndex();
        auto pathSet = propertyIndex.FindPrimsByPropertyPrefix(m_propertyPrefix);
        
        paths.reserve(pathSet.size());
        for (const SdfPath& path : pathSet) {
            paths.push_back(path);
        }
    } else {
        // No filter, traverse all prims
        for (const UsdPrim& prim : m_stage->TraverseAll()) {
            paths.push_back(prim.GetPath());
        }
    }
    
    return Iterator(m_stage, std::move(paths), 0);
}

/**
 * Implementation of FilteredPrimRange::end()
 */
FilteredPrimRange::Iterator FilteredPrimRange::end() const {
    return Iterator(m_stage, {}, std::numeric_limits<size_t>::max());
}

/**
 * Helper functions for filtered traversal
 */
namespace SchemaTraversal {
    /**
     * Get a filtered range of prims by schema type
     * 
     * @param stage The USD stage to traverse
     * @param schemaType The schema type to filter by
     * @return Filtered prim range
     */
    FilteredPrimRange FilterBySchemaType(const UsdStageRefPtr& stage, const std::string& schemaType) {
        return FilteredPrimRange(stage, schemaType);
    }
    
    /**
     * Get a filtered range of prims by property
     * 
     * @param stage The USD stage to traverse
     * @param propertyName The property name to filter by
     * @return Filtered prim range
     */
    FilteredPrimRange FilterByProperty(const UsdStageRefPtr& stage, const TfToken& propertyName) {
        return FilteredPrimRange(stage, propertyName);
    }
    
    /**
     * Get a filtered range of prims by property prefix
     * 
     * @param stage The USD stage to traverse
     * @param propertyPrefix The property prefix to filter by
     * @return Filtered prim range
     */
    FilteredPrimRange FilterByPropertyPrefix(const UsdStageRefPtr& stage, const std::string& propertyPrefix) {
        return FilteredPrimRange(stage, propertyPrefix, true);
    }
    
    /**
     * Get a filtered range of prims by component type
     * 
     * @param stage The USD stage to traverse
     * @param componentType The component type (e.g., "health", "combat")
     * @return Filtered prim range
     */
    FilteredPrimRange FilterByComponent(const UsdStageRefPtr& stage, const std::string& componentType) {
        std::string prefix = "sparkle:" + componentType + ":";
        return FilterByPropertyPrefix(stage, prefix);
    }
    
    /**
     * Get a filtered range of prims with combined criteria
     * 
     * @param stage The USD stage to traverse
     * @param schemaType The schema type to filter by (optional)
     * @param propertyName The property name to filter by (optional)
     * @param propertyPrefix The property prefix to filter by (optional)
     * @return Filtered prim range
     */
    FilteredPrimRange FilterWithCriteria(
        const UsdStageRefPtr& stage,
        const std::string& schemaType = "",
        const TfToken& propertyName = TfToken(),
        const std::string& propertyPrefix = "") {
        
        return FilteredPrimRange(stage, schemaType, propertyName, propertyPrefix);
    }
}

/**
 * Example of how to use the schema registry index
 */
void SchemaRegistryIndexExample() {
    // Create a stage with test prims
    UsdStageRefPtr stage = UsdStage::CreateInMemory();
    
    // Define enemy prims with different properties
    UsdPrim enemy1 = stage->DefinePrim(SdfPath("/Game/Enemy1"), TfToken("SparkleEnemyCarrot"));
    enemy1.CreateAttribute(TfToken("sparkle:health:current"), SdfValueTypeNames->Float).Set(100.0f);
    enemy1.CreateAttribute(TfToken("sparkle:combat:damage"), SdfValueTypeNames->Float).Set(20.0f);
    
    UsdPrim enemy2 = stage->DefinePrim(SdfPath("/Game/Enemy2"), TfToken("SparkleEnemyCarrot"));
    enemy2.CreateAttribute(TfToken("sparkle:health:current"), SdfValueTypeNames->Float).Set(150.0f);
    enemy2.CreateAttribute(TfToken("sparkle:combat:damage"), SdfValueTypeNames->Float).Set(30.0f);
    enemy2.CreateAttribute(TfToken("sparkle:movement:speed"), SdfValueTypeNames->Float).Set(5.0f);
    
    // Define player prim
    UsdPrim player = stage->DefinePrim(SdfPath("/Game/Player"), TfToken("SparklePlayer"));
    player.CreateAttribute(TfToken("sparkle:health:current"), SdfValueTypeNames->Float).Set(200.0f);
    player.CreateAttribute(TfToken("sparkle:movement:speed"), SdfValueTypeNames->Float).Set(8.0f);
    
    // Create patrol path relationship
    UsdPrim patrolPath = stage->DefinePrim(SdfPath("/Game/Paths/PatrolPath"), TfToken("Xform"));
    UsdRelationship pathRel = enemy1.CreateRelationship(TfToken("sparkle:ai:patrolPath"));
    pathRel.AddTarget(patrolPath.GetPath());
    
    // Build indices
    auto& registry = SchemaRegistryIndex::GetInstance();
    registry.BuildIndices(stage);
    
    // Example 1: Find prims by schema type
    std::vector<UsdPrim> enemies = registry.FindPrimsBySchemaType("SparkleEnemyCarrot");
    std::cout << "Found " << enemies.size() << " enemies" << std::endl;
    
    // Example 2: Find prims by property
    std::vector<UsdPrim> healthEntities = registry.FindPrimsByProperty(TfToken("sparkle:health:current"));
    std::cout << "Found " << healthEntities.size() << " entities with health" << std::endl;
    
    // Example 3: Find prims by component
    std::vector<UsdPrim> movementEntities = registry.FindEntitiesByComponent("movement");
    std::cout << "Found " << movementEntities.size() << " entities with movement component" << std::endl;
    
    // Example 4: Find relationship targets
    std::vector<UsdPrim> targets = registry.FindRelationshipTargets(enemy1, TfToken("sparkle:ai:patrolPath"));
    std::cout << "Found " << targets.size() << " patrol path targets" << std::endl;
    
    // Example 5: Find sources of relationships
    auto sourcesMap = registry.FindRelationshipSources(patrolPath);
    for (const auto& pair : sourcesMap) {
        std::cout << "Relationship '" << pair.first << "' targets patrol path from " << pair.second.size() << " sources" << std::endl;
    }
    
    // Example 6: Filtered traversal
    std::cout << "Traversing all enemies with health:" << std::endl;
    for (const UsdPrim& prim : SchemaTraversal::FilterWithCriteria(stage, "SparkleEnemyCarrot", TfToken("sparkle:health:current"))) {
        float health = 0.0f;
        prim.GetAttribute(TfToken("sparkle:health:current")).Get(&health);
        std::cout << "  " << prim.GetPath() << ": Health = " << health << std::endl;
    }
    
    // Example 7: Component-based traversal
    std::cout << "Traversing all entities with movement component:" << std::endl;
    for (const UsdPrim& prim : SchemaTraversal::FilterByComponent(stage, "movement")) {
        float speed = 0.0f;
        prim.GetAttribute(TfToken("sparkle:movement:speed")).Get(&speed);
        std::cout << "  " << prim.GetPath() << ": Speed = " << speed << std::endl;
    }
    
    // Clean up
    registry.ClearIndices();
}
