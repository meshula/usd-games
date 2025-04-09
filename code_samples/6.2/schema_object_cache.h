/**
 * schema_object_cache.h
 * 
 * Referenced in Chapter 6.2: Caching and Optimization Strategies
 * 
 * A comprehensive caching system for USD schema objects that helps reduce
 * string-based lookup overhead. This system pre-caches TfTokens and
 * UsdAttribute objects for commonly accessed properties, enabling efficient
 * access patterns for game engines.
 */

#pragma once

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/tf/weakBase.h>
#include <pxr/base/tf/hash.h>

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <atomic>
#include <thread>

PXR_NAMESPACE_USING_DIRECTIVE

/**
 * TokenCache
 * 
 * A cache for commonly used TfToken objects to avoid repeated string conversions
 */
class TokenCache {
public:
    /**
     * Get the global token cache instance
     */
    static TokenCache& GetInstance() {
        static TokenCache instance;
        return instance;
    }
    
    /**
     * Get a cached token
     * 
     * @param tokenStr String representation of the token
     * @return The cached TfToken
     */
    const TfToken& GetToken(const std::string& tokenStr) {
        // Check if token already exists in cache
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            auto it = m_tokenCache.find(tokenStr);
            if (it != m_tokenCache.end()) {
                return it->second;
            }
        }
        
        // If not cached, create a new token and add to cache
        {
            std::unique_lock<std::shared_mutex> lock(m_mutex);
            auto it = m_tokenCache.find(tokenStr);
            if (it != m_tokenCache.end()) {
                return it->second;  // Another thread may have added it
            }
            
            auto result = m_tokenCache.emplace(tokenStr, TfToken(tokenStr));
            return result.first->second;
        }
    }
    
    /**
     * Pre-cache a list of tokens
     * 
     * @param tokenStrs List of token strings to cache
     */
    void PreCacheTokens(const std::vector<std::string>& tokenStrs) {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        for (const auto& tokenStr : tokenStrs) {
            if (m_tokenCache.find(tokenStr) == m_tokenCache.end()) {
                m_tokenCache.emplace(tokenStr, TfToken(tokenStr));
            }
        }
    }
    
    /**
     * Remove a token from the cache
     * 
     * @param tokenStr String representation of the token to remove
     */
    void RemoveToken(const std::string& tokenStr) {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_tokenCache.erase(tokenStr);
    }
    
    /**
     * Clear all cached tokens
     */
    void Clear() {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_tokenCache.clear();
    }
    
    /**
     * Get the number of cached tokens
     */
    size_t GetCacheSize() const {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_tokenCache.size();
    }

private:
    // Private constructor for singleton
    TokenCache() {
        // Pre-cache common tokens used in most USD schemas
        PreCacheTokens({
            // Common USD property tokens
            "kind",
            "purpose",
            "extent",
            "xformOp:translate",
            "xformOp:rotateXYZ",
            "xformOp:scale",
            
            // Game-specific tokens
            "sparkle:health:current",
            "sparkle:health:maximum",
            "sparkle:combat:damage",
            "sparkle:movement:speed",
            "sparkle:ai:behavior",
            "sparkle:entity:id",
            "sparkle:entity:category",
            "sparkle:entity:enabled"
        });
    }
    
    // Prevent copying or moving
    TokenCache(const TokenCache&) = delete;
    TokenCache& operator=(const TokenCache&) = delete;
    TokenCache(TokenCache&&) = delete;
    TokenCache& operator=(TokenCache&&) = delete;
    
    // Member variables
    std::unordered_map<std::string, TfToken> m_tokenCache;
    mutable std::shared_mutex m_mutex;
};

/**
 * TokenGroup
 * 
 * A group of related tokens for a specific domain (like health, combat, etc.)
 */
