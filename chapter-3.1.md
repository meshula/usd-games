# Chapter 3.1: USD Composition Fundamentals for Games

## Understanding Composition Arcs in Game Contexts

USD's composition system provides a rich toolkit for building complex game scenes and entities. In this section, we'll explore each composition mechanism (LIVRPS) in detail, with a specific focus on how game developers can leverage these tools for building interactive experiences.

Understanding these fundamentals is essential before diving into specific game systems, as they form the building blocks of any composition-based approach.

## Local Opinions: The Foundation of Composition

Local opinions represent the direct property values authored on a prim. They are the most basic form of composition and typically have the highest strength in the composition arc.

### Game Development Applications

In game development, local opinions serve several key purposes:

1. **Instance-specific customization**: Setting unique properties on individual game entities
2. **Runtime property changes**: Updating dynamic properties during gameplay
3. **Level design tweaks**: Making specific adjustments to placed instances
4. **Gameplay state**: Representing the current state of interactive elements

### Example: Enemy Instance Customization

```usda
def "Level" {
    def "Enemies" {
        def "Carrot_01" (
            references = @enemy_templates/carrot_grunt.usda@</Enemy>
        )
        {
            # Local opinion overriding the referenced template
            float sparkle:health:current = 150
            token sparkle:ai:behavior = "aggressive"
        }
    }
}
```

In this example, we've referenced a template but used local opinions to customize this particular instance, making it stronger and more aggressive than the standard template.

## Inherits: Class-Based Schema Relationships

The `inherits` composition arc provides class-based inheritance, allowing prims to inherit properties and structure from one or more class prims. This creates an is-a relationship that's continuously resolved at runtime.

### Game Development Applications

1. **Game entity class hierarchies**: Creating base classes for enemy types, items, etc.
2. **Shared behavior definitions**: Defining common behaviors across related entities
3. **Template libraries**: Building hierarchies of increasingly specialized game elements
4. **Abstract gameplay components**: Defining interfaces for gameplay systems

### Example: Enemy Class Hierarchy with Codeless Schemas

The class inheritance we're using here works perfectly with the codeless schemas we learned about in Chapter 2. Let's see how this looks:

```usda
# In enemy_types.usda
class "BaseEnemy" (
    # This is a codeless schema - no C++ code generation needed
    inherits = </Typed>
    customData = {
        bool skipCodeGeneration = true
    }
)
{
    float sparkle:health:maximum = 100
    float sparkle:health:current = 100
    float sparkle:combat:damage = 10
    token sparkle:ai:behavior = "neutral"
}

class "MeleeEnemy" (
    inherits = </BaseEnemy>
    # Inherits the codeless nature from BaseEnemy
)
{
    float sparkle:combat:attackRadius = 2
    token sparkle:combat:damageType = "physical"
}

class "RangedEnemy" (
    inherits = </BaseEnemy>
    # Inherits the codeless nature from BaseEnemy
)
{
    float sparkle:combat:attackRadius = 20
    token sparkle:combat:damageType = "projectile"
}
```

```usda
# In level.usda
def "Level" {
    def "Enemies" {
        def "Carrot_01" (
            inherits = </MeleeEnemy>
        )
        {
            # Specific properties for this instance
            token sparkle:entity:id = "carrot_grunt_01"
        }
    }
}
```

This approach creates a clean inheritance hierarchy using codeless schemas - meaning these enemy types can be distributed as assets rather than compiled plugins. Artists and designers can create and modify these class hierarchies without requiring engineering support or engine rebuilds. The engine accesses these properties with standard USD property access (as we saw in Chapter 2) rather than through generated C++ schema classes.

## Variants: Swappable Alternate Representations

Variants allow for different representations of a prim to be included in the same file, with a mechanism to select which one is active. A variant set is a named collection of variants, and each variant contains a different set of opinions.

### Game Development Applications

1. **Equipment and customization**: Different visual representations of characters
2. **Level state changes**: Alternate versions of environments (day/night, damaged/intact)
3. **Difficulty modes**: Different enemy configurations based on game difficulty
4. **Platform-specific content**: Optimized assets for different hardware capabilities
5. **Gameplay options**: Alternative implementations of game mechanics

