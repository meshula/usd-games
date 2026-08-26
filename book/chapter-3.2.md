# Chapter 3.2: Advanced LOD Through Composition

## Beyond Geometric Level of Detail

Level of Detail (LOD) is a familiar concept in game development, traditionally focused on swapping geometric complexity based on camera distance. USD's composition system offers a significantly more powerful approach to LOD that extends well beyond mesh simplification, enabling developers to manage complexity across all aspects of a game scene.

In this chapter, we'll explore how to leverage USD composition to implement comprehensive LOD strategies that encompass not just geometry, but behavior, simulation, functionality, and more.

## The Multi-Dimensional Nature of Game Complexity

Games are complex systems with multiple dimensions of complexity:

1. **Visual complexity**: Geometry, textures, materials, effects
2. **Behavioral complexity**: AI, animation, character logic
3. **Simulation complexity**: Physics, cloth, fluid, particles
4. **Functional complexity**: Game mechanics, interactions
5. **Audio complexity**: Sound effects, ambient audio, mixing

Traditional LOD approaches primarily address visual complexity, leaving other dimensions to be managed through separate, often ad-hoc systems. USD composition allows us to unify these approaches under a consistent framework.

## Implementing System-Level LOD Through Composition

USD's composition arcs provide several mechanisms for implementing advanced LOD:

### Variants for Complete Substitution

Variants offer a clean way to swap entire subsystems based on distance or other criteria:

```usda
def "Enemy" {
    # Basic properties that don't change with LOD
    token sparkle:entity:id = "goblin_01"
    
    # LOD variant set
    variantSet "complexityLOD" = {
        "high" {
            # High-detail mesh
            over "Appearance" {
                def Mesh "HighDetailMesh" {
                    # 10,000 polygons
                }
            }
            
            # Complex animation
            over "Animation" {
                def Skeleton "FullSkeleton" {
                    # 80 joints
                }
            }
            
            # Full AI behavior
            over "Behavior" {
                def "FullBehaviorTree" {
                    string sparkle:ai:type = "complex"
                    int sparkle:ai:maxDecisionDepth = 5
                    rel sparkle:ai:patrolPath = </Level/Paths/ComplexPatrol>
                }
            }
            
            # Detailed physics
            over "Physics" {
                def "DetailedPhysics" {
                    token sparkle:physics:collisionType = "perBone"
                    rel sparkle:physics:collisionMesh = </Enemy/Appearance/HighDetailMesh>
                }
            }
        }
        
        "medium" {
            # Medium-detail mesh
            over "Appearance" {
                def Mesh "MediumDetailMesh" {
                    # 3,000 polygons
                }
            }
            
            # Simplified animation
            over "Animation" {
                def Skeleton "MediumSkeleton" {
                    # 30 joints
                }
            }
            
            # Reduced AI behavior
            over "Behavior" {
                def "SimplifiedBehaviorTree" {
                    string sparkle:ai:type = "basic"
                    int sparkle:ai:maxDecisionDepth = 3
                    rel sparkle:ai:patrolPath = </Level/Paths/SimplePatrol>
                }
            }
            
            # Simplified physics
            over "Physics" {
                def "SimplifiedPhysics" {
                    token sparkle:physics:collisionType = "capsule"
                    rel sparkle:physics:collisionMesh = </Enemy/Appearance/MediumDetailMesh>
                }
            }
        }
        
        "low" {
            # Low-detail mesh
            over "Appearance" {
                def Mesh "LowDetailMesh" {
                    # 500 polygons
                }
            }
            
            # Minimal animation
            over "Animation" {
                def Skeleton "LowSkeleton" {
                    # 10 joints
                }
            }
            
            # Minimal AI behavior
            over "Behavior" {
                def "MinimalBehaviorTree" {
                    string sparkle:ai:type = "stationary"
                    int sparkle:ai:maxDecisionDepth = 1
                }
            }
            
            # Minimal physics
            over "Physics" {
                def "MinimalPhysics" {
                    token sparkle:physics:collisionType = "simple"
                }
            }
        }
    }
}
```

This approach allows for complete, consistent swapping of entire subsystems based on LOD level.

### Payloads for On-Demand System Activation

Payloads enable dynamic loading of entire subsystems when needed:

```usda
def "Castle" {
    # Always-loaded core components
    def "ExteriorStructure" {
        # Base castle geometry always present
    }
    
    # Detailed exterior elements
    def "ExteriorDetails" (
        payload = @castle/exterior_details.usda@</Details>
    )
    {
        # Load when player approaches
    }
    
    # Interior components only loaded when player enters
    def "Interior" (
        payload = @castle/interior.usda@</Interior>
    )
    {
        # Interior spaces and furnishings
    }
    
    # NPC system only loaded when player is close enough to interact
    def "GuardSystem" (
        payload = @castle/guard_system.usda@</Guards>
    )
    {
        # Guard patrols, AI behavior, etc.
    }
    
    # Distant encounter system
    def "DistantEncounters" (
        payload = @castle/distant_encounters.usda@</Encounters>
    )
    {
        # Distant encounters visible from castle
    }
}
```

The engine would control payload loading based on player position, game state, and available resources:

```cpp
// Player is approaching the castle
if (distanceToPlayer < 1000.0f) {
    stage->LoadPayload(SdfPath("/Castle/ExteriorDetails"));
}

// Player is very close to the castle
if (distanceToPlayer < 200.0f) {
    stage->LoadPayload(SdfPath("/Castle/GuardSystem"));
}

// Player enters the castle
if (playerIsInside) {
    stage->LoadPayload(SdfPath("/Castle/Interior"));
    
    // Unload distant encounters to save resources
    stage->UnloadPayload(SdfPath("/Castle/DistantEncounters"));
}
```

### Active Metadata for System Toggling

The `active` metadata provides a lightweight way to toggle entire subsystems:

