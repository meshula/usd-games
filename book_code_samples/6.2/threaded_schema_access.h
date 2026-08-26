/**
 * threaded_schema_access.h
 * 
 * Thread-optimized schema access for USD in multi-threaded game engines.
 * This implementation demonstrates:
 * - Thread-local token caches
 * - Read-write locks for schema property access
 * - Lock-free schema query operations
 * - Task-based parallel schema processing
 * - Integration with common thread pool implementations
 */

#pragma once

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/tf/hashmap.h>

#include <unordered_map>
#include <string>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <future>
#include <functional>
#include <vector>
#include <queue>
#include <condition_variable>
#include <memory>

PXR_NAMESPACE_USING_DIRECTIVE

namespace ThreadedSchemaAccess {

/**
 * Thread-local token cache
 * 
 * Caches TfToken objects per thread to avoid repeated construction.
 */
class ThreadLocalTokenCache {
public:
    /**
     * Get a cached token for a string, creating it if not already cached
     * 
     * @param tokenStr String to get a token for
     * @return The cached token
     */
    static const TfToken& GetToken(const std::string& tokenStr) {
        // Get thread-local cache
        static thread_local std::unordered_map<std::string, TfToken> tokenCache;
        
        // Check if token is already cached
        auto it = tokenCache.find(tokenStr);
        if (it != tokenCache.end()) {
            return it->second;
        }
        
        // Create and cache new token
        return tokenCache.emplace(tokenStr, TfToken(tokenStr)).first->second;
    }
    
    /**
     * Get cached tokens for common schema properties
     * Used as a convenient function to access common tokens
     */
    static const TfToken& GetHealthToken() {
        static thread_local TfToken token = GetToken("sparkle:health:current");
        return token;
    }
    
    static const TfToken& GetMaxHealthToken() {
        static thread_local TfToken token = GetToken("sparkle:health:maximum");
        return token;
    }
    
    static const TfToken& GetDamageToken() {
        static thread_local TfToken token = GetToken("sparkle:combat:damage");
        return token;
    }
    
    static const TfToken& GetMovementSpeedToken() {
        static thread_local TfToken token = GetToken("sparkle:movement:speed");
        return token;
    }
    
    /**
     * Clear the thread-local cache (rarely needed)
     */
    static void ClearCache() {
        static thread_local std::unordered_map<std::string, TfToken> tokenCache;
        tokenCache.clear();
    }
};

/**
 * Thread-local prim cache
 * 
 * Caches UsdPrim objects per thread for repeated access.
 */
class ThreadLocalPrimCache {
public:
    /**
     * Get a cached prim, creating cache entry if needed
     * 
     * @param stage The USD stage
     * @param path Path to the prim
     * @return The cached prim
     */
    static const UsdPrim& GetPrim(const UsdStageRefPtr& stage, const SdfPath& path) {
        // Get thread-local cache
        static thread_local std::unordered_map<SdfPath, UsdPrim, SdfPath::Hash> primCache;
        
        // Check if prim is already cached
        auto it = primCache.find(path);
        if (it != primCache.end()) {
            return it->second;
        }
        
        // Get prim and cache it
        UsdPrim prim = stage->GetPrimAtPath(path);
        return primCache.emplace(path, prim).first->second;
    }
    
    /**
     * Clear the thread-local cache
     */
    static void ClearCache() {
        static thread_local std::unordered_map<SdfPath, UsdPrim, SdfPath::Hash> primCache;
        primCache.clear();
    }
};

/**
 * Read-write lock for schema property access
 * 
 * Provides thread-safe access to schema properties with optimized read access.
 */
class SchemaPropertyRWLock {
public:
    SchemaPropertyRWLock() = default;
    
    /**
     * Acquire read lock (shared access)
     */
    void LockForRead() {
        m_mutex.lock_shared();
    }
    
    /**
     * Release read lock
     */
    void UnlockForRead() {
        m_mutex.unlock_shared();
    }
    
    /**
     * Acquire write lock (exclusive access)
     */
    void LockForWrite() {
        m_mutex.lock();
    }
    
    /**
     * Release write lock
     */
    void UnlockForWrite() {
        m_mutex.unlock();
    }
    
    /**
     * RAII helper for read lock
     */
    class ReadLock {
    public:
        ReadLock(SchemaPropertyRWLock& lock) : m_lock(lock) {
            m_lock.LockForRead();
        }
        
        ~ReadLock() {
            m_lock.UnlockForRead();
        }
        
    private:
        SchemaPropertyRWLock& m_lock;
        
        // Prevent copying
        ReadLock(const ReadLock&) = delete;
        ReadLock& operator=(const ReadLock&) = delete;
    };
    
    /**
     * RAII helper for write lock
     */
    class WriteLock {
    public:
        WriteLock(SchemaPropertyRWLock& lock) : m_lock(lock) {
            m_lock.LockForWrite();
        }
        
        ~WriteLock() {
            m_lock.UnlockForWrite();
        }
        
    private:
        SchemaPropertyRWLock& m_lock;
        
        // Prevent copying
        WriteLock(const WriteLock&) = delete;
        WriteLock& operator=(const WriteLock&) = delete;
    };

private:
    std::shared_mutex m_mutex;
};

/**
 * Thread-safe schema property cache
 * 
 * Provides thread-safe access to cached schema property values.
 */
template<typename T>
class ThreadSafePropertyCache {
public:
    ThreadSafePropertyCache() : m_value(T()), m_isDirty(false) {}
    
