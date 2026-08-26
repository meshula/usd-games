# Chapter 3.5: Level Design Through Composition

## Building Worlds with USD Composition

Level design is one of the most composition-intensive aspects of game development. Whether creating linear levels, open worlds, or procedural environments, effective level design requires systems for reusing content, managing large scenes, and creating interactive environments. USD's composition capabilities provide powerful tools for addressing these challenges.

In this chapter, we'll explore how USD composition can transform level design workflows, from modular environment creation to dynamic world states and efficient streaming systems.

## Modular Level Design with References

Modular level design is a cornerstone of efficient game production, allowing artists to create large, varied environments from a smaller set of reusable pieces. USD's reference composition arc is perfectly suited for this approach.

### Building Block Organization

Let's start with a library of modular building blocks:

```usda
#usda 1.0
(
    defaultPrim = "ModularPieces"
)

def "ModularPieces" (
    kind = "group"
)
{
    def "Dungeon" (
        kind = "group"
    )
    {
        def "Floor" (
            kind = "group"
        )
        {
            def "Floor_2x2" (
                kind = "component"
                inherits = </SparkleGameEntity>
            )
            {
                def Mesh "Geometry" (
                    references = @meshes/dungeon/floor_2x2.usda@</Mesh>
                )
                {
                }
                
                def "Collider" {
                    token sparkle:physics:type = "mesh"
                    rel sparkle:physics:mesh = </ModularPieces/Dungeon/Floor/Floor_2x2/Geometry>
                }
            }
            
            def "Floor_4x4" (
                kind = "component"
                inherits = </SparkleGameEntity>
            )
            {
                def Mesh "Geometry" (
                    references = @meshes/dungeon/floor_4x4.usda@</Mesh>
                )
                {
                }
                
                def "Collider" {
                    token sparkle:physics:type = "mesh"
                    rel sparkle:physics:mesh = </ModularPieces/Dungeon/Floor/Floor_4x4/Geometry>
                }
            }
        }
        
        def "Walls" (
            kind = "group"
        )
        {
            def "Wall_Straight" (
                kind = "component"
                inherits = </SparkleGameEntity>
            )
            {
                def Mesh "Geometry" (
                    references = @meshes/dungeon/wall_straight.usda@</Mesh>
                )
                {
                }
                
                def "Collider" {
                    token sparkle:physics:type = "box"
                    float3 sparkle:physics:boxSize = (2, 3, 0.3)
                }
            }
            
            def "Wall_Corner" (
                kind = "component"
                inherits = </SparkleGameEntity>
            )
            {
                def Mesh "Geometry" (
                    references = @meshes/dungeon/wall_corner.usda@</Mesh>
                )
                {
                }
                
                def "Collider" {
                    token sparkle:physics:type = "mesh"
                    rel sparkle:physics:mesh = </ModularPieces/Dungeon/Walls/Wall_Corner/Geometry>
                }
            }
            
            def "Wall_Doorway" (
                kind = "component"
                inherits = </SparkleGameEntity>
            )
            {
                def Mesh "Geometry" (
                    references = @meshes/dungeon/wall_doorway.usda@</Mesh>
                )
                {
                }
                
                def "Collider" {
                    token sparkle:physics:type = "mesh"
                    rel sparkle:physics:mesh = </ModularPieces/Dungeon/Walls/Wall_Doorway/Geometry>
                }
                
                # Interaction point for the doorway
                def "DoorwayTrigger" {
                    token sparkle:interaction:type = "doorway"
                    float3 sparkle:interaction:triggerSize = (1.5, 2, 0.5)
                    float3 sparkle:interaction:triggerOffset = (0, 1, 0)
                }
            }
        }
        
        def "Props" (
            kind = "group"
        )
        {
            def "Torch" (
                kind = "component"
                inherits = </SparkleGameEntity>
            )
            {
                def Mesh "Geometry" (
                    references = @meshes/dungeon/torch.usda@</Mesh>
                )
                {
                }
                
                def "Collider" {
                    token sparkle:physics:type = "capsule"
                    float sparkle:physics:radius = 0.1
                    float sparkle:physics:height = 0.5
                }
                
                def "Light" {
                    token sparkle:light:type = "point"
                    float sparkle:light:intensity = 800
                    float3 sparkle:light:color = (1, 0.7, 0.2)
                    float sparkle:light:radius = 10
                }
                
                def "Particles" {
                    token sparkle:particles:type = "fire"
                    float3 sparkle:particles:position = (0, 0.5, 0)
                    float sparkle:particles:scale = 0.2
                }
            }
            
            def "Chest" (
                kind = "component"
                inherits = </SparkleGameEntity>
            )
            {
                def Mesh "Geometry" (
                    references = @meshes/dungeon/chest.usda@</Mesh>
                )
                {
                }
                
                def "Collider" {
                    token sparkle:physics:type = "box"
                    float3 sparkle:physics:boxSize = (1, 0.6, 0.6)
                }
                
                def "Interaction" {
                    token sparkle:interaction:type = "container"
                    rel sparkle:interaction:lootTable = </LootTables/DungeonChest>
                }
                
                # Variant for open/closed states
                variantSet "state" = {
                    "closed" {
                        over "Geometry" {
                            uniform token[] xformOpOrder = ["xformOp:transform"]
                        }
                        
                        over "Interaction" {
                            bool sparkle:interaction:isActive = true
                        }
                    }
                    
                    "open" {
                        over "Geometry" {
                            float3 xformOp:rotateXYZ = (0, 0, -110)
                            uniform token[] xformOpOrder = ["xformOp:rotateXYZ"]
                        }
                        
                        over "Interaction" {
                            bool sparkle:interaction:isActive = false
                        }
                    }
                }
            }
        }
        
        def "Rooms" (
            kind = "group"
        )
        {
            def "StartRoom" (
                kind = "assembly"
            )
            {
                # A pre-assembled room using the modular pieces
                def "Floor" (
                    references = </ModularPieces/Dungeon/Floor/Floor_4x4>
                )
                {
                }
                
                def "WallNorth" (
                    references = </ModularPieces/Dungeon/Walls/Wall_Straight>
                )
                {
                    float3 xformOp:translate = (0, 2, -2)
                    uniform token[] xformOpOrder = ["xformOp:translate"]
                }
                
                def "WallEast" (
                    references = </ModularPieces/Dungeon/Walls/Wall_Straight>
                )
                {
                    float3 xformOp:translate = (2, 2, 0)
                    float3 xformOp:rotateXYZ = (0, 90, 0)
                    uniform token[] xformOpOrder = ["xformOp:translate", "xformOp:rotateXYZ"]
                }
                
                def "WallSouth" (
                    references = </ModularPieces/Dungeon/Walls/Wall_Doorway>
                )
                {
                    float3 xformOp:translate = (0, 2, 2)
                    float3 xformOp:rotateXYZ = (0, 180, 0)
                    uniform token[] xformOpOrder = ["xformOp:translate", "xformOp:rotateXYZ"]
                }
                
                def "WallWest" (
                    references = </ModularPieces/Dungeon/Walls/Wall_Straight>
                )
                {
                    float3 xformOp:translate = (-2, 2, 0)
                    float3 xformOp:rotateXYZ = (0, 270, 0)
                    uniform token[] xformOpOrder = ["xformOp:translate", "xformOp:rotateXYZ"]
                }
                
                def "NE_Corner" (
                    references = </ModularPieces/Dungeon/Walls/Wall_Corner>
                )
                {
                    float3 xformOp:translate = (2, 2, -2)
                    uniform token[] xformOpOrder = ["xformOp:translate"]
                }
                
                def "NW_Corner" (
                    references = </ModularPieces/Dungeon/Walls/Wall_Corner>
                )
                {
                    float3 xformOp:translate = (-2, 2, -2)
                    float3 xformOp:rotateXYZ = (0, 270, 0)
                    uniform token[] xformOpOrder = ["xformOp:translate", "xformOp:rotateXYZ"]
                }
                
                def "SE_Corner" (
                    references = </ModularPieces/Dungeon/Walls/Wall_Corner>
                )
                {
                    float3 xformOp:translate = (2, 2, 2)
                    float3 xformOp:rotateXYZ = (0, 90, 0)
                    uniform token[] xformOpOrder = ["xformOp:translate", "xformOp:rotateXYZ"]
                }
                
                def "SW_Corner" (
                    references = </ModularPieces/Dungeon/Walls/Wall_Corner>
                )
                {
                    float3 xformOp:translate = (-2, 2, 2)
                    float3 xformOp:rotateXYZ = (0, 180, 0)
                    uniform token[] xformOpOrder = ["xformOp:translate", "xformOp:rotateXYZ"]
                }
                
                def "Torch1" (
                    references = </ModularPieces/Dungeon/Props/Torch>
                )
                {
                    float3 xformOp:translate = (1.8, 1.5, -1.8)
                    float3 xformOp:rotateXYZ = (0, 225, 0)
                    uniform token[] xformOpOrder = ["xformOp:translate", "xformOp:rotateXYZ"]
                }
                
                def "Torch2" (
                    references = </ModularPieces/Dungeon/Props/Torch>
                )
                {
                    float3 xformOp:translate = (-1.8, 1.5, -1.8)
                    float3 xformOp:rotateXYZ = (0, 135, 0)
                    uniform token[] xformOpOrder = ["xformOp:translate", "xformOp:rotateXYZ"]
                }
                
                def "Chest1" (
                    references = </ModularPieces/Dungeon/Props/Chest>
                )
                {
                    float3 xformOp:translate = (-1, 0, -1)
                    float3 xformOp:rotateXYZ = (0, 45, 0)
                    uniform token[] xformOpOrder = ["xformOp:translate", "xformOp:rotateXYZ"]
                }
            }
            
            def "Corridor" (
                kind = "assembly"
            )
            {
                # A pre-assembled corridor
                def "Floor1" (
                    references = </ModularPieces/Dungeon/Floor/Floor_2x2>
                )
                {
                }
                
                def "Floor2" (
                    references = </ModularPieces/Dungeon/Floor/Floor_2x2>
                )
                {
                    float3 xformOp:translate = (0, 0, 2)
                    uniform token[] xformOpOrder = ["xformOp:translate"]
                }
                
                def "WallEast1" (
                    references = </ModularPieces/Dungeon/Walls/Wall_Straight>
                )
                {
                    float3 xformOp:translate = (1, 2, 0)
                    float3 xformOp:rotateXYZ = (0, 90, 0)
                    uniform token[] xformOpOrder = ["xformOp:translate", "xformOp:rotateXYZ"]
                }
                
                def "WallEast2" (
                    references = </ModularPieces/Dungeon/Walls/Wall_Straight>
                )
                {
                    float3 xformOp:translate = (1, 2, 2)
                    float3 xformOp:rotateXYZ = (0, 90, 0)
                    uniform token[] xformOpOrder = ["xformOp:translate", "xformOp:rotateXYZ"]
                }
                
                def "WallWest1" (
                    references = </ModularPieces/Dungeon/Walls/Wall_Straight>
                )
                {
                    float3 xformOp:translate = (-1, 2, 0)
                    float3 xformOp:rotateXYZ = (0, 270, 0)
                    uniform token[] xformOpOrder = ["xformOp:translate", "xformOp:rotateXYZ"]
                }
                
                def "WallWest2" (
                    references = </ModularPieces/Dungeon/Walls/Wall_Straight>
                )
                {
                    float3 xformOp:translate = (-1, 2, 2)
                    float3 xformOp:rotateXYZ = (0, 270, 0)
                    uniform token[] xformOpOrder = ["xformOp:translate", "xformOp:rotateXYZ"]
                }
                
                def "Torch1" (
                    references = </ModularPieces/Dungeon/Props/Torch>
                )
                {
                    float3 xformOp:translate = (-0.8, 1.5, 2)
                    float3 xformOp:rotateXYZ = (0, 180, 0)
                    uniform token[] xformOpOrder = ["xformOp:translate", "xformOp:rotateXYZ"]
                }
            }
        }
    }
    
    # More environment sets...
}
```