```usda
def "EnemyCamp" {
    # Base camp structure always active
    def "Structures" {
        # Tents, campfire, etc.
    }
    
    # Guard patrol system - only active when player is nearby
    def "GuardPatrols" (
        active = false  # Initially inactive
    )
    {
        def "PatrolRoutes" {
            # Patrol path definitions
        }
        
        def "Guards" {
            def "Guard_01" {
                # Guard properties and behavior
            }
            
            def "Guard_02" {
                # Guard properties and behavior
            }
        }
    }
    
    # Ambient life - birds, small animals, etc. - only active at medium distance
    def "AmbientLife" (
        active = false  # Initially inactive
    )
    {
        # Birds, small animals, etc.
    }
    
    # Distant visual effects - only active when viewed from far away
    def "DistantEffects" (
        active = true  # Initially active for distant viewing
    )
    {
        # Smoke plumes, distant activity, etc.
    }
}
```

At runtime, the engine would toggle these systems based on player distance:

```cpp
// Update system activation based on distance
void UpdateSystemLOD(UsdStage* stage, const SdfPath& basePath, float distance) {
    // Get prims for different subsystems
    UsdPrim guardPrim = stage->GetPrimAtPath(basePath.AppendChild(TfToken("GuardPatrols")));
    UsdPrim ambientPrim = stage->GetPrimAtPath(basePath.AppendChild(TfToken("AmbientLife")));
    UsdPrim distantPrim = stage->GetPrimAtPath(basePath.AppendChild(TfToken("DistantEffects")));
    
    // Very close - activate guard patrols, ambient life, deactivate distant effects
    if (distance < 100.0f) {
        guardPrim.SetActive(true);
        ambientPrim.SetActive(true);
        distantPrim.SetActive(false);
    }
    // Medium distance - deactivate guards, activate ambient, deactivate distant
    else if (distance < 500.0f) {
        guardPrim.SetActive(false);
        ambientPrim.SetActive(true);
        distantPrim.SetActive(false);
    }
    // Far distance - deactivate guards, deactivate ambient, activate distant
    else {
        guardPrim.SetActive(false);
        ambientPrim.SetActive(false);
        distantPrim.SetActive(true);
    }
}
```

## LOD Control Through Schema Properties

While composition arcs provide powerful LOD capabilities, you can also implement LOD control through schema properties:

```usda
class "SparkleGameEntity" (
    inherits = </Typed>
    customData = {
        bool skipCodeGeneration = true
    }
)
{
    # LOD control properties
    float3 sparkle:lod:distanceRanges = (100, 500, 1000)  # Distances for LOD0, LOD1, LOD2
    bool sparkle:lod:dynamicBehaviorEnabled = true  # Whether behavior LOD is enabled
    bool sparkle:lod:dynamicPhysicsEnabled = true   # Whether physics LOD is enabled
    token sparkle:lod:falloffMode = "smooth"  # How to transition between LODs
}

class "SparkleLODAPI" (
    inherits = </APISchemaBase>
    customData = {
        token apiSchemaType = "singleApply"
        bool skipCodeGeneration = true
    }
)
{
    # Current LOD state (updated at runtime)
    int sparkle:lod:currentLevel = 0
    float sparkle:lod:blendFactor = 0.0  # For smooth transitions
    
    # Detail level properties for different systems
    token sparkle:lod:visualDetail = "high" (
        allowedTokens = ["ultra", "high", "medium", "low", "minimal"]
    )
    
    token sparkle:lod:behaviorDetail = "high" (
        allowedTokens = ["full", "high", "medium", "low", "minimal"]
    )
    
    token sparkle:lod:physicsDetail = "high" (
        allowedTokens = ["full", "high", "medium", "low", "minimal"]
    )
    
    token sparkle:lod:audioDetail = "high" (
        allowedTokens = ["full", "high", "medium", "low", "minimal"]
    )
}
```

Game code can then read these properties to control system behavior:

```cpp
// Determine current LOD level based on distance
int DetermineLODLevel(const UsdPrim& prim, float distance) {
    // Get distance ranges
    GfVec3f lodRanges(100.0f, 500.0f, 1000.0f);
    UsdAttribute lodRangesAttr = prim.GetAttribute(TfToken("sparkle:lod:distanceRanges"));
    if (lodRangesAttr) {
        lodRangesAttr.Get(&lodRanges);
    }
    
    // Determine LOD level
    if (distance < lodRanges[0]) {
        return 0;  // Highest detail
    } else if (distance < lodRanges[1]) {
        return 1;  // Medium detail
    } else if (distance < lodRanges[2]) {
        return 2;  // Low detail
    } else {
        return 3;  // Minimal detail
    }
}

// Apply LOD level to entity
void ApplyLODLevel(UsdPrim& prim, int lodLevel) {
    // Set current LOD level
    UsdAttribute currentLODAttr = prim.GetAttribute(TfToken("sparkle:lod:currentLevel"));
    if (currentLODAttr) {
        currentLODAttr.Set(lodLevel);
    }
    
    // Determine detail levels based on LOD level
    const char* detailLevels[] = {"full", "high", "medium", "low", "minimal"};
    const char* detailLevel = (lodLevel < 4) ? detailLevels[lodLevel] : detailLevels[4];
    
    // Set detail levels for different systems
    UsdAttribute visualDetailAttr = prim.GetAttribute(TfToken("sparkle:lod:visualDetail"));
    if (visualDetailAttr) {
        visualDetailAttr.Set(TfToken(detailLevel));
    }
    
    UsdAttribute behaviorDetailAttr = prim.GetAttribute(TfToken("sparkle:lod:behaviorDetail"));
    if (behaviorDetailAttr) {
        behaviorDetailAttr.Set(TfToken(detailLevel));
    }
    
    // Additional systems...
}
```

