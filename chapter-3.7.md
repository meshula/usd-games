# Chapter 3.7: Schema-Driven AI and Behavior

## Composable Behavior Systems with USD

Character behaviors, AI systems, and interactions form the backbone of dynamic gameplay. Traditionally, these systems have been heavily code-driven, with data elements scattered across various configuration files, scripts, and hard-coded values. USD's composition model offers an alternative approach, bringing the power of composition to behavior definition and AI configuration.

In this chapter, we'll explore how to use schema-based composition to create more data-driven, flexible behavior systems that integrate naturally with the rest of your game data.

## Core Behavior Schemas

To begin our exploration of schema-driven AI, let's first establish a foundation with behavior-focused schemas that define the building blocks of our AI systems. The key insight here is identifying the core behavioral components that can be composed together to create complex entity behaviors.

Here are excerpts from our core behavior schemas:

```usda
class "SparkleBehaviorAPI" (
    inherits = </APISchemaBase>
    customData = {
        token apiSchemaType = "singleApply"
        bool skipCodeGeneration = true
    }
    doc = """Core behavior properties for game entities."""
)
{
    # Behavior type
    token sparkle:behavior:type = "idle" (
        allowedTokens = [
            "idle", "passive", "neutral", "aggressive", "friendly", 
            "defensive", "fleeing", "investigating", "patrolling"
        ]
        doc = "Primary behavior type for this entity"
    )
    
    # Awareness
    float sparkle:behavior:sightRadius = 10 (
        doc = "Distance in meters at which this entity can see targets"
    )
    
    # Decision making
    token sparkle:behavior:decisionSystem = "simple" (
        allowedTokens = ["none", "simple", "utility", "behaviorTree", "stateMachine", "goap"]
        doc = "Type of decision-making system to use"
    )
    
    rel sparkle:behavior:decisionData = [] (
        doc = "Relationship to decision-making data (behavior tree, state machine, etc.)"
    )
}
```

Notice how we're using the relationship (`rel`) property to link to the actual decision data, which enables us to separate the decision-making mechanism from its implementation. This is a powerful pattern for behavior composition.

For NPCs that follow schedules, we define a specialized schema:

```usda
class "SparkleScheduleAPI" (
    inherits = </APISchemaBase>
    customData = {
        token apiSchemaType = "singleApply"
        bool skipCodeGeneration = true
    }
    doc = """Schedule-based behavior for NPCs and other entities."""
)
{
    # Schedule entries
    string[] sparkle:schedule:activities = [] (
        doc = "List of activities in the schedule"
    )
    
    float[] sparkle:schedule:times = [] (
        doc = "Times (in hours, 0-24) when each activity starts"
    )
    
    # Schedule constraints
    token[] sparkle:schedule:interruptible = [] (
        doc = "Which activities can be interrupted by other behaviors"
    )
}
```

This API schema can be applied to any entity that should follow a schedule, such as villagers, guards, or even some monster types.

Additional behavior schemas for patrol paths, dialog capabilities, emotional states, and perception systems follow similar patterns, creating a rich vocabulary for describing game behaviors. The key benefit of these schemas is that they can be applied to entities independently, enabling a component-based approach to behavior design.

## Behavior Trees with Composition

Behavior trees are a popular approach for AI decision-making in games. USD's hierarchical structure and composition system is a natural fit for representing behavior trees. Let's examine how we can define a behavior tree for a guard character:

```usda
def "GuardBehaviorTree" (
    kind = "component"
)
{
    token sparkle:tree:type = "behaviorTree"
    
    # Root selector node
    def "RootSelector" {
        token sparkle:node:type = "selector"
        
        # Combat sequence when enemies are present
        def "CombatSequence" {
            token sparkle:node:type = "sequence"
            int sparkle:node:priority = 100
            
            # Condition: Is enemy detected?
            def "EnemyDetectedCondition" {
                token sparkle:node:type = "condition"
                token sparkle:condition:type = "entityDetected"
                token sparkle:condition:entityType = "enemy"
                rel sparkle:condition:perceptionSource = </Entity/Perception>
            }
            
            # More combat nodes...
        }
        
        # Alert sequence when something suspicious is detected
        def "AlertSequence" {
            token sparkle:node:type = "sequence"
            int sparkle:node:priority = 80
            
            # Condition and action nodes...
        }
        
        # More sequences...
    }
}
```

Note how the behavior tree structure is represented naturally as a hierarchy of USD prims, with node types and properties defining their behavior. The relationships between nodes are implicit in the USD hierarchy.

### Specialized Behavior Trees through Composition

