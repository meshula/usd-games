# Chapter 5: Pipeline Integration Strategies

## Deploying Schemas Across Tools and Engines

Creating schemas is only the first step. For effective game development, you need robust strategies for integrating these schemas throughout your pipeline. This chapter explores practical approaches to tool integration, schema distribution, versioning, and migration.

## Schema Distribution Mechanisms

### 1. Centralized Schema Repository

Establish a central repository for all schema definitions:

```
project/
├── schemas/
│   ├── game/
│   │   ├── sparkleCarrot_schema.usda  # Game-specific schemas
│   │   ├── sparkleWeapons_schema.usda # Weapon system schemas
│   │   └── sparkleQuest_schema.usda   # Quest system schemas
│   ├── engine/
│   │   └── sparkleEngine_schema.usda  # Engine-specific schemas
│   └── shared/
│       └── sparkleCore_schema.usda    # Core schemas used by all systems
├── assets/
│   └── levels/
│       └── level_01.usda              # References schema layers
└── tools/
    └── schema_tools/                  # Schema management scripts
```

This approach makes schema versioning and deployment easier by providing a single source of truth.

### 2. Schema Layer References

Make your asset files reference schema layers explicitly:

```usda
#usda 1.0
(
    # Reference schema definitions
    subLayers = [
        @../../../schemas/game/sparkleCarrot_schema.usda@,
        @../../../schemas/game/sparkleWeapons_schema.usda@
    ]
)

def "Level" {
    # Level contents using schemas
}
```

This ensures assets always carry their schema dependencies with them.

### 3. Plugin-Based Distribution

For integrated DCC tools, create lightweight plugins that register schema layers:

```python
# Maya plugin example
import maya.cmds as cmds
import maya.api.OpenMaya as om
import os

def maya_useNewAPI():
    # Indicate that this plugin uses the new Maya Python API 2.0
    pass

# Initialize plugin
def initializePlugin(plugin):
    pluginFn = om.MFnPlugin(plugin)
    
    # Register USD schemas for this plugin
    schema_dir = os.path.join(os.path.dirname(__file__), "schemas")
    schema_path = os.path.join(schema_dir, "sparkleCarrot_schema.usda")
    
    # Register with Maya's USD plugin (simplified example)
    cmds.mayaUSDRegisterSchemaPath(schema_path)
    
    print("SparkleCarrot schemas registered!")

# Uninitialize plugin
def uninitializePlugin(plugin):
    # Unregister schemas if needed
    pass
```

### 4. Pipeline Integration Hooks

Create hooks in your pipeline tools to ensure schemas are properly loaded:

```python
# Example hook for a pipeline synchronization tool
def pre_sync_hook(context):
    """Ensure schemas are up to date before sync."""
    # Check schema versions
    current_version = get_current_schema_version()
    latest_version = get_latest_schema_version()
    
    if current_version < latest_version:
        # Update schemas
        update_local_schemas()
        
        # Log the update
        print(f"Updated schemas from v{current_version} to v{latest_version}")
```

## Tool and Engine Integration Patterns

### 1. DCC Tool Integration

Each DCC tool requires specific integration approaches:

#### Maya Integration

```python
# Maya schema integration example
import maya.cmds as cmds
from pxr import Usd, UsdGeom, Sdf, Tf

def create_carrot_enemy():
    # Create USD stage
    stage = Usd.Stage.CreateInMemory()
    
    # Define carrot enemy
    carrot = stage.DefinePrim("/Enemy", "SparkleEnemyCarrot")
    
    # Apply API schemas
    carrot.ApplyAPI(Tf.Type.FindByName("SparkleHealthAPI"))
    carrot.ApplyAPI(Tf.Type.FindByName("SparkleCombatAPI"))
    
    # Set properties
    carrot.CreateAttribute("sparkle:health:current", Sdf.ValueTypeNames.Float).Set(50.0)
    carrot.CreateAttribute("sparkle:combat:damage", Sdf.ValueTypeNames.Float).Set(10.0)
    
    # Export to USD file
    stage.Export("/path/to/enemy.usda")
    
    # Import into Maya scene
    cmds.mayaUSDImport(file="/path/to/enemy.usda")
```

