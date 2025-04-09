# Chapter 4: Implementing Game Schemas with Codeless API Schemas

## From Design to Implementation: A Practical Guide

Building on the composition concepts we explored in Chapter 3, this chapter provides a step-by-step guide to implementing game-specific schemas using the codeless approach. We'll walk through the complete process from initial design to integration, expanding on the SparkleCarrotPopper game example introduced earlier. For the complete implementation, refer to the accompanying code samples available in the `code_samples/sparkle_carrot` directory.

## Step 1: Identify Schema Requirements

Before writing any schema definitions, it's essential to analyze your game's data requirements. This initial analysis ensures that your schemas will meet the actual needs of your game systems.

### Entity Types

Start by identifying the core entity types in your game:

```
SparkleCarrotPopper Game Entities:
- Player Character
- Carrot Enemies (basic, elite, boss variants)
- Power-ups
- Projectiles
- Level Segments
- Spawn Points
- Collectibles
```

### Properties and Relationships

For each entity type, identify three categories of properties:

1. **Intrinsic properties**: Values that define this entity's core attributes
2. **Relational properties**: How this entity relates to or interacts with others
3. **Behavioral properties**: What configures this entity's behavior in the game

For example, here's an analysis for Carrot Enemies:

```
Intrinsic Properties:
- Health (float)
- Damage (float)
- Movement Speed (float)
- Size (float)
- Team (token)

Relational Properties:
- Patrol Path (relation)
- Spawn Point (relation)
- Target (relation)

Behavioral Properties:
- Aggression Radius (float)
- Attack Pattern (token)
- Difficulty Scaling (float)
```

### Schema Organization

Based on your analysis, decide on your schema organization strategy. The three main approaches are:

1. **Entity-Based Schemas**: Define separate schemas for each entity type
2. **Component-Based Schemas**: Define reusable API schemas that can be combined
3. **Hybrid Approach**: Use entity types for core objects, API schemas for reusable behaviors

As discussed in Chapter 3, most games benefit from a hybrid approach, which we'll demonstrate in this implementation guide.

## Step 2: Create Schema Definition Files

With your requirements analyzed, now create the schema definition files. Start with the base schema file:

```usda
#usda 1.0
(
    subLayers = [
        @usd/schema.usda@,
        @usdGeom/schema.usda@
    ]
)

over "GLOBAL" (
    customData = {
        string libraryName = "sparkleCarrotSchema"
        string libraryPath = "./"
        string libraryPrefix = "SparkleCarrot"
        bool skipCodeGeneration = true
    }
) {
}
```

Notice the `skipCodeGeneration = true` flag that marks this as a codeless schema, as explained in Chapter 3.

### Define Entity Type Schemas

Next, define the entity type schemas, starting with a base game entity:

```usda
# Base game entity for the SparkleCarrot game
class "SparkleGameEntity" (
    inherits = </Typed>
    doc = """Base type for all game entities in SparkleCarrotPopper."""
)
{
    # Basic identity properties
    string sparkle:entity:id = "" (
        doc = """Unique identifier for this entity instance."""
    )
    
    token sparkle:entity:category = "undefined" (
        doc = """Category of entity for game systems."""
        allowedTokens = ["player", "enemy", "pickup", "projectile", "environment", "trigger", "undefined"]
    )
    
    # Common game properties
    bool sparkle:entity:enabled = true (
        doc = """Whether this entity is currently active in gameplay."""
    )
}

# Carrot enemy schema
class "SparkleEnemyCarrot" (
    inherits = </SparkleGameEntity>
    doc = """A carrot enemy in the SparkleCarrot game."""
)
{
    # Default values appropriate for this entity type
    token sparkle:entity:category = "enemy" (
        doc = """Category of entity for game systems."""
    )
}

# Player character schema
class "SparklePlayer" (
    inherits = </SparkleGameEntity>
    doc = """Player character in the SparkleCarrot game."""
)
{
    token sparkle:entity:category = "player" (
        doc = """Category of entity for game systems."""
    )
}

# Pickup item schema
class "SparklePickup" (
    inherits = </SparkleGameEntity>
    doc = """Collectible pickup item in the SparkleCarrot game."""
)
{
    token sparkle:entity:category = "pickup" (
        doc = """Category of entity for game systems."""
    )
}
```

