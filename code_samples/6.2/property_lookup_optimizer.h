/**
 * property_lookup_optimizer.h
 * 
 * Referenced in Chapter 6.2: Caching and Optimization Strategies
 * 
 * Techniques for optimizing property lookups in USD schemas, including
 * namespace-based organization, property path hashing, batch property access,
 * vectorized access, and attribute dictionary caching.
 */

#pragma once

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/tf/hash.h>
#include <pxr/base/vt/dictionary.h>

#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <memory>
#include <functional>
#include <optional>
#include <algorithm>
#include <array>
#include <type_traits>

PXR_NAMESPACE_USING_DIRECTIVE

/**
 * NamespaceOrganizer
 * 
 * Organizes properties by namespace for efficient access
 */
class NamespaceOrganizer {
public:
    /**
     * Add a property to the organizer
     * 
     * @param propertyName The property name
     * @param property The property to add (attribute or relationship)
     */
    template<typename PropertyType>
    void AddProperty(const TfToken& propertyName, const PropertyType& property) {
        std::string name = propertyName.GetString();
        
        // Find namespace separator
        size_t pos = name.find(':');
        std::string ns;
        
        if (pos != std::string::npos) {
            // Extract namespace
            ns = name.substr(0, pos);
            
            // Check for sub-namespace
            size_t nextPos = name.find(':', pos + 1);
            if (nextPos != std::string::npos) {
                ns = name.substr(0, nextPos);
            }
        }
        
        // If no namespace found, use "default"
        if (ns.empty()) {
            ns = "default";
        }
        
        // Add to namespace map
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_namespaceProperties[ns][propertyName] = property;
    }
    
    /**
     * Get properties in a namespace
     * 
     * @param ns The namespace
     * @return Map of property names to properties
     */
    template<typename PropertyType>
    std::unordered_map<TfToken, PropertyType, TfToken::HashFunctor> GetNamespaceProperties(const std::string& ns) const {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        
        auto it = m_namespaceProperties.find(ns);
        if (it != m_namespaceProperties.end()) {
            // Need to copy and cast properties
            std::unordered_map<TfToken, PropertyType, TfToken::HashFunctor> result;
            for (const auto& pair : it->second) {
                if (pair.second.template IsHolding<PropertyType>()) {
                    result[pair.first] = pair.second.template UncheckedGet<PropertyType>();
                }
            }
            return result;
        }
        
        return {};
    }
    
    /**
     * Get all namespaces
     * 
     * @return Vector of namespaces
     */
    std::vector<std::string> GetNamespaces() const {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        
        std::vector<std::string> result;
        result.reserve(m_namespaceProperties.size());
        
        for (const auto& pair : m_namespaceProperties) {
            result.push_back(pair.first);
        }
        
        return result;
    }
    
    /**
     * Get property count for a namespace
     * 
     * @param ns The namespace
     * @return Number of properties in the namespace
     */
    size_t GetNamespacePropertyCount(const std::string& ns) const {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        
        auto it = m_namespaceProperties.find(ns);
        if (it != m_namespaceProperties.end()) {
            return it->second.size();
        }
        
        return 0;
    }
    
    /**
     * Clear all properties
     */
    void Clear() {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_namespaceProperties.clear();
    }

private:
    mutable std::shared_mutex m_mutex;
    std::unordered_map<std::string, std::unordered_map<TfToken, VtValue, TfToken::HashFunctor>> m_namespaceProperties;
};

/**
 * PropertyPathHashTable
 * 
 * A fast hash table for property path lookup
 */
class PropertyPathHashTable {
public:
    /**
     * Constructor
     * 
     * @param initialCapacity Initial hash table capacity
     */
    explicit PropertyPathHashTable(size_t initialCapacity = 256)
        : m_capacity(initialCapacity)
        , m_size(0)
        , m_loadFactor(0.75)
    {
        m_table.resize(m_capacity);
    }
    
