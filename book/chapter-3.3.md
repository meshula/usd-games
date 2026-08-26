# Chapter 3.3: Entity Templates and Instance Variation

## The Power of Templated Game Entities

Game worlds are built from repeating elements: enemies, items, props, buildings, and more. Creating and maintaining these entities efficiently is crucial for scalable game development. USD's composition system provides powerful mechanisms for implementing template-based workflows that will feel familiar to game developers accustomed to prefab systems, but with additional flexibility and capabilities.

In this chapter, we'll explore how to create reusable entity templates and efficiently generate variations using USD's composition arcs.

## Entity Templates vs. Traditional Prefabs

Traditional game engine prefabs typically offer a simple parent-child relationship between a template and its instances. USD's composition system provides a richer set of tools:

| Feature | Traditional Prefabs | USD Composition |
|---------|---------------------|-----------------|
| Template changes propagate to instances | ✓ | ✓ |
| Instance-specific overrides | ✓ | ✓ |
| Deep nesting of templates | Sometimes | ✓ |
| Multiple inheritance from templates | Rarely | ✓ |
| Layered override system | Limited | ✓ |
| Variant configurations | Limited | ✓ |
| Template versioning | Rarely | ✓ |
| Late-binding of references | Rarely | ✓ |

This expanded toolkit enables more flexible and powerful entity authoring workflows.

## Creating Entity Template Libraries

A well-organized template library forms the foundation of efficient entity creation. Let's explore how to structure such a library using USD composition.

### Base Template Structure

```usda
#usda 1.0
(
    defaultPrim = "Templates"
)

def "Templates" (
    kind = "group"
)
{
    def "Characters" (
        kind = "group"
    )
    {
        def "BaseCharacter" (
            kind = "component"
        )
        {
            # Common character properties
            float sparkle:health:maximum = 100
            float sparkle:health:current = 100
            float sparkle:movement:speed = 5
            
            # Common character structure
            def Xform "Appearance" {}
            def Xform "Equipment" {}
            def Xform "Animation" {}
            def Xform "Audio" {}
            def Xform "Effects" {}
            def Xform "Behavior" {}
            def Xform "Physics" {}
        }
        
        def "BaseEnemy" (
            inherits = </Templates/Characters/BaseCharacter>
            kind = "component"
        )
        {
            # Enemy-specific defaults
            token sparkle:entity:category = "enemy"
            token sparkle:ai:behavior = "aggressive"
        }
        
        def "BaseNPC" (
            inherits = </Templates/Characters/BaseCharacter>
            kind = "component"
        )
        {
            # NPC-specific defaults
            token sparkle:entity:category = "npc"
            token sparkle:ai:behavior = "neutral"
        }
    }
    
    def "Items" (
        kind = "group"
    )
    {
        def "BaseItem" (
            kind = "component"
        )
        {
            # Common item properties
            token sparkle:entity:category = "item"
            token sparkle:interaction:type = "pickup"
            
            # Common item structure
            def Xform "Appearance" {}
            def Xform "Effects" {}
            def Xform "Physics" {}
        }
        
        def "BaseWeapon" (
            inherits = </Templates/Items/BaseItem>
            kind = "component"
        )
        {
            # Weapon-specific defaults
            token sparkle:item:type = "weapon"
            float sparkle:weapon:damage = 10
            float sparkle:weapon:range = 2
        }
        
        def "BaseConsumable" (
            inherits = </Templates/Items/BaseItem>
            kind = "component"
        )
        {
            # Consumable-specific defaults
            token sparkle:item:type = "consumable"
            token sparkle:consumable:effect = "none"
            float sparkle:consumable:amount = 0
        }
    }
    
    def "Environment" (
        kind = "group"
    )
    {
        def "BaseProp" (
            kind = "component"
        )
        {
            # Common prop properties
            token sparkle:entity:category = "prop"
            bool sparkle:physics:isStatic = true
            
            # Common prop structure
            def Xform "Appearance" {}
            def Xform "Physics" {}
        }
        
        def "BaseBuilding" (
            kind = "component"
        )
        {
            # Common building properties
            token sparkle:entity:category = "building"
            bool sparkle:physics:isStatic = true
            
            # Common building structure
            def Xform "Exterior" {}
            def Xform "Interior" {}
            def Xform "Physics" {}
        }
    }
}
```