These entity types establish the core classes in our game's type hierarchy. Each specializes the base entity with default values and type-specific behaviors.

### Define Component API Schemas

Now, define reusable API schemas for common game mechanics. As we saw in Chapter 3, API schemas let us compose functionality onto entities:

```usda
# Health component
class "SparkleHealthAPI" (
    inherits = </APISchemaBase>
    customData = {
        token apiSchemaType = "singleApply"
    }
    doc = """Health-related properties for damageable entities."""
)
{
    float sparkle:health:current = 100 (
        doc = """Current health value."""
    )
    
    float sparkle:health:maximum = 100 (
        doc = """Maximum health value."""
    )
    
    bool sparkle:health:invulnerable = false (
        doc = """Whether entity is currently invulnerable to damage."""
    )
    
    float sparkle:health:regenerationRate = 0 (
        doc = """Health points regenerated per second."""
    )
}

# Combat component
class "SparkleCombatAPI" (
    inherits = </APISchemaBase>
    customData = {
        token apiSchemaType = "singleApply"
    }
    doc = """Combat-related properties for entities that can inflict damage."""
)
{
    float sparkle:combat:damage = 10 (
        doc = """Base damage inflicted by this entity."""
    )
    
    float sparkle:combat:attackRadius = 1 (
        doc = """Radius in which this entity can attack targets."""
    )
    
    float sparkle:combat:attackCooldown = 1 (
        doc = """Time in seconds between attacks."""
    )
    
    token sparkle:combat:damageType = "normal" (
        doc = """Type of damage inflicted."""
        allowedTokens = ["normal", "fire", "ice", "poison", "explosive"]
    )
}

# Movement component
class "SparkleMovementAPI" (
    inherits = </APISchemaBase>
    customData = {
        token apiSchemaType = "singleApply"
    }
    doc = """Movement-related properties for entities that can move."""
)
{
    float sparkle:movement:speed = 5 (
        doc = """Base movement speed in units per second."""
    )
    
    float sparkle:movement:acceleration = 10 (
        doc = """Acceleration in units per second squared."""
    )
    
    float sparkle:movement:jumpHeight = 0 (
        doc = """Jump height in units. Zero means entity cannot jump."""
    )
    
    token sparkle:movement:pattern = "direct" (
        doc = """Movement pattern for AI entities."""
        allowedTokens = ["direct", "patrol", "wander", "charge", "flee", "stationary"]
    )
}

# AI component
class "SparkleAIAPI" (
    inherits = </APISchemaBase>
    customData = {
        token apiSchemaType = "singleApply"
    }
    doc = """AI-related properties for computer-controlled entities."""
)
{
    float sparkle:ai:detectionRadius = 10 (
        doc = """Radius in which this entity detects targets."""
    )
    
    token sparkle:ai:behavior = "aggressive" (
        doc = """Primary behavior pattern."""
        allowedTokens = ["passive", "defensive", "aggressive", "neutral", "flee"]
    )
    
    rel sparkle:ai:patrolPath (
        doc = """Relationship to a path prim defining patrol route."""
    )
    
    float sparkle:ai:difficultyMultiplier = 1.0 (
        doc = """Scalar multiplier for AI difficulty."""
    )
}
```

### Multiple-Apply API Example: Loot Drop System

As covered in Chapter 3, some components benefit from the multiple-apply pattern, where multiple instances of the same schema can be applied to a single entity. Here's a practical implementation for a loot system:

```usda
# Loot drop component (multi-apply)
class "SparkleLootAPI" (
    inherits = </APISchemaBase>
    customData = {
        token apiSchemaType = "multipleApply"
        token propertyNamespacePrefix = "sparkle:loot"
    }
    doc = """Defines a potential loot drop from this entity."""
)
{
    string itemId = "" (
        doc = """Item identifier in the game database."""
    )
    
    float dropChance = 0.1 (
        doc = """Probability (0-1) of dropping this item."""
    )
    
    int minQuantity = 1 (
        doc = """Minimum quantity to drop."""
    )
    
    int maxQuantity = 1 (
        doc = """Maximum quantity to drop."""
    )
    
    token dropCondition = "onDeath" (
        doc = """When this item drops."""
        allowedTokens = ["onDeath", "onDamage", "onInteract", "onTimer"]
    )
}
```