### Example: Character Equipment System

```usda
def "Character" {
    # Basic character properties
    float sparkle:health:maximum = 100
    float sparkle:health:current = 100
    
    # Equipment variant set
    variantSet "armor" = {
        "none" {
            over "Appearance" {
                def "BaseAppearance" { }
                # No additional defense
            }
        }
        "light" {
            over "Appearance" {
                def "BaseAppearance" { }
                def "LightArmor" { }
            }
            float sparkle:combat:defense = 5
        }
        "heavy" {
            over "Appearance" {
                def "BaseAppearance" { }
                def "HeavyArmor" { }
            }
            float sparkle:combat:defense = 15
            float sparkle:movement:speed = 3  # Reduced speed
        }
    }
}
```

In a game, you could switch variants at runtime to represent equipment changes:

```cpp
// Switch armor type during gameplay
UsdPrim character = stage->GetPrimAtPath("/Character");
UsdVariantSet armorVariant = character.GetVariantSet("armor");
armorVariant.SetVariantSelection("heavy");
```

## References: Including Objects by Reference

References allow a prim to incorporate the opinions from another prim, potentially in another layer or file. References create a composition relationship where the referenced prim contributes its properties and children to the referencing prim.

### Game Development Applications

1. **Prefab/template instances**: Placing pre-configured game entities in levels
2. **Modular level design**: Combining environment modules to create larger spaces
3. **Asset reuse**: Including common game elements across multiple levels
4. **NPC placement**: Positioning character templates throughout the game world

### Example: Modular Level Construction

```usda
def "ModularDungeon" {
    def "Room_01" (
        references = @dungeon_modules/entrance_hall.usda@</Module>
    )
    {
        # Position the module
        matrix4d xformOp:transform = ( (1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0), (0, 0, 0, 1) )
        uniform token[] xformOpOrder = ["xformOp:transform"]
    }
    
    def "Room_02" (
        references = @dungeon_modules/treasure_chamber.usda@</Module>
    )
    {
        # Position the module
        matrix4d xformOp:transform = ( (1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0), (10, 0, 5, 1) )
        uniform token[] xformOpOrder = ["xformOp:transform"]
    }
    
    def "Enemies" {
        def "Guard_01" (
            references = @enemy_templates/skeleton_guard.usda@</Enemy>
        )
        {
            # Position the enemy
            matrix4d xformOp:transform = ( (1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0), (5, 0, 2, 1) )
            uniform token[] xformOpOrder = ["xformOp:transform"]
        }
    }
}
```

This pattern is similar to prefab instantiation in game engines, but with USD's additional composition capabilities.

## Payloads: On-Demand Loading for Streaming

Payloads are similar to references but are loaded on demand rather than automatically. This makes payloads perfect for implementing streaming content in games.

### Game Development Applications

1. **Level streaming**: Loading sections of levels as the player approaches
2. **Detailed asset streaming**: Loading high-detail assets at appropriate distances
3. **Optional content**: Loading content based on player choices or system capabilities
4. **Background loading**: Preparing content without blocking gameplay

### Example: Streaming Level Implementation

```usda
def "OpenWorld" {
    def "MainVillage" (
        payload = @levels/village.usda@</Village>
    )
    {
        # Always loaded with the main level
    }
    
    def "ForestRegion" (
        payload = @levels/forest.usda@</Forest>
        prepayload = true  # Start loading this early
    )
    {
        # Will be loaded when activated
    }
    
    def "MountainRegion" (
        payload = @levels/mountains.usda@</Mountains>
    )
    {
        # Will be loaded when activated
    }
    
    def "DungeonEntrance" (
        payload = @levels/dungeon_exterior.usda@</Entrance>
    )
    {
        # Exterior only - dungeon interior is loaded separately
        
        def "DungeonInterior" (
            payload = @levels/dungeon_interior.usda@</Dungeon>
        )
        {
            # Interior - loaded when player enters
        }
    }
}
```

In game code, you would load payloads as needed:

```cpp
// When player approaches forest region
stage->LoadPayload(SdfPath("/OpenWorld/ForestRegion"));

// When player leaves mountain region
stage->UnloadPayload(SdfPath("/OpenWorld/MountainRegion"));
```