This foundational structure provides base templates organized by category, establishing consistent default properties and structure.

### Specific Entity Templates

Building on this foundation, we can create more specific entity templates:

```usda
#usda 1.0
(
    defaultPrim = "Templates"
    subLayers = [
        @base_templates.usda@
    ]
)

over "Templates" {
    over "Characters" {
        def "Carrot" (
            inherits = </Templates/Characters/BaseEnemy>
            kind = "component"
        )
        {
            # Carrot enemy template
            token sparkle:entity:type = "carrot"
            float sparkle:health:maximum = 75
            float sparkle:combat:damage = 15
            float sparkle:movement:speed = 3
            
            over "Appearance" {
                def Xform "Geometry" {
                    def Mesh "Body" (
                        references = @assets/characters/carrot_body.usda@</Mesh>
                    )
                    {
                    }
                    
                    def Mesh "Leaves" (
                        references = @assets/characters/carrot_leaves.usda@</Mesh>
                    )
                    {
                    }
                }
                
                def Xform "Materials" {
                    def Material "CarrotMaterial" (
                        references = @assets/materials/carrot_material.usda@</Material>
                    )
                    {
                    }
                    
                    def Material "LeavesMaterial" (
                        references = @assets/materials/leaves_material.usda@</Material>
                    )
                    {
                    }
                }
            }
            
            over "Animation" {
                def Skeleton "CarrotSkeleton" (
                    references = @assets/skeletons/carrot_skeleton.usda@</Skeleton>
                )
                {
                }
                def SkelAnimation "CarrotIdleAnim" (
                    references = @assets/animations/carrot_idle.usda@</Animation>
                )
                {
                }
                def SkelAnimation "CarrotWalkAnim" (
                    references = @assets/animations/carrot_walk.usda@</Animation>
                )
                {
                }
                def SkelAnimation "CarrotAttackAnim" (
                    references = @assets/animations/carrot_attack.usda@</Animation>
                )
                {
                }
            }
            
            over "Behavior" {
                def "CarrotBehavior" {
                    token sparkle:ai:attackType = "melee"
                    token sparkle:ai:movementType = "ground"
                    float sparkle:ai:aggroRadius = 8
                    float sparkle:ai:attackRadius = 2
                }
            }
            
            # Variant set for different carrot types
            variantSet "carrotType" = {
                "basic" {
                    # Default values from above
                }
                
                "elite" {
                    over "Appearance" {
                        over "Materials" {
                            over "CarrotMaterial" (
                                references = @assets/materials/elite_carrot_material.usda@</Material>
                            )
                            {
                            }
                        }
                    }
                    float sparkle:health:maximum = 150
                    float sparkle:combat:damage = 25
                }
                
                "giant" {
                    over "Appearance" {
                        over "Geometry" {
                            # Scaling transformation
                            matrix4d xformOp:transform = ( (2, 0, 0, 0), (0, 2, 0, 0), (0, 0, 2, 0), (0, 0, 0, 1) )
                            uniform token[] xformOpOrder = ["xformOp:transform"]
                        }
                    }
                    float sparkle:health:maximum = 300
                    float sparkle:combat:damage = 40
                    float sparkle:movement:speed = 2
                }
            }
        }
        
        def "Farmer" (
            inherits = </Templates/Characters/BaseNPC>
            kind = "component"
        )
        {
            # Farmer NPC template
            token sparkle:entity:type = "farmer"
            
            over "Appearance" {
                def Mesh "FarmerMesh" (
                    references = @assets/characters/farmer.usda@</Mesh>
                )
                {
                }
            }
            
            over "Behavior" {
                def "FarmerBehavior" {
                    token sparkle:ai:behavior = "neutral"
                    rel sparkle:ai:dialogTree = </DialogTrees/Farmer>
                    string[] sparkle:ai:schedule:activities = [
                        "sleep", "work_field", "rest", "work_field", "socialize", "sleep"
                    ]
                    float[] sparkle:ai:schedule:times = [
                        0.0, 6.0, 12.0, 13.0, 18.0, 22.0
                    ]
                }
            }
            
            # Variant set for different farmer types
            variantSet "farmerType" = {
                "regular" {
                    # Default values from above
                }
                
                "merchant" {
                    over "Behavior" {
                        over "FarmerBehavior" {
                            string[] sparkle:ai:schedule:activities = [
                                "sleep", "open_shop", "close_shop", "sleep"
                            ]
                            float[] sparkle:ai:schedule:times = [
                                0.0, 8.0, 18.0, 22.0
                            ]
                            rel sparkle:ai:dialogTree = </DialogTrees/Merchant>
                            rel sparkle:merchant:inventory = </InventoryTemplates/FarmerShop>
                        }
                    }
                }
            }
        }
    }
    
    over "Items" {
        def "HealthPotion" (
            inherits = </Templates/Items/BaseConsumable>
            kind = "component"
        )
        {
            # Health potion template
            token sparkle:entity:id = "item_health_potion"
            token sparkle:consumable:effect = "health"
            float sparkle:consumable:amount = 50
            
            over "Appearance" {
                def Mesh "PotionMesh" (
                    references = @assets/items/potion_bottle.usda@</Mesh>
                )
                {
                }
                
                def Material "PotionMaterial" (
                    references = @assets/materials/red_liquid.usda@</Material>
                )
                {
                }
            }
            
            over "Effects" {
                def Xform "UseEffect" {
                    def PointInstancer "HealParticles" (
                        references = @assets/effects/heal_particles.usda@</Effect>
                    )
                    {
                    }
                }
            }
            
            # Variant set for different potion strengths
            variantSet "potionStrength" = {
                "minor" {
                    float sparkle:consumable:amount = 25
                }
                
                "regular" {
                    # Default values from above
                }
                
                "major" {
                    float sparkle:consumable:amount = 100
                    over "Appearance" {
                        over "PotionMesh" (
                            references = @assets/items/large_potion_bottle.usda@</Mesh>
                        )
                        {
                        }
                    }
                }
            }
        }
    }
}
```