This modular library defines individual pieces (floors, walls, props) as well as pre-assembled room templates, all using the inherits composition arc to ensure they have game entity properties and references to include the actual geometry.

### Level Assembly with References

With our modular pieces defined, we can assemble complete levels:

```usda
#usda 1.0
(
    defaultPrim = "DungeonLevel"
)

def "DungeonLevel" (
    kind = "assembly"
)
{
    # Start room
    def "Room1" (
        references = </ModularPieces/Dungeon/Rooms/StartRoom>
    )
    {
        float3 xformOp:translate = (0, 0, 0)
        uniform token[] xformOpOrder = ["xformOp:translate"]
    }
    
    # Corridor connecting to next room
    def "Corridor1" (
        references = </ModularPieces/Dungeon/Rooms/Corridor>
    )
    {
        float3 xformOp:translate = (0, 0, 4)
        uniform token[] xformOpOrder = ["xformOp:translate"]
    }
    
    # Second room, custom-assembled from pieces
    def "Room2" {
        def "Floor" (
            references = </ModularPieces/Dungeon/Floor/Floor_4x4>
        )
        {
            float3 xformOp:translate = (0, 0, 8)
            uniform token[] xformOpOrder = ["xformOp:translate"]
        }
        
        def "WallNorth" (
            references = </ModularPieces/Dungeon/Walls/Wall_Doorway>
        )
        {
            float3 xformOp:translate = (0, 2, 6)
            uniform token[] xformOpOrder = ["xformOp:translate"]
        }
        
        # More walls and props...
        
        # Enemy in this room
        def "Enemy1" (
            references = @entities/enemies/skeleton.usda@</Enemy>
        )
        {
            float3 xformOp:translate = (1, 0, 8)
            uniform token[] xformOpOrder = ["xformOp:translate"]
        }
    }
    
    # Level-specific lighting
    def "GlobalLighting" {
        def "AmbientLight" {
            token sparkle:light:type = "ambient"
            float sparkle:light:intensity = 0.2
            float3 sparkle:light:color = (0.1, 0.1, 0.3)
        }
    }
    
    # Level-specific gameplay settings
    def "GameSettings" {
        token sparkle:level:difficulty = "normal"
        token sparkle:level:theme = "dungeon"
        string sparkle:level:name = "Forgotten Crypts"
    }
}
```