## Behavioral LOD Through Composition

One of the most significant advantages of USD's composition-based LOD is the ability to implement behavioral LOD—varying the complexity of AI, animation, and other behavioral systems based on distance or importance.

### Example: NPC Behavior LOD

```usda
def "Villager" {
    # Base villager properties
    token sparkle:entity:id = "villager_03"
    token sparkle:entity:type = "npc"
    
    # AI behavior with LOD variants
    variantSet "behaviorLOD" = {
        "full" {
            over "Behavior" {
                # Full daily schedule
                def "Schedule" {
                    string[] sparkle:ai:schedule:activities = [
                        "sleep", "wake", "eat", "work", "socialize", "eat", "leisure", "sleep"
                    ]
                    float[] sparkle:ai:schedule:times = [
                        0.0, 7.0, 7.5, 9.0, 12.0, 13.0, 18.0, 22.0
                    ]
                }
                
                # Rich dialog tree
                def "Dialog" {
                    rel sparkle:ai:dialog:tree = </DialogTrees/Villager/Full>
                    int sparkle:ai:dialog:memorySize = 10
                    bool sparkle:ai:dialog:remembersPlayer = true
                }
                
                # Full perception system
                def "Perception" {
                    float sparkle:ai:perception:sightRadius = 20.0
                    float sparkle:ai:perception:hearingRadius = 15.0
                    string[] sparkle:ai:perception:interests = [
                        "player", "otherNPCs", "animals", "unusualEvents", "combat"
                    ]
                }
                
                # Detailed emotional model
                def "Emotions" {
                    string[] sparkle:ai:emotions:types = [
                        "happiness", "sadness", "fear", "anger", "surprise", "disgust"
                    ]
                    float[] sparkle:ai:emotions:values = [
                        0.7, 0.1, 0.0, 0.0, 0.0, 0.0
                    ]
                    float sparkle:ai:emotions:decayRate = 0.1
                }
            }
        }
        
        "medium" {
            over "Behavior" {
                # Simplified schedule
                def "Schedule" {
                    string[] sparkle:ai:schedule:activities = [
                        "sleep", "awake", "work", "leisure", "sleep"
                    ]
                    float[] sparkle:ai:schedule:times = [
                        0.0, 7.0, 9.0, 18.0, 22.0
                    ]
                }
                
                # Basic dialog
                def "Dialog" {
                    rel sparkle:ai:dialog:tree = </DialogTrees/Villager/Basic>
                    int sparkle:ai:dialog:memorySize = 3
                    bool sparkle:ai:dialog:remembersPlayer = false
                }
                
                # Limited perception
                def "Perception" {
                    float sparkle:ai:perception:sightRadius = 15.0
                    float sparkle:ai:perception:hearingRadius = 10.0
                    string[] sparkle:ai:perception:interests = [
                        "player", "unusualEvents"
                    ]
                }
                
                # No emotional model at medium LOD
            }
        }
        
        "minimal" {
            over "Behavior" {
                # Stationary behavior only
                def "Schedule" {
                    string[] sparkle:ai:schedule:activities = ["idle"]
                    float[] sparkle:ai:schedule:times = [0.0]
                }
                
                # No dialog
                
                # Minimal perception
                def "Perception" {
                    float sparkle:ai:perception:sightRadius = 10.0
                    float sparkle:ai:perception:hearingRadius = 0.0
                    string[] sparkle:ai:perception:interests = ["player"]
                }
            }
        }
    }
}
```

## Simulation LOD Through Composition

Physics and other simulation systems can be expensive. USD composition allows for precise control over simulation complexity based on distance and importance.

### Example: Physics LOD for a Dynamic Bridge

```usda
def "RopeBridge" {
    # Bridge visual components
    def "Visual" {
        # Bridge visual components
    }
    
    # Physics representation with LOD variants
    variantSet "physicsLOD" = {
        "full" {
            over "Physics" {
                # Full rope simulation with 50 segments
                def "RopePhysics" {
                    int sparkle:physics:ropeSegments = 50
                    token sparkle:physics:simulationType = "verlet"
                    float sparkle:physics:tensionStiffness = 1000.0
                    float sparkle:physics:bendingStiffness = 100.0
                    bool sparkle:physics:twoWayCoupling = true  # Objects affect rope and vice versa
                    
                    # Each plank is a separate rigid body
                    def "PlankPhysics" {
                        token sparkle:physics:type = "separateRigidBodies"
                        bool sparkle:physics:planksAffectRope = true
                    }
                }
            }
        }
        
        "medium" {
            over "Physics" {
                # Simplified rope with 20 segments
                def "RopePhysics" {
                    int sparkle:physics:ropeSegments = 20
                    token sparkle:physics:simulationType = "verlet"
                    float sparkle:physics:tensionStiffness = 1000.0
                    float sparkle:physics:bendingStiffness = 0.0  # No bending stiffness at medium LOD
                    bool sparkle:physics:twoWayCoupling = true
                    
                    # Planks combined into groups
                    def "PlankPhysics" {
                        token sparkle:physics:type = "groupedRigidBodies"
                        int sparkle:physics:plankGroups = 5
                        bool sparkle:physics:planksAffectRope = false
                    }
                }
            }
        }
        
        "low" {
            over "Physics" {
                # Simple rope with 5 segments
                def "RopePhysics" {
                    int sparkle:physics:ropeSegments = 5
                    token sparkle:physics:simulationType = "simplified"
                    float sparkle:physics:tensionStiffness = 1000.0
                    float sparkle:physics:bendingStiffness = 0.0
                    bool sparkle:physics:twoWayCoupling = false  # Objects don't affect rope
                    
                    # Single rigid body for all planks
                    def "PlankPhysics" {
                        token sparkle:physics:type = "singleRigidBody"
                        bool sparkle:physics:planksAffectRope = false
                    }
                }
            }
        }
        
        "static" {
            over "Physics" {
                # No physics simulation, just static colliders
                def "StaticPhysics" {
                    token sparkle:physics:type = "staticCollider"
                    rel sparkle:physics:collisionMesh = </RopeBridge/Visual/BridgeModel>
                }
            }
        }
    }
}
```

