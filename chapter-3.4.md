# Chapter 3.4: Equipment and Inventory Systems

## Creating Dynamic Item Systems with USD Composition

Equipment and inventory systems are fundamental to many game genres, from RPGs to action adventures and survival games. These systems present unique challenges as they must dynamically manage relationships between characters and items, visual representation, and gameplay effects. USD's composition system offers elegant solutions for implementing these systems.

In this chapter, we'll explore how to use USD's composition arcs to create flexible, data-driven equipment and inventory systems.

## The Challenges of Equipment Systems

Equipment systems involve several interconnected aspects:

1. **Visual representation**: How equipped items appear on characters
2. **Attachment points**: Where and how items connect to characters
3. **Item properties**: Stats, effects, and behaviors of items
4. **State management**: Tracking equipped vs. unequipped states
5. **Animation integration**: How equipped items affect character animation
6. **Gameplay effects**: How equipped items modify character capabilities

Traditional implementations often require custom code to manage these relationships. USD's composition system can handle many of these aspects declaratively, reducing code complexity and increasing flexibility.

## Schema Design for Equipment and Inventory

Before diving into composition, let's establish the schemas we'll use:

```usda
# Item-related schemas
class "SparkleItemAPI" (
    inherits = </APISchemaBase>
    customData = {
        token apiSchemaType = "singleApply"
        bool skipCodeGeneration = true
    }
    doc = """Properties for inventory items."""
)
{
    # Core item properties
    string sparkle:item:id = ""            # Unique item identifier
    token sparkle:item:type = "misc"       # Item type category
    token sparkle:item:rarity = "common"   # Item rarity
    int sparkle:item:level = 1             # Item level/tier
    int sparkle:item:value = 0             # Currency value
    int sparkle:item:weight = 0            # Weight for encumbrance systems
    bool sparkle:item:isEquippable = false # Whether item can be equipped
    token sparkle:item:equipSlot = ""      # Which slot this item equips to
}

# Equipment-related schemas
class "SparkleEquipmentAPI" (
    inherits = </APISchemaBase>
    customData = {
        token apiSchemaType = "singleApply"
        bool skipCodeGeneration = true
    }
    doc = """Properties for character equipment systems."""
)
{
    # Equipment slots and their current state
    string sparkle:equipment:headSlot = ""       # Currently equipped head item
    string sparkle:equipment:chestSlot = ""      # Currently equipped chest item
    string sparkle:equipment:legsSlot = ""       # Currently equipped legs item
    string sparkle:equipment:feetSlot = ""       # Currently equipped feet item
    string sparkle:equipment:mainHandSlot = ""   # Currently equipped main hand item
    string sparkle:equipment:offHandSlot = ""    # Currently equipped off hand item
    
    # Equipment stats and effects
    float sparkle:equipment:defenseBonus = 0     # Defense from all equipment
    float sparkle:equipment:attackBonus = 0      # Attack from all equipment
    token[] sparkle:equipment:activeEffects = [] # Special effects from equipment
}

# Inventory-related schemas
class "SparkleInventoryAPI" (
    inherits = </APISchemaBase>
    customData = {
        token apiSchemaType = "singleApply"
        bool skipCodeGeneration = true
    }
    doc = """Properties for character inventory systems."""
)
{
    # Inventory properties
    int sparkle:inventory:capacity = 20       # Maximum inventory slots
    int sparkle:inventory:gold = 0            # Currency amount
    int sparkle:inventory:usedSlots = 0       # Number of used slots
    
    # Inventory contents are modeled as relationships to item prims
    rel sparkle:inventory:items = []          # References to item prims
}

# Weapon-specific schema
class "SparkleWeaponAPI" (
    inherits = </APISchemaBase>
    customData = {
        token apiSchemaType = "singleApply"
        bool skipCodeGeneration = true
    }
    doc = """Properties for weapon items."""
)
{
    # Weapon properties
    float sparkle:weapon:damage = 10                # Base damage
    float sparkle:weapon:range = 1                  # Attack range
    float sparkle:weapon:attackSpeed = 1            # Attacks per second
    token sparkle:weapon:damageType = "physical"    # Type of damage dealt
    token sparkle:weapon:weaponClass = "oneHanded"  # Weapon classification
    
    # Special abilities
    token[] sparkle:weapon:specialAbilities = []    # Special weapon abilities
}

# Armor-specific schema
class "SparkleArmorAPI" (
    inherits = </APISchemaBase>
    customData = {
        token apiSchemaType = "singleApply"
        bool skipCodeGeneration = true
    }
    doc = """Properties for armor items."""
)
{
    # Armor properties
    float sparkle:armor:defense = 5                  # Defense value
    float sparkle:armor:weight = 10                  # Weight value affecting movement
    token sparkle:armor:armorClass = "medium"        # Armor classification
    token[] sparkle:armor:resistances = []           # Damage types this armor resists
}
```

With these schemas defined, we can build our equipment and inventory systems.

## Item Templates and Variants

First, let's create a library of item templates using USD's variant system to handle different item tiers and variations:

```usda
#usda 1.0
(
    defaultPrim = "Items"
)

def "Items" (
    kind = "group"
)
{
    def "Weapons" (
        kind = "group"
    )
    {
        def "Sword" (
            kind = "component"
            apiSchemas = ["SparkleItemAPI", "SparkleWeaponAPI"]
        )
        {
            # Base properties for all swords
            string sparkle:item:id = "weapon_sword"
            token sparkle:item:type = "weapon"
            bool sparkle:item:isEquippable = true
            token sparkle:item:equipSlot = "mainHand"
            
            token sparkle:weapon:weaponClass = "oneHanded"
            token sparkle:weapon:damageType = "physical"
            
            # Visual representation
            def "Model" {
                def Mesh "SwordMesh" (
                    references = @meshes/weapons/sword_base.usda@</Mesh>
                )
                {
                }
                
                def Material "SwordMaterial" (
                    references = @materials/metal_base.usda@</Material>
                )
                {
                }
            }
            
            # Attachment information
            def "AttachmentPoint" {
                matrix4d xformOp:transform = ( (1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0), (0, 0, 0, 1) )
                uniform token[] xformOpOrder = ["xformOp:transform"]
                
                rel sparkle:attachment:slot = </Character/Equipment/MainHand>
                token sparkle:attachment:type = "rightHand"
            }
            
            # Variants for different tiers
            variantSet "tier" = {
                "common" {
                    # Base properties are fine for common
                    token sparkle:item:rarity = "common"
                    int sparkle:item:level = 1
                    int sparkle:item:value = 10
                    float sparkle:weapon:damage = 10
                }
                
                "uncommon" {
                    token sparkle:item:rarity = "uncommon"
                    int sparkle:item:level = 5
                    int sparkle:item:value = 50
                    float sparkle:weapon:damage = 15
                    
                    over "Model" {
                        over "SwordMaterial" (
                            references = @materials/metal_uncommon.usda@</Material>
                        )
                        {
                        }
                    }
                }
                
                "rare" {
                    token sparkle:item:rarity = "rare"
                    int sparkle:item:level = 10
                    int sparkle:item:value = 200
                    float sparkle:weapon:damage = 25
                    token[] sparkle:weapon:specialAbilities = ["bleed"]
                    
                    over "Model" {
                        over "SwordMesh" (
                            references = @meshes/weapons/sword_ornate.usda@</Mesh>
                        )
                        {
                        }
                        
                        over "SwordMaterial" (
                            references = @materials/metal_rare.usda@</Material>
                        )
                        {
                        }
                    }
                }
                
                "epic" {
                    token sparkle:item:rarity = "epic"
                    int sparkle:item:level = 20
                    int sparkle:item:value = 1000
                    float sparkle:weapon:damage = 40
                    token[] sparkle:weapon:specialAbilities = ["bleed", "frost"]
                    
                    over "Model" {
                        over "SwordMesh" (
                            references = @meshes/weapons/sword_epic.usda@</Mesh>
                        )
                        {
                        }
                        
                        over "SwordMaterial" (
                            references = @materials/metal_epic.usda@</Material>
                        )
                        {
                        }
                        
                        def PointInstancer "GlowEffect" (
                            references = @effects/weapon_glow.usda@</Effect>
                        )
                        {
                            token sparkle:effect:color = "blue"
                        }
                    }
                }
            }
            
            # Visual variants for different styles
            variantSet "style" = {
                "standard" {
                    # Default style
                }
                
                "curved" {
                    over "Model" {
                        over "SwordMesh" (
                            references = @meshes/weapons/sword_curved.usda@</Mesh>
                        )
                        {
                        }
                    }
                    
                    # Gameplay differences for curved style
                    float sparkle:weapon:attackSpeed = 1.2
                    float sparkle:weapon:damage = 8
                }
                
                "greatsword" {
                    over "Model" {
                        over "SwordMesh" (
                            references = @meshes/weapons/greatsword.usda@</Mesh>
                        )
                        {
                        }
                    }
                    
                    # Gameplay differences for greatsword style
                    token sparkle:weapon:weaponClass = "twoHanded"
                    float sparkle:weapon:attackSpeed = 0.7
                    float sparkle:weapon:damage = 18
                    
                    # Modify attachment point for two-handed weapons
                    over "AttachmentPoint" {
                        rel sparkle:attachment:slot = </Character/Equipment/BothHands>
                        token sparkle:attachment:type = "bothHands"
                    }
                }
            }
        }
        
        # More weapon types...
    }
    
    def "Armor" (
        kind = "group"
    )
    {
        def "Helmet" (
            kind = "component"
            apiSchemas = ["SparkleItemAPI", "SparkleArmorAPI"]
        )
        {
            # Base properties for all helmets
            string sparkle:item:id = "armor_helmet"
            token sparkle:item:type = "armor"
            bool sparkle:item:isEquippable = true
            token sparkle:item:equipSlot = "head"
            
            token sparkle:armor:armorClass = "medium"
            
            # Visual representation
            def "Model" {
                def Mesh "HelmetMesh" (
                    references = @meshes/armor/helmet_base.usda@</Mesh>
                )
                {
                }
                
                def Material "HelmetMaterial" (
                    references = @materials/metal_base.usda@</Material>
                )
                {
                }
            }
            
            # Attachment information
            def "AttachmentPoint" {
                matrix4d xformOp:transform = ( (1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0), (0, 0, 0, 1) )
                uniform token[] xformOpOrder = ["xformOp:transform"]
                
                rel sparkle:attachment:slot = </Character/Equipment/Head>
                token sparkle:attachment:type = "head"
            }
            
            # Variants for different tiers
            variantSet "tier" = {
                "common" {
                    # Base properties are fine for common
                    token sparkle:item:rarity = "common"
                    int sparkle:item:level = 1
                    int sparkle:item:value = 5
                    float sparkle:armor:defense = 3
                }
                
                # More tiers...
            }
            
            # Visual variants for different styles
            variantSet "style" = {
                "standard" {
                    # Default style
                }
                
                "fullface" {
                    over "Model" {
                        over "HelmetMesh" (
                            references = @meshes/armor/helmet_fullface.usda@</Mesh>
                        )
                        {
                        }
                    }
                    
                    # Gameplay differences for full face
                    float sparkle:armor:defense = 5
                }
                
                # More styles...
            }
        }
        
        # More armor types...
    }
}
```