This approach allows level designers to quickly assemble levels using both pre-built room templates and individual modular pieces, all while maintaining the connection to the original assets through references.

## Environmental Variation Through Variants

While reference-based composition enables modular assembly, variant-based composition allows for environmental variation. This is particularly useful for:

1. Day/night cycles
2. Weather changes
3. Seasonal variations
4. Damage states
5. Story progression changes

Let's see how to implement this for an outdoor environment:

```usda
#usda 1.0
(
    defaultPrim = "ForestClearing"
)

def "ForestClearing" (
    kind = "assembly"
)
{
    # Base terrain and static elements
    def "Terrain" (
        references = @environments/terrain/forest_clearing.usda@</Terrain>
    )
    {
    }
    
    def "StaticVegetation" {
        def "LargeTree1" (
            references = @environments/vegetation/large_oak.usda@</Tree>
        )
        {
            float3 xformOp:translate = (5, 0, 8)
            uniform token[] xformOpOrder = ["xformOp:translate"]
        }
        
        # More static vegetation...
    }
    
    # Time of day variant set
    variantSet "timeOfDay" = {
        "morning" {
            over "Lighting" {
                over "DirectionalLight" {
                    float3 xformOp:rotateXYZ = (45, 30, 0)
                    uniform token[] xformOpOrder = ["xformOp:rotateXYZ"]
                    
                    float sparkle:light:intensity = 40000
                    float3 sparkle:light:color = (1.0, 0.9, 0.8)
                }
                
                over "AmbientLight" {
                    float sparkle:light:intensity = 5000
                    float3 sparkle:light:color = (0.8, 0.9, 1.0)
                }
                
                over "SkyDome" {
                    token sparkle:sky:type = "morning"
                }
            }
            
            over "EnvironmentEffects" {
                over "MorningFog" {
                    bool sparkle:effect:active = true
                }
                
                over "MorningBirds" {
                    bool sparkle:effect:active = true
                }
            }
        }
        
        "midday" {
            over "Lighting" {
                over "DirectionalLight" {
                    float3 xformOp:rotateXYZ = (80, 0, 0)
                    uniform token[] xformOpOrder = ["xformOp:rotateXYZ"]
                    
                    float sparkle:light:intensity = 100000
                    float3 sparkle:light:color = (1.0, 1.0, 0.9)
                }
                
                over "AmbientLight" {
                    float sparkle:light:intensity = 10000
                    float3 sparkle:light:color = (1.0, 1.0, 1.0)
                }
                
                over "SkyDome" {
                    token sparkle:sky:type = "midday"
                }
            }
        }
        
        "sunset" {
            over "Lighting" {
                over "DirectionalLight" {
                    float3 xformOp:rotateXYZ = (10, -30, 0)
                    uniform token[] xformOpOrder = ["xformOp:rotateXYZ"]
                    
                    float sparkle:light:intensity = 30000
                    float3 sparkle:light:color = (1.0, 0.6, 0.3)
                }
                
                over "AmbientLight" {
                    float sparkle:light:intensity = 3000
                    float3 sparkle:light:color = (0.9, 0.6, 0.4)
                }
                
                over "SkyDome" {
                    token sparkle:sky:type = "sunset"
                }
            }
        }
        
        "night" {
            over "Lighting" {
                over "DirectionalLight" {
                    float3 xformOp:rotateXYZ = (-10, 0, 0)
                    uniform token[] xformOpOrder = ["xformOp:rotateXYZ"]
                    
                    float sparkle:light:intensity = 1000
                    float3 sparkle:light:color = (0.2, 0.2, 0.8)
                }
                
                over "AmbientLight" {
                    float sparkle:light:intensity = 500
                    float3 sparkle:light:color = (0.1, 0.1, 0.3)
                }
                
                over "MoonLight" {
                    bool sparkle:light:enabled = true
                    float sparkle:light:intensity = 5000
                    float3 sparkle:light:color = (0.8, 0.8, 1.0)
                }
                
                over "SkyDome" {
                    token sparkle:sky:type = "night"
                }
            }
            
            over "EnvironmentEffects" {
                over "Fireflies" {
                    bool sparkle:effect:active = true
                }
                
                over "CicadaSounds" {
                    bool sparkle:effect:active = true
                }
            }
        }
    }
    
    # Weather variant set
    variantSet "weather" = {
        "clear" {
            # Default state, no overrides needed
        }
        
        "cloudy" {
            over "Lighting" {
                over "DirectionalLight" {
                    float sparkle:light:intensity = {
                        # Time-sampled animation for light flickering
                        0: 60000,
                        10: 40000,
                        20: 55000,
                        30: 45000
                    }
                }
                
                over "SkyDome" {
                    token sparkle:sky:cloudCover = "heavy"
                }
            }
            
            over "EnvironmentEffects" {
                over "WindSystem" {
                    float sparkle:wind:strength = 2.0
                }
                
                over "CloudShadows" {
                    bool sparkle:effect:active = true
                }
            }
        }
        
        "rainy" {
            over "Lighting" {
                over "DirectionalLight" {
                    float sparkle:light:intensity = 20000
                }
                
                over "SkyDome" {
                    token sparkle:sky:cloudCover = "overcast"
                }
            }
            
            over "EnvironmentEffects" {
                over "RainParticles" {
                    bool sparkle:effect:active = true
                    float sparkle:rain:intensity = 0.5
                }
                
                over "WetSurfaces" {
                    bool sparkle:effect:active = true
                }
                
                over "WindSystem" {
                    float sparkle:wind:strength = 3.0
                    float sparkle:wind:gustiness = 0.6
                }
                
                over "RainSounds" {
                    bool sparkle:effect:active = true
                }
                
                over "PuddleSystem" {
                    bool sparkle:effect:active = true
                }
            }
        }
        
        "stormy" {
            over "Lighting" {
                over "DirectionalLight" {
                    float sparkle:light:intensity = 10000
                }
                
                over "SkyDome" {
                    token sparkle:sky:cloudCover = "storm"
                }
                
                def "LightningSystem" {
                    bool sparkle:effect:active = true
                    float sparkle:lightning:frequency = 0.2
                    float sparkle:lightning:intensity = 100000
                }
            }
            
            over "EnvironmentEffects" {
                over "RainParticles" {
                    bool sparkle:effect:active = true
                    float sparkle:rain:intensity = 1.0
                }
                
                over "WetSurfaces" {
                    bool sparkle:effect:active = true
                }
                
                over "WindSystem" {
                    float sparkle:wind:strength = 8.0
                    float sparkle:wind:gustiness = 0.8
                }
                
                over "ThunderSounds" {
                    bool sparkle:effect:active = true
                }
                
                over "PuddleSystem" {
                    bool sparkle:effect:active = true
                    float sparkle:puddle:size = 2.0
                }
                
                over "TreeSwaySystem" {
                    float sparkle:treeSway:intensity = 3.0
                }
            }
        }
    }
    
    # Season variant set
    variantSet "season" = {
        "summer" {
            # Default state, no overrides needed
        }
        
        "autumn" {
            over "StaticVegetation" {
                over "LargeTree1" {
                    over "Materials" {
                        over "LeafMaterial" {
                            string sparkle:material:variation = "autumn"
                            float3 sparkle:material:baseColor = (0.8, 0.4, 0.1)
                        }
                    }
                }
                
                # Similar overrides for other trees...
            }
            
            over "GroundCover" {
                string sparkle:material:variation = "autumn"
                def PointInstancer "FallenLeaves" {
                    bool sparkle:effect:active = true
                }
            }
            
            over "EnvironmentEffects" {
                over "WindSystem" {
                    float sparkle:wind:leafDetachment = 0.3
                }
            }
        }
        
        "winter" {
            over "StaticVegetation" {
                over "LargeTree1" {
                    over "Materials" {
                        over "LeafMaterial" {
                            string sparkle:material:variation = "winter"
                            bool sparkle:material:leafless = true
                        }
                    }
                }
                
                # Similar overrides for other trees...
            }
            
            over "GroundCover" {
                string sparkle:material:variation = "winter"
                float sparkle:vegetation:density = 0.2
            }
            
            over "Terrain" {
                over "Materials" {
                    over "GroundMaterial" {
                        string sparkle:material:variation = "snow"
                    }
                }
            }
            
            over "EnvironmentEffects" {
                over "SnowSystem" {
                    bool sparkle:effect:active = true
                    float sparkle:snow:depth = 0.2
                }
                
                over "FrostSystem" {
                    bool sparkle:effect:active = true
                }
            }
        }
        
        "spring" {
            over "StaticVegetation" {
                over "LargeTree1" {
                    over "Materials" {
                        over "LeafMaterial" {
                            string sparkle:material:variation = "spring"
                            float3 sparkle:material:baseColor = (0.5, 0.8, 0.2)
                        }
                    }
                }
                
                # Similar overrides for other trees...
            }
            
            over "GroundCover" {
                string sparkle:material:variation = "spring"
                float sparkle:vegetation:density = 1.2
                
                def PointInstancer "Flowers" {
                    bool sparkle:effect:active = true
                }
            }
        }
    }
    
    # Damage state variant set
    variantSet "damageState" = {
        "pristine" {
            # Default state, no overrides needed
        }
        
        "damaged" {
            over "StaticVegetation" {
                over "LargeTree1" {
                    token sparkle:damage:state = "damaged"
                    over "Materials" {
                        over "BarkMaterial" {
                            string sparkle:material:variation = "damaged"
                        }
                    }
                }
                
                # Similar overrides for other trees...
            }
            
            over "Terrain" {
                def PointInstancer "DebrisScatter" {
                    bool sparkle:effect:active = true
                }
                
                over "Materials" {
                    over "GroundMaterial" {
                        string sparkle:material:variation = "damaged"
                    }
                }
            }
        }
        
        "destroyed" {
            over "StaticVegetation" {
                over "LargeTree1" {
                    token sparkle:damage:state = "destroyed"
                    over "Mesh" {
                        references = @environments/vegetation/fallen_tree.usda@</Tree>
                    }
                }
                
                # Similar overrides for other vegetation...
            }
            
            over "Terrain" {
                def PointInstancer "CraterScatter" {
                    bool sparkle:effect:active = true
                }
                
                def PointInstancer "BurnMarkScatter" {
                    bool sparkle:effect:active = true
                }
                
                over "Materials" {
                    over "GroundMaterial" {
                        string sparkle:material:variation = "scorched"
                    }
                }
            }
            
            over "EnvironmentEffects" {
                over "SmokeSystem" {
                    bool sparkle:effect:active = true
                }
                
                over "EmberSystem" {
                    bool sparkle:effect:active = true
                }
            }
        }
    }
    
    # Lighting and effects setup
    def "Lighting" {
        def "DirectionalLight" {
            token sparkle:light:type = "directional"
            float sparkle:light:intensity = 100000
            float3 sparkle:light:color = (1.0, 1.0, 0.9)
        }
        
        def "AmbientLight" {
            token sparkle:light:type = "ambient"
            float sparkle:light:intensity = 10000
            float3 sparkle:light:color = (1.0, 1.0, 1.0)
        }
        
        def "MoonLight" {
            token sparkle:light:type = "directional"
            bool sparkle:light:enabled = false
        }
        
        def "SkyDome" {
            token sparkle:sky:type = "midday"
            token sparkle:sky:cloudCover = "light"
        }
    }
    
    def "EnvironmentEffects" {
        def "WindSystem" {
            token sparkle:system:type = "wind"
            float sparkle:wind:strength = 1.0
            float sparkle:wind:gustiness = 0.3
            float sparkle:wind:direction = 45.0
        }
        
        def "AmbientSounds" {
            token sparkle:system:type = "ambientAudio"
            token sparkle:ambientAudio:daytime = "day_forest"
            token sparkle:ambientAudio:nighttime = "night_forest"
        }
        
        # Various environment effects that are activated in different variants
        def "MorningFog" {
            token sparkle:effect:type = "volumeFog"
            bool sparkle:effect:active = false
        }
        
        def "MorningBirds" {
            token sparkle:effect:type = "ambienceSound"
            bool sparkle:effect:active = false
        }
        
        def "Fireflies" {
            token sparkle:effect:type = "particleSystem"
            bool sparkle:effect:active = false
        }
        
        def "CicadaSounds" {
            token sparkle:effect:type = "ambienceSound"
            bool sparkle:effect:active = false
        }
        
        def "RainParticles" {
            token sparkle:effect:type = "particleSystem"
            bool sparkle:effect:active = false
        }
        
        def "WetSurfaces" {
            token sparkle:effect:type = "materialOverride"
            bool sparkle:effect:active = false
        }
        
        def "RainSounds" {
            token sparkle:effect:type = "ambienceSound"
            bool sparkle:effect:active = false
        }
        
        def "ThunderSounds" {
            token sparkle:effect:type = "ambienceSound"
            bool sparkle:effect:active = false
        }
        
        def "PuddleSystem" {
            token sparkle:effect:type = "decalSystem"
            bool sparkle:effect:active = false
        }
        
        def "CloudShadows" {
            token sparkle:effect:type = "projectedTexture"
            bool sparkle:effect:active = false
        }
        
        def "TreeSwaySystem" {
            token sparkle:effect:type = "animationSystem"
            bool sparkle:effect:active = true
            float sparkle:treeSway:intensity = 1.0
        }
        
        def "SnowSystem" {
            token sparkle:effect:type = "particleSystem"
            bool sparkle:effect:active = false
        }
        
        def "FrostSystem" {
            token sparkle:effect:type = "materialOverride"
            bool sparkle:effect:active = false
        }
        
        def "SmokeSystem" {
            token sparkle:effect:type = "particleSystem"
            bool sparkle:effect:active = false
        }
        
        def "EmberSystem" {
            token sparkle:effect:type = "particleSystem"
            bool sparkle:effect:active = false
        }
    }
}
```

