/**
 * component_attribute_cache.h
 * 
 * An entity-component system that efficiently caches schema properties
 * for high-performance access in game engines. This implementation bridges
 * the gap between USD schemas and traditional game engine component systems.
 */

#pragma once

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/tf/type.h>
#include <pxr/base/gf/vec3f.h>

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <type_traits>
#include <typeinfo>
#include <cassert>
#include <chrono>

PXR_NAMESPACE_USING_DIRECTIVE

/**
 * CachedAttributeBase
 * 
 * Base class for cached attributes providing common functionality
 */
class CachedAttributeBase {
public:
    /**
     * Constructor
     * 
     * @param attr The USD attribute to cache
     * @param attrToken The token for the attribute name
     */
    CachedAttributeBase(const UsdAttribute& attr, const TfToken& attrToken) 
        : m_attribute(attr)
        , m_attributeName(attrToken)
        , m_isDirty(false) 
    {}
    
    virtual ~CachedAttributeBase() = default;
    
    /**
     * Get the attribute name
     */
    const TfToken& GetAttributeName() const { return m_attributeName; }
    
    /**
     * Get the USD attribute
     */
    const UsdAttribute& GetAttribute() const { return m_attribute; }
    
    /**
     * Mark the attribute as dirty, needing sync to USD
     */
    void MarkDirty() { m_isDirty = true; }
    
    /**
     * Check if the attribute is dirty
     */
    bool IsDirty() const { return m_isDirty; }
    
    /**
     * Reset the dirty flag after syncing to USD
     */
    void ResetDirty() { m_isDirty = false; }
    
    /**
     * Virtual function to update the USD attribute with the cached value
     * 
     * @return Whether the update was successful
     */
    virtual bool SyncToUsd() = 0;
    
    /**
     * Virtual function to update the cached value from the USD attribute
     * 
     * @return Whether the update was successful
     */
    virtual bool SyncFromUsd() = 0;
    
    /**
     * Get the name of the value type
     */
    virtual std::string GetTypeName() const = 0;

protected:
    UsdAttribute m_attribute;
    TfToken m_attributeName;
    bool m_isDirty;
};

/**
 * CachedAttribute<T>
 * 
 * Template class for caching attributes of specific types
 */
template<typename T>
class CachedAttribute : public CachedAttributeBase {
public:
    /**
     * Constructor
     * 
     * @param attr The USD attribute to cache
     * @param attrToken The token for the attribute name
     * @param defaultValue Default value to use if attribute doesn't exist
     */
    CachedAttribute(const UsdAttribute& attr, const TfToken& attrToken, const T& defaultValue = T()) 
        : CachedAttributeBase(attr, attrToken)
        , m_value(defaultValue)
        , m_defaultValue(defaultValue)
    {
        // Initialize with current value from USD if available
        if (attr) {
            SyncFromUsd();
        }
    }
    
    /**
     * Get the cached value
     */
    const T& Get() const { return m_value; }
    
    /**
     * Set a new value and mark dirty
     * 
     * @param value The new value
     */
    void Set(const T& value) {
        if (m_value != value) {
            m_value = value;
            MarkDirty();
        }
    }
    
    /**
     * Update the USD attribute with the cached value
     * 
     * @return Whether the update was successful
     */
    virtual bool SyncToUsd() override {
        if (!m_attribute) {
            return false;
        }
        
        if (m_isDirty) {
            bool success = m_attribute.Set(m_value);
            if (success) {
                ResetDirty();
            }
            return success;
        }
        
        return true; // Nothing to update
    }
    
    /**
     * Update the cached value from the USD attribute
     * 
     * @return Whether the update was successful
     */
    virtual bool SyncFromUsd() override {
        if (!m_attribute) {
            m_value = m_defaultValue;
            return false;
        }
        
        T value;
        bool success = m_attribute.Get(&value);
        
        if (success) {
            m_value = value;
            ResetDirty();
        } else {
            m_value = m_defaultValue;
        }
        
        return success;
    }
    
    /**
     * Get the name of the value type
     */
    virtual std::string GetTypeName() const override {
        return typeid(T).name();
    }

private:
    T m_value;
    T m_defaultValue;
};

/**
 * ComponentBase
 * 
 * Base class for all components in the entity-component system
 */