## Coordinating LOD Systems in an Open World

In large open-world games, coordinating LOD across multiple systems requires a structured approach. USD's composition gives us the tools to implement comprehensive LOD strategies:

```usda
def "OpenWorld" {
    # LOD management system
    def "LODSystem" {
        # Global LOD settings
        float sparkle:lod:playerViewDistance = 2000.0
        int sparkle:lod:maxActiveSystems = 20
        float sparkle:lod:systemPriorityRadius = 500.0
        bool sparkle:lod:dynamicLODEnabled = true
        
        # LOD distance thresholds for different system types
        dictionary sparkle:lod:systemThresholds = {
            "visualDetail": {
                "high": 100.0,
                "medium": 500.0,
                "low": 1000.0,
                "minimal": 2000.0
            },
            "behaviorDetail": {
                "high": 50.0,
                "medium": 200.0,
                "low": 500.0,
                "minimal": 1000.0
            },
            "physicsDetail": {
                "high": 30.0,
                "medium": 100.0,
                "low": 300.0,
                "minimal": 600.0
            },
            "audioDetail": {
                "high": 50.0,
                "medium": 200.0,
                "low": 400.0,
                "minimal": 800.0
            }
        }
    }
    
    # Game regions
    def "Regions" {
        def "ForestRegion" (
            payload = @regions/forest.usda@</Forest>
        )
        {
            # LOD control specific to this region
            uniform token[] sparkle:lod:criticalSystems = ["wildlife", "vegetation", "weather"]
            float sparkle:lod:vegetationDensityMultiplier = 1.2
            
            # Entity-specific LOD overrides
            over "POIs" {
                over "AncientTree" {
                    # Always use high detail for this landmark
                    float sparkle:lod:minDetailLevel = 1.0  # Force at least high detail
                    float sparkle:lod:distanceMultiplier = 0.5  # Appear higher detail than normal
                }
            }
        }
        
        def "MountainRegion" (
            payload = @regions/mountain.usda@</Mountain>
        )
        {
            # LOD control specific to this region
            uniform token[] sparkle:lod:criticalSystems = ["pathfinding", "weather", "avalanche"]
            float sparkle:lod:viewDistanceMultiplier = 1.5  # See further in mountains
        }
    }
}
```

Game code would use this structure to make LOD decisions:

```cpp
// Determine detail level for a system based on global LOD settings
token GetSystemDetailLevel(UsdStage* stage, const UsdPrim& entityPrim, 
                           const std::string& systemType, float distance) {
    // Get LOD system prim
    UsdPrim lodSystemPrim = stage->GetPrimAtPath(SdfPath("/OpenWorld/LODSystem"));
    if (!lodSystemPrim) {
        return TfToken("medium");  // Default if not found
    }
    
    // Get system thresholds
    VtDictionary systemThresholds;
    UsdAttribute thresholdsAttr = lodSystemPrim.GetAttribute(
        TfToken("sparkle:lod:systemThresholds"));
    if (thresholdsAttr) {
        thresholdsAttr.Get(&systemThresholds);
    }
    
    // Look up thresholds for this system type
    VtDictionary typeThresholds;
    auto it = systemThresholds.find(systemType);
    if (it != systemThresholds.end()) {
        typeThresholds = VtDictionaryCast<VtDictionary>(it->second);
    }
    
    // Get region-specific multipliers
    float distanceMultiplier = 1.0f;
    UsdPrim regionPrim;
    for (UsdPrim p = entityPrim; p; p = p.GetParent()) {
        if (p.GetPath().GetNameToken() == TfToken("Regions")) {
            regionPrim = p;
            break;
        }
    }
    
    if (regionPrim) {
        UsdAttribute multAttr = regionPrim.GetAttribute(
            TfToken("sparkle:lod:viewDistanceMultiplier"));
        if (multAttr) {
            multAttr.Get(&distanceMultiplier);
        }
    }
    
    // Apply region multiplier to distance
    float adjustedDistance = distance / distanceMultiplier;
    
    // Entity-specific overrides
    float entityMultiplier = 1.0f;
    UsdAttribute entityMultAttr = entityPrim.GetAttribute(
        TfToken("sparkle:lod:distanceMultiplier"));
    if (entityMultAttr) {
        entityMultAttr.Get(&entityMultiplier);
    }
    adjustedDistance /= entityMultiplier;
    
    // Determine appropriate detail level
    if (adjustedDistance < VtDictionaryGet<float>(typeThresholds, "high", 100.0f)) {
        return TfToken("high");
    } else if (adjustedDistance < VtDictionaryGet<float>(typeThresholds, "medium", 500.0f)) {
        return TfToken("medium");
    } else if (adjustedDistance < VtDictionaryGet<float>(typeThresholds, "low", 1000.0f)) {
        return TfToken("low");
    } else {
        return TfToken("minimal");
    }
}
```

## Case Study: Castle with Dynamic LOD

To illustrate these concepts, let's examine a complete castle entity with comprehensive LOD management:

```usda
def "MountainCastle" (
    inherits = </SparkleGameEntity>
    apiSchemas = ["SparkleLODAPI"]
)
{
    # LOD distance configuration
    float3 sparkle:lod:distanceRanges = (200, 800, 2000)
    
    # Visual components with LOD
    def "Visual" {
        variantSet "visualLOD" = {
            "high" {
                over "Geometry" {
                    def "HighDetail" (
                        references = @castle/high_detail.usda@</Castle>
                    )
                    {
                        # 1 million polygons
                    }
                }
                
                over "Materials" {
                    def "HighQuality" (
                        references = @castle/high_quality_materials.usda@</Materials>
                    )
                    {
                        # PBR materials with high-res textures
                    }
                }
                
                over "Effects" {
                    def "FullEffects" {
                        # Flags, smoke, birds, etc.
                    }
                }
            }
            
            "medium" {
                over "Geometry" {
                    def "MediumDetail" (
                        references = @castle/medium_detail.usda@</Castle>
                    )
                    {
                        # 250,000 polygons
                    }
                }
                
                over "Materials" {
                    def "MediumQuality" (
                        references = @castle/medium_quality_materials.usda@</Materials>
                    )
                    {
                        # Simplified PBR materials
                    }
                }
                
                over "Effects" {
                    def "LimitedEffects" {
                        # Reduced effects
                    }
                }
            }
            
            "low" {
                over "Geometry" {
                    def "LowDetail" (
                        references = @castle/low_detail.usda@</Castle>
                    )
                    {
                        # 50,000 polygons
                    }
                }
                
                over "Materials" {
                    def "LowQuality" (
                        references = @castle/low_quality_materials.usda@</Materials>
                    )
                    {
                        # Basic materials, no PBR
                    }
                }
                
                over "Effects" {
                    def "MinimalEffects" {
                        # Minimal effects (just smoke)
                    }
                }
            }
            
            "minimal" {
                over "Geometry" {
                    def "MinimalDetail" (
                        references = @castle/minimal_detail.usda@</Castle>
                    )
                    {
                        # 5,000 polygons
                    }
                }
                
                over "Materials" {
                    def "MinimalQuality" (
                        references = @castle/minimal_quality_materials.usda@</Materials>
                    )
                    {
                        # Basic textures only
                    }
                }
                
                # No effects at minimal LOD
            }
        }
    }
    
    # Interior - loaded only when player approaches
    def "Interior" (
        payload = @castle/interior.usda@</Interior>
        active = false  # Initially inactive
    )
    {
        # Interior details
    }
    
    # AI systems
    def "Inhabitants" {
        variantSet "behaviorLOD" = {
            "full" {
                over "Guards" (
                    references = @castle/full_guard_system.usda@</Guards>
                )
                {
                    # Full guard AI with patrols, schedules, etc.
                }
                
                over "Civilians" (
                    references = @castle/full_civilian_system.usda@</Civilians>
                )
                {
                    # Full civilian AI with schedules, interactions, etc.
                }
            }
            
            "simplified" {
                over "Guards" (
                    references = @castle/simplified_guard_system.usda@</Guards>
                )
                {
                    # Simplified guard AI with basic patrols
                }
                
                over "Civilians" (
                    references = @castle/simplified_civilian_system.usda@</Civilians>
                )
                {
                    # Simplified civilian AI with basic activities
                }
            }
            
            "minimal" {
                over "Guards" (
                    references = @castle/minimal_guard_system.usda@</Guards>
                )
                {
                    # Stationary guards only
                }
                
                # No civilians at minimal LOD
            }
            
            "inactive" {
                # No active AI systems
            }
        }
    }
    
    # Gameplay systems
    def "GameSystems" {
        variantSet "functionLOD" = {
            "full" {
                over "QuestSystem" (
                    active = true
                )
                {
                    # Full quest system
                }
                                
                over "InteractionSystem" (
                    active = true
                )
                {
                    # Full interaction system
                    int sparkle:interaction:maxActiveInteractions = 10
                    float sparkle:interaction:responseTime = 0.1
                }
                
                over "EconomySystem" (
                    active = true
                )
                {
                    # Full economy simulation
                }
            }
            
            "limited" {
                over "QuestSystem" (
                    active = true
                )
                {
                    # Limited quest functionality
                }
                
                over "InteractionSystem" (
                    active = true
                )
                {
                    # Limited interaction system
                    int sparkle:interaction:maxActiveInteractions = 5
                    float sparkle:interaction:responseTime = 0.5
                }
                
                over "EconomySystem" (
                    active = false
                )
                {
                    # Economy system disabled
                }
            }
            
            "minimal" {
                over "QuestSystem" (
                    active = false
                )
                {
                    # Quest system disabled
                }
                
                over "InteractionSystem" (
                    active = true
                )
                {
                    # Minimal interaction system (just basic dialog)
                    int sparkle:interaction:maxActiveInteractions = 1
                    float sparkle:interaction:responseTime = 1.0
                }
                
                over "EconomySystem" (
                    active = false
                )
                {
                    # Economy system disabled
                }
            }
            
            "inactive" {
                # All systems inactive
                over "QuestSystem" (
                    active = false
                )
                {
                }
                
                over "InteractionSystem" (
                    active = false
                )
                {
                }
                
                over "EconomySystem" (
                    active = false
                )
                {
                }
            }
        }
    }
    
    # Physics systems
    def "Physics" {
        variantSet "physicsLOD" = {
            "full" {
                over "Colliders" (
                    references = @castle/detailed_physics.usda@</Physics>
                )
                {
                    # Detailed physics with complex colliders
                }
                
                over "Destructibles" (
                    active = true
                )
                {
                    # Fully destructible elements
                }
                
                over "ClothSim" (
                    active = true
                )
                {
                    # Banners, tapestries with cloth simulation
                }
            }
            
            "simplified" {
                over "Colliders" (
                    references = @castle/simplified_physics.usda@</Physics>
                )
                {
                    # Simplified physics with basic colliders
                }
                
                over "Destructibles" (
                    active = true
                )
                {
                    # Limited destructibles
                }
                
                over "ClothSim" (
                    active = false
                )
                {
                    # No cloth simulation
                }
            }
            
            "minimal" {
                over "Colliders" (
                    references = @castle/minimal_physics.usda@</Physics>
                )
                {
                    # Minimal blocking volumes only
                }
                
                over "Destructibles" (
                    active = false
                )
                {
                    # No destructibles
                }
                
                over "ClothSim" (
                    active = false
                )
                {
                    # No cloth simulation
                }
            }
            
            "none" {
                # No physics
            }
        }
    }
    
    # Audio systems
    def "Audio" {
        variantSet "audioLOD" = {
            "full" {
                over "AmbientSounds" (
                    active = true
                )
                {
                    # Full ambient audio system
                    int sparkle:audio:maxSources = 32
                    float sparkle:audio:dynamicRange = 60.0
                }
                
                over "CharacterVoice" (
                    active = true
                )
                {
                    # Full voice system
                }
                
                over "MusicSystem" (
                    active = true
                )
                {
                    # Full adaptive music system
                }
            }
            
            "medium" {
                over "AmbientSounds" (
                    active = true
                )
                {
                    # Reduced ambient system
                    int sparkle:audio:maxSources = 16
                    float sparkle:audio:dynamicRange = 40.0
                }
                
                over "CharacterVoice" (
                    active = true
                )
                {
                    # Limited voice system
                }
                
                over "MusicSystem" (
                    active = true
                )
                {
                    # Basic music system
                }
            }
            
            "minimal" {
                over "AmbientSounds" (
                    active = true
                )
                {
                    # Minimal ambient system
                    int sparkle:audio:maxSources = 4
                    float sparkle:audio:dynamicRange = 20.0
                }
                
                over "CharacterVoice" (
                    active = false
                )
                {
                    # No voice system
                }
                
                over "MusicSystem" (
                    active = true
                )
                {
                    # Minimal music system
                }
            }
            
            "distant" {
                # Just distant audio cues
                over "AmbientSounds" (
                    active = true
                )
                {
                    # Extremely limited ambient
                    int sparkle:audio:maxSources = 1
                    float sparkle:audio:dynamicRange = 10.0
                }
                
                over "CharacterVoice" (
                    active = false
                )
                {
                    # No voice
                }
                
                over "MusicSystem" (
                    active = false
                )
                {
                    # No music
                }
            }
        }
    }
}
```