This example demonstrates how variant sets can create an incredibly flexible environment that can adapt to different times of day, weather conditions, seasons, and damage states. Game code can select appropriate variants based on gameplay state:

```cpp
// Update environment based on game time and weather
void UpdateEnvironment(UsdStage* stage, const SdfPath& environmentPath,
                      float gameTimeHours, const std::string& weather, 
                      const std::string& season, const std::string& damageState) {
    // Get environment prim
    UsdPrim environmentPrim = stage->GetPrimAtPath(environmentPath);
    if (!environmentPrim) {
        return;
    }
    
    // Update time of day
    UsdVariantSet timeVariant = environmentPrim.GetVariantSet("timeOfDay");
    if (timeVariant) {
        // Select time variant based on game time
        if (gameTimeHours >= 5.0f && gameTimeHours < 10.0f) {
            timeVariant.SetVariantSelection("morning");
        }
        else if (gameTimeHours >= 10.0f && gameTimeHours < 17.0f) {
            timeVariant.SetVariantSelection("midday");
        }
        else if (gameTimeHours >= 17.0f && gameTimeHours < 20.0f) {
            timeVariant.SetVariantSelection("sunset");
        }
        else {
            timeVariant.SetVariantSelection("night");
        }
    }
    
    // Update weather
    UsdVariantSet weatherVariant = environmentPrim.GetVariantSet("weather");
    if (weatherVariant) {
        weatherVariant.SetVariantSelection(weather);
    }
    
    // Update season
    UsdVariantSet seasonVariant = environmentPrim.GetVariantSet("season");
    if (seasonVariant) {
        seasonVariant.SetVariantSelection(season);
    }
    
    // Update damage state
    UsdVariantSet damageVariant = environmentPrim.GetVariantSet("damageState");
    if (damageVariant) {
        damageVariant.SetVariantSelection(damageState);
    }
}
```