    /**
     * Add or update a property
     * 
     * @param path The property path
     * @param value The property value
     */
    template<typename T>
    void Put(const SdfPath& path, const T& value) {
        // Check if resize needed
        if (m_size >= m_capacity * m_loadFactor) {
            Resize(m_capacity * 2);
        }
        
        // Find bucket
        size_t hash = SdfPath::Hash()(path);
        size_t index = hash % m_capacity;
        
        // Check if path already exists
        for (auto& entry : m_table[index]) {
            if (entry.path == path) {
                // Update existing entry
                entry.value = VtValue(value);
                return;
            }
        }
        
        // Add new entry
        m_table[index].emplace_back(path, VtValue(value));
        ++m_size;
    }
    
    /**
     * Get a property value
     * 
     * @param path The property path
     * @param value Output parameter for the value
     * @return Whether the property was found
     */
    template<typename T>
    bool Get(const SdfPath& path, T* value) const {
        // Find bucket
        size_t hash = SdfPath::Hash()(path);
        size_t index = hash % m_capacity;
        
        // Search bucket
        for (const auto& entry : m_table[index]) {
            if (entry.path == path) {
                // Entry found, extract value
                if (entry.value.IsHolding<T>()) {
                    *value = entry.value.UncheckedGet<T>();
                    return true;
                }
                return false;
            }
        }
        
        return false;
    }
    
    /**
     * Remove a property
     * 
     * @param path The property path
     * @return Whether the property was removed
     */
    bool Remove(const SdfPath& path) {
        // Find bucket
        size_t hash = SdfPath::Hash()(path);
        size_t index = hash % m_capacity;
        
        // Search bucket
        auto& bucket = m_table[index];
        for (auto it = bucket.begin(); it != bucket.end(); ++it) {
            if (it->path == path) {
                // Remove entry
                bucket.erase(it);
                --m_size;
                return true;
            }
        }
        
        return false;
    }
    
    /**
     * Check if a property exists
     * 
     * @param path The property path
     * @return Whether the property exists
     */
    bool Contains(const SdfPath& path) const {
        // Find bucket
        size_t hash = SdfPath::Hash()(path);
        size_t index = hash % m_capacity;
        
        // Search bucket
        for (const auto& entry : m_table[index]) {
            if (entry.path == path) {
                return true;
            }
        }
        
        return false;
    }
    
    /**
     * Clear the hash table
     */
    void Clear() {
        for (auto& bucket : m_table) {
            bucket.clear();
        }
        m_size = 0;
    }
    
    /**
     * Get the number of properties
     */
    size_t Size() const {
        return m_size;
    }
    
    /**
     * Get all property paths
     * 
     * @return Vector of property paths
     */
    std::vector<SdfPath> GetAllPaths() const {
        std::vector<SdfPath> paths;
        paths.reserve(m_size);
        
        for (const auto& bucket : m_table) {
            for (const auto& entry : bucket) {
                paths.push_back(entry.path);
            }
        }
        
        return paths;
    }

private:
    /**
     * Resize the hash table
     * 
     * @param newCapacity New hash table capacity
     */
    void Resize(size_t newCapacity) {
        // Save old table
        auto oldTable = std::move(m_table);
        size_t oldCapacity = m_capacity;
        
        // Create new table
        m_capacity = newCapacity;
        m_table.resize(m_capacity);
        m_size = 0;
        
        // Rehash entries
        for (size_t i = 0; i < oldCapacity; ++i) {
            for (const auto& entry : oldTable[i]) {
                // Find new bucket
                size_t hash = SdfPath::Hash()(entry.path);
                size_t index = hash % m_capacity;
                
                // Add to new bucket
                m_table[index].push_back(entry);
                ++m_size;
            }
        }
    }
    
    struct Entry {
        SdfPath path;
        VtValue value;
        
        Entry(const SdfPath& p, const VtValue& v)
            : path(p), value(v) {}
    };
    
    std::vector<std::vector<Entry>> m_table;
    size_t m_capacity;
    size_t m_size;
    float m_loadFactor;
};