To control this complex LOD system, game code would manage both the variant selections and payload loading based on player distance and system resources:

```cpp
// Update castle LOD based on player distance
void UpdateCastleLOD(UsdStage* stage, const SdfPath& castlePath, float playerDistance) {
    // Get the castle prim
    UsdPrim castlePrim = stage->GetPrimAtPath(castlePath);
    if (!castlePrim) {
        return;
    }
    
    // Get variant sets
    UsdVariantSet visualLOD = castlePrim.GetVariantSet("Visual.visualLOD");
    UsdVariantSet behaviorLOD = castlePrim.GetVariantSet("Inhabitants.behaviorLOD");
    UsdVariantSet functionLOD = castlePrim.GetVariantSet("GameSystems.functionLOD");
    UsdVariantSet physicsLOD = castlePrim.GetVariantSet("Physics.physicsLOD");
    UsdVariantSet audioLOD = castlePrim.GetVariantSet("Audio.audioLOD");
    
    // Get interior prim for payload management
    UsdPrim interiorPrim = stage->GetPrimAtPath(castlePath.AppendChild(TfToken("Interior")));
    
    // Update variants based on distance
    if (playerDistance < 50.0f) {
        // Player is inside the castle
        visualLOD.SetVariantSelection("high");
        behaviorLOD.SetVariantSelection("full");
        functionLOD.SetVariantSelection("full");
        physicsLOD.SetVariantSelection("full");
        audioLOD.SetVariantSelection("full");
        
        // Load interior payload and activate
        if (interiorPrim) {
            stage->LoadPayload(interiorPrim.GetPath());
            interiorPrim.SetActive(true);
        }
    }
    else if (playerDistance < 200.0f) {
        // Player is very close to the castle
        visualLOD.SetVariantSelection("high");
        behaviorLOD.SetVariantSelection("simplified");
        functionLOD.SetVariantSelection("limited");
        physicsLOD.SetVariantSelection("simplified");
        audioLOD.SetVariantSelection("medium");
        
        // Load interior payload but keep inactive
        if (interiorPrim) {
            stage->LoadPayload(interiorPrim.GetPath());
            interiorPrim.SetActive(false);
        }
    }
    else if (playerDistance < 800.0f) {
        // Player is at medium distance
        visualLOD.SetVariantSelection("medium");
        behaviorLOD.SetVariantSelection("minimal");
        functionLOD.SetVariantSelection("minimal");
        physicsLOD.SetVariantSelection("minimal");
        audioLOD.SetVariantSelection("minimal");
        
        // Unload interior
        if (interiorPrim) {
            stage->UnloadPayload(interiorPrim.GetPath());
            interiorPrim.SetActive(false);
        }
    }
    else if (playerDistance < 2000.0f) {
        // Player is far from the castle
        visualLOD.SetVariantSelection("low");
        behaviorLOD.SetVariantSelection("inactive");
        functionLOD.SetVariantSelection("inactive");
        physicsLOD.SetVariantSelection("none");
        audioLOD.SetVariantSelection("distant");
        
        // Unload interior
        if (interiorPrim) {
            stage->UnloadPayload(interiorPrim.GetPath());
            interiorPrim.SetActive(false);
        }
    }
    else {
        // Player is very far from the castle
        visualLOD.SetVariantSelection("minimal");
        behaviorLOD.SetVariantSelection("inactive");
        functionLOD.SetVariantSelection("inactive");
        physicsLOD.SetVariantSelection("none");
        audioLOD.SetVariantSelection("none");
        
        // Unload interior
        if (interiorPrim) {
            stage->UnloadPayload(interiorPrim.GetPath());
            interiorPrim.SetActive(false);
        }
    }
}
```