This multi-apply schema allows an entity to define multiple different loot drops, each with its own configuration.

### Team System Schema

Here's another reusable component for handling team affiliations and relationships:

```usda
# Team affiliation component
class "SparkleTeamAPI" (
    inherits = </APISchemaBase>
    customData = {
        token apiSchemaType = "singleApply"
    }
    doc = """Team affiliation properties."""
)
{
    token sparkle:team:affiliation = "neutral" (
        doc = """Team this entity belongs to."""
        allowedTokens = ["player", "enemy", "neutral", "wildlife"]
    )
    
    token[] sparkle:team:hostileTo = ["enemy"] (
        doc = """Teams this entity is hostile towards."""
    )
    
    token[] sparkle:team:friendlyTo = ["player"] (
        doc = """Teams this entity is friendly towards."""
    )
}
```

## Step 3: Using Schemas in Game Levels

With our schemas defined, we can now author game entities in a level. The following example shows how to compose functionality by applying multiple API schemas to different entity types:

```usda
#usda 1.0
(
    defaultPrim = "Level"
)

def Xform "Level" (
    kind = "component"
)
{
    # Player character
    def SparklePlayer "Player_01" (
        apiSchemas = ["SparkleHealthAPI", "SparkleCombatAPI", "SparkleMovementAPI", "SparkleTeamAPI"]
    )
    {
        string sparkle:entity:id = "player_main"
        
        # Health properties
        float sparkle:health:current = 100
        float sparkle:health:maximum = 100
        
        # Combat properties
        float sparkle:combat:damage = 25
        token sparkle:combat:damageType = "normal"
        
        # Movement properties
        float sparkle:movement:speed = 8
        float sparkle:movement:jumpHeight = 4
        
        # Team properties
        token sparkle:team:affiliation = "player"
        token[] sparkle:team:hostileTo = ["enemy"]
    }
    
    # Regular enemy
    def SparkleEnemyCarrot "Enemy_Carrot_01" (
        apiSchemas = ["SparkleHealthAPI", "SparkleCombatAPI", "SparkleMovementAPI", "SparkleAIAPI", "SparkleTeamAPI", "SparkleLootAPI:commonDrop"]
    )
    {
        string sparkle:entity:id = "carrot_grunt_01"
        
        # Health properties
        float sparkle:health:current = 50
        float sparkle:health:maximum = 50
        
        # Combat properties
        float sparkle:combat:damage = 10
        float sparkle:combat:attackRadius = 2
        token sparkle:combat:damageType = "normal"
        
        # Movement properties
        float sparkle:movement:speed = 4
        token sparkle:movement:pattern = "patrol"
        
        # AI properties
        float sparkle:ai:detectionRadius = 15
        token sparkle:ai:behavior = "aggressive"
        rel sparkle:ai:patrolPath = </Level/Paths/PatrolPath_01>
        
        # Team properties
        token sparkle:team:affiliation = "enemy"
        token[] sparkle:team:hostileTo = ["player"]
        
        # Loot properties (using multi-apply instance)
        string sparkle:loot:commonDrop:itemId = "carrot_piece"
        float sparkle:loot:commonDrop:dropChance = 0.75
        int sparkle:loot:commonDrop:minQuantity = 1
        int sparkle:loot:commonDrop:maxQuantity = 3
    }
    
    # Boss enemy
    def SparkleEnemyCarrot "Boss_KingCarrot" (
        apiSchemas = ["SparkleHealthAPI", "SparkleCombatAPI", "SparkleMovementAPI", "SparkleAIAPI", "SparkleTeamAPI", "SparkleLootAPI:commonDrop", "SparkleLootAPI:rareDrop"]
    )
    {
        string sparkle:entity:id = "carrot_king_boss"
        
        # Health properties
        float sparkle:health:current = 500
        float sparkle:health:maximum = 500
        float sparkle:health:regenerationRate = 5
        
        # Combat properties
        float sparkle:combat:damage = 30
        float sparkle:combat:attackRadius = 5
        token sparkle:combat:damageType = "fire"
        
        # Movement properties
        float sparkle:movement:speed = 3
        token sparkle:movement:pattern = "charge"
        
        # AI properties
        float sparkle:ai:detectionRadius = 25
        token sparkle:ai:behavior = "aggressive"
        float sparkle:ai:difficultyMultiplier = 2.0
        
        # Team properties
        token sparkle:team:affiliation = "enemy"
        token[] sparkle:team:hostileTo = ["player"]
        
        # Common loot drop
        string sparkle:loot:commonDrop:itemId = "carrot_piece"
        float sparkle:loot:commonDrop:dropChance = 1.0
        int sparkle:loot:commonDrop:minQuantity = 10
        int sparkle:loot:commonDrop:maxQuantity = 15
        
        # Rare loot drop
        string sparkle:loot:rareDrop:itemId = "crown_of_carrots"
        float sparkle:loot:rareDrop:dropChance = 0.1
        int sparkle:loot:rareDrop:minQuantity = 1
        int sparkle:loot:rareDrop:maxQuantity = 1
    }
    
    # Health pickup
    def SparklePickup "Pickup_HealthCarrot" (
        apiSchemas = ["SparkleLootAPI:healthEffect"]
    )
    {
        string sparkle:entity:id = "health_pickup_01"
        
        # Loot effect
        string sparkle:loot:healthEffect:itemId = "health_boost"
        float sparkle:loot:healthEffect:dropChance = 1.0
        token sparkle:loot:healthEffect:dropCondition = "onInteract"
    }
    
    # Define a patrol path for enemies
    def "Paths" {
        def PointInstancer "PatrolPath_01" {
            point3f[] points = [(0, 0, 0), (10, 0, 0), (10, 0, 10), (0, 0, 10)]
            uniform token purpose = "guide"
        }
    }
}
```