class ComponentBase {
public:
    ComponentBase() = default;
    virtual ~ComponentBase() = default;
    
    /**
     * Initialize the component from a USD prim
     * 
     * @param prim The USD prim containing component data
     * @return Whether initialization was successful
     */
    virtual bool Initialize(const UsdPrim& prim) = 0;
    
    /**
     * Sync all cached values to USD
     * 
     * @return Whether sync was successful
     */
    virtual bool SyncToUsd() = 0;
    
    /**
     * Sync all cached values from USD
     * 
     * @return Whether sync was successful
     */
    virtual bool SyncFromUsd() = 0;
    
    /**
     * Get the name of the component type
     */
    virtual std::string GetTypeName() const = 0;
    
    /**
     * Check if the component is dirty and needs sync
     */
    virtual bool IsDirty() const = 0;
    
    /**
     * Mark the component as enabled/disabled
     */
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    
    /**
     * Check if the component is enabled
     */
    bool IsEnabled() const { return m_enabled; }

protected:
    bool m_enabled = true;
};

/**
 * HealthComponent
 * 
 * Example component for health-related attributes with optimized caching
 */
class HealthComponent : public ComponentBase {
public:
    HealthComponent() = default;
    
    /**
     * Initialize from USD prim with SparkleHealthAPI schema
     */
    virtual bool Initialize(const UsdPrim& prim) override {
        if (!prim.IsValid()) {
            return false;
        }
        
        m_prim = prim;
        
        // Cache attribute tokens (only done once per component type)
        static const TfToken currentHealthToken("sparkle:health:current");
        static const TfToken maxHealthToken("sparkle:health:maximum");
        static const TfToken invulnerableToken("sparkle:health:invulnerable");
        static const TfToken regenRateToken("sparkle:health:regenerationRate");
        
        // Create cached attributes with appropriate defaults
        m_currentHealth = std::make_unique<CachedAttribute<float>>(
            prim.GetAttribute(currentHealthToken), currentHealthToken, 100.0f);
        
        m_maxHealth = std::make_unique<CachedAttribute<float>>(
            prim.GetAttribute(maxHealthToken), maxHealthToken, 100.0f);
        
        m_invulnerable = std::make_unique<CachedAttribute<bool>>(
            prim.GetAttribute(invulnerableToken), invulnerableToken, false);
        
        m_regenerationRate = std::make_unique<CachedAttribute<float>>(
            prim.GetAttribute(regenRateToken), regenRateToken, 0.0f);
        
        return true;
    }
    
    /**
     * Sync all health attributes to USD
     */
    virtual bool SyncToUsd() override {
        if (!m_prim) {
            return false;
        }
        
        bool success = true;
        success &= m_currentHealth->SyncToUsd();
        success &= m_maxHealth->SyncToUsd();
        success &= m_invulnerable->SyncToUsd();
        success &= m_regenerationRate->SyncToUsd();
        
        return success;
    }
    
    /**
     * Sync all health attributes from USD
     */
    virtual bool SyncFromUsd() override {
        if (!m_prim) {
            return false;
        }
        
        bool success = true;
        success &= m_currentHealth->SyncFromUsd();
        success &= m_maxHealth->SyncFromUsd();
        success &= m_invulnerable->SyncFromUsd();
        success &= m_regenerationRate->SyncFromUsd();
        
        return success;
    }
    
    /**
     * Check if any health attributes are dirty
     */
    virtual bool IsDirty() const override {
        return m_currentHealth->IsDirty() ||
               m_maxHealth->IsDirty() ||
               m_invulnerable->IsDirty() ||
               m_regenerationRate->IsDirty();
    }
    
    /**
     * Get component type name
     */
    virtual std::string GetTypeName() const override {
        return "HealthComponent";
    }
    
    // Fast accessors for cached values
    float GetCurrentHealth() const { return m_currentHealth->Get(); }
    float GetMaxHealth() const { return m_maxHealth->Get(); }
    bool IsInvulnerable() const { return m_invulnerable->Get(); }
    float GetRegenerationRate() const { return m_regenerationRate->Get(); }
    