class TokenGroup {
public:
    /**
     * Constructor
     * 
     * @param groupName Name of the token group
     * @param tokens Map of token names to token strings
     */
    TokenGroup(const std::string& groupName, 
              const std::unordered_map<std::string, std::string>& tokens) 
        : m_groupName(groupName)
    {
        // Pre-cache all tokens in this group
        std::vector<std::string> tokenStrs;
        tokenStrs.reserve(tokens.size());
        
        for (const auto& pair : tokens) {
            tokenStrs.push_back(pair.second);
            m_tokens[pair.first] = &TokenCache::GetInstance().GetToken(pair.second);
        }
        
        // Ensure tokens are cached
        TokenCache::GetInstance().PreCacheTokens(tokenStrs);
    }
    
    /**
     * Get a token from the group
     * 
     * @param name Name of the token within the group
     * @return Pointer to the cached token, or nullptr if not found
     */
    const TfToken* GetToken(const std::string& name) const {
        auto it = m_tokens.find(name);
        if (it != m_tokens.end()) {
            return it->second;
        }
        return nullptr;
    }
    
    /**
     * Get the group name
     */
    const std::string& GetGroupName() const {
        return m_groupName;
    }
    
    /**
     * Get all token names in this group
     */
    std::vector<std::string> GetTokenNames() const {
        std::vector<std::string> names;
        names.reserve(m_tokens.size());
        
        for (const auto& pair : m_tokens) {
            names.push_back(pair.first);
        }
        
        return names;
    }

private:
    std::string m_groupName;
    std::unordered_map<std::string, const TfToken*> m_tokens;
};

/**
 * Common token groups for game schemas
 */
namespace SchemaTokens {

// Health component tokens
static const TokenGroup HealthTokens("Health", {
    {"current", "sparkle:health:current"},
    {"maximum", "sparkle:health:maximum"},
    {"regenerationRate", "sparkle:health:regenerationRate"},
    {"invulnerable", "sparkle:health:invulnerable"}
});

// Combat component tokens
static const TokenGroup CombatTokens("Combat", {
    {"damage", "sparkle:combat:damage"},
    {"attackRadius", "sparkle:combat:attackRadius"},
    {"attackCooldown", "sparkle:combat:attackCooldown"},
    {"damageType", "sparkle:combat:damageType"}
});

// Movement component tokens
static const TokenGroup MovementTokens("Movement", {
    {"speed", "sparkle:movement:speed"},
    {"acceleration", "sparkle:movement:acceleration"},
    {"jumpHeight", "sparkle:movement:jumpHeight"},
    {"pattern", "sparkle:movement:pattern"}
});

// AI component tokens
static const TokenGroup AITokens("AI", {
    {"behavior", "sparkle:ai:behavior"},
    {"detectionRadius", "sparkle:ai:detectionRadius"},
    {"patrolPath", "sparkle:ai:patrolPath"},
    {"difficultyMultiplier", "sparkle:ai:difficultyMultiplier"}
});

// Entity tokens
static const TokenGroup EntityTokens("Entity", {
    {"id", "sparkle:entity:id"},
    {"category", "sparkle:entity:category"},
    {"enabled", "sparkle:entity:enabled"}
});

} // namespace SchemaTokens

/**
 * AttributeHandle
 * 
 * A lightweight handle for a UsdAttribute that caches the attribute object
 * and its type information for efficient access.
 */
class AttributeHandle {
public:
    /**
     * Default constructor
     */
    AttributeHandle() = default;
    
    /**
     * Constructor
     * 
     * @param attr The USD attribute to wrap
     */
    explicit AttributeHandle(const UsdAttribute& attr)
        : m_attribute(attr)
        , m_typeName(attr.GetTypeName())
        , m_isValid(attr.IsValid())
    {}
    
    /**
     * Check if the handle is valid
     */
    bool IsValid() const {
        return m_isValid && m_attribute;
    }
    
    /**
     * Get the wrapped attribute
     */
    const UsdAttribute& GetAttribute() const {
        return m_attribute;
    }
    
    /**
     * Get the attribute's type name
     */
    SdfValueTypeName GetTypeName() const {
        return m_typeName;
    }
    