Notice how the multi-apply API schema `SparkleLootAPI` is used in practice:
1. The regular enemy has a single instance named `commonDrop`
2. The boss enemy has two instances: `commonDrop` and `rareDrop`
3. Properties are accessed using the instance name, e.g., `sparkle:loot:rareDrop:dropChance`

This pattern allows for great flexibility without needing to create new schemas for each variation.

## Step 4: Accessing Schema Data in Game Code

Now that we've defined schemas and used them in a level, we need to access this data in game code. As explained in Chapter 3, codeless schemas require working with attributes directly rather than through generated C++ APIs.

### Efficient C++ Access Patterns

Building on the access patterns introduced in Chapter 3, here's how to efficiently implement game systems that use our schemas:

```cpp
// Entity component system implementation for SparkleCarrotPopper
#include "pxr/usd/usd/stage.h"
#include "pxr/usd/usd/primRange.h"
#include "pxr/usd/usdGeom/xform.h"

using namespace PXR_NS;

// Game entity loader
void LoadGameEntities(UsdStageRefPtr stage) {
    // Iterate through all prims
    for (const UsdPrim& prim : stage->TraverseAll()) {
        // Check if this is a game entity
        if (prim.IsA<TfType::Find<class SparkleGameEntity>>()) {
            // Get common entity properties
            std::string entityId;
            prim.GetAttribute(TfToken("sparkle:entity:id")).Get(&entityId);
            
            TfToken category;
            prim.GetAttribute(TfToken("sparkle:entity:category")).Get(&category);
            
            // Create game entity in our engine...
            GameEntity* entity = CreateGameEntity(entityId, category);
            
            // Process API schemas
            ProcessEntityComponents(prim, entity);
        }
    }
}

// Process entity components from API schemas
void ProcessEntityComponents(const UsdPrim& prim, GameEntity* entity) {
    // Check for health component
    if (prim.HasAPI<TfType::Find<class SparkleHealthAPI>>()) {
        float current = 0.0f, maximum = 0.0f;
        prim.GetAttribute(TfToken("sparkle:health:current")).Get(&current);
        prim.GetAttribute(TfToken("sparkle:health:maximum")).Get(&maximum);
        
        // Add health component to entity
        entity->AddComponent<HealthComponent>(current, maximum);
    }
    
    // Check for movement component
    if (prim.HasAPI<TfType::Find<class SparkleMovementAPI>>()) {
        float speed = 0.0f, jumpHeight = 0.0f;
        prim.GetAttribute(TfToken("sparkle:movement:speed")).Get(&speed);
        prim.GetAttribute(TfToken("sparkle:movement:jumpHeight")).Get(&jumpHeight);
        
        TfToken pattern;
        prim.GetAttribute(TfToken("sparkle:movement:pattern")).Get(&pattern);
        
        // Add movement component to entity
        entity->AddComponent<MovementComponent>(speed, jumpHeight, pattern.GetString());
    }
    
    // Process loot drops (multi-apply API schema)
    ProcessLootDrops(prim, entity);
}

// Process multi-apply loot drop schemas
void ProcessLootDrops(const UsdPrim& prim, GameEntity* entity) {
    std::vector<std::string> appliedSchemas;
    prim.GetAppliedSchemas(&appliedSchemas);
    
    for (const std::string& schema : appliedSchemas) {
        // Look for SparkleLootAPI instances
        if (schema.find("SparkleLootAPI:") == 0) {
            std::string instanceName = schema.substr(std::string("SparkleLootAPI:").length());
            
            // Extract loot drop information
            LootDrop lootDrop;
            
            std::string itemId;
            TfToken attrName = TfToken("sparkle:loot:" + instanceName + ":itemId");
            if (prim.GetAttribute(attrName).Get(&itemId)) {
                lootDrop.itemId = itemId;
            }
            
            float dropChance = 0.0f;
            prim.GetAttribute(TfToken("sparkle:loot:" + instanceName + ":dropChance")).Get(&dropChance);
            lootDrop.chance = dropChance;
            
            int minQuantity = 1, maxQuantity = 1;
            prim.GetAttribute(TfToken("sparkle:loot:" + instanceName + ":minQuantity")).Get(&minQuantity);
            prim.GetAttribute(TfToken("sparkle:loot:" + instanceName + ":maxQuantity")).Get(&maxQuantity);
            lootDrop.minQuantity = minQuantity;
            lootDrop.maxQuantity = maxQuantity;
            
            // Add loot drop to entity
            entity->AddLootDrop(lootDrop);
        }
    }
}
```