/**
 * BatchPropertyAccessor
 * 
 * A utility for accessing multiple properties in a batch
 */
class BatchPropertyAccessor {
public:
    /**
     * Constructor
     * 
     * @param prim The USD prim to access properties from
     */
    explicit BatchPropertyAccessor(const UsdPrim& prim)
        : m_prim(prim)
    {}
    
    /**
     * Add a property to the batch
     * 
     * @param propertyName The property name
     * @return Reference to this accessor for chaining
     */
    BatchPropertyAccessor& AddProperty(const TfToken& propertyName) {
        m_propertyNames.push_back(propertyName);
        return *this;
    }
    
    /**
     * Add properties to the batch
     * 
     * @param propertyNames The property names
     * @return Reference to this accessor for chaining
     */
    BatchPropertyAccessor& AddProperties(const std::vector<TfToken>& propertyNames) {
        m_propertyNames.insert(m_propertyNames.end(), propertyNames.begin(), propertyNames.end());
        return *this;
    }
    
    /**
     * Execute the batch property access
     * 
     * @return Map of property names to values
     */
    template<typename T>
    std::unordered_map<TfToken, T, TfToken::HashFunctor> Execute() const {
        std::unordered_map<TfToken, T, TfToken::HashFunctor> result;
        
        if (!m_prim) {
            return result;
        }
        
        // Access all properties in batch
        for (const TfToken& propertyName : m_propertyNames) {
            UsdAttribute attr = m_prim.GetAttribute(propertyName);
            if (attr) {
                T value;
                if (attr.Get(&value)) {
                    result[propertyName] = value;
                }
            }
        }
        
        return result;
    }
    
    /**
     * Execute the batch property access with different types
     * 
     * @param handlers Map of property names to type-specific handlers
     * @return Whether all handlers were successful
     */
    bool ExecuteWithHandlers(
        const std::unordered_map<TfToken, std::function<bool(const UsdAttribute&)>, TfToken::HashFunctor>& handlers) const {
        
        if (!m_prim) {
            return false;
        }
        
        bool allSucceeded = true;
        
        // Access properties with specific handlers
        for (const TfToken& propertyName : m_propertyNames) {
            UsdAttribute attr = m_prim.GetAttribute(propertyName);
            if (attr) {
                auto it = handlers.find(propertyName);
                if (it != handlers.end()) {
                    // Call handler
                    allSucceeded &= it->second(attr);
                }
            } else {
                allSucceeded = false;
            }
        }
        
        return allSucceeded;
    }
    
    /**
     * Clear the batch
     */
    void Clear() {
        m_propertyNames.clear();
    }
    
    /**
     * Get the prim
     */
    const UsdPrim& GetPrim() const {
        return m_prim;
    }
    
    /**
     * Get the property names
     */
    const std::vector<TfToken>& GetPropertyNames() const {
        return m_propertyNames;
    }

private:
    UsdPrim m_prim;
    std::vector<TfToken> m_propertyNames;
};

/**
 * VectorizedPropertyAccess
 * 
 * A utility for processing multiple properties in parallel
 */
class VectorizedPropertyAccess {
public:
    /**
     * Process properties in parallel
     * 
     * @param prims The USD prims to process
     * @param propertyName The property name to access
     * @param processor The function to call for each property value
     * @return Number of properties successfully processed
     */
    template<typename T>
    static int ProcessProperties(
        const std::vector<UsdPrim>& prims,
        const TfToken& propertyName,
        const std::function<void(const UsdPrim&, const T&)>& processor) {
        
        int successCount = 0;
        
        // Pre-allocate vectors
        std::vector<UsdAttribute> attributes;
        std::vector<T> values;
        attributes.reserve(prims.size());
        values.resize(prims.size());
        
        // Collect attributes
        for (const UsdPrim& prim : prims) {
            UsdAttribute attr = prim.GetAttribute(propertyName);
            attributes.push_back(attr);
        }
        
        // Process attributes in batches
        const size_t batchSize = 64;  // Adjust based on performance testing
        
        for (size_t i = 0; i < attributes.size(); i += batchSize) {
            size_t end = std::min(i + batchSize, attributes.size());
            size_t count = end - i;
            
            // Get values in batch
            for (size_t j = 0; j < count; ++j) {
                const UsdAttribute& attr = attributes[i + j];
                if (attr) {
                    attr.Get(&values[i + j]);
                }
            }
            
            // Process values in batch
            for (size_t j = 0; j < count; ++j) {
                const UsdAttribute& attr = attributes[i + j];
                if (attr) {
                    processor(prims[i + j], values[i + j]);
                    ++successCount;
                }
            }
        }
        
        return successCount;
    }
    