    // Mutators that automatically mark as dirty
    void SetCurrentHealth(float value) { m_currentHealth->Set(value); }
    void SetMaxHealth(float value) { m_maxHealth->Set(value); }
    void SetInvulnerable(bool value) { m_invulnerable->Set(value); }
    void SetRegenerationRate(float value) { m_regenerationRate->Set(value); }
    
    // Game-specific helper methods using cached values
    bool IsDead() const { return GetCurrentHealth() <= 0.0f; }
    
    void TakeDamage(float damage) {
        if (IsInvulnerable()) return;
        
        float newHealth = GetCurrentHealth() - damage;
        newHealth = std::max(0.0f, newHealth);
        SetCurrentHealth(newHealth);
    }
    
    void Heal(float amount) {
        float newHealth = GetCurrentHealth() + amount;
        newHealth = std::min(newHealth, GetMaxHealth());
        SetCurrentHealth(newHealth);
    }
    
    // Update method for game logic
    void Update(float deltaTime) {
        if (GetRegenerationRate() > 0.0f && GetCurrentHealth() < GetMaxHealth()) {
            float newHealth = GetCurrentHealth() + (GetRegenerationRate() * deltaTime);
            newHealth = std::min(newHealth, GetMaxHealth());
            SetCurrentHealth(newHealth);
        }
    }

private:
    UsdPrim m_prim;
    std::unique_ptr<CachedAttribute<float>> m_currentHealth;
    std::unique_ptr<CachedAttribute<float>> m_maxHealth;
    std::unique_ptr<CachedAttribute<bool>> m_invulnerable;
    std::unique_ptr<CachedAttribute<float>> m_regenerationRate;
};

/**
 * MovementComponent
 * 
 * Example component for movement-related attributes with optimized caching
 */
class MovementComponent : public ComponentBase {
public:
    MovementComponent() = default;
    
    /**
     * Initialize from USD prim with SparkleMovementAPI schema
     */
    virtual bool Initialize(const UsdPrim& prim) override {
        if (!prim.IsValid()) {
            return false;
        }
        
        m_prim = prim;
        
        // Cache attribute tokens (only done once per component type)
        static const TfToken speedToken("sparkle:movement:speed");
        static const TfToken accelerationToken("sparkle:movement:acceleration");
        static const TfToken jumpHeightToken("sparkle:movement:jumpHeight");
        static const TfToken patternToken("sparkle:movement:pattern");
        
        // Create cached attributes with appropriate defaults
        m_speed = std::make_unique<CachedAttribute<float>>(
            prim.GetAttribute(speedToken), speedToken, 5.0f);
        
        m_acceleration = std::make_unique<CachedAttribute<float>>(
            prim.GetAttribute(accelerationToken), accelerationToken, 10.0f);
        
        m_jumpHeight = std::make_unique<CachedAttribute<float>>(
            prim.GetAttribute(jumpHeightToken), jumpHeightToken, 0.0f);
        
        m_pattern = std::make_unique<CachedAttribute<TfToken>>(
            prim.GetAttribute(patternToken), patternToken, TfToken("direct"));
        
        return true;
    }
    
    /**
     * Sync all movement attributes to USD
     */
    virtual bool SyncToUsd() override {
        if (!m_prim) {
            return false;
        }
        
        bool success = true;
        success &= m_speed->SyncToUsd();
        success &= m_acceleration->SyncToUsd();
        success &= m_jumpHeight->SyncToUsd();
        success &= m_pattern->SyncToUsd();
        
        return success;
    }
    
    /**
     * Sync all movement attributes from USD
     */
    virtual bool SyncFromUsd() override {
        if (!m_prim) {
            return false;
        }
        
        bool success = true;
        success &= m_speed->SyncFromUsd();
        success &= m_acceleration->SyncFromUsd();
        success &= m_jumpHeight->SyncFromUsd();
        success &= m_pattern->SyncFromUsd();
        
        return success;
    }
    
    /**
     * Check if any movement attributes are dirty
     */
    virtual bool IsDirty() const override {
        return m_speed->IsDirty() ||
               m_acceleration->IsDirty() ||
               m_jumpHeight->IsDirty() ||
               m_pattern->IsDirty();
    }
    
    /**
     * Get component type name
     */
    virtual std::string GetTypeName() const override {
        return "MovementComponent";
    }
    