#### Blender Integration

```python
# Blender schema integration example
import bpy
from pxr import Usd, UsdGeom, Sdf, Tf

class CreateCarrotEnemyOperator(bpy.types.Operator):
    """Create a carrot enemy with USD schemas"""
    bl_idname = "sparkle.create_carrot_enemy"
    bl_label = "Create Carrot Enemy"
    
    def execute(self, context):
        # Create USD stage
        stage = Usd.Stage.CreateInMemory()
        
        # Define carrot enemy
        carrot = stage.DefinePrim("/Enemy", "SparkleEnemyCarrot")
        
        # Apply API schemas
        carrot.ApplyAPI(Tf.Type.FindByName("SparkleHealthAPI"))
        carrot.ApplyAPI(Tf.Type.FindByName("SparkleCombatAPI"))
        
        # Set properties
        carrot.CreateAttribute("sparkle:health:current", Sdf.ValueTypeNames.Float).Set(50.0)
        carrot.CreateAttribute("sparkle:combat:damage", Sdf.ValueTypeNames.Float).Set(10.0)
        
        # Export to USD file
        stage.Export("/path/to/enemy.usda")
        
        # Import into Blender scene
        bpy.ops.wm.usd_import(filepath="/path/to/enemy.usda")
        
        return {'FINISHED'}

# Register the operator
def register():
    bpy.utils.register_class(CreateCarrotEnemyOperator)

def unregister():
    bpy.utils.unregister_class(CreateCarrotEnemyOperator)
```

### 2. Game Engine Integration

The game engine needs to properly load and interpret these schemas:

#### Custom USD Stage Loading

```cpp
// Game engine stage loading with schema support
UsdStageRefPtr LoadGameLevel(const std::string& levelPath) {
    // Create resolver context with search paths for schemas
    UsdStagePopulationMask mask;
    UsdStageCache stageCache;
    
    ArResolverContext resolverContext = CreateGameResolverContext();
    UsdStageCacheContext stageCacheContext(stageCache);
    
    // Set custom resolver context while loading the stage
    ArResolverContextBinder binder(resolverContext);
    
    // Load the stage with pre-populated mask for game-relevant prims
    UsdStageRefPtr stage = UsdStage::Open(levelPath, mask);
    if (!stage) {
        std::cerr << "Failed to open stage: " << levelPath << std::endl;
        return nullptr;
    }
    
    // Register custom schema types with the engine
    RegisterGameSchemas(stage);
    
    // Validate game-relevant schema properties are available
    if (!ValidateGameSchemas(stage)) {
        std::cerr << "Stage failed schema validation!" << std::endl;
        return nullptr;
    }
    
    return stage;
}
```

#### Schema-Based Entity Factory