By organizing environmental changes as variant sets, we enable level designers to create rich, dynamic environments that respond to gameplay events while maintaining clean, organized scene structures.

## Streaming Strategies with Payloads

For large open worlds or levels, streaming content dynamically is essential for performance. USD's payload composition arc is specifically designed for this purpose, allowing content to be loaded on demand.

### Level Streaming Architecture

Here's how we might organize a large open-world game using payloads:

```usda
#usda 1.0
(
    defaultPrim = "OpenWorld"
)

def "OpenWorld" (
    kind = "assembly"
)
{
    # Persistent elements (always loaded)
    def "Core" {
        # Global lighting and environment settings that are always active
        def "GlobalLighting" {
            def "SkyLight" {
                token sparkle:light:type = "sky"
            }
            
            def "AtmosphereSettings" {
                token sparkle:atmosphere:type = "realistic"
            }
        }
        
        # Global gameplay systems
        def "GameSystems" {
            def "WeatherSystem" {
                token sparkle:system:type = "weather"
            }
            
            def "TimeSystem" {
                token sparkle:system:type = "gameTime"
            }
            
            def "QuestSystem" {
                token sparkle:system:type = "quest"
            }
        }
        
        # Low-detail distant terrain (always visible)
        def "DistantTerrain" (
            references = @terrain/distant_terrain.usda@</Terrain>
        )
        {
        }
    }
    
    # Streaming regions with payloads
    def "Regions" {
        def "ForestRegion" (
            payload = @regions/forest_region.usda@</Region>
        )
        {
            # Region properties (loaded even when payload isn't)
            string sparkle:region:name = "Verdant Forest"
            float3 sparkle:region:bounds = (2000, 500, 2000)
            float3 sparkle:region:center = (0, 0, 0)
            token sparkle:region:type = "wilderness"
            
            # Streaming properties
            float sparkle:streaming:priority = 1.0
            string sparkle:streaming:loadTrigger = "distance"
            float sparkle:streaming:loadDistance = 1000
            float sparkle:streaming:unloadDistance = 1200
        }
        
        def "MountainRegion" (
            payload = @regions/mountain_region.usda@</Region>
        )
        {
            string sparkle:region:name = "Mistpeak Mountains"
            float3 sparkle:region:bounds = (2000, 1000, 2000)
            float3 sparkle:region:center = (2000, 400, 0)
            token sparkle:region:type = "wilderness"
            
            float sparkle:streaming:priority = 1.0
            string sparkle:streaming:loadTrigger = "distance"
            float sparkle:streaming:loadDistance = 1000
            float sparkle:streaming:unloadDistance = 1200
        }
        
        def "VillageRegion" (
            payload = @regions/village_region.usda@</Region>
        )
        {
            string sparkle:region:name = "Riverdale Village"
            float3 sparkle:region:bounds = (1000, 300, 1000)
            float3 sparkle:region:center = (-1500, 0, -1000)
            token sparkle:region:type = "settlement"
            
            # Village is higher priority for streaming
            float sparkle:streaming:priority = 1.5
            string sparkle:streaming:loadTrigger = "distance"
            float sparkle:streaming:loadDistance = 1500  # Load from further away
            float sparkle:streaming:unloadDistance = 2000
        }
        
        # More regions...
    }
    
    # Points of interest that stream independently
    def "PointsOfInterest" {
        def "AncientRuins" (
            payload = @poi/ancient_ruins.usda@</POI>
        )
        {
            string sparkle:poi:name = "Temple of the Forgotten"
            float3 sparkle:poi:position = (500, 50, 800)
            token sparkle:poi:type = "dungeon"
            
            float sparkle:streaming:priority = 1.2
            string sparkle:streaming:loadTrigger = "distance"
            float sparkle:streaming:loadDistance = 800
            float sparkle:streaming:unloadDistance = 1000
        }
        
        def "BanditCamp" (
            payload = @poi/bandit_camp.usda@</POI>
        )
        {
            string sparkle:poi:name = "Ravager's Hideout"
            float3 sparkle:poi:position = (-800, 20, 1200)
            token sparkle:poi:type = "camp"
            
            float sparkle:streaming:priority = 1.0
            string sparkle:streaming:loadTrigger = "distance"
            float sparkle:streaming:loadDistance = 600
            float sparkle:streaming:unloadDistance = 800
        }
        
        # More points of interest...
    }
    
    # Quest-related content that streams based on story progression
    def "QuestContent" {
        def "MainQuest1Content" (
            payload = @quests/main_quest_1.usda@</QuestContent>
            active = false  # Initially inactive until quest starts
        )
        {
            string sparkle:quest:id = "MQ001"
            token sparkle:quest:type = "main"
            
            string sparkle:streaming:loadTrigger = "quest"
            string sparkle:streaming:questTrigger = "MQ001:active"
        }
        
        # More quest content...
    }
}
```