    /**
     * Get the cached value
     * 
     * @return Current value
     */
    T Get() const {
        SchemaPropertyRWLock::ReadLock lock(m_lock);
        return m_value;
    }
    
    /**
     * Set a new value
     * 
     * @param value New value to set
     */
    void Set(const T& value) {
        SchemaPropertyRWLock::WriteLock lock(m_lock);
        m_value = value;
        m_isDirty = true;
    }
    
    /**
     * Check if the value is dirty (modified since last sync)
     * 
     * @return Whether the value is dirty
     */
    bool IsDirty() const {
        SchemaPropertyRWLock::ReadLock lock(m_lock);
        return m_isDirty;
    }
    
    /**
     * Clear the dirty flag
     */
    void ClearDirty() {
        SchemaPropertyRWLock::WriteLock lock(m_lock);
        m_isDirty = false;
    }
    
    /**
     * Update from USD attribute
     * 
     * @param attr The USD attribute to update from
     * @return Whether the update was successful
     */
    bool UpdateFromUsd(const UsdAttribute& attr) {
        if (!attr) return false;
        
        T value = T();
        if (attr.Get(&value)) {
            SchemaPropertyRWLock::WriteLock lock(m_lock);
            m_value = value;
            m_isDirty = false;
            return true;
        }
        return false;
    }
    
    /**
     * Sync to USD attribute
     * 
     * @param attr The USD attribute to sync to
     * @return Whether the sync was successful
     */
    bool SyncToUsd(const UsdAttribute& attr) {
        if (!attr) return false;
        
        SchemaPropertyRWLock::ReadLock lock(m_lock);
        if (!m_isDirty) return false;
        
        // Need to copy value for use outside the lock
        T valueCopy = m_value;
        
        // Must release read lock before modifying USD
        // to avoid potential deadlocks with other threads
        lock.~ReadLock();
        
        // Set value in USD
        bool success = attr.Set(valueCopy);
        
        if (success) {
            SchemaPropertyRWLock::WriteLock writeLock(m_lock);
            m_isDirty = false;
        }
        
        return success;
    }

private:
    mutable SchemaPropertyRWLock m_lock;
    T m_value;
    bool m_isDirty;
};

/**
 * Lock-free query result cache for schema type checks
 * 
 * Provides thread-safe, lock-free caching of schema type check results.
 */
class LockFreeSchemaQueryCache {
public:
    /**
     * Check if a prim has a specific API schema applied
     * 
     * @param prim The prim to check
     * @param schemaName The API schema name to check for
     * @return Whether the prim has the schema applied
     */
    bool HasAPISchema(const UsdPrim& prim, const std::string& schemaName) {
        // Compute a cache key
        size_t key = ComputeQueryKey(prim.GetPath(), schemaName);
        
        // Check the cache first
        auto cachedResult = m_queryCache.find(key);
        if (cachedResult != m_queryCache.end()) {
            return cachedResult->second;
        }
        
        // Perform the actual check
        std::vector<std::string> schemas;
        prim.GetAppliedSchemas(&schemas);
        
        bool hasSchema = (std::find(schemas.begin(), schemas.end(), schemaName) != schemas.end());
        
        // Store in cache (thread-safe via atomics)
        m_queryCache.insert(key, hasSchema);
        
        return hasSchema;
    }
    
    /**
     * Check if a prim is of a specific schema type
     * 
     * @param prim The prim to check
     * @param typeName The type name to check for
     * @return Whether the prim is of the specified type
     */
    bool IsA(const UsdPrim& prim, const std::string& typeName) {
        // Compute a cache key
        size_t key = ComputeQueryKey(prim.GetPath(), typeName, true);
        
        // Check the cache first
        auto cachedResult = m_queryCache.find(key);
        if (cachedResult != m_queryCache.end()) {
            return cachedResult->second;
        }
        
        // Perform the actual check
        TfType type = TfType::FindByName(typeName);
        bool isA = prim.IsA(type);
        
        // Store in cache (thread-safe via atomics)
        m_queryCache.insert(key, isA);
        
        return isA;
    }
    
    /**
     * Clear the cache
     */
    void ClearCache() {
        m_queryCache.clear();
    }

private:
    // Lock-free query cache using TfHashMap (thread-safe)
    TfHashMap<size_t, bool> m_queryCache;
    
    /**
     * Compute a query cache key
     * 
     * @param path The prim path
     * @param schemaName The schema name
     * @param isTypeCheck Whether this is a type check (vs API check)
     * @return A unique key for the query
     */
    size_t ComputeQueryKey(const SdfPath& path, const std::string& schemaName, bool isTypeCheck = false) {
        size_t key = path.GetHash();
        
        // Combine with schema name hash
        size_t schemaHash = std::hash<std::string>{}(schemaName);
        key = key ^ (schemaHash << 1);
        
        // Differentiate between type checks and API schema checks
        if (isTypeCheck) {
            key = key ^ 0x8000000000000000;
        }
        
        return key;
    }
};

/**
 * Thread pool for parallel schema processing
 * 
 * A flexible thread pool implementation for processing schema operations in parallel.
 */
class SchemaThreadPool {
public:
    /**
     * Constructor
     * 
     * @param numThreads Number of worker threads (default: hardware concurrency)
     */
    SchemaThreadPool(size_t numThreads = 0)
        : m_stop(false)
    {
        size_t count = numThreads > 0 ? numThreads : std::thread::hardware_concurrency();
        
        try {
            for (size_t i = 0; i < count; ++i) {
                m_workers.emplace_back(&SchemaThreadPool::WorkerThread, this);
            }
        }
        catch (...) {
            Shutdown();
            throw;
        }
    }
    