    // Fast accessors for cached values
    float GetSpeed() const { return m_speed->Get(); }
    float GetAcceleration() const { return m_acceleration->Get(); }
    float GetJumpHeight() const { return m_jumpHeight->Get(); }
    TfToken GetPattern() const { return m_pattern->Get(); }
    
    // Mutators that automatically mark as dirty
    void SetSpeed(float value) { m_speed->Set(value); }
    void SetAcceleration(float value) { m_acceleration->Set(value); }
    void SetJumpHeight(float value) { m_jumpHeight->Set(value); }
    void SetPattern(const TfToken& value) { m_pattern->Set(value); }
    
    // Game-specific helper methods using cached values
    bool CanJump() const { return GetJumpHeight() > 0.0f; }
    
    float CalculateDistancePerSecond() const {
        return GetSpeed(); // Simple case, could include acceleration factor
    }
    
    // Example movement pattern handler
    GfVec3f CalculateMovementVector(const GfVec3f& currentPos, const GfVec3f& targetPos) const {
        TfToken pattern = GetPattern();
        
        if (pattern == TfToken("direct")) {
            // Direct movement towards target
            GfVec3f direction = targetPos - currentPos;
            if (direction.GetLength() > 0) {
                direction.Normalize();
            }
            return direction * GetSpeed();
        } 
        else if (pattern == TfToken("patrol")) {
            // Patrol movement logic would go here
            return GfVec3f(0, 0, 0);
        }
        // Add other patterns as needed
        
        return GfVec3f(0, 0, 0);
    }

private:
    UsdPrim m_prim;
    std::unique_ptr<CachedAttribute<float>> m_speed;
    std::unique_ptr<CachedAttribute<float>> m_acceleration;
    std::unique_ptr<CachedAttribute<float>> m_jumpHeight;
    std::unique_ptr<CachedAttribute<TfToken>> m_pattern;
};

/**
 * CombatComponent
 * 
 * Example component for combat-related attributes with optimized caching
 */
class CombatComponent : public ComponentBase {
public:
    CombatComponent() = default;
    
    /**
     * Initialize from USD prim with SparkleCombatAPI schema
     */
    virtual bool Initialize(const UsdPrim& prim) override {
        if (!prim.IsValid()) {
            return false;
        }
        
        m_prim = prim;
        
        // Cache attribute tokens (only done once per component type)
        static const TfToken damageToken("sparkle:combat:damage");
        static const TfToken attackRadiusToken("sparkle:combat:attackRadius");
        static const TfToken attackCooldownToken("sparkle:combat:attackCooldown");
        static const TfToken damageTypeToken("sparkle:combat:damageType");
        
        // Create cached attributes with appropriate defaults
        m_damage = std::make_unique<CachedAttribute<float>>(
            prim.GetAttribute(damageToken), damageToken, 10.0f);
        
        m_attackRadius = std::make_unique<CachedAttribute<float>>(
            prim.GetAttribute(attackRadiusToken), attackRadiusToken, 1.0f);
        
        m_attackCooldown = std::make_unique<CachedAttribute<float>>(
            prim.GetAttribute(attackCooldownToken), attackCooldownToken, 1.0f);
        
        m_damageType = std::make_unique<CachedAttribute<TfToken>>(
            prim.GetAttribute(damageTypeToken), damageTypeToken, TfToken("normal"));
        
        return true;
    }
    
    /**
     * Sync all combat attributes to USD
     */
    virtual bool SyncToUsd() override {
        if (!m_prim) {
            return false;
        }
        
        bool success = true;
        success &= m_damage->SyncToUsd();
        success &= m_attackRadius->SyncToUsd();
        success &= m_attackCooldown->SyncToUsd();
        success &= m_damageType->SyncToUsd();
        
        return success;
    }
    
    /**
     * Sync all combat attributes from USD
     */
    virtual bool SyncFromUsd() override {
        if (!m_prim) {
            return false;
        }
        
        bool success = true;
        success &= m_damage->SyncFromUsd();
        success &= m_attackRadius->SyncFromUsd();
        success &= m_attackCooldown->SyncFromUsd();
        success &= m_damageType->SyncFromUsd();
        
        return success;
    }
    