## Specialize: Lightweight Inheritance

Specializes is similar to inherits but creates a weaker composition arc. The specialized prim starts with the same properties as the target but can override them freely. Unlike inherits, changes to the source don't automatically propagate to specializations after they diverge.

### Game Development Applications

1. **Variant implementations**: Creating variations of entities with substantial differences
2. **One-way templating**: Starting with a template but then diverging significantly
3. **Prototype branching**: Creating offshoots of prototype entities
4. **Asset customization**: Making heavily modified versions of base assets

### Example: Enemy Specialization

```usda
def "EnemyTemplates" {
    def "BaseCarrot" {
        float sparkle:health:maximum = 100
        float sparkle:combat:damage = 10
        token sparkle:entity:category = "enemy"
    }
}

def "Level" {
    def "Enemies" {
        def "EliteCarrot" (
            specializes = </EnemyTemplates/BaseCarrot>
        )
        {
            # Significant customization that won't be affected by future changes to BaseCarrot
            float sparkle:health:maximum = 250
            float sparkle:combat:damage = 30
            token sparkle:entity:type = "elite"
            
            # Additional properties not in the base template
            token sparkle:combat:specialAttack = "poison"
            float sparkle:loot:modifier = 2.0
        }
    }
}
```

## Composition Strength and Opinions

USD resolves composition using a strength-ordering system, where stronger opinions override weaker ones. The default strength ordering from strongest to weakest is:

1. **Local opinions** (direct values on the prim)
2. **References** (including payloads when loaded)  
3. **Inherits**
4. **Specializes**

Within each composition arc, time-based samples are resolved by time, and list-edits are combined based on their operations.

### Game Development Applications

1. **Layered game data**: Building data from defaults, then specializations, then instance-specific values
2. **Runtime modifications**: Applying temporary gameplay effects by adding stronger opinions
3. **Override systems**: Creating override layers for debugging or testing
4. **Persistent changes**: Storing player modifications in stronger layers

### Example: Layered Character Definition

```usda
# Base template (weakest)
def "PlayerCharacter" {
    float sparkle:health:maximum = 100
    float sparkle:health:current = 100
    float sparkle:combat:damage = 10
    
    # Equipment slots defined but empty
    def "Equipment" {
        def "Weapon" { }
        def "Armor" { }
    }
}

# Class specialization (stronger)
over "PlayerCharacter" (
    specializes = </PlayerCharacter>
)
{
    # Class-specific modifications
    float sparkle:health:maximum = 150  # Higher base health for this class
    
    over "Equipment" {
        over "Weapon" (
            references = @weapons/starter_sword.usda@</Weapon>
        )
        {
        }
    }
}

# Player customization (even stronger)
over "PlayerCharacter" {
    # Player-chosen name
    string sparkle:character:name = "Sparklebeard"
    
    over "Equipment" {
        over "Armor" (
            references = @armor/leather_tunic.usda@</Armor>
        )
        {
        }
    }
}

# Dynamic gameplay state (strongest)
over "PlayerCharacter" {
    # Current state during gameplay
    float sparkle:health:current = 75  # Taken some damage
    
    # Temporary gameplay effect
    float sparkle:combat:damageMultiplier = 1.5  # Power-up active
}
```

This layered approach allows you to separate permanent and temporary changes, game design defaults, and player customizations.

## Composition Metadata

USD provides several metadata fields that control how composition works:

1. **active**: Determines if a prim and its descendants contribute to the composition
2. **hidden**: Controls visibility in UIs but doesn't affect composition
3. **kind**: Categorizes prims (component, assembly, group, etc.)
4. **instanceable**: Enables instance-based optimization
5. **variants** and **variantSets**: Define and select variants
6. **references**, **inherits**, **specializes**, **payload**: Establish composition arcs

### Game Development Applications

1. **Gameplay activation**: Using `active` to enable/disable game elements
2. **Optimization hints**: Using `instanceable` for repeated elements
3. **Organizational metadata**: Using `kind` to categorize game content
4. **Debug visibility**: Using `hidden` for developer-only elements

### Example: Using Metadata for Gameplay