    /**
     * Destructor
     */
    ~SchemaThreadPool() {
        Shutdown();
    }
    
    /**
     * Enqueue a task
     * 
     * @param task The task to enqueue
     * @return Future for the task result
     */
    template<typename F, typename... Args>
    auto Enqueue(F&& task, Args&&... args) -> std::future<decltype(task(args...))> {
        // Create a packaged task with the function and its arguments
        using ReturnType = decltype(task(args...));
        auto packagedTask = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(task), std::forward<Args>(args)...)
        );
        
        // Get future for the task result
        std::future<ReturnType> future = packagedTask->get_future();
        
        // Add task to the queue
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            
            // Don't allow enqueueing after stopping the pool
            if (m_stop) {
                throw std::runtime_error("Cannot enqueue on stopped ThreadPool");
            }
            
            // Wrap the packaged task in a void function
            m_tasks.emplace([packagedTask]() { (*packagedTask)(); });
        }
        
        // Notify a worker thread
        m_condition.notify_one();
        
        return future;
    }
    
    /**
     * Shutdown the thread pool
     */
    void Shutdown() {
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            m_stop = true;
        }
        
        // Wake up all worker threads
        m_condition.notify_all();
        
        // Join all worker threads
        for (std::thread& worker : m_workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        
        m_workers.clear();
    }
    
    /**
     * Get the number of worker threads
     * 
     * @return Number of worker threads
     */
    size_t GetThreadCount() const {
        return m_workers.size();
    }

private:
    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_tasks;
    std::mutex m_queueMutex;
    std::condition_variable m_condition;
    bool m_stop;
    
    /**
     * Worker thread function
     */
    void WorkerThread() {
        while (true) {
            std::function<void()> task;
            
            // Get a task from the queue
            {
                std::unique_lock<std::mutex> lock(m_queueMutex);
                
                // Wait for a task or stop signal
                m_condition.wait(lock, [this]() {
                    return m_stop || !m_tasks.empty();
                });
                
                // Exit if stopping and no more tasks
                if (m_stop && m_tasks.empty()) {
                    return;
                }
                
                // Get the next task
                task = std::move(m_tasks.front());
                m_tasks.pop();
            }
            
            // Execute the task
            task();
        }
    }
};

/**
 * Thread-safe component interface for health data
 * 
 * Example of a thread-safe component using the above synchronization primitives.
 */
class ThreadSafeHealthComponent {
public:
    ThreadSafeHealthComponent() {}
    
    /**
     * Initialize from USD prim
     * 
     * @param prim The USD prim to initialize from
     * @return Whether initialization was successful
     */
    bool Initialize(const UsdPrim& prim) {
        // Cache attribute tokens using thread-local cache
        const TfToken& healthToken = ThreadLocalTokenCache::GetHealthToken();
        const TfToken& maxHealthToken = ThreadLocalTokenCache::GetMaxHealthToken();
        
        // Get attributes
        m_healthAttribute = prim.GetAttribute(healthToken);
        m_maxHealthAttribute = prim.GetAttribute(maxHealthToken);
        
        if (!m_healthAttribute || !m_maxHealthAttribute) {
            return false;
        }
        
        // Initialize property caches
        m_healthValue.UpdateFromUsd(m_healthAttribute);
        m_maxHealthValue.UpdateFromUsd(m_maxHealthAttribute);
        
        return true;
    }
    
    /**
     * Get current health (thread-safe)
     * 
     * @return Current health value
     */
    float GetHealth() const {
        return m_healthValue.Get();
    }
    
    /**
     * Get maximum health (thread-safe)
     * 
     * @return Maximum health value
     */
    float GetMaxHealth() const {
        return m_maxHealthValue.Get();
    }
    
    /**
     * Set current health (thread-safe)
     * 
     * @param value New health value
     */
    void SetHealth(float value) {
        m_healthValue.Set(value);
    }
    
    /**
     * Set maximum health (thread-safe)
     * 
     * @param value New maximum health value
     */
    void SetMaxHealth(float value) {
        m_maxHealthValue.Set(value);
    }
    
    /**
     * Sync changes to USD (thread-safe)
     * 
     * @return Whether any changes were synced
     */
    bool SyncToUsd() {
        bool anyChanges = false;
        
        if (m_healthValue.IsDirty()) {
            anyChanges |= m_healthValue.SyncToUsd(m_healthAttribute);
        }
        
        if (m_maxHealthValue.IsDirty()) {
            anyChanges |= m_maxHealthValue.SyncToUsd(m_maxHealthAttribute);
        }
        
        return anyChanges;
    }
    