This approach creates a flexible template library with multiple dimensions of variation. Items can vary by both tier (affecting stats) and style (affecting appearance and some stats).

## Character Equipment System

Next, let's define a character with equipment slots and attachment points:

```usda
#usda 1.0
(
    defaultPrim = "Character"
)

def "Character" (
    kind = "component"
    apiSchemas = ["SparkleEquipmentAPI", "SparkleInventoryAPI"]
)
{
    # Base character properties
    string sparkle:character:name = "Player"
    
    # Equipment and inventory properties
    string sparkle:equipment:headSlot = ""
    string sparkle:equipment:chestSlot = ""
    string sparkle:equipment:legsSlot = ""
    string sparkle:equipment:feetSlot = ""
    string sparkle:equipment:mainHandSlot = ""
    string sparkle:equipment:offHandSlot = ""
    
    int sparkle:inventory:capacity = 20
    int sparkle:inventory:gold = 0
    int sparkle:inventory:usedSlots = 0
    rel sparkle:inventory:items = []
    
    # Character visual representation
    def "Appearance" {
        def Mesh "CharacterMesh" (
            references = @meshes/characters/humanoid_base.usda@</Mesh>
        )
        {
        }
        
        def Skeleton "CharacterSkeleton" (
            references = @skeletons/humanoid_base.usda@</Skeleton>
        )
        {
        }
    }
    
    # Equipment attachment points
    def "Equipment" {
        def "Head" {
            rel sparkle:attachment:jointTarget = </Character/Appearance/CharacterSkeleton/Joints/Head>
            matrix4d xformOp:transform = ( (1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0), (0, 0, 0, 1) )
            uniform token[] xformOpOrder = ["xformOp:transform"]
        }
        
        def "Chest" {
            rel sparkle:attachment:jointTarget = </Character/Appearance/CharacterSkeleton/Joints/Spine2>
            matrix4d xformOp:transform = ( (1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0), (0, 0, 0, 1) )
            uniform token[] xformOpOrder = ["xformOp:transform"]
        }
        
        def "Legs" {
            rel sparkle:attachment:jointTarget = </Character/Appearance/CharacterSkeleton/Joints/Hips>
            matrix4d xformOp:transform = ( (1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0), (0, 0, 0, 1) )
            uniform token[] xformOpOrder = ["xformOp:transform"]
        }
        
        def "Feet" {
            # Left and right feet might have separate attachment points
            def "LeftFoot" {
                rel sparkle:attachment:jointTarget = </Character/Appearance/CharacterSkeleton/Joints/LeftFoot>
                matrix4d xformOp:transform = ( (1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0), (0, 0, 0, 1) )
                uniform token[] xformOpOrder = ["xformOp:transform"]
            }
            
            def "RightFoot" {
                rel sparkle:attachment:jointTarget = </Character/Appearance/CharacterSkeleton/Joints/RightFoot>
                matrix4d xformOp:transform = ( (1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0), (0, 0, 0, 1) )
                uniform token[] xformOpOrder = ["xformOp:transform"]
            }
        }
        
        def "MainHand" {
            rel sparkle:attachment:jointTarget = </Character/Appearance/CharacterSkeleton/Joints/RightHand>
            matrix4d xformOp:transform = ( (1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0), (0, 0, 0, 1) )
            uniform token[] xformOpOrder = ["xformOp:transform"]
        }
        
        def "OffHand" {
            rel sparkle:attachment:jointTarget = </Character/Appearance/CharacterSkeleton/Joints/LeftHand>
            matrix4d xformOp:transform = ( (1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0), (0, 0, 0, 1) )
            uniform token[] xformOpOrder = ["xformOp:transform"]
        }
        
        def "BothHands" {
            # Special attachment point for two-handed items
            rel sparkle:attachment:jointTarget = </Character/Appearance/CharacterSkeleton/Joints/RightHand>
            matrix4d xformOp:transform = ( (1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0), (0, 0, 0, 1) )
            uniform token[] xformOpOrder = ["xformOp:transform"]
        }
    }
    
    # Equipped items
    def "EquippedItems" {
        # Initially empty, will be populated when items are equipped
    }
    
    # Inventory items
    def "Inventory" {
        # Initially empty, will be populated when items are acquired
    }
}
```

This character setup defines both equipment slots (in the schema properties) and physical attachment points (in the scene hierarchy) where items will be positioned.

## Dynamic Equipment Visualization Using Composition

The real power of USD for equipment systems comes from its composition capabilities. Let's see how to implement equipping and unequipping items:

```cpp
// Equip an item on a character
bool EquipItem(UsdStage* stage, const SdfPath& characterPath, const SdfPath& itemPath) {
    // Get character and item prims
    UsdPrim characterPrim = stage->GetPrimAtPath(characterPath);
    UsdPrim itemPrim = stage->GetPrimAtPath(itemPath);
    
    if (!characterPrim || !itemPrim) {
        return false;
    }
    
    // Check if item is equippable
    bool isEquippable = false;
    UsdAttribute equippableAttr = itemPrim.GetAttribute(TfToken("sparkle:item:isEquippable"));
    if (!equippableAttr || !equippableAttr.Get(&isEquippable) || !isEquippable) {
        return false;
    }
    
    // Get item ID and equipment slot
    std::string itemId;
    TfToken equipSlot;
    
    UsdAttribute itemIdAttr = itemPrim.GetAttribute(TfToken("sparkle:item:id"));
    UsdAttribute equipSlotAttr = itemPrim.GetAttribute(TfToken("sparkle:item:equipSlot"));
    
    if (!itemIdAttr.Get(&itemId) || !equipSlotAttr.Get(&equipSlot)) {
        return false;
    }
    
    // Update character equipment slot property
    std::string slotPropertyName = "sparkle:equipment:" + equipSlot.GetString() + "Slot";
    UsdAttribute slotAttr = characterPrim.GetAttribute(TfToken(slotPropertyName));
    
    if (!slotAttr) {
        return false;
    }
    
    // See if we need to unequip an existing item
    std::string currentItemId;
    if (slotAttr.Get(&currentItemId) && !currentItemId.empty()) {
        // Unequip current item
        UnequipItem(stage, characterPath, equipSlot);
    }
    
    // Set the slot property to this item's ID
    slotAttr.Set(itemId);
    
    // Create a reference to the item under EquippedItems
    SdfPath equippedItemPath = characterPath.AppendPath(SdfPath("EquippedItems/" + equipSlot.GetString()));
    UsdPrim equippedItemPrim = stage->DefinePrim(equippedItemPath);
    equippedItemPrim.GetReferences().AddReference(
        stage->GetRootLayer()->GetIdentifier(),
        itemPath
    );
    
    // Get attachment point information
    UsdPrim attachmentPrim = itemPrim.GetChild(TfToken("AttachmentPoint"));
    if (attachmentPrim) {
        // Get target attachment slot
        SdfPathVector targetPaths;
        UsdRelationship slotRel = attachmentPrim.GetRelationship(TfToken("sparkle:attachment:slot"));
        if (slotRel) {
            slotRel.GetTargets(&targetPaths);
            
            if (!targetPaths.empty()) {
                // Reparent the equipped item to the attachment point
                SdfPath targetPath = targetPaths[0];
                
                // Add a reference to the equipment slot
                SdfPath equipmentSlotPath = characterPath.AppendPath(
                    SdfPath("Equipment/" + equipSlot.GetString())
                );
                
                // Set the position
                UsdGeomXformable xformable(equippedItemPrim);
                if (xformable) {
                    GfMatrix4d localTransform;
                    bool resetsXformStack;
                    UsdGeomXformable attachmentXform(attachmentPrim);
                    if (attachmentXform) {
                        attachmentXform.GetLocalTransformation(&localTransform, &resetsXformStack);
                        xformable.SetTransformOrder(UsdGeomXformOp::TypeTransform);
                        xformable.AddTransformOp().Set(localTransform);
                    }
                }
            }
        }
    }
    
    // Update character stats based on equipped item
    UpdateCharacterEquipmentStats(stage, characterPath);
    
    return true;
}

// Unequip an item from a specified slot
bool UnequipItem(UsdStage* stage, const SdfPath& characterPath, const TfToken& slot) {
    // Get character prim
    UsdPrim characterPrim = stage->GetPrimAtPath(characterPath);
    if (!characterPrim) {
        return false;
    }
    
    // Get slot property
    std::string slotPropertyName = "sparkle:equipment:" + slot.GetString() + "Slot";
    UsdAttribute slotAttr = characterPrim.GetAttribute(TfToken(slotPropertyName));
    
    if (!slotAttr) {
        return false;
    }
    
    // Clear the slot property
    slotAttr.Set(std::string());
    
    // Remove the equipped item prim
    SdfPath equippedItemPath = characterPath.AppendPath(SdfPath("EquippedItems/" + slot.GetString()));
    stage->RemovePrim(equippedItemPath);
    
    // Update character stats
    UpdateCharacterEquipmentStats(stage, characterPath);
    
    return true;
}

// Update character stats based on all equipped items
void UpdateCharacterEquipmentStats(UsdStage* stage, const SdfPath& characterPath) {
    // Get character prim
    UsdPrim characterPrim = stage->GetPrimAtPath(characterPath);
    if (!characterPrim) {
        return;
    }
    
    // Reset equipment bonuses
    float defenseBonus = 0.0f;
    float attackBonus = 0.0f;
    std::vector<TfToken> activeEffects;
    
    // Get all equipment slots
    std::vector<std::string> slotProperties = {
        "sparkle:equipment:headSlot",
        "sparkle:equipment:chestSlot",
        "sparkle:equipment:legsSlot",
        "sparkle:equipment:feetSlot",
        "sparkle:equipment:mainHandSlot",
        "sparkle:equipment:offHandSlot"
    };
    
    // Check each slot for equipped items
    for (const auto& slotProperty : slotProperties) {
        UsdAttribute slotAttr = characterPrim.GetAttribute(TfToken(slotProperty));
        if (!slotAttr) continue;
        
        std::string itemId;
        if (!slotAttr.Get(&itemId) || itemId.empty()) continue;
        
        // Find corresponding equipped item
        std::string slotName = slotProperty.substr(std::string("sparkle:equipment:").length());
        slotName = slotName.substr(0, slotName.length() - 4); // Remove "Slot" suffix
        
        SdfPath equippedItemPath = characterPath.AppendPath(SdfPath("EquippedItems/" + slotName));
        UsdPrim equippedItemPrim = stage->GetPrimAtPath(equippedItemPath);
        if (!equippedItemPrim) continue;
        
        // Check for armor properties
        UsdAttribute defenseAttr = equippedItemPrim.GetAttribute(TfToken("sparkle:armor:defense"));
        if (defenseAttr) {
            float defense = 0.0f;
            if (defenseAttr.Get(&defense)) {
                defenseBonus += defense;
            }
        }
        
        // Check for weapon properties
        UsdAttribute damageAttr = equippedItemPrim.GetAttribute(TfToken("sparkle:weapon:damage"));
        if (damageAttr) {
            float damage = 0.0f;
            if (damageAttr.Get(&damage)) {
                attackBonus += damage;
            }
        }
        
        // Check for special abilities
        UsdAttribute abilitiesAttr;
        
        // Check weapon special abilities
        abilitiesAttr = equippedItemPrim.GetAttribute(TfToken("sparkle:weapon:specialAbilities"));
        if (abilitiesAttr) {
            std::vector<TfToken> abilities;
            if (abilitiesAttr.Get(&abilities)) {
                activeEffects.insert(activeEffects.end(), abilities.begin(), abilities.end());
            }
        }
        
        // Check armor resistances
        abilitiesAttr = equippedItemPrim.GetAttribute(TfToken("sparkle:armor:resistances"));
        if (abilitiesAttr) {
            std::vector<TfToken> resistances;
            if (abilitiesAttr.Get(&resistances)) {
                for (const auto& resistance : resistances) {
                    activeEffects.push_back(TfToken("resist_" + resistance.GetString()));
                }
            }
        }
    }
    
    // Update character equipment stats
    UsdAttribute defenseAttr = characterPrim.GetAttribute(TfToken("sparkle:equipment:defenseBonus"));
    if (defenseAttr) {
        defenseAttr.Set(defenseBonus);
    }
    
    UsdAttribute attackAttr = characterPrim.GetAttribute(TfToken("sparkle:equipment:attackBonus"));
    if (attackAttr) {
        attackAttr.Set(attackBonus);
    }
    
    UsdAttribute effectsAttr = characterPrim.GetAttribute(TfToken("sparkle:equipment:activeEffects"));
    if (effectsAttr) {
        effectsAttr.Set(activeEffects);
    }
}
```