This structure organizes content into different streaming categories:

1. **Core content** that's always loaded
2. **Region content** that streams based on player position
3. **Points of Interest** that stream independently
4. **Quest content** that loads based on story progression

### Runtime Streaming Management

To implement this streaming system in code:

```cpp
// Streaming manager for open world content
class WorldStreamingManager {
public:
    WorldStreamingManager(UsdStage* stage) : m_stage(stage) {
        // Collect all streamable prims at initialization
        CollectStreamablePrims();
    }
    
    // Update streaming based on player position
    void Update(const GfVec3f& playerPosition, const GameState& gameState) {
        // Process distance-based streaming
        for (const auto& entry : m_distanceStreamingPrims) {
            const SdfPath& primPath = entry.first;
            const StreamingInfo& info = entry.second;
            
            UsdPrim prim = m_stage->GetPrimAtPath(primPath);
            if (!prim) continue;
            
            // Calculate distance to region center
            GfVec3f primCenter(0, 0, 0);
            UsdAttribute centerAttr = prim.GetAttribute(TfToken("sparkle:region:center"));
            if (centerAttr) {
                centerAttr.Get(&primCenter);
            }
            else {
                // Try POI position
                UsdAttribute posAttr = prim.GetAttribute(TfToken("sparkle:poi:position"));
                if (posAttr) {
                    posAttr.Get(&primCenter);
                }
            }
            
            float distance = (primCenter - playerPosition).GetLength();
            
            // Check if we should load or unload
            if (!prim.HasAuthoredPayloads()) {
                continue;  // No payload to manage
            }
            
            if (m_stage->HasLoadedPayload(prim)) {
                // Check if we should unload
                if (distance > info.unloadDistance) {
                    m_stage->UnloadPayload(prim);
                }
            }
            else {
                // Check if we should load
                if (distance < info.loadDistance) {
                    m_stage->LoadPayload(prim);
                }
            }
        }
        
        // Process quest-based streaming
        for (const auto& entry : m_questStreamingPrims) {
            const SdfPath& primPath = entry.first;
            const std::string& questTrigger = entry.second;
            
            UsdPrim prim = m_stage->GetPrimAtPath(primPath);
            if (!prim) continue;
            
            // Check if quest is active
            bool shouldBeLoaded = gameState.IsQuestConditionMet(questTrigger);
            bool isLoaded = prim.IsActive() && m_stage->HasLoadedPayload(prim);
            
            if (shouldBeLoaded && !isLoaded) {
                prim.SetActive(true);
                m_stage->LoadPayload(prim);
            }
            else if (!shouldBeLoaded && isLoaded) {
                m_stage->UnloadPayload(prim);
                prim.SetActive(false);
            }
        }
    }
    
private:
    struct StreamingInfo {
        float loadDistance;
        float unloadDistance;
        float priority;
    };
    
    // Collect all streamable prims and their properties
    void CollectStreamablePrims() {
        m_distanceStreamingPrims.clear();
        m_questStreamingPrims.clear();
        
        for (const UsdPrim& prim : m_stage->TraverseAll()) {
            if (!prim.HasAuthoredPayloads()) {
                continue;
            }
            
            // Check streaming trigger type
            UsdAttribute triggerAttr = prim.GetAttribute(TfToken("sparkle:streaming:loadTrigger"));
            if (!triggerAttr) {
                continue;
            }
            
            std::string trigger;
            triggerAttr.Get(&trigger);
            
            if (trigger == "distance") {
                // Distance-based streaming
                StreamingInfo info;
                
                UsdAttribute loadDistAttr = prim.GetAttribute(TfToken("sparkle:streaming:loadDistance"));
                UsdAttribute unloadDistAttr = prim.GetAttribute(TfToken("sparkle:streaming:unloadDistance"));
                UsdAttribute priorityAttr = prim.GetAttribute(TfToken("sparkle:streaming:priority"));
                
                if (loadDistAttr) {
                    loadDistAttr.Get(&info.loadDistance);
                }
                else {
                    info.loadDistance = 1000.0f; // Default
                }
                
                if (unloadDistAttr) {
                    unloadDistAttr.Get(&info.unloadDistance);
                }
                else {
                    info.unloadDistance = info.loadDistance * 1.2f; // Default
                }
                
                if (priorityAttr) {
                    priorityAttr.Get(&info.priority);
                }
                else {
                    info.priority = 1.0f; // Default
                }
                
                m_distanceStreamingPrims[prim.GetPath()] = info;
            }
            else if (trigger == "quest") {
                // Quest-based streaming
                UsdAttribute questTriggerAttr = prim.GetAttribute(TfToken("sparkle:streaming:questTrigger"));
                if (questTriggerAttr) {
                    std::string questTrigger;
                    questTriggerAttr.Get(&questTrigger);
                    m_questStreamingPrims[prim.GetPath()] = questTrigger;
                }
            }
        }
    }
    
    UsdStage* m_stage;
    std::unordered_map<SdfPath, StreamingInfo, SdfPath::Hash> m_distanceStreamingPrims;
    std::unordered_map<SdfPath, std::string, SdfPath::Hash> m_questStreamingPrims;
};
```