```cpp
// Game entity factory using schemas
class GameEntityFactory {
public:
    // Create a game entity from a USD prim
    std::shared_ptr<GameEntity> CreateEntity(const UsdPrim& prim) {
        if (!prim.IsValid()) {
            return nullptr;
        }
        
        // Determine entity type based on prim type
        std::shared_ptr<GameEntity> entity;
        
        if (prim.IsA<TfType::Find<class SparkleEnemyCarrot>>()) {
            entity = std::make_shared<EnemyEntity>();
        }
        else if (prim.IsA<TfType::Find<class SparklePlayer>>()) {
            entity = std::make_shared<PlayerEntity>();
        }
        else if (prim.IsA<TfType::Find<class SparklePickup>>()) {
            entity = std::make_shared<PickupEntity>();
        }
        else {
            // Default game entity for unknown prim types
            entity = std::make_shared<GameEntity>();
        }
        
        // Initialize entity from USD prim
        if (entity) {
            InitializeEntityFromPrim(entity, prim);
        }
        
        return entity;
    }
    
private:
    // Initialize entity properties from USD prim
    void InitializeEntityFromPrim(std::shared_ptr<GameEntity> entity, const UsdPrim& prim) {
        // Set basic properties
        std::string id;
        if (prim.HasAttribute(TfToken("sparkle:entity:id"))) {
            prim.GetAttribute(TfToken("sparkle:entity:id")).Get(&id);
            entity->SetId(id);
        }
        
        // Check for health component
        if (prim.HasAPI<TfType::Find<class SparkleHealthAPI>>()) {
            auto healthComponent = std::make_shared<HealthComponent>();
            
            float current = 100.0f;
            float maximum = 100.0f;
            
            prim.GetAttribute(TfToken("sparkle:health:current")).Get(&current);
            prim.GetAttribute(TfToken("sparkle:health:maximum")).Get(&maximum);
            
            healthComponent->SetCurrentHealth(current);
            healthComponent->SetMaxHealth(maximum);
            
            entity->AddComponent(healthComponent);
        }
        
        // Check for combat component
        if (prim.HasAPI<TfType::Find<class SparkleCombatAPI>>()) {
            auto combatComponent = std::make_shared<CombatComponent>();
            
            float damage = 10.0f;
            prim.GetAttribute(TfToken("sparkle:combat:damage")).Get(&damage);
            combatComponent->SetDamage(damage);
            
            TfToken damageType;
            prim.GetAttribute(TfToken("sparkle:combat:damageType")).Get(&damageType);
            combatComponent->SetDamageType(damageType.GetString());
            
            entity->AddComponent(combatComponent);
        }
        
        // Add other components based on API schemas...
    }
};
```

#### Schema Properties Editor for Game Tools

In-engine tools can use schema reflection to generate editing UIs:

```cpp
// Schema-based property editor
class SchemaPropertyEditor {
public:
    // Initialize with a USD prim
    SchemaPropertyEditor(const UsdPrim& prim) : m_prim(prim) {}
    
    // Generate UI for editing schema properties
    void GenerateUI() {
        if (!m_prim) {
            return;
        }
        
        // Display basic information
        std::string name = m_prim.GetName();
        std::string type = m_prim.GetTypeName().GetString();
        
        ImGui::Text("Entity: %s", name.c_str());
        ImGui::Text("Type: %s", type.c_str());
        ImGui::Separator();
        
        // List schema API instances
        std::vector<std::string> appliedSchemas;
        m_prim.GetAppliedSchemas(&appliedSchemas);
        
        for (const auto& schema : appliedSchemas) {
            if (ImGui::CollapsingHeader(schema.c_str())) {
                // Generate UI for schema properties
                DisplaySchemaProperties(schema);
            }
        }
    }

private:
    UsdPrim m_prim;
    
    // Display editable properties for a specific schema
    void DisplaySchemaProperties(const std::string& schemaName) {
        // Get schema properties based on naming convention
        std::string prefix;
        
        if (schemaName == "SparkleHealthAPI") {
            prefix = "sparkle:health:";
        }
        else if (schemaName == "SparkleCombatAPI") {
            prefix = "sparkle:combat:";
        }
        else if (schemaName.find("SparkleLootAPI:") == 0) {
            std::string instance = schemaName.substr(std::string("SparkleLootAPI:").size());
            prefix = "sparkle:loot:" + instance + ":";
        }
        
        // Find attributes with this prefix
        for (const UsdAttribute& attr : m_prim.GetAttributes()) {
            std::string attrName = attr.GetName();
            if (attrName.find(prefix) == 0) {
                // Display property editor based on type
                DisplayAttributeEditor(attr);
            }
        }
    }
    
    // Display an editor for a specific attribute
    void DisplayAttributeEditor(const UsdAttribute& attr) {
        std::string name = attr.GetName();
        SdfValueTypeName typeName = attr.GetTypeName();
        
        // Extract display name (remove prefix)
        size_t lastColon = name.find_last_of(':');
        std::string displayName = (lastColon != std::string::npos) ? 
                                 name.substr(lastColon + 1) : name;
        
        // Edit based on type
        if (typeName == SdfValueTypeNames->Float) {
            float value;
            attr.Get(&value);
            
            if (ImGui::DragFloat(displayName.c_str(), &value, 0.1f)) {
                attr.Set(value);
            }
        }
        else if (typeName == SdfValueTypeNames->Int) {
            int value;
            attr.Get(&value);
            
            if (ImGui::DragInt(displayName.c_str(), &value, 1)) {
                attr.Set(value);
            }
        }
        else if (typeName == SdfValueTypeNames->String) {
            std::string value;
            attr.Get(&value);
            
            char buffer[256];
            strncpy(buffer, value.c_str(), sizeof(buffer) - 1);
            buffer[sizeof(buffer) - 1] = '\0';
            
            if (ImGui::InputText(displayName.c_str(), buffer, sizeof(buffer))) {
                attr.Set(std::string(buffer));
            }
        }
        else if (typeName == SdfValueTypeNames->Token) {
            TfToken value;
            attr.Get(&value);
            
            // Check if we have allowed tokens
            VtTokenArray allowedTokens;
            if (attr.GetMetadata(SdfFieldKeys->AllowedTokens, &allowedTokens) && 
                !allowedTokens.empty()) {
                
                // Create a dropdown
                int currentIndex = -1;
                for (size_t i = 0; i < allowedTokens.size(); ++i) {
                    if (allowedTokens[i] == value) {
                        currentIndex = i;
                        break;
                    }
                }
                
                if (currentIndex >= 0) {
                    std::string comboLabel = value.GetString();
                    if (ImGui::BeginCombo(displayName.c_str(), comboLabel.c_str())) {
                        for (size_t i = 0; i < allowedTokens.size(); ++i) {
                            bool isSelected = (i == currentIndex);
                            if (ImGui::Selectable(allowedTokens[i].GetString().c_str(), isSelected)) {
                                attr.Set(allowedTokens[i]);
                            }
                            
                            if (isSelected) {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        ImGui::EndCombo();
                    }
                }
            }
            else {
                // No allowed tokens, use a text input
                std::string valueStr = value.GetString();
                char buffer[256];
                strncpy(buffer, valueStr.c_str(), sizeof(buffer) - 1);
                buffer[sizeof(buffer) - 1] = '\0';
                
                if (ImGui::InputText(displayName.c_str(), buffer, sizeof(buffer))) {
                    attr.Set(TfToken(buffer));
                }
            }
        }
        // Handle other types...
    }
};
```