    /**
     * Check if any combat attributes are dirty
     */
    virtual bool IsDirty() const override {
        return m_damage->IsDirty() ||
               m_attackRadius->IsDirty() ||
               m_attackCooldown->IsDirty() ||
               m_damageType->IsDirty();
    }
    
    /**
     * Get component type name
     */
    virtual std::string GetTypeName() const override {
        return "CombatComponent";
    }
    
    // Fast accessors for cached values
    float GetDamage() const { return m_damage->Get(); }
    float GetAttackRadius() const { return m_attackRadius->Get(); }
    float GetAttackCooldown() const { return m_attackCooldown->Get(); }
    TfToken GetDamageType() const { return m_damageType->Get(); }
    
    // Mutators that automatically mark as dirty
    void SetDamage(float value) { m_damage->Set(value); }
    void SetAttackRadius(float value) { m_attackRadius->Set(value); }
    void SetAttackCooldown(float value) { m_attackCooldown->Set(value); }
    void SetDamageType(const TfToken& value) { m_damageType->Set(value); }
    
    // Game-specific helper methods
    bool CanAttack() const {
        return GetTimeUntilNextAttack() <= 0.0f;
    }
    
    bool IsInAttackRange(const GfVec3f& myPos, const GfVec3f& targetPos) const {
        float distanceSq = (targetPos - myPos).GetLengthSq();
        float radiusSq = GetAttackRadius() * GetAttackRadius();
        return distanceSq <= radiusSq;
    }
    
    // Attack cooldown handling
    void StartCooldown() {
        m_lastAttackTime = std::chrono::steady_clock::now();
    }
    
    float GetTimeUntilNextAttack() const {
        if (m_lastAttackTime.time_since_epoch().count() == 0) {
            return 0.0f; // Never attacked yet
        }
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastAttackTime).count();
        float cooldownMs = GetAttackCooldown() * 1000.0f;
        
        return std::max(0.0f, cooldownMs - static_cast<float>(elapsed)) / 1000.0f;
    }

private:
    UsdPrim m_prim;
    std::unique_ptr<CachedAttribute<float>> m_damage;
    std::unique_ptr<CachedAttribute<float>> m_attackRadius;
    std::unique_ptr<CachedAttribute<float>> m_attackCooldown;
    std::unique_ptr<CachedAttribute<TfToken>> m_damageType;
    std::chrono::steady_clock::time_point m_lastAttackTime;
};

/**
 * Entity
 * 
 * A game entity composed of multiple cached components
 */
class Entity {
public:
    Entity(const UsdPrim& prim) : m_prim(prim) {}
    
    /**
     * Get the USD prim
     */
    const UsdPrim& GetPrim() const { return m_prim; }
    
    /**
     * Get entity name/ID
     */
    std::string GetName() const { 
        return m_prim.GetName(); 
    }
    
    /**
     * Add a component of specific type
     * 
     * @tparam T Component type
     * @return Pointer to the new component
     */
    template<typename T>
    T* AddComponent() {
        static_assert(std::is_base_of<ComponentBase, T>::value, 
                     "T must derive from ComponentBase");
                     
        // Create component
        std::unique_ptr<T> component = std::make_unique<T>();
        
        // Initialize from prim
        if (component->Initialize(m_prim)) {
            T* componentPtr = component.get();
            m_components.push_back(std::move(component));
            return componentPtr;
        }
        
        return nullptr;
    }
    
    /**
     * Get a component of specific type
     * 
     * @tparam T Component type
     * @return Pointer to the component, or nullptr if not found
     */
    template<typename T>
    T* GetComponent() const {
        static_assert(std::is_base_of<ComponentBase, T>::value, 
                     "T must derive from ComponentBase");
                     
        for (const auto& component : m_components) {
            if (auto* typed = dynamic_cast<T*>(component.get())) {
                return typed;
            }
        }
        
        return nullptr;
    }
    
    /**
     * Check if entity has a component of specific type
     * 
     * @tparam T Component type
     * @return Whether the entity has the component
     */
    template<typename T>
    bool HasComponent() const {
        return GetComponent<T>() != nullptr;
    }
    
    /**
     * Remove a component of specific type
     * 
     * @tparam T Component type
     * @return Whether the component was removed
     */
    template<typename T>
    bool RemoveComponent() {
        static_assert(std::is_base_of<ComponentBase, T>::value, 
                     "T must derive from ComponentBase");
                     
        for (auto it = m_components.begin(); it != m_components.end(); ++it) {
            if (dynamic_cast<T*>(it->get())) {
                m_components.erase(it);
                return true;
            }
        }
        
        return false;
    }
    