This streaming manager handles both distance-based and quest-based streaming, loading and unloading payloads as needed based on player position and game state.

## State-Based Level Changes via Composition

Beyond environmental variations and streaming, levels often need to respond to player actions or story events. USD's composition system offers multiple approaches for implementing state-based changes.

### Variant-Based State Changes

For localized state changes, variant sets provide an efficient solution:

```usda
def "IronGate" (
    kind = "component"
    inherits = </SparkleGameEntity>
)
{
    string sparkle:entity:id = "gate_dungeon_entrance"
    
    # Gate state variant set
    variantSet "state" = {
        "locked" {
            over "Visual" {
                over "GateMesh" {
                    # Closed position
                    float3 xformOp:rotateXYZ = (0, 0, 0)
                    uniform token[] xformOpOrder = ["xformOp:rotateXYZ"]
                }
                
                over "LockMesh" {
                    bool sparkle:render:visible = true
                }
            }
            
            over "Physics" {
                over "Collider" {
                    bool sparkle:physics:enabled = true
                }
            }
            
            over "Interaction" {
                string sparkle:interaction:prompt = "Unlock Gate"
                rel sparkle:interaction:requiredItem = </Items/RustedKey>
                token sparkle:interaction:outcome = "changeState:unlocked"
            }
            
            over "Audio" {
                token sparkle:audio:ambientSound = "gate_locked_rattle"
            }
        }
        
        "unlocked" {
            over "Visual" {
                over "GateMesh" {
                    # Closed position, but unlocked
                    float3 xformOp:rotateXYZ = (0, 0, 0)
                    uniform token[] xformOpOrder = ["xformOp:rotateXYZ"]
                }
                
                over "LockMesh" {
                    bool sparkle:render:visible = false
                }
            }
            
            over "Physics" {
                over "Collider" {
                    bool sparkle:physics:enabled = true
                }
            }
            
            over "Interaction" {
                string sparkle:interaction:prompt = "Open Gate"
                token sparkle:interaction:outcome = "changeState:open"
            }
            
            over "Audio" {
                token sparkle:audio:ambientSound = "gate_unlocked_creak"
            }
        }
        
        "open" {
            over "Visual" {
                over "GateMesh" {
                    # Open position
                    float3 xformOp:rotateXYZ = (0, 90, 0)
                    uniform token[] xformOpOrder = ["xformOp:rotateXYZ"]
                }
                
                over "LockMesh" {
                    bool sparkle:render:visible = false
                }
            }
            
            over "Physics" {
                over "Collider" {
                    bool sparkle:physics:enabled = false
                }
            }
            
            over "Interaction" {
                string sparkle:interaction:prompt = "Close Gate"
                token sparkle:interaction:outcome = "changeState:unlocked"
            }
            
            over "Audio" {
                token sparkle:audio:ambientSound = "gate_open_ambient"
            }
        }
    }
    
    def "Visual" {
        def "GateMesh" (
            references = @meshes/dungeon/iron_gate.usda@</Mesh>
        )
        {
        }
        
        def "LockMesh" (
            references = @meshes/dungeon/iron_lock.usda@</Mesh>
        )
        {
        }
    }
    
    def "Physics" {
        def "Collider" {
            token sparkle:physics:type = "mesh"
            rel sparkle:physics:mesh = </IronGate/Visual/GateMesh>
        }
    }
    
    def "Interaction" {
        token sparkle:interaction:type = "usable"
    }
    
    def "Audio" {
        token sparkle:audio:type = "ambient"
    }
}
```