    /**
     * Process properties in parallel with different handlers
     * 
     * @param prims The USD prims to process
     * @param handlers Map of property names to handlers
     * @return Number of properties successfully processed
     */
    template<typename T>
    static int ProcessPropertiesWithHandlers(
        const std::vector<UsdPrim>& prims,
        const std::unordered_map<TfToken, std::function<void(const UsdPrim&, const T&)>, TfToken::HashFunctor>& handlers) {
        
        int successCount = 0;
        
        // Process each property type
        for (const auto& handler : handlers) {
            const TfToken& propertyName = handler.first;
            const auto& processor = handler.second;
            
            successCount += ProcessProperties(prims, propertyName, processor);
        }
        
        return successCount;
    }
};

/**
 * AttributeDictionaryCache
 * 
 * A cache for attribute dictionaries to avoid repeated parsing
 */
class AttributeDictionaryCache {
public:
    /**
     * Get the singleton instance
     */
    static AttributeDictionaryCache& GetInstance() {
        static AttributeDictionaryCache instance;
        return instance;
    }
    
    /**
     * Get a dictionary value
     * 
     * @param attr The USD attribute containing a dictionary
     * @return Pointer to the cached dictionary, or nullptr if not found
     */
    const VtDictionary* GetDictionary(const UsdAttribute& attr) {
        if (!attr) {
            return nullptr;
        }
        
        SdfPath path = attr.GetPath();
        
        // Check cache
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            auto it = m_dictionaries.find(path);
            if (it != m_dictionaries.end()) {
                return &it->second;
            }
        }
        
        // Not in cache, load dictionary
        VtDictionary dict;
        if (!attr.Get(&dict)) {
            return nullptr;
        }
        
        // Add to cache
        {
            std::unique_lock<std::shared_mutex> lock(m_mutex);
            auto result = m_dictionaries.emplace(path, std::move(dict));
            return &result.first->second;
        }
    }
    
    /**
     * Get a value from a dictionary attribute
     * 
     * @param attr The USD attribute containing a dictionary
     * @param key The dictionary key
     * @param value Output parameter for the value
     * @return Whether the value was found
     */
    template<typename T>
    bool GetDictionaryValue(const UsdAttribute& attr, const std::string& key, T* value) {
        const VtDictionary* dict = GetDictionary(attr);
        if (!dict) {
            return false;
        }
        
        // Find key in dictionary
        auto it = dict->find(key);
        if (it == dict->end()) {
            return false;
        }
        
        // Extract value
        const VtValue& vtValue = it->second;
        if (!vtValue.IsHolding<T>()) {
            return false;
        }
        
        *value = vtValue.UncheckedGet<T>();
        return true;
    }
    
    /**
     * Get a value from a dictionary attribute path
     * 
     * @param attr The USD attribute containing a dictionary
     * @param path The dictionary path (keys separated by dots)
     * @param value Output parameter for the value
     * @return Whether the value was found
     */
    template<typename T>
    bool GetDictionaryValueAtPath(const UsdAttribute& attr, const std::string& path, T* value) {
        const VtDictionary* dict = GetDictionary(attr);
        if (!dict) {
            return false;
        }
        
        // Split path into keys
        std::vector<std::string> keys;
        size_t start = 0;
        size_t end = path.find('.');
        
        while (end != std::string::npos) {
            keys.push_back(path.substr(start, end - start));
            start = end + 1;
            end = path.find('.', start);
        }
        
        keys.push_back(path.substr(start));
        
        // Traverse dictionary
        const VtDictionary* currentDict = dict;
        for (size_t i = 0; i < keys.size() - 1; ++i) {
            const std::string& key = keys[i];
            
            auto it = currentDict->find(key);
            if (it == currentDict->end() || !it->second.IsHolding<VtDictionary>()) {
                return false;
            }
            
            currentDict = &it->second.UncheckedGet<VtDictionary>();
        }
        
        // Get final value
        auto it = currentDict->find(keys.back());
        if (it == currentDict->end() || !it->second.IsHolding<T>()) {
            return false;
        }
        
        *value = it->second.UncheckedGet<T>();
        return true;
    }
    
    /**
     * Invalidate a cached dictionary
     * 
     * @param path The attribute path
     */
    void InvalidateDictionary(const SdfPath& path) {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_dictionaries.erase(path);
    }
    
    /**
     * Clear the cache
     */
    void Clear() {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_dictionaries.clear();
    }
    
    /**
     * Get the number of cached dictionaries
     */
    size_t Size() const {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_dictionaries.size();
    }