    /**
     * Sync all dirty components to USD
     * 
     * @return Whether all syncs were successful
     */
    bool SyncToUsd() {
        bool success = true;
        
        for (auto& component : m_components) {
            if (component->IsDirty()) {
                success &= component->SyncToUsd();
            }
        }
        
        return success;
    }
    
    /**
     * Sync all components from USD
     * 
     * @return Whether all syncs were successful
     */
    bool SyncFromUsd() {
        bool success = true;
        
        for (auto& component : m_components) {
            success &= component->SyncFromUsd();
        }
        
        return success;
    }
    
    /**
     * Update entity logic
     * 
     * @param deltaTime Time since last update in seconds
     */
    void Update(float deltaTime) {
        // Update components that need time-based updates
        
        // Update health regeneration
        if (auto* health = GetComponent<HealthComponent>()) {
            health->Update(deltaTime);
        }
        
        // Other component updates would go here
    }

private:
    UsdPrim m_prim;
    std::vector<std::unique_ptr<ComponentBase>> m_components;
};

/**
 * ComponentRegistry
 * 
 * Factory for creating components based on API schema types
 */
class ComponentRegistry {
public:
    /**
     * Register a component type for a specific API schema
     * 
     * @tparam T Component type
     * @param apiSchemaName The name of the API schema
     */
    template<typename T>
    void RegisterComponentType(const std::string& apiSchemaName) {
        static_assert(std::is_base_of<ComponentBase, T>::value, 
                     "T must derive from ComponentBase");
                     
        m_factories[apiSchemaName] = [](Entity* entity) {
            return entity->AddComponent<T>();
        };
    }
    
    /**
     * Create appropriate components for a prim based on applied schemas
     * 
     * @param entity The entity to create components for
     */
    void CreateComponentsForEntity(Entity* entity) {
        if (!entity) return;
        
        const UsdPrim& prim = entity->GetPrim();
        std::vector<std::string> apiSchemas;
        prim.GetAppliedSchemas(&apiSchemas);
        
        for (const auto& schema : apiSchemas) {
            auto it = m_factories.find(schema);
            if (it != m_factories.end()) {
                it->second(entity);
            }
        }
    }
    
    /**
     * Get the singleton instance
     */
    static ComponentRegistry& Get() {
        static ComponentRegistry instance;
        return instance;
    }

private:
    ComponentRegistry() = default;
    
    using ComponentFactory = std::function<ComponentBase*(Entity*)>;
    std::unordered_map<std::string, ComponentFactory> m_factories;
};

/**
 * EntityManager
 * 
 * Manages entities and their component caching
 */
class EntityManager {
public:
    /**
     * Create an entity manager for a USD stage
     * 
     * @param stage The USD stage
     */
    EntityManager(const UsdStageRefPtr& stage) : m_stage(stage) {
        // Register known component types
        ComponentRegistry::Get().RegisterComponentType<HealthComponent>("SparkleHealthAPI");
        ComponentRegistry::Get().RegisterComponentType<MovementComponent>("SparkleMovementAPI");
        ComponentRegistry::Get().RegisterComponentType<CombatComponent>("SparkleCombatAPI");
    }
    
    /**
     * Create entities for all compatible prims in the stage
     */
    void CreateEntitiesFromStage() {
        if (!m_stage) return;
        
        for (const UsdPrim& prim : m_stage->Traverse()) {
            if (prim.IsA<TfType::Find<class SparkleGameEntity>>()) {
                CreateEntity(prim);
            }
        }
    }
    
    /**
     * Create an entity for a specific prim
     * 
     * @param prim The USD prim
     * @return Pointer to the created entity, or nullptr on failure
     */
    Entity* CreateEntity(const UsdPrim& prim) {
        if (!prim.IsValid()) return nullptr;
        
        // Create entity
        std::unique_ptr<Entity> entity = std::make_unique<Entity>(prim);
        
        // Add components based on applied schemas
        ComponentRegistry::Get().CreateComponentsForEntity(entity.get());
        
        // Store and return
        Entity* entityPtr = entity.get();
        m_entities.push_back(std::move(entity));
        m_entityMap[prim.GetPath()] = entityPtr;
        
        return entityPtr;
    }
    