    /**
     * Get the attribute's value (template specialization)
     * 
     * @param value Reference to store the retrieved value
     * @return Whether the value was successfully retrieved
     */
    template<typename T>
    bool Get(T* value) const {
        if (!IsValid()) {
            return false;
        }
        return m_attribute.Get(value);
    }
    
    /**
     * Set the attribute's value (template specialization)
     * 
     * @param value The value to set
     * @return Whether the value was successfully set
     */
    template<typename T>
    bool Set(const T& value) const {
        if (!IsValid()) {
            return false;
        }
        return m_attribute.Set(value);
    }

private:
    UsdAttribute m_attribute;
    SdfValueTypeName m_typeName;
    bool m_isValid = false;
};

/**
 * PrimAttributeCache
 * 
 * A cache for attribute handles associated with a specific prim.
 */
class PrimAttributeCache {
public:
    /**
     * Constructor
     * 
     * @param prim The USD prim to cache attributes for
     */
    explicit PrimAttributeCache(const UsdPrim& prim)
        : m_prim(prim)
    {}
    
    /**
     * Get a cached attribute handle
     * 
     * @param tokenStr String representation of the attribute token
     * @return The cached attribute handle
     */
    const AttributeHandle& GetAttributeHandle(const std::string& tokenStr) {
        // Check if attribute handle already exists in cache
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            auto it = m_attributeCache.find(tokenStr);
            if (it != m_attributeCache.end()) {
                return it->second;
            }
        }
        
        // If not cached, create a new handle and add to cache
        {
            std::unique_lock<std::shared_mutex> lock(m_mutex);
            auto it = m_attributeCache.find(tokenStr);
            if (it != m_attributeCache.end()) {
                return it->second;  // Another thread may have added it
            }
            
            const TfToken& token = TokenCache::GetInstance().GetToken(tokenStr);
            UsdAttribute attr = m_prim.GetAttribute(token);
            auto result = m_attributeCache.emplace(tokenStr, AttributeHandle(attr));
            return result.first->second;
        }
    }
    
    /**
     * Get a cached attribute handle using a pre-cached token
     * 
     * @param token Pre-cached token for the attribute
     * @return The cached attribute handle
     */
    const AttributeHandle& GetAttributeHandle(const TfToken& token) {
        return GetAttributeHandle(token.GetString());
    }
    
    /**
     * Pre-cache attribute handles for a list of token strings
     * 
     * @param tokenStrs List of token strings to cache attributes for
     */
    void PreCacheAttributes(const std::vector<std::string>& tokenStrs) {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        for (const auto& tokenStr : tokenStrs) {
            if (m_attributeCache.find(tokenStr) == m_attributeCache.end()) {
                const TfToken& token = TokenCache::GetInstance().GetToken(tokenStr);
                UsdAttribute attr = m_prim.GetAttribute(token);
                m_attributeCache.emplace(tokenStr, AttributeHandle(attr));
            }
        }
    }
    
    /**
     * Pre-cache attributes for a token group
     * 
     * @param tokenGroup Token group to cache attributes for
     */
    void PreCacheAttributes(const TokenGroup& tokenGroup) {
        std::vector<std::string> tokenStrs;
        for (const auto& name : tokenGroup.GetTokenNames()) {
            const TfToken* token = tokenGroup.GetToken(name);
            if (token) {
                tokenStrs.push_back(token->GetString());
            }
        }
        PreCacheAttributes(tokenStrs);
    }
    
    /**
     * Clear all cached attribute handles
     */
    void Clear() {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_attributeCache.clear();
    }
    
    /**
     * Get the associated prim
     */
    const UsdPrim& GetPrim() const {
        return m_prim;
    }

private:
    UsdPrim m_prim;
    std::unordered_map<std::string, AttributeHandle> m_attributeCache;
    mutable std::shared_mutex m_mutex;
};

/**
 * AttributeCacheManager
 * 
 * A manager for prim attribute caches that helps track and clean up caches.
 */
class AttributeCacheManager : public TfWeakBase {
public:
    /**
     * Get the global attribute cache manager instance
     */
    static AttributeCacheManager& GetInstance() {
        static AttributeCacheManager instance;
        return instance;
    }
    