private:
    // Private constructor for singleton
    AttributeDictionaryCache() = default;
    
    // Prevent copying or moving
    AttributeDictionaryCache(const AttributeDictionaryCache&) = delete;
    AttributeDictionaryCache& operator=(const AttributeDictionaryCache&) = delete;
    AttributeDictionaryCache(AttributeDictionaryCache&&) = delete;
    AttributeDictionaryCache& operator=(AttributeDictionaryCache&&) = delete;
    
    // Member variables
    mutable std::shared_mutex m_mutex;
    std::unordered_map<SdfPath, VtDictionary, SdfPath::Hash> m_dictionaries;
};

/**
 * PropertyLookupOptimizer
 * 
 * A utility that combines all property lookup optimization techniques
 */
class PropertyLookupOptimizer {
public:
    /**
     * Constructor
     * 
     * @param prim The USD prim to optimize property lookups for
     */
    explicit PropertyLookupOptimizer(const UsdPrim& prim)
        : m_prim(prim)
    {
        // Initialize namespace organizer
        if (m_prim) {
            for (const UsdAttribute& attr : m_prim.GetAttributes()) {
                m_namespaceOrganizer.AddProperty(attr.GetName(), attr);
            }
            
            for (const UsdRelationship& rel : m_prim.GetRelationships()) {
                m_namespaceOrganizer.AddProperty(rel.GetName(), rel);
            }
        }
    }
    
    /**
     * Get an attribute by name with optimized lookup
     * 
     * @param name The attribute name
     * @return The USD attribute
     */
    UsdAttribute GetAttribute(const TfToken& name) const {
        if (!m_prim) {
            return UsdAttribute();
        }
        
        // Extract namespace
        std::string nameStr = name.GetString();
        std::string ns = "default";
        
        size_t pos = nameStr.find(':');
        if (pos != std::string::npos) {
            ns = nameStr.substr(0, pos);
            
            // Check for sub-namespace
            size_t nextPos = nameStr.find(':', pos + 1);
            if (nextPos != std::string::npos) {
                ns = nameStr.substr(0, nextPos);
            }
        }
        
        // Get properties in namespace
        auto properties = m_namespaceOrganizer.GetNamespaceProperties<UsdAttribute>(ns);
        
        // Find attribute in namespace
        auto it = properties.find(name);
        if (it != properties.end()) {
            return it->second;
        }
        
        // Fallback to direct lookup
        return m_prim.GetAttribute(name);
    }
    