    /**
     * Get an entity by prim path
     * 
     * @param path The prim path
     * @return Pointer to the entity, or nullptr if not found
     */
    Entity* GetEntity(const SdfPath& path) const {
        auto it = m_entityMap.find(path);
        return (it != m_entityMap.end()) ? it->second : nullptr;
    }
    
    /**
     * Get all entities
     * 
     * @return Vector of entity pointers
     */
    std::vector<Entity*> GetAllEntities() const {
        std::vector<Entity*> result;
        result.reserve(m_entities.size());
        
        for (const auto& entity : m_entities) {
            result.push_back(entity.get());
        }
        
        return result;
    }
    
    /**
     * Get entities by type
     * 
     * @tparam T Component type to filter by
     * @return Vector of entities with the specified component
     */
    template<typename T>
    std::vector<Entity*> GetEntitiesByComponent() const {
        std::vector<Entity*> result;
        
        for (const auto& entity : m_entities) {
            if (entity->HasComponent<T>()) {
                result.push_back(entity.get());
            }
        }
        
        return result;
    }
    
    /**
     * Sync all dirty entities to USD
     * 
     * @return Whether all syncs were successful
     */
    bool SyncToUsd() {
        bool success = true;
        
        for (auto& entity : m_entities) {
            success &= entity->SyncToUsd();
        }
        
        return success;
    }
    
    /**
     * Sync all entities from USD
     * 
     * @return Whether all syncs were successful
     */
    bool SyncFromUsd() {
        bool success = true;
        
        for (auto& entity : m_entities) {
            success &= entity->SyncFromUsd();
        }
        
        return success;
    }
    
    /**
     * Update all entities
     * 
     * @param deltaTime Time since last update in seconds
     */
    void Update(float deltaTime) {
        for (auto& entity : m_entities) {
            entity->Update(deltaTime);
        }
    }
    
    /**
     * Remove an entity
     * 
     * @param entity The entity to remove
     * @return Whether the entity was removed
     */
    bool RemoveEntity(Entity* entity) {
        if (!entity) return false;
        
        // Remove from map
        m_entityMap.erase(entity->GetPrim().GetPath());
        
        // Find and remove from vector
        for (auto it = m_entities.begin(); it != m_entities.end(); ++it) {
            if (it->get() == entity) {
                m_entities.erase(it);
                return true;
            }
        }
        
        return false;
    }
    
    /**
     * Clear all entities
     */
    void Clear() {
        m_entities.clear();
        m_entityMap.clear();
    }

private:
    UsdStageRefPtr m_stage;
    std::vector<std::unique_ptr<Entity>> m_entities;
    std::unordered_map<SdfPath, Entity*, SdfPath::Hash> m_entityMap;
};

/**
 * Example usage of the component attribute cache system
 */
inline void ComponentAttributeCacheExample() {
    // Open a USD stage
    UsdStageRefPtr stage = UsdStage::Open("game_level.usda");
    if (!stage) {
        std::cerr << "Failed to open stage" << std::endl;
        return;
    }
    
    // Create entity manager
    EntityManager entityManager(stage);
    
    // Create entities from stage
    entityManager.CreateEntitiesFromStage();
    
    // Get all entities with health component
    auto healthEntities = entityManager.GetEntitiesByComponent<HealthComponent>();
    std::cout << "Found " << healthEntities.size() << " entities with health component" << std::endl;
    
    // Apply some game logic using cached attributes
    float deltaTime = 0.016f; // ~60fps
    
    // Example: Apply damage to all enemies within range of player
    Entity* player = entityManager.GetEntity(SdfPath("/Game/Player"));
    if (player && player->HasComponent<CombatComponent>()) {
        auto* playerCombat = player->GetComponent<CombatComponent>();
        
        // Simple game update loop
        for (int frame = 0; frame < 100; ++frame) {
            // Update all entities
            entityManager.Update(deltaTime);
            
            // Process combat
            for (auto* entity : healthEntities) {
                // Skip player and dead entities
                if (entity == player) continue;
                
                auto* health = entity->GetComponent<HealthComponent>();
                if (health->IsDead()) continue;
                
                // Calculate distance (simplified)
                float distance = 2.0f; // Placeholder for actual distance calculation
                
                // Check if in range and attack is ready
                if (playerCombat->IsInAttackRange(GfVec3f(0, 0, 0), GfVec3f(distance, 0, 0)) && 
                    playerCombat->CanAttack()) {
                    
                    // Apply damage using cached attributes
                    health->TakeDamage(playerCombat->GetDamage());
                    playerCombat->StartCooldown();
                    
                    std::cout << "Player attacked " << entity->GetName() 
                              << " for " << playerCombat->GetDamage() << " damage. "
                              << "Remaining health: " << health->GetCurrentHealth() 
                              << "/" << health->GetMaxHealth() << std::endl;
                }
            }
            
            // Only sync to USD occasionally for efficiency (not every frame)
            if (frame % 10 == 0) {
                entityManager.SyncToUsd();
            }
        }
    }
    
    // Make sure to sync all changes back to USD at the end
    entityManager.SyncToUsd();
    
    // Save stage changes
    stage->Save();
}