This example shows how to create specific entity templates like "Carrot" enemies and "Farmer" NPCs, both with variant sets for additional customization options.

## Instance Creation with References

With our template library established, we can create instances using references:

```usda
#usda 1.0
(
    defaultPrim = "Level"
)

def "Level" (
    kind = "assembly"
)
{
    def "Enemies" {
        # Basic carrot enemy
        def "Carrot_01" (
            references = @templates/characters.usda@</Templates/Characters/Carrot>
        )
        {
            # Instance-specific properties
            float3 xformOp:translate = (10, 0, 5)
            uniform token[] xformOpOrder = ["xformOp:translate"]
            token sparkle:entity:id = "enemy_carrot_01"
        }
        
        # Elite carrot enemy
        def "Carrot_02" (
            references = @templates/characters.usda@</Templates/Characters/Carrot>
        )
        {
            # Select the "elite" variant
            string variants:carrotType = "elite"
            
            # Instance-specific properties
            float3 xformOp:translate = (15, 0, 8)
            uniform token[] xformOpOrder = ["xformOp:translate"]
            token sparkle:entity:id = "enemy_carrot_02"
            
            # Override a template property
            float sparkle:health:current = 120
        }
        
        # Giant carrot enemy
        def "Carrot_03" (
            references = @templates/characters.usda@</Templates/Characters/Carrot>
        )
        {
            # Select the "giant" variant
            string variants:carrotType = "giant"
            
            # Instance-specific properties
            float3 xformOp:translate = (20, 0, 12)
            uniform token[] xformOpOrder = ["xformOp:translate"]
            token sparkle:entity:id = "enemy_carrot_boss"
            
            # Override template properties
            float sparkle:health:maximum = 500
            float sparkle:health:current = 500
            token sparkle:ai:behavior = "boss"
        }
    }
    
    def "NPCs" {
        # Regular farmer
        def "Farmer_01" (
            references = @templates/characters.usda@</Templates/Characters/Farmer>
        )
        {
            # Instance-specific properties
            float3 xformOp:translate = (5, 0, 5)
            uniform token[] xformOpOrder = ["xformOp:translate"]
            token sparkle:entity:id = "npc_farmer_01"
            string sparkle:character:name = "Bob the Farmer"
        }
        
        # Merchant farmer
        def "Farmer_02" (
            references = @templates/characters.usda@</Templates/Characters/Farmer>
        )
        {
            # Select the "merchant" variant
            string variants:farmerType = "merchant"
            
            # Instance-specific properties
            float3 xformOp:translate = (8, 0, 8)
            uniform token[] xformOpOrder = ["xformOp:translate"]
            token sparkle:entity:id = "npc_merchant_01"
            string sparkle:character:name = "Alice the Merchant"
        }
    }
    
    def "Items" {
        # Health potions with different variants
        def "HealthPotion_01" (
            references = @templates/items.usda@</Templates/Items/HealthPotion>
        )
        {
            # Minor potion
            string variants:potionStrength = "minor"
            float3 xformOp:translate = (3, 1, 3)
            uniform token[] xformOpOrder = ["xformOp:translate"]
            token sparkle:entity:id = "item_minor_potion_01"
        }
        
        def "HealthPotion_02" (
            references = @templates/items.usda@</Templates/Items/HealthPotion>
        )
        {
            # Regular potion (default variant)
            float3 xformOp:translate = (4, 1, 3)
            uniform token[] xformOpOrder = ["xformOp:translate"]
            token sparkle:entity:id = "item_potion_01"
        }
        
        def "HealthPotion_03" (
            references = @templates/items.usda@</Templates/Items/HealthPotion>
        )
        {
            # Major potion
            string variants:potionStrength = "major"
            float3 xformOp:translate = (5, 1, 3)
            uniform token[] xformOpOrder = ["xformOp:translate"]
            token sparkle:entity:id = "item_major_potion_01"
        }
    }
}
```