    /**
     * Get relationship by name with optimized lookup
     * 
     * @param name The relationship name
     * @return The USD relationship
     */
    UsdRelationship GetRelationship(const TfToken& name) const {
        if (!m_prim) {
            return UsdRelationship();
        }
        
        // Extract namespace
        std::string nameStr = name.GetString();
        std::string ns = "default";
        
        size_t pos = nameStr.find(':');
        if (pos != std::string::npos) {
            ns = nameStr.substr(0, pos);
            
            // Check for sub-namespace
            size_t nextPos = nameStr.find(':', pos + 1);
            if (nextPos != std::string::npos) {
                ns = nameStr.substr(0, nextPos);
            }
        }
        
        // Get properties in namespace
        auto properties = m_namespaceOrganizer.GetNamespaceProperties<UsdRelationship>(ns);
        
        // Find relationship in namespace
        auto it = properties.find(name);
        if (it != properties.end()) {
            return it->second;
        }
        
        // Fallback to direct lookup
        return m_prim.GetRelationship(name);
    }
    
    /**
     * Get a property value with optimized lookup
     * 
     * @param name The property name
     * @param value Output parameter for the value
     * @return Whether the value was successfully retrieved
     */
    template<typename T>
    bool GetPropertyValue(const TfToken& name, T* value) const {
        UsdAttribute attr = GetAttribute(name);
        if (!attr) {
            return false;
        }
        
        return attr.Get(value);
    }
    
    /**
     * Get multiple property values in a batch
     * 
     * @param names The property names
     * @return Map of property names to values
     */
    template<typename T>
    std::unordered_map<TfToken, T, TfToken::HashFunctor> GetPropertyValues(const std::vector<TfToken>& names) const {
        BatchPropertyAccessor accessor(m_prim);
        accessor.AddProperties(names);
        return accessor.Execute<T>();
    }
    
    /**
     * Get a dictionary value with caching
     * 
     * @param name The dictionary attribute name
     * @param key The dictionary key
     * @param value Output parameter for the value
     * @return Whether the value was successfully retrieved
     */
    template<typename T>
    bool GetDictionaryValue(const TfToken& name, const std::string& key, T* value) const {
        UsdAttribute attr = GetAttribute(name);
        if (!attr) {
            return false;
        }
        
        return AttributeDictionaryCache::GetInstance().GetDictionaryValue(attr, key, value);
    }
    
    /**
     * Get a dictionary value at path with caching
     * 
     * @param name The dictionary attribute name
     * @param path The dictionary path (keys separated by dots)
     * @param value Output parameter for the value
     * @return Whether the value was successfully retrieved
     */
    template<typename T>
    bool GetDictionaryValueAtPath(const TfToken& name, const std::string& path, T* value) const {
        UsdAttribute attr = GetAttribute(name);
        if (!attr) {
            return false;
        }
        
        return AttributeDictionaryCache::GetInstance().GetDictionaryValueAtPath(attr, path, value);
    }
    
    /**
     * Get all properties in a namespace
     * 
     * @param ns The namespace
     * @return Vector of property names
     */
    std::vector<TfToken> GetNamespaceProperties(const std::string& ns) const {
        std::vector<TfToken> result;
        
        auto attributes = m_namespaceOrganizer.GetNamespaceProperties<UsdAttribute>(ns);
        auto relationships = m_namespaceOrganizer.GetNamespaceProperties<UsdRelationship>(ns);
        
        result.reserve(attributes.size() + relationships.size());
        
        for (const auto& pair : attributes) {
            result.push_back(pair.first);
        }
        
        for (const auto& pair : relationships) {
            result.push_back(pair.first);
        }
        
        return result;
    }
    
    /**
     * Get all namespaces
     * 
     * @return Vector of namespaces
     */
    std::vector<std::string> GetNamespaces() const {
        return m_namespaceOrganizer.GetNamespaces();
    }
    
    /**
     * Get the prim being optimized
     */
    const UsdPrim& GetPrim() const {
        return m_prim;
    }

private:
    UsdPrim m_prim;
    NamespaceOrganizer m_namespaceOrganizer;
};