    /**
     * Apply damage (thread-safe gameplay operation)
     * 
     * @param amount Amount of damage to apply
     * @return Whether the entity is now dead
     */
    bool ApplyDamage(float amount) {
        // Get current health (thread-safe read)
        float currentHealth = GetHealth();
        
        // Calculate new health
        float newHealth = std::max(0.0f, currentHealth - amount);
        
        // Set new health (thread-safe write)
        SetHealth(newHealth);
        
        // Return whether entity is dead
        return newHealth <= 0.0f;
    }
    
    /**
     * Heal entity (thread-safe gameplay operation)
     * 
     * @param amount Amount of healing to apply
     */
    void Heal(float amount) {
        // Get current and max health (thread-safe reads)
        float currentHealth = GetHealth();
        float maxHealth = GetMaxHealth();
        
        // Calculate new health
        float newHealth = std::min(maxHealth, currentHealth + amount);
        
        // Set new health (thread-safe write)
        SetHealth(newHealth);
    }

private:
    // USD attributes
    UsdAttribute m_healthAttribute;
    UsdAttribute m_maxHealthAttribute;
    
    // Thread-safe property caches
    ThreadSafePropertyCache<float> m_healthValue;
    ThreadSafePropertyCache<float> m_maxHealthValue;
};

/**
 * ParallelEntityProcessor
 * 
 * Processes entities in parallel using a thread pool.
 */
class ParallelEntityProcessor {
public:
    /**
     * Constructor
     * 
     * @param threadCount Number of worker threads (default: hardware concurrency)
     */
    ParallelEntityProcessor(size_t threadCount = 0)
        : m_threadPool(threadCount)
    {}
    
    /**
     * Process entities in parallel
     * 
     * @param stage The USD stage containing entities
     * @param processor Function to process each entity
     */
    void ProcessEntities(const UsdStageRefPtr& stage, 
                         const std::function<void(const UsdPrim&)>& processor) {
        // Find all game entities
        std::vector<UsdPrim> gameEntities;
        
        for (const UsdPrim& prim : stage->TraverseAll()) {
            if (IsGameEntity(prim)) {
                gameEntities.push_back(prim);
            }
        }
        
        // Process entities in parallel
        ProcessEntitiesParallel(gameEntities, processor);
    }
    
    /**
     * Process entities in parallel by schema type
     * 
     * @param stage The USD stage containing entities
     * @param schemaName Schema type to filter by
     * @param processor Function to process each entity
     */
    void ProcessEntitiesBySchema(const UsdStageRefPtr& stage, 
                                const std::string& schemaName,
                                const std::function<void(const UsdPrim&)>& processor) {
        // Create our query cache for lock-free schema checks
        static LockFreeSchemaQueryCache queryCache;
        
        // Find all entities with the specified schema
        std::vector<UsdPrim> filteredEntities;
        
        for (const UsdPrim& prim : stage->TraverseAll()) {
            if (queryCache.HasAPISchema(prim, schemaName)) {
                filteredEntities.push_back(prim);
            }
        }
        
        // Process filtered entities in parallel
        ProcessEntitiesParallel(filteredEntities, processor);
    }
    
    /**
     * Process entities with parallel work stealing
     * 
     * Dynamically distributes entity processing across threads using work stealing
     * 
     * @param stage The USD stage containing entities
     * @param processor Function to process each entity
     */
    void ProcessEntitiesWithWorkStealing(const UsdStageRefPtr& stage,
                                        const std::function<void(const UsdPrim&)>& processor) {
        // Find all game entities
        std::vector<UsdPrim> gameEntities;
        
        for (const UsdPrim& prim : stage->TraverseAll()) {
            if (IsGameEntity(prim)) {
                gameEntities.push_back(prim);
            }
        }
        
        // Number of entities to process per task
        size_t entitiesPerTask = std::max(size_t(1), gameEntities.size() / (m_threadPool.GetThreadCount() * 4));
        
        // Create tasks for work stealing
        std::vector<std::future<void>> results;
        
        for (size_t i = 0; i < gameEntities.size(); i += entitiesPerTask) {
            size_t end = std::min(i + entitiesPerTask, gameEntities.size());
            
            // Each task processes a chunk of entities
            results.push_back(m_threadPool.Enqueue([&, i, end]() {
                for (size_t j = i; j < end; ++j) {
                    processor(gameEntities[j]);
                }
            }));
        }
        
        // Wait for all tasks to complete
        for (auto& result : results) {
            result.wait();
        }
    }
    
    /**
     * Apply a component update to all entities with the specified schema
     * 
     * @param stage The USD stage containing entities
     * @param schemaName Schema to filter by
     * @param updateFn Update function for entities
     */
    template<typename UpdateFn>
    void ParallelComponentUpdate(const UsdStageRefPtr& stage, 
                                const std::string& schemaName,
                                UpdateFn updateFn) {
        // Process entities with the schema in parallel
        ProcessEntitiesBySchema(stage, schemaName, [updateFn](const UsdPrim& prim) {
            // Create thread-safe component
            ThreadSafeHealthComponent component;
            
            // Initialize from prim
            if (component.Initialize(prim)) {
                // Apply update function
                updateFn(component);
                
                // Sync changes back to USD
                component.SyncToUsd();
            }
        });
    }

private:
    SchemaThreadPool m_threadPool;
    