This example creates instances of our templates with specific positioning, variant selections, and property overrides. The references ensure that any changes to the templates will propagate to all instances.

## Programmatic Instance Creation

In game development, entities are often created dynamically. Here's how you might generate instances programmatically:

```cpp
// Create an enemy instance at a specific position
UsdPrim CreateEnemyInstance(UsdStage* stage, const GfVec3f& position, const std::string& type) {
    // Generate a unique ID
    static int enemyCounter = 0;
    std::string entityId = "enemy_" + type + "_" + std::to_string(++enemyCounter);
    
    // Create a path for the new enemy
    SdfPath enemyPath = SdfPath("/Level/Enemies/" + entityId);
    
    // Create the enemy prim with a reference to the template
    UsdPrim enemyPrim = stage->DefinePrim(enemyPath);
    enemyPrim.GetReferences().AddReference("templates/characters.usda", 
                                         SdfPath("/Templates/Characters/Carrot"));
    
    // Set position
    UsdGeomXformable xformable(enemyPrim);
    xformable.AddTranslateOp().Set(position);
    
    // Set entity ID
    UsdAttribute idAttr = enemyPrim.CreateAttribute(
        TfToken("sparkle:entity:id"), SdfValueTypeNames->Token);
    idAttr.Set(TfToken(entityId));
    
    // Set variant based on type
    if (type == "elite") {
        UsdVariantSet variantSet = enemyPrim.GetVariantSet("carrotType");
        variantSet.SetVariantSelection("elite");
    }
    else if (type == "giant") {
        UsdVariantSet variantSet = enemyPrim.GetVariantSet("carrotType");
        variantSet.SetVariantSelection("giant");
        
        // Custom properties for giant type
        UsdAttribute healthAttr = enemyPrim.CreateAttribute(
            TfToken("sparkle:health:maximum"), SdfValueTypeNames->Float);
        healthAttr.Set(300.0f);
        
        UsdAttribute currentHealthAttr = enemyPrim.CreateAttribute(
            TfToken("sparkle:health:current"), SdfValueTypeNames->Float);
        currentHealthAttr.Set(300.0f);
    }
    
    return enemyPrim;
}

// Example usage in spawn system
void SpawnEnemies(UsdStage* stage, int count, const GfVec3f& centerPosition, float radius) {
    for (int i = 0; i < count; ++i) {
        // Calculate random position within spawn radius
        float angle = (float)i / count * 2 * M_PI;
        float distance = radius * sqrt(((float)rand() / RAND_MAX));
        GfVec3f position(
            centerPosition[0] + cos(angle) * distance,
            centerPosition[1],
            centerPosition[2] + sin(angle) * distance
        );
        
        // Determine enemy type based on random chance
        float typeRoll = (float)rand() / RAND_MAX;
        std::string enemyType;
        if (typeRoll < 0.1f) {
            enemyType = "giant";
        }
        else if (typeRoll < 0.3f) {
            enemyType = "elite";
        }
        else {
            enemyType = "basic";
        }
        
        // Create the enemy instance
        CreateEnemyInstance(stage, position, enemyType);
    }
}
```