/**
 * Performance benchmark to compare direct vs. cached attribute access
 */
inline void ComponentAttributeCacheBenchmark() {
    // Open a USD stage
    UsdStageRefPtr stage = UsdStage::Open("game_level.usda");
    if (!stage) {
        std::cerr << "Failed to open stage" << std::endl;
        return;
    }
    
    // Find a test prim
    UsdPrim testPrim = stage->GetPrimAtPath(SdfPath("/Game/Enemies/Enemy_01"));
    if (!testPrim) {
        std::cerr << "Test prim not found" << std::endl;
        return;
    }
    
    // Create direct attribute handles
    UsdAttribute healthAttr = testPrim.GetAttribute(TfToken("sparkle:health:current"));
    UsdAttribute maxHealthAttr = testPrim.GetAttribute(TfToken("sparkle:health:maximum"));
    
    // Create cached component
    HealthComponent healthComponent;
    healthComponent.Initialize(testPrim);
    
    // Number of iterations for the benchmark
    const int numIterations = 100000;
    
    // Benchmark direct USD attribute access
    auto directStart = std::chrono::high_resolution_clock::now();
    
    float totalHealth = 0.0f;
    for (int i = 0; i < numIterations; ++i) {
        float health = 0.0f;
        float maxHealth = 0.0f;
        
        healthAttr.Get(&health);
        maxHealthAttr.Get(&maxHealth);
        
        // Simple operation to prevent optimization
        if (health > 0 && health <= maxHealth) {
            totalHealth += health;
        }
    }
    
    auto directEnd = std::chrono::high_resolution_clock::now();
    auto directDuration = std::chrono::duration_cast<std::chrono::microseconds>(directEnd - directStart).count();
    
    // Benchmark cached component access
    auto cachedStart = std::chrono::high_resolution_clock::now();
    
    float cachedTotalHealth = 0.0f;
    for (int i = 0; i < numIterations; ++i) {
        float health = healthComponent.GetCurrentHealth();
        float maxHealth = healthComponent.GetMaxHealth();
        
        // Simple operation to prevent optimization
        if (health > 0 && health <= maxHealth) {
            cachedTotalHealth += health;
        }
    }
    
    auto cachedEnd = std::chrono::high_resolution_clock::now();
    auto cachedDuration = std::chrono::duration_cast<std::chrono::microseconds>(cachedEnd - cachedStart).count();
    
    // Print results
    std::cout << "Benchmark Results (microseconds for " << numIterations << " iterations):" << std::endl;
    std::cout << "Direct USD access: " << directDuration << " µs" << std::endl;
    std::cout << "Cached component access: " << cachedDuration << " µs" << std::endl;
    std::cout << "Speedup factor: " << static_cast<double>(directDuration) / cachedDuration << "x" << std::endl;
    
    // Verify results match
    if (std::abs(totalHealth - cachedTotalHealth) < 0.001f) {
        std::cout << "Results match: " << totalHealth << " == " << cachedTotalHealth << std::endl;
    } else {
        std::cerr << "Results don't match: " << totalHealth << " != " << cachedTotalHealth << std::endl;
    }
}