/**
 * Example of how to use the property lookup optimizer
 */
void PropertyLookupOptimizerExample() {
    // Create a stage with a test prim
    UsdStageRefPtr stage = UsdStage::CreateInMemory();
    
    // Define a prim with properties in different namespaces
    UsdPrim prim = stage->DefinePrim(SdfPath("/Game/Entity"));
    
    // Health properties
    prim.CreateAttribute(TfToken("sparkle:health:current"), SdfValueTypeNames->Float).Set(100.0f);
    prim.CreateAttribute(TfToken("sparkle:health:maximum"), SdfValueTypeNames->Float).Set(100.0f);
    
    // Combat properties
    prim.CreateAttribute(TfToken("sparkle:combat:damage"), SdfValueTypeNames->Float).Set(20.0f);
    prim.CreateAttribute(TfToken("sparkle:combat:attackRange"), SdfValueTypeNames->Float).Set(2.0f);
    
    // Movement properties
    prim.CreateAttribute(TfToken("sparkle:movement:speed"), SdfValueTypeNames->Float).Set(5.0f);
    prim.CreateAttribute(TfToken("sparkle:movement:acceleration"), SdfValueTypeNames->Float).Set(10.0f);
    
    // Dictionary property
    VtDictionary metadata;
    metadata["name"] = VtValue(std::string("Test Entity"));
    metadata["type"] = VtValue(std::string("Enemy"));
    
    VtDictionary stats;
    stats["strength"] = VtValue(15);
    stats["dexterity"] = VtValue(12);
    stats["constitution"] = VtValue(14);
    
    metadata["stats"] = VtValue(stats);
    
    prim.CreateAttribute(TfToken("sparkle:metadata"), SdfValueTypeNames->Dictionary).Set(metadata);
    
    // Create optimizer
    PropertyLookupOptimizer optimizer(prim);
    
    // Example 1: Namespace-based organization
    std::vector<std::string> namespaces = optimizer.GetNamespaces();
    std::cout << "Namespaces: ";
    for (const std::string& ns : namespaces) {
        std::cout << ns << " ";
    }
    std::cout << std::endl;
    
    // Example 2: Get properties in a namespace
    std::vector<TfToken> healthProps = optimizer.GetNamespaceProperties("sparkle:health");
    std::cout << "Health properties: ";
    for (const TfToken& prop : healthProps) {
        std::cout << prop << " ";
    }
    std::cout << std::endl;
    
    // Example 3: Optimized property access
    float health = 0.0f;
    optimizer.GetPropertyValue(TfToken("sparkle:health:current"), &health);
    std::cout << "Current health: " << health << std::endl;
    
    // Example 4: Batch property access
    std::vector<TfToken> combatProps = {
        TfToken("sparkle:combat:damage"),
        TfToken("sparkle:combat:attackRange")
    };
    
    auto combatValues = optimizer.GetPropertyValues<float>(combatProps);
    std::cout << "Combat damage: " << combatValues[TfToken("sparkle:combat:damage")] << std::endl;
    std::cout << "Attack range: " << combatValues[TfToken("sparkle:combat:attackRange")] << std::endl;
    
    // Example 5: Dictionary cache access
    std::string entityName;
    optimizer.GetDictionaryValue(TfToken("sparkle:metadata"), "name", &entityName);
    std::cout << "Entity name: " << entityName << std::endl;
    
    // Example 6: Nested dictionary access
    int strength = 0;
    optimizer.GetDictionaryValueAtPath(TfToken("sparkle:metadata"), "stats.strength", &strength);
    std::cout << "Entity strength: " << strength << std::endl;
    
    // Example 7: Vectorized property access
    std::vector<UsdPrim> prims = { prim };
    VectorizedPropertyAccess::ProcessProperties<float>(
        prims,
        TfToken("sparkle:movement:speed"),
        [](const UsdPrim& p, const float& value) {
            std::cout << "Processed " << p.GetPath() << " with speed: " << value << std::endl;
        }
    );
}