This code demonstrates how to create enemy instances programmatically, setting positions, variants, and properties as needed.

## Template Overrides in Layers

USD's layer composition allows for organized overrides of template properties. This is useful for implementing game features like difficulty levels or special game modes:

```usda
# In difficulty_easy.usda
over "Templates" {
    over "Characters" {
        over "Carrot" {
            # Make enemies weaker on easy difficulty
            float sparkle:health:maximum = 50
            float sparkle:combat:damage = 10
            
            over "Behavior" {
                over "CarrotBehavior" {
                    float sparkle:ai:aggroRadius = 5  # Less aggressive
                    float sparkle:ai:attackRadius = 1.5
                }
            }
        }
    }
}

# In difficulty_hard.usda
over "Templates" {
    over "Characters" {
        over "Carrot" {
            # Make enemies stronger on hard difficulty
            float sparkle:health:maximum = 100
            float sparkle:combat:damage = 25
            
            over "Behavior" {
                over "CarrotBehavior" {
                    float sparkle:ai:aggroRadius = 12  # More aggressive
                    float sparkle:ai:attackRadius = 3
                }
            }
        }
    }
}
```

The game could then set the appropriate difficulty layer:

```cpp
// Set game difficulty
void SetGameDifficulty(UsdStage* stage, const std::string& difficulty) {
    // Get current sublayers
    SdfLayerRefPtr rootLayer = stage->GetRootLayer();
    SdfSubLayerVector sublayers = rootLayer->GetSubLayerPaths();
    
    // Remove any existing difficulty layers
    for (auto it = sublayers.begin(); it != sublayers.end(); ) {
        if (it->find("difficulty_") != std::string::npos) {
            it = sublayers.erase(it);
        } else {
            ++it;
        }
    }
    
    // Add the new difficulty layer
    std::string difficultyLayer = "difficulty_" + difficulty + ".usda";
    sublayers.push_back(difficultyLayer);
    
    // Update sublayers
    rootLayer->SetSubLayerPaths(sublayers);
}
```

## Multi-Template Composition

USD's ability to combine multiple references allows for more flexible entity composition than traditional prefab systems:

```usda
# Combining templates to create a specialized entity
def "EliteGuardFarmer" (
    # Multiple inheritance allows combining different templates
    inherits = [
        </Templates/Characters/Farmer>,  # Base appearance and farmer behavior
        </Templates/Characters/BaseEnemy>  # Combat capabilities
    ]
)
{
    # Customize the combination
    string sparkle:character:name = "Guard"
    token sparkle:entity:category = "guard"  # Override category from BaseEnemy
    
    # Combine elements from both templates
    over "Behavior" {
        token sparkle:ai:behavior = "guardian"  # New behavior type
        float sparkle:ai:aggroRadius = 10       # From enemy template
        rel sparkle:ai:dialogTree = </DialogTrees/Guard>  # From farmer template
    }
}
```

This multi-template composition allows for more flexible entity creation than traditional single-inheritance prefab systems.

## Template Versioning and Stability

Game development is iterative, and templates evolve over time. USD provides several mechanisms for managing template evolution:

### Asset Path Resolution

Using layer-configurable asset path resolution allows templates to evolve without breaking existing references:

```usda
# Using asset path resolution for version flexibility
def "Level" {
    def "Enemies" {
        def "Carrot_01" (
            # Reference uses an asset path that can be resolved differently in different contexts
            references = @templates/characters.usda@</Templates/Characters/Carrot>
        )
        {
            # Instance properties
        }
    }
}
```