    /**
     * Get an attribute cache for a prim
     * 
     * @param prim The USD prim to get a cache for
     * @return Shared pointer to the attribute cache
     */
    std::shared_ptr<PrimAttributeCache> GetCache(const UsdPrim& prim) {
        SdfPath path = prim.GetPath();
        
        // Check if cache already exists
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            auto it = m_caches.find(path);
            if (it != m_caches.end()) {
                std::shared_ptr<PrimAttributeCache> cache = it->second.lock();
                if (cache) {
                    return cache;
                }
                // Cache exists but has been destroyed, will be recreated below
            }
        }
        
        // Create new cache
        {
            std::unique_lock<std::shared_mutex> lock(m_mutex);
            auto it = m_caches.find(path);
            if (it != m_caches.end()) {
                std::shared_ptr<PrimAttributeCache> cache = it->second.lock();
                if (cache) {
                    return cache;  // Another thread may have created it
                }
            }
            
            auto cache = std::make_shared<PrimAttributeCache>(prim);
            m_caches[path] = cache;
            return cache;
        }
    }
    
    /**
     * Clear all caches
     */
    void ClearAll() {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_caches.clear();
    }
    
    /**
     * Purge expired caches
     */
    void PurgeExpired() {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        for (auto it = m_caches.begin(); it != m_caches.end();) {
            if (it->second.expired()) {
                it = m_caches.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    /**
     * Get the number of cached prims
     */
    size_t GetCacheCount() const {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_caches.size();
    }

private:
    // Private constructor for singleton
    AttributeCacheManager() = default;
    
    // Prevent copying or moving
    AttributeCacheManager(const AttributeCacheManager&) = delete;
    AttributeCacheManager& operator=(const AttributeCacheManager&) = delete;
    AttributeCacheManager(AttributeCacheManager&&) = delete;
    AttributeCacheManager& operator=(AttributeCacheManager&&) = delete;
    
    // Member variables
    std::unordered_map<SdfPath, std::weak_ptr<PrimAttributeCache>, SdfPath::Hash> m_caches;
    mutable std::shared_mutex m_mutex;
};

/**
 * ResultCache
 * 
 * A cache for property values to avoid repeated retrieval.
 */
template<typename T>
class ResultCache {
public:
    /**
     * Constructor
     * 
     * @param capacity Maximum number of cached results
     */
    explicit ResultCache(size_t capacity = 1000)
        : m_capacity(capacity)
    {}
    
    /**
     * Get a cached result
     * 
     * @param key The cache key (e.g. property path)
     * @param value Reference to store the retrieved value
     * @return Whether the value was found in the cache
     */
    bool Get(const std::string& key, T& value) const {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        auto it = m_cache.find(key);
        if (it != m_cache.end()) {
            value = it->second;
            return true;
        }
        return false;
    }
    
    /**
     * Set a cached result
     * 
     * @param key The cache key (e.g. property path)
     * @param value The value to cache
     */
    void Set(const std::string& key, const T& value) {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        
        // Check if we need to evict an entry
        if (m_cache.size() >= m_capacity && m_cache.find(key) == m_cache.end()) {
            // Simple LRU: remove random entry (in a real implementation would use LRU queue)
            if (!m_cache.empty()) {
                auto it = m_cache.begin();
                m_cache.erase(it);
            }
        }
        
        // Add or update entry
        m_cache[key] = value;
    }
    
    /**
     * Remove an entry from the cache
     * 
     * @param key The cache key to remove
     */
    void Remove(const std::string& key) {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_cache.erase(key);
    }
    
    /**
     * Clear all cached results
     */
    void Clear() {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_cache.clear();
    }
    
    /**
     * Get the number of cached results
     */
    size_t Size() const {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_cache.size();
    }
    
    /**
     * Check if the cache contains a key
     */
    bool Contains(const std::string& key) const {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_cache.find(key) != m_cache.end();
    }

private:
    size_t m_capacity;
    mutable std::shared_mutex m_mutex;
    std::unordered_map<std::string, T> m_cache;
};

/**
 * SchemaPropertyCache
 * 
 * A high-level class that combines token, attribute, and result caching
 * for efficient schema property access.
 */
class SchemaPropertyCache {
public:
    /**
     * Get the global schema property cache instance
     */
    static SchemaPropertyCache& GetInstance() {
        static SchemaPropertyCache instance;
        return instance;
    }
    
    /**
     * Get a property value with caching
     * 
     * @param prim The USD prim to get the property from
     * @param propertyName The property name
     * @param value Reference to store the retrieved value
     * @return Whether the value was successfully retrieved
     */
    template<typename T>
    bool GetProperty(const UsdPrim& prim, const std::string& propertyName, T& value) {
        // Generate a cache key
        std::string cacheKey = prim.GetPath().GetString() + ":" + propertyName;
        
        // Check result cache first
        if (m_resultCache.Get(cacheKey, value)) {
            return true;
        }
        
        // Get attribute handle from cache
        auto cache = AttributeCacheManager::GetInstance().GetCache(prim);
        const AttributeHandle& handle = cache->GetAttributeHandle(propertyName);
        
        // Get value from attribute
        if (handle.Get(&value)) {
            // Cache the result
            m_resultCache.Set(cacheKey, value);
            return true;
        }
        
        return false;
    }
    
    /**
     * Get a property value with cached token
     * 
     * @param prim The USD prim to get the property from
     * @param tokenGroup The token group containing the property
     * @param tokenName The token name within the group
     * @param value Reference to store the retrieved value
     * @return Whether the value was successfully retrieved
     */
    template<typename T>
    bool GetProperty(const UsdPrim& prim, const TokenGroup& tokenGroup, 
                    const std::string& tokenName, T& value) {
        const TfToken* token = tokenGroup.GetToken(tokenName);
        if (!token) {
            return false;
        }
        
        return GetProperty(prim, token->GetString(), value);
    }
    
    /**
     * Set a property value with caching
     * 
     * @param prim The USD prim to set the property on
     * @param propertyName The property name
     * @param value The value to set
     * @return Whether the value was successfully set
     */
    template<typename T>
    bool SetProperty(const UsdPrim& prim, const std::string& propertyName, const T& value) {
        // Generate a cache key
        std::string cacheKey = prim.GetPath().GetString() + ":" + propertyName;
        
        // Get attribute handle from cache
        auto cache = AttributeCacheManager::GetInstance().GetCache(prim);
        const AttributeHandle& handle = cache->GetAttributeHandle(propertyName);
        
        // Set value on attribute
        if (handle.Set(value)) {
            // Update result cache
            m_resultCache.Set(cacheKey, value);
            return true;
        }
        
        return false;
    }
    
    /**
     * Set a property value with cached token
     * 
     * @param prim The USD prim to set the property on
     * @param tokenGroup The token group containing the property
     * @param tokenName The token name within the group
     * @param value The value to set
     * @return Whether the value was successfully set
     */
    template<typename T>
    bool SetProperty(const UsdPrim& prim, const TokenGroup& tokenGroup, 
                    const std::string& tokenName, const T& value) {
        const TfToken* token = tokenGroup.GetToken(tokenName);
        if (!token) {
            return false;
        }
        
        return SetProperty(prim, token->GetString(), value);
    }
    
    /**
     * Invalidate a cached property value
     * 
     * @param prim The USD prim containing the property
     * @param propertyName The property name
     */
    void InvalidateProperty(const UsdPrim& prim, const std::string& propertyName) {
        std::string cacheKey = prim.GetPath().GetString() + ":" + propertyName;
        m_resultCache.Remove(cacheKey);
    }
    
    /**
     * Invalidate all cached values for a prim
     * 
     * @param prim The USD prim to invalidate
     */
    void InvalidatePrim(const UsdPrim& prim) {
        // This is a simple implementation that clears the entire cache
        // A more sophisticated implementation would only clear entries for the specific prim
        m_resultCache.Clear();
    }
    
    /**
     * Clear all cached values
     */
    void ClearAll() {
        m_resultCache.Clear();
    }

private:
    // Private constructor for singleton
    SchemaPropertyCache() = default;
    
    // Prevent copying or moving
    SchemaPropertyCache(const SchemaPropertyCache&) = delete;
    SchemaPropertyCache& operator=(const SchemaPropertyCache&) = delete;
    SchemaPropertyCache(SchemaPropertyCache&&) = delete;
    SchemaPropertyCache& operator=(SchemaPropertyCache&&) = delete;
    
    // Member variables - using different result caches for different types
    ResultCache<float> m_resultCache;
    
    // For a real implementation, you would use multiple caches for different types,
    // or a type-erased value container
};

/**
 * ThreadLocalTokenCache
 * 
 * A thread-local cache for TfToken objects to avoid mutex contention.
 */
class ThreadLocalTokenCache {
public:
    /**
     * Get the thread-local token cache instance
     */
    static ThreadLocalTokenCache& GetInstance() {
        static thread_local ThreadLocalTokenCache instance;
        return instance;
    }
    
    /**
     * Get a cached token
     * 
     * @param tokenStr String representation of the token
     * @return The cached TfToken
     */
    const TfToken& GetToken(const std::string& tokenStr) {
        auto it = m_tokenCache.find(tokenStr);
        if (it != m_tokenCache.end()) {
            return it->second;
        }
        
        // If not cached, check global cache first
        const TfToken& globalToken = TokenCache::GetInstance().GetToken(tokenStr);
        
        // Add to thread-local cache
        auto result = m_tokenCache.emplace(tokenStr, globalToken);
        return result.first->second;
    }
    
    /**
     * Pre-cache a list of tokens
     * 
     * @param tokenStrs List of token strings to cache
     */
    void PreCacheTokens(const std::vector<std::string>& tokenStrs) {
        for (const auto& tokenStr : tokenStrs) {
            if (m_tokenCache.find(tokenStr) == m_tokenCache.end()) {
                const TfToken& globalToken = TokenCache::GetInstance().GetToken(tokenStr);
                m_tokenCache.emplace(tokenStr, globalToken);
            }
        }
    }
    
    /**
     * Clear all cached tokens
     */
    void Clear() {
        m_tokenCache.clear();
    }
    
    /**
     * Get the number of cached tokens
     */
    size_t GetCacheSize() const {
        return m_tokenCache.size();
    }

private:
    // Private constructor for singleton
    ThreadLocalTokenCache() = default;
    
    // Prevent copying or moving
    ThreadLocalTokenCache(const ThreadLocalTokenCache&) = delete;
    ThreadLocalTokenCache& operator=(const ThreadLocalTokenCache&) = delete;
    ThreadLocalTokenCache(ThreadLocalTokenCache&&) = delete;
    ThreadLocalTokenCache& operator=(ThreadLocalTokenCache&&) = delete;
    
    // Member variables - no mutex needed for thread-local data
    std::unordered_map<std::string, TfToken> m_tokenCache;
};

/**
 * GameComponentCache
 * 
 * A specialized cache for game component properties that provides
 * component-specific accessors for commonly used attributes.
 */
class GameComponentCache {
public:
    /**
     * Constructor
     * 
     * @param prim The USD prim to cache components for
     */
    explicit GameComponentCache(const UsdPrim& prim)
        : m_prim(prim)
        , m_attrCache(AttributeCacheManager::GetInstance().GetCache(prim))
    {
        // Pre-cache commonly used attributes
        m_attrCache->PreCacheAttributes(SchemaTokens::HealthTokens);
        m_attrCache->PreCacheAttributes(SchemaTokens::CombatTokens);
        m_attrCache->PreCacheAttributes(SchemaTokens::MovementTokens);
        m_attrCache->PreCacheAttributes(SchemaTokens::AITokens);
        m_attrCache->PreCacheAttributes(SchemaTokens::EntityTokens);
    }
    
    /**
     * Check if the prim has a health component
     */
    bool HasHealthComponent() const {
        // Check for required health attributes
        const AttributeHandle& currentHealth = m_attrCache->GetAttributeHandle(
            *SchemaTokens::HealthTokens.GetToken("current"));
        return currentHealth.IsValid();
    }
    
    /**
     * Get current health
     */
    float GetCurrentHealth() const {
        float health = 0.0f;
        SchemaPropertyCache::GetInstance().GetProperty(
            m_prim, SchemaTokens::HealthTokens, "current", health);
        return health;
    }
    
    /**
     * Set current health
     */
    bool SetCurrentHealth(float health) {
        return SchemaPropertyCache::GetInstance().SetProperty(
            m_prim, SchemaTokens::HealthTokens, "current", health);
    }
    
    /**
     * Get maximum health
     */
    float GetMaxHealth() const {
        float maxHealth = 0.0f;
        SchemaPropertyCache::GetInstance().GetProperty(
            m_prim, SchemaTokens::HealthTokens, "maximum", maxHealth);
        return maxHealth;
    }
    
    /**
     * Check if the prim has a combat component
     */
    bool HasCombatComponent() const {
        // Check for required combat attributes
        const AttributeHandle& damage = m_attrCache->GetAttributeHandle(
            *SchemaTokens::CombatTokens.GetToken("damage"));
        return damage.IsValid();
    }
    
    /**
     * Get damage
     */
    float GetDamage() const {
        float damage = 0.0f;
        SchemaPropertyCache::GetInstance().GetProperty(
            m_prim, SchemaTokens::CombatTokens, "damage", damage);
        return damage;
    }
    
    /**
     * Get damage type
     */
    std::string GetDamageType() const {
        TfToken damageType;
        SchemaPropertyCache::GetInstance().GetProperty(
            m_prim, SchemaTokens::CombatTokens, "damageType", damageType);
        return damageType.GetString();
    }
    
    /**
     * Check if the prim has a movement component
     */
    bool HasMovementComponent() const {
        // Check for required movement attributes
        const AttributeHandle& speed = m_attrCache->GetAttributeHandle(
            *SchemaTokens::MovementTokens.GetToken("speed"));
        return speed.IsValid();
    }
    
    /**
     * Get movement speed
     */
    float GetMovementSpeed() const {
        float speed = 0.0f;
        SchemaPropertyCache::GetInstance().GetProperty(
            m_prim, SchemaTokens::MovementTokens, "speed", speed);
        return speed;
    }
    
    /**
     * Get movement pattern
     */
    std::string GetMovementPattern() const {
        TfToken pattern;
        SchemaPropertyCache::GetInstance().GetProperty(
            m_prim, SchemaTokens::MovementTokens, "pattern", pattern);
        return pattern.GetString();
    }
    
    /**
     * Check if the prim has an AI component
     */
    bool HasAIComponent() const {
        // Check for required AI attributes
        const AttributeHandle& behavior = m_attrCache->GetAttributeHandle(
            *SchemaTokens::AITokens.GetToken("behavior"));
        return behavior.IsValid();
    }
    
    /**
     * Get AI behavior
     */
    std::string GetAIBehavior() const {
        TfToken behavior;
        SchemaPropertyCache::GetInstance().GetProperty(
            m_prim, SchemaTokens::AITokens, "behavior", behavior);
        return behavior.GetString();
    }
    
    /**
     * Get entity ID
     */
    std::string GetEntityId() const {
        std::string id;
        SchemaPropertyCache::GetInstance().GetProperty(
            m_prim, SchemaTokens::EntityTokens, "id", id);
        return id;
    }
    
    /**
     * Get the underlying prim
     */
    const UsdPrim& GetPrim() const {
        return m_prim;
    }
    
    /**
     * Get the attribute cache
     */
    std::shared_ptr<PrimAttributeCache> GetAttributeCache() const {
        return m_attrCache;
    }

private:
    UsdPrim m_prim;
    std::shared_ptr<PrimAttributeCache> m_attrCache;
};

/**
 * GameComponentManager
 * 
 * A manager for game component caches that helps track and clean up caches.
 */
class GameComponentManager {
public:
    /**
     * Get the global game component manager instance
     */
    static GameComponentManager& GetInstance() {
        static GameComponentManager instance;
        return instance;
    }
    
    /**
     * Get a component cache for a prim
     * 
     * @param prim The USD prim to get a cache for
     * @return Shared pointer to the component cache
     */
    std::shared_ptr<GameComponentCache> GetComponentCache(const UsdPrim& prim) {
        SdfPath path = prim.GetPath();
        
        // Check if cache already exists
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            auto it = m_caches.find(path);
            if (it != m_caches.end()) {
                std::shared_ptr<GameComponentCache> cache = it->second.lock();
                if (cache) {
                    return cache;
                }
                // Cache exists but has been destroyed, will be recreated below
            }
        }
        
        // Create new cache
        {
            std::unique_lock<std::shared_mutex> lock(m_mutex);
            auto it = m_caches.find(path);
            if (it != m_caches.end()) {
                std::shared_ptr<GameComponentCache> cache = it->second.lock();
                if (cache) {
                    return cache;  // Another thread may have created it
                }
            }
            
            auto cache = std::make_shared<GameComponentCache>(prim);
            m_caches[path] = cache;
            return cache;
        }
    }
    
    /**
     * Clear all caches
     */
    void ClearAll() {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_caches.clear();
    }
    
    /**
     * Purge expired caches
     */
    void PurgeExpired() {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        for (auto it = m_caches.begin(); it != m_caches.end();) {
            if (it->second.expired()) {
                it = m_caches.erase(it);
            } else {
                ++it;
            }
        }
    }

private:
    // Private constructor for singleton
    GameComponentManager() = default;
    
    // Prevent copying or moving
    GameComponentManager(const GameComponentManager&) = delete;
    GameComponentManager& operator=(const GameComponentManager&) = delete;
    GameComponentManager(GameComponentManager&&) = delete;
    GameComponentManager& operator=(GameComponentManager&&) = delete;
    
    // Member variables
    std::unordered_map<SdfPath, std::weak_ptr<GameComponentCache>, SdfPath::Hash> m_caches;
    mutable std::shared_mutex m_mutex;
};

// Example usage
/**
 * Example of how to use the schema object caching system
 */
void SchemaObjectCacheExample() {
    // Example usage of token cache
    const TfToken& healthToken = TokenCache::GetInstance().GetToken("sparkle:health:current");
    
    // Example usage of token groups
    const TfToken* damageToken = SchemaTokens::CombatTokens.GetToken("damage");
    
    // Example usage of thread-local token cache
    const TfToken& speedToken = ThreadLocalTokenCache::GetInstance().GetToken("sparkle:movement:speed");
    
    // Example usage with a prim
    UsdStageRefPtr stage = UsdStage::CreateInMemory();
    UsdPrim prim = stage->DefinePrim(SdfPath("/Game/Enemy"));
    
    // Create attributes
    prim.CreateAttribute(healthToken, SdfValueTypeNames->Float).Set(100.0f);
    prim.CreateAttribute(*damageToken, SdfValueTypeNames->Float).Set(20.0f);
    prim.CreateAttribute(speedToken, SdfValueTypeNames->Float).Set(5.0f);
    
    // Use attribute cache
    auto attrCache = AttributeCacheManager::GetInstance().GetCache(prim);
    const AttributeHandle& healthHandle = attrCache->GetAttributeHandle(healthToken);
    
    float health = 0.0f;
    healthHandle.Get(&health);
    
    // Use result cache
    float cachedDamage = 0.0f;
    SchemaPropertyCache::GetInstance().GetProperty(prim, "sparkle:combat:damage", cachedDamage);
    
    // Use component cache
    auto componentCache = GameComponentManager::GetInstance().GetComponentCache(prim);
    float speed = componentCache->GetMovementSpeed();
    
    // Print results
    std::cout << "Health: " << health << std::endl;
    std::cout << "Damage: " << cachedDamage << std::endl;
    std::cout << "Speed: " << speed << std::endl;
}