### Updates and Game Systems

In addition to loading data, you'll need to update entities based on game logic. Here's an example implementation of a health regeneration system:

```cpp
// Health component system update
void UpdateHealthSystem(UsdStageRefPtr stage, float deltaTime) {
    for (const UsdPrim& prim : stage->TraverseAll()) {
        if (prim.HasAPI<TfType::Find<class SparkleHealthAPI>>()) {
            // Get regeneration rate
            float regenRate = 0.0f;
            prim.GetAttribute(TfToken("sparkle:health:regenerationRate")).Get(&regenRate);
            
            // Skip entities with no regeneration
            if (regenRate <= 0.0f) {
                continue;
            }
            
            // Update health with regeneration
            UsdAttribute currentHealthAttr = prim.GetAttribute(TfToken("sparkle:health:current"));
            float currentHealth = 0.0f;
            currentHealthAttr.Get(&currentHealth);
            
            float maxHealth = 0.0f;
            prim.GetAttribute(TfToken("sparkle:health:maximum")).Get(&maxHealth);
            
            // Apply regeneration
            float newHealth = std::min(currentHealth + (regenRate * deltaTime), maxHealth);
            if (newHealth != currentHealth) {
                currentHealthAttr.Set(newHealth);
                
                // Game-specific notification
                std::string entityId;
                prim.GetAttribute(TfToken("sparkle:entity:id")).Get(&entityId);
                NotifyHealthChanged(entityId, newHealth);
            }
        }
    }
}
```

This pattern allows for clean, system-based game logic while leveraging USD for data storage and composition.

## Step 5: Converting Existing Game Models to USD Schemas

If you're migrating from an existing game database or entity system, you'll need to map your existing data model to USD schemas. Here's a systematic approach:

### 1. Inventory Existing Data Models

Start by documenting your current entity types and components:

```
Legacy System:
  Entity: Player
  Components:
    - PlayerController (Movement, Input)
    - Health (Current, Max)
    - Inventory (Items)
    - WeaponSystem (CurrentWeapon, Ammo)
```

### 2. Map to USD Schema Structure

Create a mapping from your existing system to USD schemas:

```
USD Schema Mapping:
  SparklePlayer (IsA Schema)
    - SparkleMovementAPI (API Schema)
    - SparkleHealthAPI (API Schema)
    - SparkleInventoryAPI (API Schema)
    - SparkleWeaponAPI (API Schema)
```

This mapping should identify:
- Which legacy components map to typed schemas vs. API schemas
- Which legacy properties map to USD properties
- Any naming conventions that need to change
- Any data type conversions needed