One of the most powerful aspects of this approach is the ability to create specialized behavior variants through composition rather than duplication. For example, we can create an elite guard behavior tree by referencing and enhancing the base guard behavior:

```usda
def "EliteGuardBehaviorTree" (
    references = </BehaviorTrees/GuardBehaviorTree>
)
{
    # Override the combat sequence to make it more aggressive
    over "RootSelector" {
        over "CombatSequence" {
            # Elite guards respond more aggressively
            int sparkle:node:priority = 150  # Higher priority
            
            over "CombatSelector" {
                # Add a new action for elite guards: call for reinforcements
                def "CallReinforcementsSequence" {
                    token sparkle:node:type = "sequence"
                    int sparkle:node:priority = 90
                    
                    # Condition: Health below threshold?
                    def "HealthCondition" {
                        token sparkle:node:type = "condition"
                        token sparkle:condition:type = "attribute"
                        token sparkle:condition:attribute = "sparkle:health:current"
                        token sparkle:condition:comparison = "lessThan"
                        float sparkle:condition:threshold = 50.0
                    }
                    
                    # More conditions and actions...
                }
            }
        }
    }
}
```

This composition-based approach offers several advantages:

1. **Maintainability**: Changes to the base behavior tree automatically propagate to specialized versions
2. **Clarity**: The specialized version only contains the differences from the base
3. **Modularity**: New behaviors can be added without modifying the base behavior tree
4. **Traceability**: The relationship to the base behavior is explicit and preserved

## Schedules with Variants

NPC schedules are another area where USD's composition system excels. Using variants, we can create different schedule patterns for different day types or conditions:

```usda
def "VillagerSchedule" (
    kind = "component"
    apiSchemas = ["SparkleScheduleAPI"]
)
{
    # Variant set for different day types
    variantSet "dayType" = {
        "weekday" {
            string[] sparkle:schedule:activities = [
                "sleep", "wakeup", "eat_breakfast", "work", 
                "eat_lunch", "work", "free_time", "eat_dinner", "socialize", "sleep"
            ]
            
            float[] sparkle:schedule:times = [
                0.0, 6.0, 6.5, 8.0, 12.0, 13.0, 17.0, 18.0, 19.0, 22.0
            ]
            
            # Location and interruptible settings...
        }
        
        "weekend" {
            # Weekend schedule...
        }
        
        "festival" {
            # Festival schedule...
        }
        
        "rainy" {
            # Rainy day schedule...
        }
    }
}
```

Using USD's variant sets allows us to encapsulate different schedule patterns in a clean, organized way. The appropriate variant can be selected at runtime based on game state:

```cpp
// Update NPC schedule based on day type and weather
void UpdateNPCSchedule(UsdStage* stage, const SdfPath& npcPath) {
    // Get the schedule variant set
    UsdPrim schedulePrim = stage->GetPrimAtPath(npcPath.AppendPath(SdfPath("Schedule")));
    UsdVariantSet dayTypeVariant = schedulePrim.GetVariantSet("dayType");
    
    // Determine appropriate schedule
    std::string scheduleType;
    
    // Check for festival
    if (GameState::IsEventActive("festival")) {
        scheduleType = "festival";
    }
    // Check for weather
    else if (GameState::GetWeather() == "rainy" || GameState::GetWeather() == "stormy") {
        scheduleType = "rainy";
    }
    // Check day of week
    else {
        scheduleType = (GameState::GetDayOfWeek() == 0 || GameState::GetDayOfWeek() == 6) 
            ? "weekend" : "weekday";
    }
    
    // Set the variant
    if (dayTypeVariant.HasVariant(scheduleType)) {
        dayTypeVariant.SetVariantSelection(scheduleType);
    }
}
```

This approach enables NPCs to adapt to changing game conditions naturally, with schedules that respond to weather, special events, and other factors.

## Dialog Systems with Composition

Dialog systems represent another area where USD's composition capabilities shine. Dialog trees can be represented as hierarchical structures with relationship-based navigation:

```usda
def "BlacksmithDialog" (
    kind = "component"
)
{
    # Dialog metadata
    string sparkle:dialog:speakerName = "Gareth the Blacksmith"
    token sparkle:dialog:voiceType = "male_gruff"
    
    # Root dialog node
    def "Greeting" {
        token sparkle:dialog:type = "node"
        string sparkle:dialog:text = "Well met, traveler. Need something forged or repaired?"
        
        # Greeting variants based on relationship
        variantSet "relationship" = {
            "stranger" {
                string sparkle:dialog:text = "Well met, traveler. Need something forged or repaired?"
            }
            
            "acquaintance" {
                string sparkle:dialog:text = "Good to see you again. What can I help you with today?"
            }
            
            # Other relationship variants...
        }
        
        # Player response options
        def "Responses" {
            def "Shop" {
                token sparkle:dialog:type = "response"
                string sparkle:dialog:text = "I'd like to see what you have for sale."
                rel sparkle:dialog:next = </DialogTrees/BlacksmithDialog/Shop>
            }
            
            def "Quest" {
                token sparkle:dialog:type = "response"
                string sparkle:dialog:text = "I found that ore you were looking for."
                rel sparkle:dialog:next = </DialogTrees/BlacksmithDialog/QuestTurnIn>
                
                # Condition: Only show if player has the quest
                def "Condition" {
                    token sparkle:condition:type = "quest"
                    string sparkle:condition:questId = "blacksmith_ore"
                    token sparkle:condition:state = "active"
                }
                
                # More conditions...
            }
            
            # More responses...
        }
    }
    
    # More dialog nodes...
}
```

Note how the dialog tree uses relationships to connect dialog nodes, allowing for flexible navigation and conditional responses. Variants enable context-sensitive dialog options based on the player's relationship with the NPC.

The dialog controller would traverse this structure at runtime, evaluating conditions and following relationships based on player choices:

```cpp
// Select a dialog response
void DialogController::SelectResponse(int responseIndex) {
    // Get responses container
    UsdPrim responsesContainer = m_currentNode.GetChild(TfToken("Responses"));
    
    // Filter available responses based on conditions
    std::vector<UsdPrim> availableResponses;
    for (const UsdPrim& child : responsesContainer.GetChildren()) {
        UsdPrim conditionPrim = child.GetChild(TfToken("Condition"));
        if (conditionPrim && !EvaluateCondition(conditionPrim)) {
            continue;  // Skip this response if condition not met
        }
        availableResponses.push_back(child);
    }
    
    // Get the selected response
    UsdPrim responsePrim = availableResponses[responseIndex];
    
    // Navigate to next node using relationship
    UsdRelationship nextRel = responsePrim.GetRelationship(TfToken("sparkle:dialog:next"));
    SdfPathVector targets;
    nextRel.GetTargets(&targets);
    
    // Update current node and display dialog
    m_currentNode = m_stage->GetPrimAtPath(targets[0]);
    ExecuteDialogActions();
    ShowCurrentDialog();
}
```

This approach allows for complex dialog trees with quest-dependent options, relationship-based responses, and action triggers, all represented clearly in the USD data model.

### Dialog System Implementation for Game Development

The dialog system implementation we've created showcases how USD's composition capabilities can be leveraged to create sophisticated, data-driven AI behavior systems. Let's review the key aspects of this implementation:

### Schema-Driven Dialog Structure

Our dialog system uses a structured USD schema approach that represents conversations as hierarchical data:

1. **Dialog Tree** - The top-level container for an NPC's dialog content
2. **Dialog Nodes** - Individual conversation points with text and metadata
3. **Response Options** - Player choices that connect nodes together
4. **Actions** - Game effects triggered during conversation (quest updates, item transfers, etc.)
5. **Conditions** - Rules governing when dialog options are available

This schema-based approach allows us to:
- Define complex conversations in a consistent format
- Support multiple authors working on different dialog trees
- Enable tools to validate dialog content
- Separate behavior logic from presentation

### Composition Features Demonstrated

The dialog system leverages several USD composition techniques:

#### 1. Variant Sets for Contextual Dialog

Our implementation uses variant sets to adapt dialog based on:
- Player's relationship with the NPC (`stranger`, `acquaintance`, `friend`, `best_friend`)
- Player's status (`normal`, `injured`, `celebrated`)
- Quest progression (`noQuest`, `questActive`, `questComplete`)

This allows a single dialog node to have multiple variations without duplicating the entire structure.

#### 2. Relationship References for Dialog Flow

Dialog flow uses USD relationships to connect nodes:
```usda
def "Shop" {
    token sparkle:dialog:type = "response"
    string sparkle:dialog:text = "I'd like to see what you have for sale."
    rel sparkle:dialog:next = </DialogTrees/BlacksmithDialog/Shop>
}
```

These relationships make it possible to:
- Visualize conversation flow in USD viewers
- Validate dialog connections
- Dynamically modify conversation paths

#### 3. Nested Conditions and Actions

The system uses nested prims for conditions and actions, allowing for complex combinations:

```usda
def "Quest" {
    token sparkle:dialog:type = "response"
    string sparkle:dialog:text = "I found that ore you were looking for."
    rel sparkle:dialog:next = </DialogTrees/BlacksmithDialog/QuestTurnIn>
    
    # Condition: Only show if player has the quest
    def "Condition" {
        token sparkle:condition:type = "quest"
        string sparkle:condition:questId = "blacksmith_ore"
        token sparkle:condition:state = "active"
    }
    
    # Condition: Only show if player has the ore
    def "ItemCondition" {
        token sparkle:condition:type = "inventory"
        string sparkle:condition:itemId = "special_ore"
        int sparkle:condition:quantity = 1
    }
}
```

### C++ Implementation

The C++ implementation demonstrates how to:

1. **Traverse and interpret the dialog data** - Finding nodes, evaluating conditions
2. **Handle player responses** - Following relationship references
3. **Execute game actions** - Awarding items, updating quests, modifying reputation
4. **Apply context-sensitive variants** - Selecting appropriate dialog variants based on game state

The `DialogController` class provides a clean interface between game systems and the USD-based dialog data.

### Integration with Game Systems

The dialog system integrates with several game systems:
- **Quest System** - For quest conditions and updates
- **Inventory System** - For item checks, rewards, and costs
- **Reputation System** - For relationship-based dialog variations
- **UI System** - For presenting dialog to the player

This integration demonstrates how USD can serve as a core component in a larger game system architecture.

### Benefits of the USD Approach

Using USD for dialog systems offers several advantages over traditional approaches:

1. **Visual Authoring** - Dialog trees can be visualized in USD viewers
2. **Composition-Based Variations** - Context-sensitive dialog without duplication
3. **Schema-Driven Validation** - Preventing common dialog authoring errors
4. **Reusable Components** - Dialog fragments can be composed into larger conversations
5. **Extensibility** - New dialog features can be added via schema extensions
6. **Tool Integration** - Dialog authoring tools can leverage USD's APIs


## Animation Integration through Composition

Behavior systems need to drive character animation, and USD's composition system provides a clean solution through variant sets:

```usda
def "GuardAnimationController" (
    kind = "component"
)
{
    # Animation state mappings from behavior
    variantSet "behaviorState" = {
        "patrol" {
            # Patrol state animations
            token sparkle:animation:primaryState = "walk"
            token sparkle:animation:secondaryState = "scan"
            
            over "StateParameters" {
                float sparkle:animation:moveSpeed = 1.0
                float sparkle:animation:turnSpeed = 1.0
                float sparkle:animation:blendTime = 0.3
            }
            
            over "LookAt" {
                bool sparkle:animation:lookAtEnabled = true
                float sparkle:animation:lookAtWeight = 0.5
            }
        }
        
        "alert" {
            # Alert state animations...
        }
        
        "combat" {
            # Combat state animations...
        }
        
        "idle" {
            # Idle state animations...
        }
    }
    
    # Animation parameters and IK systems...
}
```

This approach separates behavior state from animation details, allowing the animation system to respond to behavior changes without tightly coupling the systems. The behavior state can be updated in code:

```cpp
void UpdateCharacterAnimation(UsdStage* stage, const SdfPath& characterPath) {
    // Get behavior state
    UsdPrim characterPrim = stage->GetPrimAtPath(characterPath);
    std::string behaviorState = "idle";
    UsdAttribute behaviorStateAttr = characterPrim.GetAttribute(TfToken("sparkle:character:behaviorState"));
    if (behaviorStateAttr) {
        TfToken state;
        if (behaviorStateAttr.Get(&state)) {
            behaviorState = state.GetString();
        }
    }
    
    // Update animation controller variant
    UsdPrim animControllerPrim = stage->GetPrimAtPath(
        characterPath.AppendPath(SdfPath("AnimationController"))
    );
    UsdVariantSet behaviorVariant = animControllerPrim.GetVariantSet("behaviorState");
    if (behaviorVariant && behaviorVariant.HasVariant(behaviorState)) {
        behaviorVariant.SetVariantSelection(behaviorState);
    }
    
    // Update additional animation parameters...
}
```

This creates a clean separation between behavior logic and animation, while enabling them to work together seamlessly through USD's composition model.

## Smart Objects with Behavior Templates

Another powerful application of USD composition for game behavior is the creation of "smart objects" - interactive elements that define their own behavior patterns. Using references and specialization, we can define reusable interaction templates:

```usda
def "DoorTemplate" (
    kind = "component"
)
{
    # Core door properties
    bool sparkle:object:interactive = true
    token sparkle:object:type = "door"
    bool sparkle:object:isOpen = false
    
    # Interaction definition
    def "Interaction" {
        token sparkle:interaction:type = "toggle"
        string sparkle:interaction:prompt = "Open Door"
        
        # State machine for door behavior
        def "StateMachine" {
            token sparkle:state:current = "closed"
            
            def "ClosedState" {
                token sparkle:state:name = "closed"
                string sparkle:state:enterAction = "close_door"
                
                def "ToOpenTransition" {
                    token sparkle:transition:targetState = "opening"
                    token sparkle:transition:trigger = "interact"
                }
            }
            
            # More states...
        }
    }
    
    # Animation, physics, and audio definitions...
}
```

Specific instances can then reference and customize this template:

```usda
def "DungeonEntrance" (
    references = </InteractionTemplates/DoorTemplate>
)
{
    # Override the door's properties
    string sparkle:object:name = "Dungeon Entrance"
    
    # Require a key to open
    over "Interaction" {
        over "StateMachine" {
            # Change initial state to locked
            token sparkle:state:current = "locked"
            
            # Add a locked state
            def "LockedState" {
                token sparkle:state:name = "locked"
                string sparkle:state:enterAction = "play_locked_sound"
                string sparkle:interaction:prompt = "Locked"
                
                def "ToOpeningTransition" {
                    token sparkle:transition:targetState = "opening"
                    token sparkle:transition:trigger = "use_key"
                    
                    # Key requirement
                    def "KeyRequirement" {
                        token sparkle:requirement:type = "inventory"
                        string sparkle:requirement:itemId = "dungeon_key"
                        bool sparkle:requirement:removeOnUse = true
                    }
                }
            }
        }
    }
    
    # Custom appearance and audio...
}
```

This smart object approach has several benefits:

1. **Encapsulation**: Each object defines its own interaction behavior
2. **Reusability**: Common interaction patterns can be reused across objects
3. **Extensibility**: New interaction states can be added without modifying base templates
4. **Clarity**: The state machine pattern clearly communicates possible interactions

## Layer-Based Behavior State Management

For more complex behavior state changes, USD's layer composition provides additional flexibility. Consider a guard that changes behavior state in response to alerts or combat:

```cpp
void UpdateGuardState(UsdStage* stage, const SdfPath& guardPath, const std::string& newState) {
    // Get root layer
    SdfLayerRefPtr rootLayer = stage->GetRootLayer();
    SdfSubLayerVector sublayers = rootLayer->GetSubLayerPaths();
    
    // Remove any existing behavior layers
    for (auto it = sublayers.begin(); it != sublayers.end();) {
        if (it->find("guard_behavior_alert.usda") != std::string::npos ||
            it->find("guard_behavior_combat.usda") != std::string::npos) {
            it = sublayers.erase(it);
        } else {
            ++it;
        }
    }
    
    // Add the appropriate behavior layer
    if (newState == "alert") {
        sublayers.push_back("behaviors/guard_behavior_alert.usda");
    }
    else if (newState == "combat") {
        sublayers.push_back("behaviors/guard_behavior_combat.usda");
    }
    // For "normal" state, we don't add any extra layers
    
    // Update sublayers
    rootLayer->SetSubLayerPaths(sublayers);
    
    // Update behavioral state property on the guard
    UsdPrim guardPrim = stage->GetPrimAtPath(guardPath);
    if (guardPrim) {
        UsdAttribute stateAttr = guardPrim.GetAttribute(TfToken("sparkle:character:behaviorState"));
        if (stateAttr) {
            stateAttr.Set(TfToken(newState));
        }
    }
}
```

This layered approach allows for flexible behavior states that can be cleanly managed through composition rather than complex hard-coded state machines. Each layer can contain overrides for properties, variant selections, and even structural changes appropriate to that behavior state.

## Key Takeaways

Schema-driven AI and behavior through USD composition offers several significant advantages:

1. **Data-Driven Design**: Behaviors are defined as data rather than code, enabling designers to modify them without programming expertise
2. **Composition Over Inheritance**: Complex behaviors can be built by composing simpler behaviors rather than creating deep inheritance hierarchies
3. **Clear Visualization**: The hierarchical structure of USD makes behavior patterns more understandable and debuggable
4. **Separation of Concerns**: Different aspects of behavior (decision-making, animation, interaction) can be cleanly separated
5. **Reusable Patterns**: Common behavior patterns can be defined once and reused across many entity types
6. **Runtime Flexibility**: Behaviors can be dynamically modified through variant selection and layer composition
7. **Pipeline Integration**: Behavior definitions can flow through the same asset pipeline as visual elements

By leveraging USD's composition system for behavior definition, games can achieve more flexible, maintainable AI systems that integrate naturally with other game systems.

In the next chapter, we'll explore how USD composition can be applied to procedural and data-driven content, further extending the power of composition to content generation workflows.