This implementation demonstrates how to use USD's composition to manage equipment:

1. The `EquipItem` function updates the character's slot properties and creates a reference to the item in the character's `EquippedItems` hierarchy
2. The `UnequipItem` function clears the slot and removes the reference
3. The `UpdateCharacterEquipmentStats` function recalculates the character's stats based on all equipped items

## Inventory Management with Relationships

USD's relationship composition provides a natural way to implement inventory systems. Here's how to add and remove items from a character's inventory:

```cpp
// Add an item to a character's inventory
bool AddItemToInventory(UsdStage* stage, const SdfPath& characterPath, const SdfPath& itemPath) {
    // Get character and item prims
    UsdPrim characterPrim = stage->GetPrimAtPath(characterPath);
    UsdPrim itemPrim = stage->GetPrimAtPath(itemPath);
    
    if (!characterPrim || !itemPrim) {
        return false;
    }
    
    // Check inventory capacity
    int capacity = 20;
    int usedSlots = 0;
    
    UsdAttribute capacityAttr = characterPrim.GetAttribute(TfToken("sparkle:inventory:capacity"));
    UsdAttribute usedSlotsAttr = characterPrim.GetAttribute(TfToken("sparkle:inventory:usedSlots"));
    
    if (capacityAttr) {
        capacityAttr.Get(&capacity);
    }
    
    if (usedSlotsAttr) {
        usedSlotsAttr.Get(&usedSlots);
    }
    
    if (usedSlots >= capacity) {
        // Inventory is full
        return false;
    }
    
    // Get item ID
    std::string itemId;
    UsdAttribute itemIdAttr = itemPrim.GetAttribute(TfToken("sparkle:item:id"));
    if (!itemIdAttr.Get(&itemId)) {
        return false;
    }
    
    // Create a copy of the item in the character's inventory
    SdfPath inventoryItemPath = characterPath.AppendPath(SdfPath("Inventory/" + itemId));
    UsdPrim inventoryItemPrim = stage->DefinePrim(inventoryItemPath);
    inventoryItemPrim.GetReferences().AddReference(
        stage->GetRootLayer()->GetIdentifier(),
        itemPath
    );
    
    // Add the item to the inventory relationships
    UsdRelationship itemsRel = characterPrim.GetRelationship(TfToken("sparkle:inventory:items"));
    if (!itemsRel) {
        itemsRel = characterPrim.CreateRelationship(TfToken("sparkle:inventory:items"));
    }
    
    SdfPathVector targetPaths;
    itemsRel.GetTargets(&targetPaths);
    targetPaths.push_back(inventoryItemPath);
    itemsRel.SetTargets(targetPaths);
    
    // Update used slots
    usedSlotsAttr.Set(usedSlots + 1);
    
    return true;
}

// Remove an item from a character's inventory
bool RemoveItemFromInventory(UsdStage* stage, const SdfPath& characterPath, const SdfPath& inventoryItemPath) {
    // Get character prim
    UsdPrim characterPrim = stage->GetPrimAtPath(characterPath);
    UsdPrim inventoryItemPrim = stage->GetPrimAtPath(inventoryItemPath);
    
    if (!characterPrim || !inventoryItemPrim) {
        return false;
    }
    
    // Remove the item from the inventory relationships
    UsdRelationship itemsRel = characterPrim.GetRelationship(TfToken("sparkle:inventory:items"));
    if (!itemsRel) {
        return false;
    }
    
    SdfPathVector targetPaths;
    itemsRel.GetTargets(&targetPaths);
    
    auto it = std::find(targetPaths.begin(), targetPaths.end(), inventoryItemPath);
    if (it == targetPaths.end()) {
        return false;
    }
    
    targetPaths.erase(it);
    itemsRel.SetTargets(targetPaths);
    
    // Remove the inventory item prim
    stage->RemovePrim(inventoryItemPath);
    
    // Update used slots
    int usedSlots = 0;
    UsdAttribute usedSlotsAttr = characterPrim.GetAttribute(TfToken("sparkle:inventory:usedSlots"));
    if (usedSlotsAttr && usedSlotsAttr.Get(&usedSlots) && usedSlots > 0) {
        usedSlotsAttr.Set(usedSlots - 1);
    }
    
    return true;
}
```