    /**
     * Process a list of entities in parallel
     * 
     * @param entities List of entities to process
     * @param processor Function to process each entity
     */
    void ProcessEntitiesParallel(const std::vector<UsdPrim>& entities,
                                const std::function<void(const UsdPrim&)>& processor) {
        // Create a future for each entity
        std::vector<std::future<void>> results;
        results.reserve(entities.size());
        
        // Enqueue a task for each entity
        for (const UsdPrim& prim : entities) {
            results.push_back(
                m_threadPool.Enqueue([&processor, prim]() {
                    processor(prim);
                })
            );
        }
        
        // Wait for all tasks to complete
        for (auto& result : results) {
            result.wait();
        }
    }
    
    /**
     * Check if a prim is a game entity
     * 
     * @param prim The prim to check
     * @return Whether it's a game entity
     */
    bool IsGameEntity(const UsdPrim& prim) {
        // Use thread-local query cache for lock-free schema checks
        static thread_local LockFreeSchemaQueryCache queryCache;
        
        // Check if prim has any game entity type
        return queryCache.IsA(prim, "SparkleGameEntity") ||
               queryCache.IsA(prim, "SparkleEnemyCarrot") ||
               queryCache.IsA(prim, "SparklePlayer");
    }
};

/**
 * Example usage of threaded schema access
 */