## Versioning Strategies

Schema evolution is inevitable in game development. Here are strategies for managing schema versions:

### 1. Schema Version Metadata

Include version information in your schema definitions:

```usda
over "GLOBAL" (
    customData = {
        string libraryName = "sparkleCarrot"
        string libraryPath = "./"
        string libraryPrefix = "SparkleCarrot"
        bool skipCodeGeneration = true
        
        # Schema versioning information
        dictionary versionInfo = {
            string majorVersion = "1"
            string minorVersion = "2"
            string patchVersion = "5"
            string releaseDate = "2023-04-15"
        }
    }
) {
}
```

### 2. Schema Registry Implementation

Maintain a registry of schema versions and compatibility:

```cpp
// Schema version registry
class SchemaVersionRegistry {
public:
    // Add schema version information
    void RegisterSchemaVersion(const std::string& schemaName, 
                              int majorVersion, 
                              int minorVersion, 
                              int patchVersion) {
        VersionInfo versionInfo;
        versionInfo.majorVersion = majorVersion;
        versionInfo.minorVersion = minorVersion;
        versionInfo.patchVersion = patchVersion;
        
        m_schemaVersions[schemaName] = versionInfo;
    }
    
    // Check compatibility between versions
    bool IsCompatible(const std::string& schemaName, 
                     int requiredMajor, 
                     int requiredMinor, 
                     int requiredPatch) {
        // Find schema version
        auto it = m_schemaVersions.find(schemaName);
        if (it == m_schemaVersions.end()) {
            return false; // Schema not found
        }
        
        const VersionInfo& version = it->second;
        
        // Check compatibility (major version must match, minor and patch must be >= required)
        return (version.majorVersion == requiredMajor) && 
               ((version.minorVersion > requiredMinor) || 
                (version.minorVersion == requiredMinor && version.patchVersion >= requiredPatch));
    }
    
    // Extract version information from a stage
    void ExtractVersionsFromStage(UsdStageRefPtr stage) {
        if (!stage) {
            return;
        }
        
        // Get global customData
        UsdPrim globalPrim = stage->GetPrimAtPath(SdfPath("/GLOBAL"));
        if (!globalPrim.IsValid()) {
            return;
        }
        
        // Extract version info
        VtDictionary customData;
        if (globalPrim.GetMetadata(SdfFieldKeys->CustomData, &customData)) {
            VtValue versionInfoValue = customData.GetValueAtPath("versionInfo");
            if (versionInfoValue.IsHolding<VtDictionary>()) {
                VtDictionary versionInfo = versionInfoValue.Get<VtDictionary>();
                
                // Extract individual version components
                VtValue libraryNameValue = customData.GetValueAtPath("libraryName");
                if (libraryNameValue.IsHolding<std::string>()) {
                    std::string libraryName = libraryNameValue.Get<std::string>();
                    
                    // Extract version numbers
                    int major = 0, minor = 0, patch = 0;
                    ExtractVersionNumber(versionInfo, "majorVersion", major);
                    ExtractVersionNumber(versionInfo, "minorVersion", minor);
                    ExtractVersionNumber(versionInfo, "patchVersion", patch);
                    
                    // Register version
                    RegisterSchemaVersion(libraryName, major, minor, patch);
                }
            }
        }
    }

private:
    struct VersionInfo {
        int majorVersion = 0;
        int minorVersion = 0;
        int patchVersion = 0;
    };
    
    std::unordered_map<std::string, VersionInfo> m_schemaVersions;
    
    // Helper to extract version numbers from dictionary
    void ExtractVersionNumber(const VtDictionary& dict, const std::string& key, int& value) {
        VtValue vtValue = dict.GetValueAtPath(key);
        if (vtValue.IsHolding<std::string>()) {
            std::string strValue = vtValue.Get<std::string>();
            try {
                value = std::stoi(strValue);
            }
            catch (const std::exception&) {
                // Handle conversion error
                value = 0;
            }
        }
        else if (vtValue.IsHolding<int>()) {
            value = vtValue.Get<int>();
        }
    }
};
```