When a player interacts with the gate, the game would switch variants:

```cpp
// Handle interaction outcome
void HandleInteractionOutcome(UsdStage* stage, const SdfPath& entityPath, const std::string& outcome) {
    UsdPrim prim = stage->GetPrimAtPath(entityPath);
    if (!prim) {
        return;
    }
    
    // Check for state change outcome
    if (outcome.find("changeState:") == 0) {
        std::string newState = outcome.substr(std::string("changeState:").length());
        
        // Change variant
        UsdVariantSet stateVariant = prim.GetVariantSet("state");
        if (stateVariant && stateVariant.HasVariant(newState)) {
            stateVariant.SetVariantSelection(newState);
        }
    }
    
    // Other outcome types...
}
```

### Layer-Based State Changes

For broader state changes affecting multiple objects, layer-based composition provides more flexibility:

```usda
# Base level layer (level_base.usda)
#usda 1.0
(
    defaultPrim = "Dungeon"
)

def "Dungeon" (
    kind = "assembly"
)
{
    # Base level contents...
    
    def "FloodableArea" {
        def "FloorMesh" (
            references = @meshes/dungeon/stone_floor.usda@</Mesh>
        )
        {
        }
        
        def "WallsMesh" (
            references = @meshes/dungeon/stone_walls.usda@</Mesh>
        )
        {
        }
        
        def "Props" {
            def "Chest1" (
                references = @props/treasure_chest.usda@</Chest>
            )
            {
                float3 xformOp:translate = (5, 0, 8)
                uniform token[] xformOpOrder = ["xformOp:translate"]
            }
            
            # More props...
        }
    }
    
    def "FloodControlMechanism" (
        references = @props/water_wheel.usda@</Mechanism>
    )
    {
        # Interaction to trigger flooding
        def "Interaction" {
            token sparkle:interaction:type = "usable"
            string sparkle:interaction:prompt = "Turn Wheel"
            token sparkle:interaction:outcome = "floodDungeon"
        }
    }
}

# Flooded state layer (level_flooded.usda)
#usda 1.0

over "Dungeon" {
    over "FloodableArea" {
        # Add water
        def "WaterVolume" (
            references = @effects/water_volume.usda@</Water>
        )
        {
            float3 xformOp:scale = (30, 1, 30)
            float3 xformOp:translate = (0, 1, 0)
            uniform token[] xformOpOrder = ["xformOp:translate", "xformOp:scale"]
        }
        
        # Modify floor material to be wet
        over "FloorMesh" {
            over "Material" {
                token sparkle:material:state = "wet"
                float sparkle:material:roughness = 0.1
            }
        }
        
        # Floating props
        over "Props" {
            over "Chest1" {
                # Make chest float on water
                float3 xformOp:translate = (5, 1.5, 8)
                uniform token[] xformOpOrder = ["xformOp:translate"]
            }
            
            # More floating props...
        }
    }
    
    # Modify mechanism to show it's been used
    over "FloodControlMechanism" {
        over "Wheel" {
            float3 xformOp:rotateXYZ = (0, 0, 180)
            uniform token[] xformOpOrder = ["xformOp:rotateXYZ"]
        }
        
        # Disable interaction now that it's been used
        over "Interaction" {
            bool sparkle:interaction:enabled = false
        }
    }
    
    # Add new environment effects
    def "FloodEffects" {
        def "WaterSound" {
            token sparkle:audio:type = "ambient"
            token sparkle:audio:sound = "rushing_water"
            float sparkle:audio:volume = 1.0
        }
        
        def "WaterParticles" {
            token sparkle:particles:type = "waterSplash"
            float sparkle:particles:rate = 0.2
        }
        
        def "WaterRipples" {
            token sparkle:effect:type = "ripple"
            float sparkle:effect:intensity = 1.0
        }
    }
}
```

When the player triggers the flood mechanism, the game would apply the flooded state layer:

```cpp
// Handle special interaction outcomes
void HandleSpecialInteraction(UsdStage* stage, const std::string& outcome) {
    if (outcome == "floodDungeon") {
        // Apply the flooded state layer
        SdfLayerRefPtr rootLayer = stage->GetRootLayer();
        SdfLayerRefPtr floodedLayer = SdfLayer::FindOrOpen("level_flooded.usda");
        
        if (floodedLayer) {
            // Add to sublayers
            SdfSubLayerVector sublayers = rootLayer->GetSubLayerPaths();
            sublayers.push_back(floodedLayer->GetIdentifier());
            rootLayer->SetSubLayerPaths(sublayers);
        }
        
        // Trigger related game events
        GameEvents::Trigger("dungeon_flooded");
    }
    
    // Other special interactions...
}
```

This layer-based approach allows for complex, coordinated changes across multiple objects in the level while maintaining a clean separation between different state representations.

## Key Takeaways

- USD's reference composition enables modular level design with reusable building blocks
- Variant sets provide a powerful mechanism for environmental variation (time, weather, seasons)
- Payload-based composition supports efficient streaming of large open worlds
- State-based composition through variants and layers enables interactive level changes
- These composition approaches can be mixed and matched to create rich, dynamic game worlds

By leveraging USD's composition capabilities, game developers can create more flexible, maintainable level design workflows and more dynamic gameplay experiences. The clear separation of concerns through composition arcs helps manage the complexity of modern game worlds while enabling iteration and experimentation.

In the next chapter, we'll explore how to use USD composition for game state management and progression systems, extending these concepts to broader game systems beyond level design.