## LOD Manager System

For a production game, you would likely implement a more sophisticated LOD management system that considers multiple factors:

1. **Distance from player**: The primary factor for most LOD decisions
2. **Importance to gameplay**: Critical gameplay elements maintain higher detail
3. **Visual prominence**: Visually important elements maintain higher detail
4. **Performance budget**: Dynamically adjust LOD based on frame rate
5. **Player focus**: Maintain higher detail in player's field of view
6. **Memory constraints**: Consider available memory when loading high-detail assets

A comprehensive LOD manager might look like:

```cpp
class LODManager {
public:
    // Initialize the LOD manager
    void Initialize(UsdStage* stage) {
        m_stage = stage;
        
        // Find LOD system configuration
        UsdPrim lodSystemPrim = stage->GetPrimAtPath(SdfPath("/OpenWorld/LODSystem"));
        if (lodSystemPrim) {
            // Read global configuration
            lodSystemPrim.GetAttribute(TfToken("sparkle:lod:playerViewDistance"))
                .Get(&m_playerViewDistance);
            lodSystemPrim.GetAttribute(TfToken("sparkle:lod:maxActiveSystems"))
                .Get(&m_maxActiveSystems);
            lodSystemPrim.GetAttribute(TfToken("sparkle:lod:systemPriorityRadius"))
                .Get(&m_systemPriorityRadius);
            lodSystemPrim.GetAttribute(TfToken("sparkle:lod:dynamicLODEnabled"))
                .Get(&m_dynamicLODEnabled);
                
            // Read system thresholds
            UsdAttribute thresholdsAttr = lodSystemPrim.GetAttribute(
                TfToken("sparkle:lod:systemThresholds"));
            if (thresholdsAttr) {
                thresholdsAttr.Get(&m_systemThresholds);
            }
        }
        
        // Build initial entity list
        RefreshEntityList();
    }
    
    // Refresh the entity list
    void RefreshEntityList() {
        m_lodEntities.clear();
        
        // Find all entities with LOD API
        for (const UsdPrim& prim : m_stage->TraverseAll()) {
            if (prim.HasAPI<TfType::Find<class SparkleLODAPI>>()) {
                m_lodEntities.push_back(prim.GetPath());
            }
        }
    }
    
    // Update LOD for all entities based on player position
    void Update(const GfVec3f& playerPosition, float deltaTime) {
        // Track active system count
        int activeSystemCount = 0;
        
        // Sort entities by priority (distance and importance)
        std::vector<std::pair<SdfPath, float>> prioritizedEntities;
        for (const SdfPath& entityPath : m_lodEntities) {
            UsdPrim prim = m_stage->GetPrimAtPath(entityPath);
            if (!prim) continue;
            
            // Get entity position
            GfVec3f entityPosition;
            UsdGeomXformable xformable(prim);
            if (xformable) {
                GfMatrix4d localToWorld;
                bool resetsXformStack;
                xformable.GetLocalToWorldTransform(UsdTimeCode::Default(), 
                                                &localToWorld, 
                                                &resetsXformStack);
                entityPosition = GfVec3f(localToWorld.ExtractTranslation());
            }
            
            // Calculate distance
            float distance = (entityPosition - playerPosition).GetLength();
            
            // Get importance factor (default to 1.0)
            float importance = 1.0f;
            UsdAttribute importanceAttr = prim.GetAttribute(TfToken("sparkle:lod:importance"));
            if (importanceAttr) {
                importanceAttr.Get(&importance);
            }
            
            // Calculate priority (lower is higher priority)
            float priority = distance / importance;
            
            // Add to prioritized list
            prioritizedEntities.push_back(std::make_pair(entityPath, priority));
        }
        
        // Sort by priority
        std::sort(prioritizedEntities.begin(), prioritizedEntities.end(),
                 [](const auto& a, const auto& b) {
                     return a.second < b.second;
                 });
        
        // Process entities in priority order
        for (const auto& [entityPath, priority] : prioritizedEntities) {
            UsdPrim prim = m_stage->GetPrimAtPath(entityPath);
            if (!prim) continue;
            
            // Get distance
            GfVec3f entityPosition;
            UsdGeomXformable xformable(prim);
            if (xformable) {
                GfMatrix4d localToWorld;
                bool resetsXformStack;
                xformable.GetLocalToWorldTransform(UsdTimeCode::Default(), 
                                                &localToWorld, 
                                                &resetsXformStack);
                entityPosition = GfVec3f(localToWorld.ExtractTranslation());
            }
            float distance = (entityPosition - playerPosition).GetLength();
            
            // Determine if this is a critical system
            bool isCritical = false;
            UsdAttribute criticalAttr = prim.GetAttribute(TfToken("sparkle:lod:isCritical"));
            if (criticalAttr) {
                criticalAttr.Get(&isCritical);
            }
            
            // Check if we've reached maximum active systems
            bool forceMinimal = false;
            if (!isCritical && activeSystemCount >= m_maxActiveSystems) {
                forceMinimal = true;
            }
            
            // Update entity LOD
            UpdateEntityLOD(prim, distance, forceMinimal);
            
            // Increment active count if within active range
            if (distance < m_systemPriorityRadius) {
                activeSystemCount++;
            }
        }
        
        // Dynamic performance-based LOD adjustment
        if (m_dynamicLODEnabled) {
            UpdatePerformanceLOD(deltaTime);
        }
    }
    
private:
    // Update LOD for a specific entity
    void UpdateEntityLOD(const UsdPrim& prim, float distance, bool forceMinimal) {
        // Get LOD configuration
        GfVec3f distanceRanges(200.0f, 800.0f, 2000.0f);
        UsdAttribute rangesAttr = prim.GetAttribute(TfToken("sparkle:lod:distanceRanges"));
        if (rangesAttr) {
            rangesAttr.Get(&distanceRanges);
        }
        
        // Determine LOD level
        int lodLevel;
        if (forceMinimal) {
            lodLevel = 3;  // Minimal
        } else if (distance < distanceRanges[0]) {
            lodLevel = 0;  // High
        } else if (distance < distanceRanges[1]) {
            lodLevel = 1;  // Medium
        } else if (distance < distanceRanges[2]) {
            lodLevel = 2;  // Low
        } else {
            lodLevel = 3;  // Minimal
        }
        
        // Update current LOD level
        UsdAttribute currentLODAttr = prim.GetAttribute(TfToken("sparkle:lod:currentLevel"));
        if (currentLODAttr) {
            int currentLevel;
            currentLODAttr.Get(&currentLevel);
            
            // Only update if necessary
            if (currentLevel != lodLevel) {
                currentLODAttr.Set(lodLevel);
                
                // Update variant selections
                UpdateVariants(prim, lodLevel);
                
                // Update payload loading
                UpdatePayloads(prim, lodLevel);
            }
        }
    }
    
    // Update variants based on LOD level
    void UpdateVariants(const UsdPrim& prim, int lodLevel) {
        const char* lodNames[] = {"high", "medium", "low", "minimal"};
        const char* level = (lodLevel < 4) ? lodNames[lodLevel] : lodNames[3];
        
        // Update all LOD variant sets
        for (const UsdVariantSet& variantSet : prim.GetVariantSets().GetVariantSets()) {
            std::string name = variantSet.GetName();
            
            // Check if this is an LOD variant set
            if (name.find("LOD") != std::string::npos) {
                // Try the direct LOD level name
                if (variantSet.HasVariant(level)) {
                    variantSet.SetVariantSelection(level);
                }
                // Fall back to closest available variant
                else if (lodLevel >= 3 && variantSet.HasVariant("minimal")) {
                    variantSet.SetVariantSelection("minimal");
                }
                else if (lodLevel >= 2 && variantSet.HasVariant("low")) {
                    variantSet.SetVariantSelection("low");
                }
                else if (lodLevel >= 1 && variantSet.HasVariant("medium")) {
                    variantSet.SetVariantSelection("medium");
                }
                else if (variantSet.HasVariant("high")) {
                    variantSet.SetVariantSelection("high");
                }
            }
        }
    }
    
    // Update payload loading based on LOD level
    void UpdatePayloads(const UsdPrim& prim, int lodLevel) {
        // Process payload attributes
        for (const UsdAttribute& attr : prim.GetAttributes()) {
            std::string name = attr.GetName();
            
            // Check for payload control attributes
            if (name.find("sparkle:lod:payload:") == 0) {
                // Extract payload path and threshold
                std::string payloadPath = name.substr(std::string("sparkle:lod:payload:").length());
                int threshold;
                attr.Get(&threshold);
                
                // Get actual payload prim
                SdfPath path = prim.GetPath().AppendPath(SdfPath(payloadPath));
                UsdPrim payloadPrim = m_stage->GetPrimAtPath(path);
                if (payloadPrim) {
                    // Load or unload based on threshold
                    if (lodLevel <= threshold) {
                        m_stage->LoadPayload(payloadPrim.GetPath());
                    } else {
                        m_stage->UnloadPayload(payloadPrim.GetPath());
                    }
                }
            }
        }
    }
    
    // Dynamic LOD adjustment based on performance
    void UpdatePerformanceLOD(float deltaTime) {
        // Simple example: adjust global LOD based on frame time
        static float accumulatedTime = 0.0f;
        static float averageFrameTime = 16.0f;  // Target ~60 FPS
        
        // Update running average
        accumulatedTime = accumulatedTime * 0.95f + deltaTime * 0.05f;
        
        // Check every second
        static float checkTimer = 0.0f;
        checkTimer += deltaTime;
        if (checkTimer > 1.0f) {
            checkTimer = 0.0f;
            
            // Calculate average frame time
            averageFrameTime = accumulatedTime;
            
            // Adjust LOD thresholds based on performance
            float performanceFactor = 16.0f / averageFrameTime;  // 16ms = 60 FPS target
            
            // Clamp to reasonable range
            performanceFactor = std::max(0.25f, std::min(performanceFactor, 2.0f));
            
            // Apply to view distance
            m_playerViewDistance = m_basePlayerViewDistance * performanceFactor;
        }
    }
    
    // Member variables
    UsdStage* m_stage = nullptr;
    std::vector<SdfPath> m_lodEntities;
    
    // LOD system configuration
    float m_playerViewDistance = 2000.0f;
    float m_basePlayerViewDistance = 2000.0f;
    int m_maxActiveSystems = 20;
    float m_systemPriorityRadius = 500.0f;
    bool m_dynamicLODEnabled = true;
    VtDictionary m_systemThresholds;
};
```

## Key Takeaways

- USD's composition system enables a comprehensive approach to LOD beyond simple geometry swapping
- Variants provide a clean way to swap entire subsystems based on distance or other criteria
- Payloads enable on-demand loading of detailed content when needed
- Active metadata allows for efficient toggling of systems without unloading
- Schema properties provide a structured way to configure LOD behavior
- Behavioral LOD through composition allows complex entities to gracefully degrade with distance
- Simulation and physics LOD can significantly improve performance in complex scenes
- A coordinated LOD strategy across all game systems maximizes performance while maintaining quality
- Composition-based LOD integrates naturally with codeless schemas for easy distribution
- Dynamic LOD adjustment based on performance ensures consistent frame rates

By leveraging USD's composition system for advanced LOD, games can achieve smooth performance scaling across a wide range of hardware while maintaining rich, detailed worlds where it matters most.

In the next section, we'll explore entity templates and instance variation using composition, expanding on these concepts for efficient game entity creation.