This implementation uses USD's reference mechanism to keep a copy of each item in the character's inventory and manages relationships to track the inventory contents.

## Equipment Variants for Visual Customization

One of the powerful features of USD composition for equipment systems is the ability to have items adapt to characters. For example, armor might have different appearances based on character race or gender:

```usda
def "ChestArmor" (
    kind = "component"
    apiSchemas = ["SparkleItemAPI", "SparkleArmorAPI"]
)
{
    # Base armor properties
    string sparkle:item:id = "armor_chest"
    token sparkle:item:type = "armor"
    bool sparkle:item:isEquippable = true
    token sparkle:item:equipSlot = "chest"
    
    token sparkle:armor:armorClass = "medium"
    float sparkle:armor:defense = 10
    
    # Character adaptation variant set
    variantSet "characterType" = {
        "human_male" {
            over "Model" {
                def Mesh "ArmorMesh" (
                    references = @meshes/armor/chest_human_male.usda@</Mesh>
                )
                {
                }
            }
        }
        
        "human_female" {
            over "Model" {
                def Mesh "ArmorMesh" (
                    references = @meshes/armor/chest_human_female.usda@</Mesh>
                )
                {
                }
            }
        }
        
        "elf_male" {
            over "Model" {
                def Mesh "ArmorMesh" (
                    references = @meshes/armor/chest_elf_male.usda@</Mesh>
                )
                {
                }
            }
        }
        
        "elf_female" {
            over "Model" {
                def Mesh "ArmorMesh" (
                    references = @meshes/armor/chest_elf_female.usda@</Mesh>
                )
                {
                }
            }
        }
        
        # Additional character types...
    }
}
```

When equipping an item, the game would select the appropriate variant based on the character's properties:

```cpp
// Select appropriate variant for an equipped item based on character properties
void AdaptItemToCharacter(UsdStage* stage, const SdfPath& characterPath, const SdfPath& equippedItemPath) {
    // Get character and item prims
    UsdPrim characterPrim = stage->GetPrimAtPath(characterPath);
    UsdPrim itemPrim = stage->GetPrimAtPath(equippedItemPath);
    
    if (!characterPrim || !itemPrim) {
        return;
    }
    
    // Check if the item has a characterType variant set
    UsdVariantSet characterTypeVarSet = itemPrim.GetVariantSet("characterType");
    if (!characterTypeVarSet.IsValid()) {
        return;
    }
    
    // Get character race and gender
    TfToken race = TfToken("human");
    TfToken gender = TfToken("male");
    
    UsdAttribute raceAttr = characterPrim.GetAttribute(TfToken("sparkle:character:race"));
    UsdAttribute genderAttr = characterPrim.GetAttribute(TfToken("sparkle:character:gender"));
    
    if (raceAttr) {
        raceAttr.Get(&race);
    }
    
    if (genderAttr) {
        genderAttr.Get(&gender);
    }
    
    // Construct the variant name
    std::string variantName = race.GetString() + "_" + gender.GetString();
    
    // Check if the variant exists
    if (characterTypeVarSet.HasVariant(variantName)) {
        characterTypeVarSet.SetVariantSelection(variantName);
    }
    else {
        // Fall back to default human_male if specific variant doesn't exist
        if (characterTypeVarSet.HasVariant("human_male")) {
            characterTypeVarSet.SetVariantSelection("human_male");
        }
    }
}
```

This adaptive approach allows a single item to appear appropriately on different character types without requiring separate items for each combination.

## Layered Equipment Visualization

For some types of equipment, especially clothing and armor, we often want to show layered items. USD's composition mechanism handles this elegantly:

```usda
# Character with layered clothing
def "Character" (
    kind = "component"
    apiSchemas = ["SparkleEquipmentAPI"]
)
{
    # Character visual representation
    def "Appearance" {
        def Mesh "BaseMesh" (
            references = @meshes/characters/human_base.usda@</Mesh>
        )
        {
            rel material:binding = </Character/Appearance/SkinMaterial>
        }
        
        def Material "SkinMaterial" (
            references = @materials/skin_default.usda@</Material>
        )
        {
        }
        
        # Layered clothing - starts empty, filled by equipment system
        def "Clothing" {
            def "Layer1" {  # Underwear layer
                # Initially empty
            }
            
            def "Layer2" {  # Clothing layer
                # Initially empty
            }
            
            def "Layer3" {  # Armor layer
                # Initially empty
            }
            
            def "Layer4" {  # Accessories layer
                # Initially empty
            }
        }
    }
    
    # Equipment slots and attachment points...
}
```

When equipping clothing items, the system would place them in the appropriate layer:

```cpp
// Equip a clothing item with layer awareness
bool EquipClothingItem(UsdStage* stage, const SdfPath& characterPath, const SdfPath& itemPath) {
    // Basic equipment logic...
    
    // Get clothing layer information
    TfToken clothingLayer;
    UsdAttribute layerAttr = itemPrim.GetAttribute(TfToken("sparkle:clothing:layer"));
    if (layerAttr && layerAttr.Get(&clothingLayer)) {
        // Get layer path
        SdfPath layerPath = characterPath.AppendPath(SdfPath("Appearance/Clothing/" + clothingLayer.GetString()));
        
        // Create a reference to the clothing item in the appropriate layer
        UsdPrim layerPrim = stage->GetPrimAtPath(layerPath);
        if (layerPrim) {
            // Create clothing item under this layer
            std::string itemType;
            UsdAttribute typeAttr = itemPrim.GetAttribute(TfToken("sparkle:clothing:type"));
            if (typeAttr && typeAttr.Get(&itemType)) {
                SdfPath clothingItemPath = layerPath.AppendPath(SdfPath(itemType));
                UsdPrim clothingItemPrim = stage->DefinePrim(clothingItemPath);
                clothingItemPrim.GetReferences().AddReference(
                    stage->GetRootLayer()->GetIdentifier(),
                    itemPath
                );
            }
        }
    }
    
    // Rest of equipment logic...
}
```

This layering approach ensures that clothing items render in the appropriate order (underwear under armor, etc.) and can be individually added or removed.

## Dynamic Animation Integration

Equipment can also affect character animations. USD's composition system can handle this by allowing items to provide animation overrides:

```usda
def "TwoHandedSword" (
    kind = "component"
    apiSchemas = ["SparkleItemAPI", "SparkleWeaponAPI"]
)
{
    # Weapon properties...
    
    # Animation overrides for the equipped character
    def "AnimationOverrides" {
        def SkelAnimation "IdleOverride" (
            references = @animations/two_handed_idle.usda@</Animation>
        )
        {
            rel skelTarget = </Character/Appearance/CharacterSkeleton>
        }
        
        def SkelAnimation "WalkOverride" (
            references = @animations/two_handed_walk.usda@</Animation>
        )
        {
            rel skelTarget = </Character/Appearance/CharacterSkeleton>
        }
        
        def SkelAnimation "AttackOverride" (
            references = @animations/two_handed_attack.usda@</Animation>
        )
        {
            rel skelTarget = </Character/Appearance/CharacterSkeleton>
        }
    }
}
```

When equipping the item, the game would add references to these animation overrides in the character's animation system:

```cpp
// Apply animation overrides from equipped item
void ApplyItemAnimationOverrides(UsdStage* stage, const SdfPath& characterPath, const SdfPath& equippedItemPath) {
    // Get character and item prims
    UsdPrim characterPrim = stage->GetPrimAtPath(characterPath);
    UsdPrim itemPrim = stage->GetPrimAtPath(equippedItemPath);
    
    if (!characterPrim || !itemPrim) {
        return;
    }
    
    // Check if the item has animation overrides
    UsdPrim overridesPrim = itemPrim.GetChild(TfToken("AnimationOverrides"));
    if (!overridesPrim) {
        return;
    }
    
    // Get character animation controller
    SdfPath animControllerPath = characterPath.AppendPath(SdfPath("Animation/Controller"));
    UsdPrim animControllerPrim = stage->GetPrimAtPath(animControllerPath);
    if (!animControllerPrim) {
        return;
    }
    
    // Apply each animation override
    for (const UsdPrim& overridePrim : overridesPrim.GetChildren()) {
        // Get the animation type from the override prim name
        std::string animType = overridePrim.GetName();
        
        // Create or update the animation reference in the controller
        SdfPath targetPath = animControllerPath.AppendPath(SdfPath(animType + "Animation"));
        UsdPrim targetPrim = stage->GetPrimAtPath(targetPath);
        
        if (!targetPrim) {
            targetPrim = stage->DefinePrim(targetPath);
        }
        
        // Apply the reference
        targetPrim.GetReferences().AddReference(
            stage->GetRootLayer()->GetIdentifier(),
            overridePrim.GetPath()
        );
        
        // Mark this animation as an equipment override
        UsdAttribute overrideAttr = targetPrim.CreateAttribute(
            TfToken("sparkle:animation:equipmentOverride"), 
            SdfValueTypeNames->Bool
        );
        overrideAttr.Set(true);
    }
}
```