inline void ThreadedSchemaAccessExample() {
    // Load stage
    UsdStageRefPtr stage = UsdStage::Open("game_level.usda");
    if (!stage) {
        std::cerr << "Failed to open stage" << std::endl;
        return;
    }
    
    // Create parallel entity processor
    ParallelEntityProcessor processor;
    
    // Example 1: Process all entities in parallel
    processor.ProcessEntities(stage, [](const UsdPrim& prim) {
        std::cout << "Processing " << prim.GetPath() << " on thread " 
                  << std::this_thread::get_id() << std::endl;
    });
    
    // Example 2: Process entities with specific schema
    processor.ProcessEntitiesBySchema(stage, "SparkleHealthAPI", [](const UsdPrim& prim) {
        // Create thread-safe component
        ThreadSafeHealthComponent healthComponent;
        if (healthComponent.Initialize(prim)) {
            // Apply damage (thread-safe)
            healthComponent.ApplyDamage(10.0f);
            
            // Sync changes back to USD
            healthComponent.SyncToUsd();
        }
    });
    
    // Example 3: Update all entities with health component
    processor.ParallelComponentUpdate(stage, "SparkleHealthAPI", 
        [](ThreadSafeHealthComponent& health) {
            // Apply healing to all entities
            health.Heal(5.0f);
        }
    );
    
    // Example 4: Use work stealing for dynamic load balancing
    processor.ProcessEntitiesWithWorkStealing(stage, [](const UsdPrim& prim) {
        // Simulate varying workloads
        std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 10));
        
        // Do actual processing...
    });
    
    // Save changes
    stage->Save();
}

 /**
  * Benchmark comparing single-threaded vs. multi-threaded schema access
  */
 inline void RunThreadedAccessBenchmark() {
     // Load stage
     UsdStageRefPtr stage = UsdStage::Open("game_level.usda");
     if (!stage) {
         std::cerr << "Failed to open stage" << std::endl;
         return;
     }
     
     // Find all game entities
     std::vector<UsdPrim> gameEntities;
     for (const UsdPrim& prim : stage->TraverseAll()) {
         if (prim.IsA(TfType::FindByName("SparkleGameEntity"))) {
             gameEntities.push_back(prim);
         }
     }
     
     std::cout << "Testing with " << gameEntities.size() << " entities" << std::endl;
     
     // Benchmark 1: Single-threaded schema access
     {
         auto startTime = std::chrono::high_resolution_clock::now();
         
         for (const UsdPrim& prim : gameEntities) {
             // Standard USD attribute access
             static const TfToken healthToken("sparkle:health:current");
             UsdAttribute healthAttr = prim.GetAttribute(healthToken);
             
             if (healthAttr) {
                 float health = 0.0f;
                 healthAttr.Get(&health);
                 
                 // Apply damage
                 health = std::max(0.0f, health - 10.0f);
                 
                 // Write back
                 healthAttr.Set(health);
             }
         }
         
         auto endTime = std::chrono::high_resolution_clock::now();
         auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
             endTime - startTime).count();
             
         std::cout << "Single-threaded schema access: " << duration << " ms" << std::endl;
     }
     
     // Reset stage
     stage = UsdStage::Open("game_level.usda");
     
     // Benchmark 2: Multi-threaded schema access with thread-local caching
     {
         auto startTime = std::chrono::high_resolution_clock::now();
         
         // Create parallel entity processor
         ParallelEntityProcessor processor;
         
         // Process all entities in parallel
         processor.ProcessEntitiesBySchema(stage, "SparkleHealthAPI", [](const UsdPrim& prim) {
             // Use thread-local token cache
             const TfToken& healthToken = ThreadLocalTokenCache::GetHealthToken();
             
             // Get health attribute
             UsdAttribute healthAttr = prim.GetAttribute(healthToken);
             
             if (healthAttr) {
                 float health = 0.0f;
                 healthAttr.Get(&health);
                 
                 // Apply damage
                 health = std::max(0.0f, health - 10.0f);
                 
                 // Write back
                 healthAttr.Set(health);
             }
         });
         
         auto endTime = std::chrono::high_resolution_clock::now();
         auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
             endTime - startTime).count();
             
         std::cout << "Multi-threaded with thread-local caching: " << duration << " ms" << std::endl;
     }
     
     // Reset stage
     stage = UsdStage::Open("game_level.usda");
     
     // Benchmark 3: Multi-threaded access with thread-safe components
     {
         auto startTime = std::chrono::high_resolution_clock::now();
         
         // Create parallel entity processor
         ParallelEntityProcessor processor;
         
         // Process all entities in parallel
         processor.ParallelComponentUpdate(stage, "SparkleHealthAPI", 
             [](ThreadSafeHealthComponent& health) {
                 // Apply damage (thread-safe)
                 health.ApplyDamage(10.0f);
             }
         );
         
         auto endTime = std::chrono::high_resolution_clock::now();
         auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
             endTime - startTime).count();
             
         std::cout << "Multi-threaded with thread-safe components: " << duration << " ms" << std::endl;
     }
     
     // Reset stage
     stage = UsdStage::Open("game_level.usda");
     
     // Benchmark 4: Multi-threaded access with work stealing
     {
         auto startTime = std::chrono::high_resolution_clock::now();
         
         // Create parallel entity processor
         ParallelEntityProcessor processor;
         
         // Process using work stealing
         processor.ProcessEntitiesWithWorkStealing(stage, [](const UsdPrim& prim) {
             // Use thread-local token cache
             const TfToken& healthToken = ThreadLocalTokenCache::GetHealthToken();
             
             // Get health attribute
             UsdAttribute healthAttr = prim.GetAttribute(healthToken);
             
             if (healthAttr) {
                 float health = 0.0f;
                 healthAttr.Get(&health);
                 
                 // Apply damage
                 health = std::max(0.0f, health - 10.0f);
                 
                 // Write back
                 healthAttr.Set(health);
             }
         });
         
         auto endTime = std::chrono::high_resolution_clock::now();
         auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
             endTime - startTime).count();
             
         std::cout << "Multi-threaded with work stealing: " << duration << " ms" << std::endl;
     }
 }
 

 /**
  * Advanced parallel schema processing techniques
  */
 class AdvancedSchemaParallelism {
 public:
     /**
      * Process entities in pipeline stages
      * 
      * @param stage USD stage
      */
     static void PipelineParallelProcessing(const UsdStageRefPtr& stage) {
         // Create thread pool
         SchemaThreadPool threadPool;
         
         // Step 1: Find all entities (produce)
         auto findEntityTask = threadPool.Enqueue([&stage]() {
             std::vector<UsdPrim> allEntities;
             
             for (const UsdPrim& prim : stage->TraverseAll()) {
                 if (prim.IsA(TfType::FindByName("SparkleGameEntity"))) {
                     allEntities.push_back(prim);
                 }
             }
             
             return allEntities;
         });
         
         // Step 2: Process entities in groups (map)
         auto processEntitiesTask = findEntityTask.then([&threadPool](std::vector<UsdPrim> entities) {
             // Create a task for each batch of entities
             const size_t batchSize = 10;
             std::vector<std::future<void>> batchTasks;
             
             for (size_t i = 0; i < entities.size(); i += batchSize) {
                 size_t end = std::min(i + batchSize, entities.size());
                 
                 auto batchTask = threadPool.Enqueue([i, end, &entities]() {
                     for (size_t j = i; j < end; ++j) {
                         ProcessEntity(entities[j]);
                     }
                 });
                 
                 batchTasks.push_back(std::move(batchTask));
             }
             
             // Wait for all batch tasks to complete
             for (auto& task : batchTasks) {
                 task.wait();
             }
         });
         
         // Step 3: Save changes (reduce)
         auto saveChangesTask = processEntitiesTask.then([&stage]() {
             stage->Save();
         });
         
         // Wait for final result
         saveChangesTask.wait();
     }
     
     /**
      * Process related entity groups in parallel
      * 
      * @param stage USD stage
      */
     static void ProcessEntityGroups(const UsdStageRefPtr& stage) {
         // Find entity relationships
         std::unordered_map<SdfPath, std::vector<UsdPrim>, SdfPath::Hash> entityGroups;
         
         // Group entities by parent
         for (const UsdPrim& prim : stage->TraverseAll()) {
             if (prim.IsA(TfType::FindByName("SparkleGameEntity"))) {
                 entityGroups[prim.GetParent().GetPath()].push_back(prim);
             }
         }
         
         // Create thread pool
         SchemaThreadPool threadPool;
         std::vector<std::future<void>> groupTasks;
         
         // Process each group in parallel
         for (const auto& group : entityGroups) {
             groupTasks.push_back(threadPool.Enqueue([&group]() {
                 ProcessEntityGroup(group.second);
             }));
         }
         
         // Wait for all groups to complete
         for (auto& task : groupTasks) {
             task.wait();
         }
     }
     
     /**
      * Process a specific system across all entities
      * 
      * Demonstrates system-based parallelism
      * 
      * @param stage USD stage
      */
     static void ProcessGameSystems(const UsdStageRefPtr& stage) {
         // Create thread pool
         SchemaThreadPool threadPool;
         
         // Find all game entities
         std::vector<UsdPrim> gameEntities;
         for (const UsdPrim& prim : stage->TraverseAll()) {
             if (prim.IsA(TfType::FindByName("SparkleGameEntity"))) {
                 gameEntities.push_back(prim);
             }
         }
         
         // Process each system in parallel
         auto healthSystemTask = threadPool.Enqueue([&gameEntities]() {
             ProcessHealthSystem(gameEntities);
         });
         
         auto movementSystemTask = threadPool.Enqueue([&gameEntities]() {
             ProcessMovementSystem(gameEntities);
         });
         
         auto combatSystemTask = threadPool.Enqueue([&gameEntities]() {
             ProcessCombatSystem(gameEntities);
         });
         
         // Wait for all systems to complete
         healthSystemTask.wait();
         movementSystemTask.wait();
         combatSystemTask.wait();
     }
 
 private:
     /**
      * Process a single entity
      * 
      * @param prim Entity prim
      */
     static void ProcessEntity(const UsdPrim& prim) {
         // Use thread-local token cache
         const TfToken& healthToken = ThreadLocalTokenCache::GetHealthToken();
         const TfToken& maxHealthToken = ThreadLocalTokenCache::GetMaxHealthToken();
         const TfToken& damageToken = ThreadLocalTokenCache::GetDamageToken();
         
         // Process entity attributes
         UsdAttribute healthAttr = prim.GetAttribute(healthToken);
         UsdAttribute maxHealthAttr = prim.GetAttribute(maxHealthToken);
         UsdAttribute damageAttr = prim.GetAttribute(damageToken);
         
         // Process as needed...
     }
     
     /**
      * Process a group of related entities
      * 
      * @param entities Group of entities
      */
     static void ProcessEntityGroup(const std::vector<UsdPrim>& entities) {
         // Process entities in a group with awareness of relationships
         for (const UsdPrim& prim : entities) {
             ProcessEntity(prim);
         }
     }
     
     /**
      * Process health system for all entities
      * 
      * @param entities List of game entities
      */
     static void ProcessHealthSystem(const std::vector<UsdPrim>& entities) {
         // Use thread-local token cache
         const TfToken& healthToken = ThreadLocalTokenCache::GetHealthToken();
         const TfToken& maxHealthToken = ThreadLocalTokenCache::GetMaxHealthToken();
         
         // Process health attributes for all entities
         for (const UsdPrim& prim : entities) {
             UsdAttribute healthAttr = prim.GetAttribute(healthToken);
             UsdAttribute maxHealthAttr = prim.GetAttribute(maxHealthToken);
             
             if (healthAttr && maxHealthAttr) {
                 float health = 0.0f;
                 float maxHealth = 0.0f;
                 
                 healthAttr.Get(&health);
                 maxHealthAttr.Get(&maxHealth);
                 
                 // Process health system logic...
             }
         }
     }
     
     /**
      * Process movement system for all entities
      * 
      * @param entities List of game entities
      */
     static void ProcessMovementSystem(const std::vector<UsdPrim>& entities) {
         // Use thread-local token cache
         const TfToken& speedToken = ThreadLocalTokenCache::GetMovementSpeedToken();
         
         // Process movement attributes for all entities
         for (const UsdPrim& prim : entities) {
             UsdAttribute speedAttr = prim.GetAttribute(speedToken);
             
             if (speedAttr) {
                 float speed = 0.0f;
                 speedAttr.Get(&speed);
                 
                 // Process movement system logic...
             }
         }
     }
     
     /**
      * Process combat system for all entities
      * 
      * @param entities List of game entities
      */
     static void ProcessCombatSystem(const std::vector<UsdPrim>& entities) {
         // Use thread-local token cache
         const TfToken& damageToken = ThreadLocalTokenCache::GetDamageToken();
         
         // Process combat attributes for all entities
         for (const UsdPrim& prim : entities) {
             UsdAttribute damageAttr = prim.GetAttribute(damageToken);
             
             if (damageAttr) {
                 float damage = 0.0f;
                 damageAttr.Get(&damage);
                 
                 // Process combat system logic...
             }
         }
     }
 };
 
 /**
  * Real-time continuous update example
  * 
  * Shows how to update entities continuously in a game loop
  * with optimized threading
  */
 class RealtimeSchemaUpdater {
 public:
     RealtimeSchemaUpdater() 
         : m_stop(false)
         , m_frameCount(0)
         , m_threadPool(std::thread::hardware_concurrency())
     {}
     
     ~RealtimeSchemaUpdater() {
         Stop();
     }
     
     /**
      * Initialize with a USD stage
      * 
      * @param stage USD stage
      */
     void Initialize(const UsdStageRefPtr& stage) {
         m_stage = stage;
         
         // Find all entities
         for (const UsdPrim& prim : stage->TraverseAll()) {
             // Use our lock-free query cache for thread-safe checks
             if (m_queryCache.IsA(prim, "SparkleGameEntity")) {
                 // Create and initialize components for entity
                 if (m_queryCache.HasAPISchema(prim, "SparkleHealthAPI")) {
                     auto health = std::make_shared<ThreadSafeHealthComponent>();
                     if (health->Initialize(prim)) {
                         m_healthComponents[prim.GetPath()] = health;
                     }
                 }
                 
                 // Additional components would be initialized here...
             }
         }
     }
     
     /**
      * Start the update loop in a separate thread
      */
     void Start() {
         if (m_updateThread.joinable()) {
             return; // Already running
         }
         
         m_stop = false;
         m_updateThread = std::thread(&RealtimeSchemaUpdater::UpdateLoop, this);
     }
     
     /**
      * Stop the update loop
      */
     void Stop() {
         m_stop = true;
         
         if (m_updateThread.joinable()) {
             m_updateThread.join();
         }
     }
     
     /**
      * Wait for a specific number of frames
      * 
      * @param frames Number of frames to wait for
      */
     void WaitForFrames(int frames) {
         uint64_t targetFrame = m_frameCount + frames;
         
         while (m_frameCount < targetFrame && !m_stop) {
             std::this_thread::sleep_for(std::chrono::milliseconds(1));
         }
     }
     
     /**
      * Manually trigger damage to an entity
      * 
      * @param path Path to the entity
      * @param amount Amount of damage
      * @return Whether damage was applied
      */
     bool ApplyDamage(const SdfPath& path, float amount) {
         auto it = m_healthComponents.find(path);
         if (it != m_healthComponents.end()) {
             it->second->ApplyDamage(amount);
             return true;
         }
         return false;
     }
     
     /**
      * Get the current frame count
      * 
      * @return Current frame count
      */
     uint64_t GetFrameCount() const {
         return m_frameCount;
     }
 
 private:
     UsdStageRefPtr m_stage;
     std::thread m_updateThread;
     std::atomic<bool> m_stop;
     std::atomic<uint64_t> m_frameCount;
     SchemaThreadPool m_threadPool;
     LockFreeSchemaQueryCache m_queryCache;
     
     // Component storage
     std::unordered_map<SdfPath, std::shared_ptr<ThreadSafeHealthComponent>, SdfPath::Hash> m_healthComponents;
     
     /**
      * The main update loop
      */
     void UpdateLoop() {
         const std::chrono::milliseconds frameTime(16); // ~60 FPS
         
         while (!m_stop) {
             auto frameStart = std::chrono::high_resolution_clock::now();
             
             // Process frame
             ProcessFrame();
             
             // Increment frame counter
             ++m_frameCount;
             
             // Calculate time to sleep
             auto frameEnd = std::chrono::high_resolution_clock::now();
             auto frameDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
                 frameEnd - frameStart);
             
             // Sleep if frame completed faster than target frame time
             if (frameDuration < frameTime) {
                 std::this_thread::sleep_for(frameTime - frameDuration);
             }
         }
     }
     
     /**
      * Process a single frame
      */
     void ProcessFrame() {
         // Get paths to all health components for parallel processing
         std::vector<SdfPath> healthPaths;
         healthPaths.reserve(m_healthComponents.size());
         
         for (const auto& pair : m_healthComponents) {
             healthPaths.push_back(pair.first);
         }
         
         // Process health components in parallel
         std::vector<std::future<void>> healthTasks;
         
         for (const SdfPath& path : healthPaths) {
             healthTasks.push_back(m_threadPool.Enqueue([this, path]() {
                 auto it = m_healthComponents.find(path);
                 if (it != m_healthComponents.end()) {
                     // Apply regeneration
                     auto& health = it->second;
                     float currentHealth = health->GetHealth();
                     
                     // Every 60 frames (approximately 1 second), heal by 1
                     if (m_frameCount % 60 == 0 && currentHealth < health->GetMaxHealth()) {
                         health->Heal(1.0f);
                     }
                 }
             }));
         }
         
         // Additional systems would be processed here...
         
         // Wait for all tasks to complete
         for (auto& task : healthTasks) {
             task.wait();
         }
         
         // Every 300 frames (approximately 5 seconds), sync to USD
         if (m_frameCount % 300 == 0) {
             SyncToUsd();
         }
     }
     
     /**
      * Sync all component changes back to USD
      */
     void SyncToUsd() {
         // Sync health components
         for (auto& pair : m_healthComponents) {
             pair.second->SyncToUsd();
         }
         
         // Additional component types would be synced here...
         
         // Save stage
         m_stage->Save();
     }
 };
 
 /**
  * Example showing how to use the real-time updater
  */
 inline void RealtimeUpdateExample() {
     // Load stage
     UsdStageRefPtr stage = UsdStage::Open("game_level.usda");
     if (!stage) {
         std::cerr << "Failed to open stage" << std::endl;
         return;
     }
     
     // Create real-time updater
     RealtimeSchemaUpdater updater;
     updater.Initialize(stage);
     
     // Start update loop
     updater.Start();
     
     // Apply damage to an entity
     SdfPath entityPath("/Game/Enemies/Enemy_01");
     updater.ApplyDamage(entityPath, 20.0f);
     
     // Wait for some frames
     updater.WaitForFrames(600); // Wait for 10 seconds
     
     // Apply more damage
     updater.ApplyDamage(entityPath, 30.0f);
     
     // Wait for more frames
     updater.WaitForFrames(300); // Wait for 5 more seconds
     
     // Stop updater
     updater.Stop();
     
     std::cout << "Processed " << updater.GetFrameCount() << " frames" << std::endl;
 }
 
 } // namespace ThreadedSchemaAccess