### 3. Semantic Versioning for Schemas

Apply semantic versioning principles to your schemas:

```
1. MAJOR version: breaking changes that require migration
2. MINOR version: new features that don't break existing assets
3. PATCH version: bug fixes and clarifications
```

Example changelog entries:

```
v1.0.0 - Initial version of SparkleCarrot game schemas
v1.1.0 - Added SparkleAIAPI with pathfinding properties (non-breaking)
v1.1.1 - Fixed documentation for health regeneration properties
v2.0.0 - Renamed sparkle:combat:damage to sparkle:combat:baseDamage (breaking)
```

## Migration Strategies

When schemas evolve, you need migration strategies to update existing assets:

### 1. Schema Migration Tools

Create tools to update assets to new schema versions:

```python
# Example schema migration script
import argparse
from pxr import Usd, Sdf

def migrate_v1_to_v2(stage):
    """Migrate assets from schema v1 to v2."""
    # Rename sparkle:combat:damage to sparkle:combat:baseDamage
    for prim in stage.Traverse():
        if prim.HasAttribute("sparkle:combat:damage"):
            # Get old value
            attr = prim.GetAttribute("sparkle:combat:damage")
            oldValue = attr.Get()
            
            # Create new attribute
            newAttr = prim.CreateAttribute("sparkle:combat:baseDamage", 
                                           Sdf.ValueTypeNames.Float, 
                                           False)
            newAttr.Set(oldValue)
            
            # Mark old attribute for removal
            attr.SetCustom(True)
            attr.Clear()
            
            print(f"Migrated damage attribute for {prim.GetPath()}")

def main():
    parser = argparse.ArgumentParser(description="Migrate schema versions")
    parser.add_argument("--input", required=True, help="Input USD file")
    parser.add_argument("--output", required=True, help="Output USD file")
    parser.add_argument("--from-version", required=True, help="Source version")
    parser.add_argument("--to-version", required=True, help="Target version")
    
    args = parser.parse_args()
    
    # Open stage
    stage = Usd.Stage.Open(args.input)
    if not stage:
        print(f"Failed to open {args.input}")
        return 1
    
    # Determine migration path
    if args.from_version == "1.0.0" and args.to_version == "2.0.0":
        migrate_v1_to_v2(stage)
    else:
        print(f"No migration path from {args.from_version} to {args.to_version}")
        return 1
    
    # Save migrated stage
    stage.Export(args.output)
    print(f"Migrated {args.input} to {args.output}")
    
    return 0

if __name__ == "__main__":
    exit(main())
```