### 3. Create Schema Conversion Tools

Develop tools to automatically convert between your legacy formats and USD:

```cpp
// C++ converter example (simplified)
void ConvertLegacyEntityToUSD(const LegacyEntity& legacyEntity, UsdStageRefPtr stage, const SdfPath& path) {
    // Determine entity type
    TfToken entityType = MapLegacyTypeToUSD(legacyEntity.GetType());
    
    // Create the prim
    UsdPrim prim = stage->DefinePrim(path, entityType);
    
    // Set basic properties
    prim.CreateAttribute(TfToken("sparkle:entity:id"), SdfValueTypeNames->String)
        .Set(legacyEntity.GetId());
    
    // Add API schemas based on components
    std::vector<TfToken> apiSchemas;
    
    // Health component
    if (legacyEntity.HasComponent("Health")) {
        apiSchemas.push_back(TfToken("SparkleHealthAPI"));
    }
    
    // Movement component
    if (legacyEntity.HasComponent("Movement")) {
        apiSchemas.push_back(TfToken("SparkleMovementAPI"));
    }
    
    // Apply API schemas
    if (!apiSchemas.empty()) {
        UsdAPISchemaBase::ApplyAPISchemas(prim, apiSchemas);
    }
    
    // Set properties from component data
    if (legacyEntity.HasComponent("Health")) {
        const HealthComponent* health = legacyEntity.GetComponent<HealthComponent>();
        prim.CreateAttribute(TfToken("sparkle:health:current"), SdfValueTypeNames->Float)
            .Set(health->GetCurrentHealth());
        prim.CreateAttribute(TfToken("sparkle:health:maximum"), SdfValueTypeNames->Float)
            .Set(health->GetMaxHealth());
    }
    
    // Process other components...
}
```

When implementing conversion tools, consider:
- Automated batch conversion for existing assets
- Runtime adapters that can bridge between systems during transition
- Validation tools to ensure data consistency

### 4. Data Migration Strategy

For a smooth transition, consider a phased migration approach:

1. **Schema Definition Phase**: Define USD schemas that match your existing data model
2. **Tool Development Phase**: Create conversion and validation tools
3. **Pilot Migration Phase**: Convert a small subset of game data to test workflows
4. **Incremental Migration Phase**: Convert production data in manageable chunks
5. **Legacy Support Phase**: Maintain bridges between old and new systems
6. **Cleanup Phase**: Remove legacy systems when no longer needed

This gradual approach minimizes risk and allows for course correction during migration.

## Implementation Patterns and Best Practices

Based on production experience with codeless schemas in games, here are some established best practices:

### Consistent Namespacing

As shown in our examples, establish a consistent namespace prefix (like `sparkle:`) and sub-namespaces for different systems:

```
sparkle:entity:*   # Core entity properties
sparkle:health:*   # Health system properties
sparkle:combat:*   # Combat system properties
```

This prevents naming collisions and enhances code readability.

### Documentation

Include comprehensive documentation in your schema definitions:
- Overall schema purpose in the class `doc` field
- Property-specific documentation with `doc` attribute
- Usage examples in comments

Well-documented schemas are easier for team members to understand and use correctly.

### Default Values

Always provide sensible default values for properties. This ensures:
- New entities work reasonably without explicit configuration
- Minimal work required for common cases
- Clear intent about expected property ranges

### Validation

Implement validation for your schemas:
- Use `allowedTokens` for enumeration-like properties
- Create validation tools that check schema constraints
- Implement runtime validation when loading entities

Good validation catches mistakes early and prevents subtle game bugs.

### Schema Evolution

As your game develops, schemas will evolve. Plan for this by:
- Maintaining schema version metadata
- Creating migration tools for existing data
- Using composition to layer schema changes rather than replacing schemas
- Avoiding breaking changes where possible

## Key Takeaways

- A well-structured game schema system combines entity types and component API schemas
- Consistent naming conventions with proper namespacing avoid conflicts and enhance readability
- Multi-apply API schemas enable extensible properties like loot tables without schema proliferation
- Converting from existing game databases requires careful mapping and a phased migration approach
- Effective schema implementations include good documentation, defaults, and validation

In the next chapter, we'll explore pipeline integration strategies to ensure these schemas work effectively across your entire development pipeline.