When the item is unequipped, the system would remove these animation overrides.

## Item Sets and Combination Effects

RPGs often feature item sets with bonus effects when multiple pieces are equipped. USD's composition system makes this straightforward to implement:

```usda
# Item set definition
def "ItemSets" {
    def "DragonKnightSet" {
        # Set pieces
        token[] sparkle:set:pieces = [
            "helmet_dragon_knight",
            "chest_dragon_knight",
            "legs_dragon_knight",
            "boots_dragon_knight",
            "gauntlets_dragon_knight"
        ]
        
        # Set bonuses by number of pieces equipped
        dictionary sparkle:set:bonuses = {
            "2": {
                "sparkle:armor:defense": (float) 10.0,
            },
            "4": {
                "sparkle:armor:defense": (float) 20.0,
                "sparkle:armor:resistances": (token[]) ["fire"],
            },
            "5": {
                "sparkle:armor:defense": (float) 30.0,
                "sparkle:armor:resistances": (token[]) ["fire", "poison"],
                "sparkle:character:specialAbilities": (token[]) ["dragon_breath"],
            }
        }
    }
}
```

When updating character stats, the system would check for set bonuses:

```cpp
// Check for item set bonuses
void ApplyItemSetBonuses(UsdStage* stage, const SdfPath& characterPath) {
    // Get character prim
    UsdPrim characterPrim = stage->GetPrimAtPath(characterPath);
    if (!characterPrim) {
        return;
    }
    
    // Get all equipped items
    std::vector<std::string> slotProperties = {
        "sparkle:equipment:headSlot",
        "sparkle:equipment:chestSlot",
        "sparkle:equipment:legsSlot",
        "sparkle:equipment:feetSlot",
        "sparkle:equipment:mainHandSlot",
        "sparkle:equipment:offHandSlot"
    };
    
    // Collect all equipped item IDs
    std::vector<std::string> equippedItemIds;
    for (const auto& slotProperty : slotProperties) {
        UsdAttribute slotAttr = characterPrim.GetAttribute(TfToken(slotProperty));
        if (!slotAttr) continue;
        
        std::string itemId;
        if (slotAttr.Get(&itemId) && !itemId.empty()) {
            equippedItemIds.push_back(itemId);
        }
    }
    
    // Check each item set
    UsdPrim itemSetsPrim = stage->GetPrimAtPath(SdfPath("/ItemSets"));
    if (!itemSetsPrim) return;
    
    for (const UsdPrim& setsPrim : itemSetsPrim.GetChildren()) {
        // Get set pieces
        std::vector<TfToken> setPieces;
        UsdAttribute piecesAttr = setsPrim.GetAttribute(TfToken("sparkle:set:pieces"));
        if (!piecesAttr || !piecesAttr.Get(&setPieces)) continue;
        
        // Count how many set pieces are equipped
        int equippedPieces = 0;
        for (const auto& piece : setPieces) {
            if (std::find(equippedItemIds.begin(), equippedItemIds.end(), piece.GetString()) != equippedItemIds.end()) {
                equippedPieces++;
            }
        }
        
        // Apply set bonuses if enough pieces are equipped
        if (equippedPieces >= 2) {
            // Get set bonuses
            VtDictionary setBonuses;
            UsdAttribute bonusesAttr = setsPrim.GetAttribute(TfToken("sparkle:set:bonuses"));
            if (!bonusesAttr || !bonusesAttr.Get(&setBonuses)) continue;
            
            // Check for bonuses at the current piece count
            std::string pieceCountStr = std::to_string(equippedPieces);
            auto it = setBonuses.find(pieceCountStr);
            
            if (it != setBonuses.end() && it->second.IsHolding<VtDictionary>()) {
                VtDictionary bonuses = it->second.Get<VtDictionary>();
                
                // Apply each bonus
                for (const auto& bonus : bonuses) {
                    std::string bonusProperty = bonus.first;
                    VtValue bonusValue = bonus.second;
                    
                    // Create or update the attribute
                    UsdAttribute bonusAttr = characterPrim.GetAttribute(TfToken(bonusProperty));
                    if (!bonusAttr) {
                        bonusAttr = characterPrim.CreateAttribute(TfToken(bonusProperty), bonusValue.GetTypeName());
                    }
                    
                    bonusAttr.Set(bonusValue);
                }
            }
        }
    }
}
```

This implementation checks for equipped set pieces and applies the appropriate bonuses based on how many pieces are equipped.

## Key Takeaways

- USD's composition system provides powerful tools for implementing equipment and inventory systems
- References allow items to be instantiated within character hierarchies while maintaining a link to the original template
- Variants enable items to adapt to different character types or configurations
- Relationships provide a natural way to track inventory contents
- Layered equipment visualization is handled naturally through USD's scenegraph
- Animation overrides can be implemented through composition
- Complex systems like item sets can be built using these fundamental mechanisms

These composition-based approaches result in equipment and inventory systems that are more declarative, flexible, and easier to maintain than traditional code-heavy implementations. By leveraging USD's strengths, game developers can create rich, data-driven item systems that integrate seamlessly with other game subsystems.

In the next section, we'll explore how USD composition can be applied to level design, extending these concepts to environmental systems and scene management.