```usda
def "GameLevel" {
    def "EnemySpawner" (
        active = true  # Can be toggled to enable/disable spawning
    )
    {
        rel sparkle:spawner:enemyTemplate = </GameTemplates/Enemies/Carrot>
        int sparkle:spawner:maxEnemies = 5
        float sparkle:spawner:spawnRadius = 10
    }
    
    def "EnvironmentProps" {
        def "Trees" (
            instanceable = true  # Many identical trees, optimize as instances
        )
        {
            def "Tree_01" (
                references = @environment/pine_tree.usda@</Tree>
            )
            {
                matrix4d xformOp:transform = ( (1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0), (10, 0, 5, 1) )
                uniform token[] xformOpOrder = ["xformOp:transform"]
            }
            # More trees...
        }
    }
    
    def "DebugHelpers" (
        hidden = true  # Not visible in game, only in editor
    )
    {
        def "SpawnPoints" {
            # Debug visualization of spawn points
        }
    }
}
```

## Composition Patterns and Anti-Patterns

Based on production experience, certain composition patterns tend to work well for games, while others can lead to problems.

### Effective Patterns

1. **Reference-based instantiation**: Using references for template-based game object creation
2. **Variant-based configuration**: Using variants for swappable configurations
3. **Layer-based state management**: Using layers to manage different aspects of state
4. **Payload-based streaming**: Using payloads for level streaming and content loading
5. **Class-based property inheritance**: Using inherits for common properties and behaviors

### Anti-Patterns to Avoid

1. **Deep composition chains**: Creating deeply nested composition relationships that are hard to debug
2. **Overreliance on variants**: Using variants for everything instead of more appropriate mechanisms
3. **Mixing composition semantics**: Using inherits where specialize would be more appropriate (or vice versa)
4. **Scattered property overrides**: Spreading overrides across many layers without clear organization
5. **Monolithic variant sets**: Creating huge variant sets with many options instead of composing smaller variants

## Integration with Game Engine Patterns

USD's composition system can map to familiar game engine patterns:

| Game Engine Concept | USD Composition Equivalent |
|---------------------|----------------------------|
| Prefab instantiation | References |
| Inheritance hierarchies | Inherits |
| Component systems | API schemas + composition |
| Level streaming | Payloads |
| Object variations | Variants |
| Object activation | active metadata |

Understanding these mappings helps developers bridge conceptual gaps between traditional game engine approaches and USD-based workflows.

## Composition with Codeless Schemas: Reinforcing the Approach

It's important to remember that all the composition techniques we've explored work seamlessly with the codeless schemas we learned about in Chapter 2. The key differences are in deployment and access patterns:

1. **Deployment as assets**: Codeless schemas with composition are distributed like any other asset update, not as compiled plugins. This means level designers can create new enemy variants, equipment types, or game mechanics without requiring an engine rebuild.

2. **Standard property access**: At runtime, the engine accesses these composed properties using standard USD property access methods rather than through generated C++ API classes:

```cpp
// Access to a composed property in a codeless schema
UsdPrim enemyPrim = stage->GetPrimAtPath("/Level/Enemies/Carrot_01");
UsdAttribute healthAttr = enemyPrim.GetAttribute("sparkle:health:current");
float health;
healthAttr.Get(&health);
```

3. **Pipeline integration**: Changes to composition structure (adding variants, changing inheritance, etc.) can flow through the content pipeline rather than requiring code changes and recompilation.

4. **Iteration speed**: This approach dramatically speeds up iteration, as game designers can experiment with different composition strategies without waiting for engineering support.

The combination of codeless schemas with USD's powerful composition system provides the best of both worlds: the flexibility and deployment advantages of data-driven approaches with the structural rigor and expression power of a schema-based type system.

## Key Takeaways

- USD's LIVRPS composition arcs provide a comprehensive toolkit for building game content
- Each composition mechanism has specific use cases in game development
- Composition strength determines how conflicts are resolved
- Metadata provides additional control over composition behavior
- Effective patterns leverage composition for maintainability and flexibility
- Understanding the mapping to familiar game engine concepts helps with adoption

In the next section, we'll explore how these composition fundamentals can be applied to create advanced level of detail systems that go beyond simple geometry swapping.