The asset resolution can be configured to point to specific versions:

```cpp
// Configure asset resolution for development vs. production
void ConfigureAssetResolution(bool isDevelopment) {
    if (isDevelopment) {
        // In development, use latest templates
        ArSetPreferredResolver("templates", "dev/latest/templates");
    } else {
        // In production, use locked template version
        ArSetPreferredResolver("templates", "production/v1.2.3/templates");
    }
}
```

### Payload-Based Templates

For large templates that might change frequently, using payloads instead of references can provide more flexibility:

```usda
# Base template structure that rarely changes
def "Carrot" (
    kind = "component"
)
{
    # Core properties that rarely change
    token sparkle:entity:type = "carrot"
    token sparkle:entity:category = "enemy"
    
    # Content that may evolve is in payloads
    def "Appearance" (
        payload = @enemy_templates/carrot_appearance.usda@</Appearance>
    )
    {
    }
    
    def "Behavior" (
        payload = @enemy_templates/carrot_behavior.usda@</Behavior>
    )
    {
    }
}
```

This approach allows individual aspects of a template to evolve independently, reducing the impact of changes.

## Template Specialization for Game Regions

Different game regions or levels might need specialized versions of common templates. USD's specializes arc is perfect for this:

```usda
# Base enemy template
def "EnemyTemplates" {
    def "BaseGoblin" {
        float sparkle:health:maximum = 100
        token sparkle:combat:damageType = "physical"
        token sparkle:ai:behavior = "aggressive"
        
        def "Appearance" {
            # Base goblin appearance
        }
    }
}

# Forest region specialization
def "ForestEnemyTemplates" {
    def "ForestGoblin" (
        specializes = </EnemyTemplates/BaseGoblin>
    )
    {
        # Forest-specific modifications
        token sparkle:combat:damageType = "poison"
        
        over "Appearance" {
            # Forest-themed appearance changes
        }
    }
}

# Desert region specialization
def "DesertEnemyTemplates" {
    def "DesertGoblin" (
        specializes = </EnemyTemplates/BaseGoblin>
    )
    {
        # Desert-specific modifications
        float sparkle:health:maximum = 80  # Weaker but more numerous
        token sparkle:combat:damageType = "fire"
        
        over "Appearance" {
            # Desert-themed appearance changes
        }
    }
}
```

When creating instances, you can reference the region-specific specializations:

```usda
def "ForestLevel" {
    def "Enemies" {
        def "Goblin_01" (
            references = @forest_templates.usda@</ForestEnemyTemplates/ForestGoblin>
        )
        {
            # Instance properties
        }
    }
}
```

This approach allows for region-specific variations while maintaining a connection to the base template.

## Instanceable Optimization for Repeated Templates

For scenarios with many instances of the same template, USD's instanceable flag provides significant optimization:

```usda
def "Forest" {
    # Define a group of instanceable trees
    def "Trees" (
        instanceable = true
    )
    {
        def "PineTree_01" (
            references = @templates/environment.usda@</Templates/Environment/PineTree>
        )
        {
            float3 xformOp:translate = (0, 0, 0)
            uniform token[] xformOpOrder = ["xformOp:translate"]
        }
        
        def "PineTree_02" (
            references = @templates/environment.usda@</Templates/Environment/PineTree>
        )
        {
            float3 xformOp:translate = (5, 0, 3)
            uniform token[] xformOpOrder = ["xformOp:translate"]
        }
        
        # Hundreds more trees...
    }
}
```

The `instanceable = true` flag tells USD to optimize these prims as instances, significantly reducing memory usage and improving performance.

## Key Takeaways

- USD's composition system provides powerful tools for template-based entity creation
- Reference-based instances ensure changes to templates propagate to all instances
- Variant sets enable configurable template options
- Layer-based overrides allow for systematic modifications like difficulty levels
- Multi-template composition enables more flexible entity creation than traditional prefabs
- Template versioning strategies help manage evolution during development
- Region-specific specializations create thematic variations
- Instanceable optimization improves performance for repeated elements

In the next section, we'll explore how to build equipment and inventory systems using USD composition, leveraging these templating capabilities for interchangeable game items.