### 2. Schema Compatibility Layers

For maintaining compatibility with older schemas:

```cpp
// Schema compatibility layer
class SchemaCompatibilityLayer {
public:
    // Register a compatibility mapping
    void RegisterCompatibilityMapping(const std::string& oldAttributeName,
                                     const std::string& newAttributeName) {
        m_attributeMappings[oldAttributeName] = newAttributeName;
    }
    
    // Get a value with compatibility mapping
    template<typename T>
    bool GetValue(const UsdPrim& prim, const std::string& attrName, T* value) {
        // Try direct access first
        UsdAttribute attr = prim.GetAttribute(TfToken(attrName));
        if (attr && attr.Get(value)) {
            return true;
        }
        
        // Check for compatible attribute name
        auto it = m_attributeMappings.find(attrName);
        if (it != m_attributeMappings.end()) {
            UsdAttribute compatAttr = prim.GetAttribute(TfToken(it->second));
            if (compatAttr && compatAttr.Get(value)) {
                return true;
            }
        }
        
        return false;
    }

private:
    std::unordered_map<std::string, std::string> m_attributeMappings;
};

// Usage example
void InitializeCompatibilityLayer(SchemaCompatibilityLayer& layer) {
    // v1 to v2 mappings
    layer.RegisterCompatibilityMapping("sparkle:combat:damage", "sparkle:combat:baseDamage");
    
    // v2 to v3 mappings
    layer.RegisterCompatibilityMapping("sparkle:ai:behavior", "sparkle:ai:behaviorType");
}

float GetEntityDamage(const UsdPrim& prim, SchemaCompatibilityLayer& compatLayer) {
    float damage = 0.0f;
    
    // This will work with both old and new schema versions
    compatLayer.GetValue(prim, "sparkle:combat:baseDamage", &damage);
    
    return damage;
}
```

### 3. Dual Schema Support

During transition periods, support both old and new schemas:

```cpp
// Example entity component that supports dual schema versions
class CombatComponent : public EntityComponent {
public:
    void InitializeFromPrim(const UsdPrim& prim) override {
        // Try new schema version first
        UsdAttribute damageAttr = prim.GetAttribute(TfToken("sparkle:combat:baseDamage"));
        if (damageAttr) {
            damageAttr.Get(&m_baseDamage);
        }
        else {
            // Fall back to old schema version
            UsdAttribute oldDamageAttr = prim.GetAttribute(TfToken("sparkle:combat:damage"));
            if (oldDamageAttr) {
                oldDamageAttr.Get(&m_baseDamage);
                
                // Log deprecation warning
                std::cerr << "Warning: Using deprecated schema attribute 'sparkle:combat:damage' at "
                         << prim.GetPath() << std::endl;
            }
        }
        
        // Initialize other properties...
    }
    
private:
    float m_baseDamage = 10.0f;
};
```

## Key Takeaways

- Schema distribution should be centralized with clear versioning strategies
- Each DCC tool requires specific integration approaches
- Game engines need robust schema loading and validation
- Semantic versioning helps manage schema evolution
- Migration tools and compatibility layers ensure smooth transitions

In the next chapter, we'll focus on performance optimization strategies when working with codeless schemas in game engines.
