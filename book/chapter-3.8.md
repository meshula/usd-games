## Procedural Generation in Code

While USD files define the structure and parameters of procedural content, we still need code to generate and instantiate the content. Let's explore how to implement procedural generation using USD composition.

### Building Generator Components

When implementing procedural systems, it's often beneficial to structure your code in a modular way that mirrors the composition approach of USD itself. Let's see this in practice with a city environment generator that creates varied environments through layer composition.

First, let's look at a component for generating time-of-day variations:

```cpp
// city_time_generator.cpp
//
// Component of city layer generation system
// Specifically handles time-of-day variations

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/gf/vec3f.h>
#include <string>

// Generate a layer containing city variations for a specific time of day
void GenerateCityTimeOfDayLayer(const std::string& filename, const std::string& timeOfDay) {
    // Create a new layer
    pxr::SdfLayerRefPtr layer = pxr::SdfLayer::CreateNew(filename);
    if (!layer) {
        return;
    }
    
    // Set up the layer structure with city lighting overrides
    layer->SetDefaultPrim(pxr::TfToken("World"));
    
    // Add overrides for the city
    std::string cityPath = "/World/City";
    
    // Lighting settings based on time of day
    if (timeOfDay == "dawn") {
        // Dawn lighting settings
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Lighting/SunLight"))
            .CreateAttribute(pxr::TfToken("sparkle:light:intensity"), pxr::SdfValueTypeNames->Float)
            .Set(20000.0f);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Lighting/SunLight"))
            .CreateAttribute(pxr::TfToken("sparkle:light:color"), pxr::SdfValueTypeNames->Color3f)
            .Set(pxr::GfVec3f(1.0f, 0.8f, 0.6f));
        
        // More dawn-specific settings...
    }
    else if (timeOfDay == "day") {
        // Daytime lighting settings
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Lighting/SunLight"))
            .CreateAttribute(pxr::TfToken("sparkle:light:intensity"), pxr::SdfValueTypeNames->Float)
            .Set(100000.0f);
            
        // More day-specific settings...
    }
    // Additional time of day conditions...
    
    // Save the layer
    layer->Save();
}

// Generate all time-of-day layers
void GenerateAllTimeLayers() {
    GenerateCityTimeOfDayLayer("city_time_dawn.usda", "dawn");
    GenerateCityTimeOfDayLayer("city_time_day.usda", "day");
    GenerateCityTimeOfDayLayer("city_time_dusk.usda", "dusk");
    GenerateCityTimeOfDayLayer("city_time_night.usda", "night");
}
```

Similarly, we can have components for weather conditions and special events:

```cpp
// city_weather_generator.cpp (excerpt)
void GenerateCityWeatherLayer(const std::string& filename, const std::string& weather) {
    // Create a new layer
    pxr::SdfLayerRefPtr layer = pxr::SdfLayer::CreateNew(filename);
    // ...
    
    if (weather == "clear") {
        // Clear weather settings
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Weather"))
            .CreateAttribute(pxr::TfToken("sparkle:weather:type"), pxr::SdfValueTypeNames->Token)
            .Set(pxr::TfToken("clear"));
        
        // More clear weather settings...
    }
    else if (weather == "rainy") {
        // Rainy weather settings
        // ...
    }
    // Additional weather conditions...
}

// city_event_generator.cpp (excerpt)
void GenerateCityEventLayer(const std::string& filename, const std::string& event) {
    // Create a new layer
    pxr::SdfLayerRefPtr layer = pxr::SdfLayer::CreateNew(filename);
    // ...
    
    if (event == "harvest_festival") {
        // Festival-specific decorations and activities
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Decorations/HarvestDecor"))
            .CreateAttribute(pxr::TfToken("sparkle:decor:active"), pxr::SdfValueTypeNames->Bool)
            .Set(true);
        
        // More festival settings...
    }
    // Additional event types...
}
```

### Coordinating Layer Composition

The real power of this approach comes from a layer manager that coordinates the different aspects of the environment:

```cpp
// city_layer_manager.cpp (excerpt)
// Apply the appropriate layers to a city based on current game state
void UpdateCityLayers(pxr::UsdStage* stage, const std::string& timeOfDay, 
                     const std::string& weather, const std::string& event = "") {
    // Get root layer
    pxr::SdfLayerRefPtr rootLayer = stage->GetRootLayer();
    pxr::SdfSubLayerVector sublayers = rootLayer->GetSubLayerPaths();
    
    // Remove any existing time/weather/event layers
    for (auto it = sublayers.begin(); it != sublayers.end();) {
        if (it->find("city_time_") != std::string::npos ||
            it->find("city_weather_") != std::string::npos ||
            it->find("city_event_") != std::string::npos) {
            it = sublayers.erase(it);
        } else {
            ++it;
        }
    }
    
    // Add the appropriate time of day layer
    std::string timeLayer = "city_time_" + timeOfDay + ".usda";
    sublayers.push_back(timeLayer);
    
    // Add the appropriate weather layer
    std::string weatherLayer = "city_weather_" + weather + ".usda";
    sublayers.push_back(weatherLayer);
    
    // Add event layer if one is active
    if (!event.empty()) {
        std::string eventLayer = "city_event_" + event + ".usda";
        sublayers.push_back(eventLayer);
    }
    
    // Update the layer stack
    rootLayer->SetSubLayerPaths(sublayers);
}
```

This modular approach to procedural generation brings several advantages:

1. **Clear separation of concerns** - Each generator deals with one aspect of the environment
2. **Independent iteration** - New time of day or weather conditions can be added without affecting other systems
3. **Runtime flexibility** - Different combinations of layers can be applied dynamically
4. **Easier debugging** - Issues can be isolated to specific layer types
5. **Collaborative development** - Team members can work on different generators independently

### Game Simulation with Dynamic Layer Composition

Let's see how this all comes together in a game simulation loop:

```cpp
// Game loop example from city_layer_manager.cpp
void GameLoop() {
    // Create or open a stage
    pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateNew("city_scene.usda");
    
    // Set up the basic city structure
    SetupCityStage(stage);
    
    // Generate all layer variations
    GenerateAllCityLayers();
    
    // Game time and state tracking
    float gameTimeHours = 12.0f; // Start at noon
    std::string currentWeather = "clear";
    std::string currentEvent = "";
    
    // Example of updating city over time
    for (int day = 1; day <= 3; day++) {
        for (int hourIncrement = 0; hourIncrement < 24; hourIncrement++) {
            // Update game time (1 real second = 1 game hour for this example)
            gameTimeHours = (gameTimeHours + 1.0f);
            if (gameTimeHours >= 24.0f) {
                gameTimeHours = 0.0f;
            }
            
            // Determine time of day
            std::string timeOfDay;
            if (gameTimeHours >= 5.0f && gameTimeHours < 8.0f) {
                timeOfDay = "dawn";
            }
            else if (gameTimeHours >= 8.0f && gameTimeHours < 18.0f) {
                timeOfDay = "day";
            }
            else if (gameTimeHours >= 18.0f && gameTimeHours < 21.0f) {
                timeOfDay = "dusk";
            }
            else {
                timeOfDay = "night";
            }
            
            // Change weather occasionally (simplified)
            if (hourIncrement % 8 == 0) {
                int weatherRoll = hourIncrement % 4;
                switch (weatherRoll) {
                    case 0: currentWeather = "clear"; break;
                    case 1: currentWeather = "cloudy"; break;
                    case 2: currentWeather = "rainy"; break;
                    case 3: currentWeather = "snowy"; break;
                }
            }
            
            // Special events on specific days
            if (day == 2 && gameTimeHours >= 10.0f && gameTimeHours < 22.0f) {
                currentEvent = "harvest_festival";
            }
            else if (day == 3 && gameTimeHours >= 12.0f && gameTimeHours < 23.0f) {
                currentEvent = "victory_day";
            }
            else {
                currentEvent = "";
            }
            
            // Update the city with the current conditions
            UpdateCityLayers(stage, timeOfDay, currentWeather, currentEvent);
            
            // In a real game, you would render or process the scene here
            // For this example, we just output the current state
            std::cout << "Day " << day << ", Time: " << gameTimeHours << "h, Weather: " 
                      << currentWeather << ", Event: " << (currentEvent.empty() ? "none" : currentEvent) << std::endl;
            
            // Save snapshots at interesting times
            if (hourIncrement == 0 || hourIncrement == 6 || hourIncrement == 12 || hourIncrement == 18) {
                std::string filename = "city_day" + std::to_string(day) + "_hour" + 
                                      std::to_string(static_cast<int>(gameTimeHours)) + ".usda";
                stage->GetRootLayer()->Export(filename);
            }
        }
    }
}
```

This game loop demonstrates how USD's composition system enables dynamic environments that evolve over time. By swapping layers in and out based on game state, we can create rich, varied worlds without duplicating content or writing complex conditional logic.

### Procedural Building Generator Example

For more specific object generation, here's an example of a procedural building generator that leverages USD composition to create individual buildings:

```cpp
// Generate a building at a specific position with given parameters
pxr::UsdPrim ProceduralBuildingGenerator::GenerateBuilding(
    pxr::UsdStage* stage, const pxr::GfVec3f& position, const std::string& buildingType) 
{
    // Create a unique name for the building
    static int buildingCount = 0;
    std::string buildingName = "Building_" + buildingType + "_" + std::to_string(++buildingCount);
    
    // Create the building prim
    pxr::SdfPath buildingPath = pxr::SdfPath("/World/Buildings").AppendChild(
        pxr::TfToken(buildingName));
    pxr::UsdPrim buildingPrim = stage->DefinePrim(buildingPath);
    
    // Add a reference to the building template
    buildingPrim.GetReferences().AddReference(
        "procedural_building.usda", pxr::SdfPath("/ProceduralBuilding"));
    
    // Set transform
    pxr::UsdGeomXformable xformable(buildingPrim);
    xformable.AddTranslateOp().Set(position);
    
    // Set the building type variant
    pxr::UsdVariantSet buildingTypeVariant = buildingPrim.GetVariantSet("buildingType");
    if (buildingTypeVariant && buildingTypeVariant.HasVariant(buildingType)) {
        buildingTypeVariant.SetVariantSelection(buildingType);
    }
    
    // Randomly select a material theme
    std::vector<std::string> materialThemes = {"stone", "wood", "brick"};
    std::uniform_int_distribution<> materialDist(0, materialThemes.size() - 1);
    std::string materialTheme = materialThemes[materialDist(m_rng)];
    
    pxr::UsdVariantSet materialVariant = buildingPrim.GetVariantSet("materialTheme");
    if (materialVariant && materialVariant.HasVariant(materialTheme)) {
        materialVariant.SetVariantSelection(materialTheme);
    }
    
    // Apply some random variation to the building dimensions
    float widthVariation = std::uniform_real_distribution<float>(0.8f, 1.2f)(m_rng);
    float heightVariation = std::uniform_real_distribution<float>(0.9f, 1.3f)(m_rng);
    
    pxr::UsdAttribute widthAttr = buildingPrim.GetAttribute(pxr::TfToken("sparkle:building:width"));
    pxr::UsdAttribute heightAttr = buildingPrim.GetAttribute(pxr::TfToken("sparkle:building:height"));
    
    if (widthAttr) {
        float baseWidth = 0.0f;
        widthAttr.Get(&baseWidth);
        widthAttr.Set(baseWidth * widthVariation);
    }
    
    if (heightAttr) {
        float baseHeight = 0.0f;
        heightAttr.Get(&baseHeight);
        heightAttr.Set(baseHeight * heightVariation);
    }
    
    return buildingPrim;
}
```

### Procedural City Block Generation

We can build on our building generator to create entire city blocks:

```cpp
// Generate a city block with multiple buildings
pxr::UsdPrim ProceduralCityGenerator::GenerateCityBlock(
    pxr::UsdStage* stage, const pxr::GfVec3f& position, const std::string& blockType)
{
    // Create a block prim
    static int blockCount = 0;
    std::string blockName = "Block_" + blockType + "_" + std::to_string(++blockCount);
    pxr::SdfPath blockPath = pxr::SdfPath("/World/Blocks").AppendChild(pxr::TfToken(blockName));
    pxr::UsdPrim blockPrim = stage->DefinePrim(blockPath);
    
    // Apply position
    pxr::UsdGeomXformable xformable(blockPrim);
    xformable.AddTranslateOp().Set(position);
    
    // Reference the block template and set variant
    blockPrim.GetReferences().AddReference(
        "procedural_city_block.usda", pxr::SdfPath("/CityBlock"));
    
    pxr::UsdVariantSet neighborhoodVariant = blockPrim.GetVariantSet("neighborhoodType");
    if (neighborhoodVariant && neighborhoodVariant.HasVariant(blockType)) {
        neighborhoodVariant.SetVariantSelection(blockType);
    }
    
    // Determine building distribution based on block type
    int buildingCount;
    std::map<std::string, float> buildingTypeWeights;
    
    if (blockType == "residential") {
        buildingCount = std::uniform_int_distribution<>(6, 12)(m_rng);
        buildingTypeWeights = {{"house", 0.7f}, {"shop", 0.2f}, {"apartment", 0.1f}};
    }
    else if (blockType == "commercial") {
        buildingCount = std::uniform_int_distribution<>(4, 8)(m_rng);
        buildingTypeWeights = {{"shop", 0.6f}, {"office", 0.3f}, {"apartment", 0.1f}};
    }
    else if (blockType == "industrial") {
        buildingCount = std::uniform_int_distribution<>(3, 6)(m_rng);
        buildingTypeWeights = {{"factory", 0.5f}, {"warehouse", 0.4f}, {"office", 0.1f}};
    }
    else {
        buildingCount = 5;
        buildingTypeWeights = {{"house", 0.5f}, {"shop", 0.5f}};
    }
    
    // Place buildings around the block perimeter
    float blockWidth = 120.0f;
    float blockDepth = 120.0f;
    
    // Get block dimensions if defined
    pxr::UsdAttribute widthAttr = blockPrim.GetAttribute(pxr::TfToken("sparkle:cityBlock:width"));
    pxr::UsdAttribute depthAttr = blockPrim.GetAttribute(pxr::TfToken("sparkle:cityBlock:depth"));
    
    if (widthAttr) widthAttr.Get(&blockWidth);
    if (depthAttr) depthAttr.Get(&blockDepth);
    
    // Generate buildings
    for (int i = 0; i < buildingCount; ++i) {
        // Determine building position around perimeter
        float perimeterPos = static_cast<float>(i) / buildingCount;
        float angle = perimeterPos * 2.0f * M_PI;
        
        float buildingOffset = 0.8f;  // How far from center to edge
        pxr::GfVec3f buildingPos(
            position[0] + std::cos(angle) * blockWidth * buildingOffset * 0.5f,
            position[1],
            position[2] + std::sin(angle) * blockDepth * buildingOffset * 0.5f
        );
        
        // Determine building type based on weights
        float typeRoll = std::uniform_real_distribution<float>(0.0f, 1.0f)(m_rng);
        float cumulative = 0.0f;
        std::string buildingType = "house";  // default
        
        for (const auto& typeWeight : buildingTypeWeights) {
            cumulative += typeWeight.second;
            if (typeRoll <= cumulative) {
                buildingType = typeWeight.first;
                break;
            }
        }
        
        // Generate and place the building
        pxr::UsdPrim building = m_buildingGenerator.GenerateBuilding(
            stage, buildingPos, buildingType);
        
        // Make it face outward from center
        pxr::UsdGeomXformable buildingXform(building);
        float rotationAngle = angle * (180.0f / M_PI) + 180.0f;  // Face outward
        buildingXform.AddRotateXYZOp().Set(pxr::GfVec3f(0.0f, rotationAngle, 0.0f));
    }
    
    return blockPrim;
}
```

### Integrating Procedural Generation with Game Systems

In a complete game environment, procedural generation isn't a one-time process but an ongoing part of the game loop. Here's how a procedural system might integrate with core game systems:

```cpp
// City Manager class that handles both procedural generation and runtime updates
class CityManager {
public:
    CityManager(pxr::UsdStage* stage) : m_stage(stage), m_cityGenerator(stage) {
        // Initialize city systems
        SetupCityStructure();
    }
    
    // Initial city generation
    void GenerateCity(int size, const std::string& cityType) {
        // Clear any existing city data
        pxr::UsdPrim cityPrim = m_stage->GetPrimAtPath(pxr::SdfPath("/World/City"));
        if (cityPrim) {
            m_stage->RemovePrim(cityPrim.GetPath());
        }
        
        // Create city root
        cityPrim = m_stage->DefinePrim(pxr::SdfPath("/World/City"));
        
        // Generate the base terrain
        m_terrainGenerator.GenerateTerrain(m_stage, size);
        
        // Generate block grid
        int blocksPerSide = std::max(1, size / 3);
        float blockSpacing = 150.0f;
        
        int blockCount = 0;
        for (int x = 0; x < blocksPerSide; ++x) {
            for (int z = 0; z < blocksPerSide; ++z) {
                // Determine district type based on position
                float distanceFromCenter = std::sqrt(
                    std::pow(x - blocksPerSide/2.0f, 2) + 
                    std::pow(z - blocksPerSide/2.0f, 2)
                );
                
                std::string blockType;
                if (distanceFromCenter < blocksPerSide * 0.2f) {
                    blockType = "commercial"; // Center is commercial
                }
                else if (distanceFromCenter < blocksPerSide * 0.6f) {
                    blockType = "residential"; // Middle ring is residential
                }
                else {
                    blockType = "industrial"; // Outer ring is industrial
                }
                
                // Place the block
                pxr::GfVec3f blockPos(
                    (x - blocksPerSide/2.0f) * blockSpacing,
                    0.0f,
                    (z - blocksPerSide/2.0f) * blockSpacing
                );
                
                m_cityGenerator.GenerateCityBlock(m_stage, blockPos, blockType);
                blockCount++;
            }
        }
        
        // Generate all environment layers
        GenerateAllCityLayers();
        
        std::cout << "Generated city with " << blockCount << " blocks." << std::endl;
    }
    
    // Update city based on game state
    void Update(float gameTime, const GameState& state) {
        // Determine time of day
        float gameHours = fmod(gameTime, 24.0f);
        std::string timeOfDay = DetermineTimeOfDay(gameHours);
        
        // Update weather system
        m_weatherSystem.Update(gameTime, state.GetLocation());
        std::string currentWeather = m_weatherSystem.GetCurrentWeatherType();
        
        // Check for active events
        std::string currentEvent = state.GetActiveEvent();
        
        // Update city layers
        UpdateCityLayers(m_stage, timeOfDay, currentWeather, currentEvent);
        
        // Update dynamic elements (NPCs, traffic, etc.)
        UpdateDynamicElements(gameTime, timeOfDay);
    }
    
private:
    // Helpers for city maintenance
    std::string DetermineTimeOfDay(float gameHours) {
        if (gameHours >= 5.0f && gameHours < 8.0f) return "dawn";
        if (gameHours >= 8.0f && gameHours < 18.0f) return "day";
        if (gameHours >= 18.0f && gameHours < 21.0f) return "dusk";
        return "night";
    }
    
    void UpdateDynamicElements(float gameTime, const std::string& timeOfDay) {
        // Update NPC density and behaviors based on time of day
        pxr::UsdPrim npcSystem = m_stage->GetPrimAtPath(pxr::SdfPath("/World/City/NPCs"));
        if (npcSystem) {
            // Adjust NPC activity patterns
            float density = 1.0f; // Default
            
            if (timeOfDay == "night") density = 0.4f;
            else if (timeOfDay == "dawn") density = 0.3f;
            else if (timeOfDay == "day") density = 1.0f;
            else if (timeOfDay == "dusk") density = 0.8f;
            
            pxr::UsdAttribute densityAttr = npcSystem.GetAttribute(
                pxr::TfToken("sparkle:npc:density"));
            if (densityAttr) {
                densityAttr.Set(density);
            }
            
            // In a real game, we'd update NPC spawning here
        }
    }
    
    // Member variables
    pxr::UsdStage* m_stage;
    ProceduralCityGenerator m_cityGenerator;
    TerrainGenerator m_terrainGenerator;
    WeatherSystem m_weatherSystem;
};
```

## Balancing Procedural and Authored Content

While USD's composition system offers powerful tools for procedural generation, the most effective approach often combines procedural and hand-authored content. Here are some strategies for balancing these approaches:

### 1. Procedural Templates with Artistic Overrides

Create procedural templates as the base, but allow artists to make targeted overrides for specific instances:

```usda
def "ImportantBuilding" (
    references = </ProceduralBuilding>
)
{
    # Artist-defined position and orientation
    float3 xformOp:translate = (120, 0, 45)
    float3 xformOp:rotateXYZ = (0, 45, 0)
    uniform token[] xformOpOrder = ["xformOp:translate", "xformOp:rotateXYZ"]
    
    # Override procedural parameters
    float sparkle:building:height = 30.0  # Make this building taller
    
    # Custom material override
    over "Materials" {
        over "PrimaryMaterial" {
            token sparkle:material:variation = "marble"
            float3 sparkle:material:color = (0.9, 0.8, 0.7)
        }
    }
    
    # Add unique elements not in the procedural template
    def "CustomFeatures" {
        def "Flags" {
            # Unique decorative elements for this building
        }
    }
}
```

### 2. Layer-Based Artistic Direction

Use layers to apply artistic direction to procedurally generated content:

```usda
# In artist_adjustments.usda
over "ProcedurallyGeneratedCity" {
    # Artistic adjustments to specific buildings
    over "Buildings" {
        over "Building_house_12" {
            # Adjust this specific building
            float3 xformOp:translate = (102, 0, 87)  # nudged position
            float sparkle:building:width = 15.0  # specific width
        }
        
        over "Building_shop_3" {
            # Make this shop stand out
            over "Materials" {
                over "PrimaryMaterial" {
                    float3 sparkle:material:color = (0.8, 0.2, 0.2)  # red building
                }
            }
        }
    }
    
    # Add completely hand-authored elements in strategic locations
    def "LandmarkStatue" {
        float3 xformOp:translate = (0, 0, 0)  # city center
        # ... detailed statue definition ...
    }
}
```

### 3. Procedural Variations of Authored Components

Start with hand-authored base components, then use procedural variation for diversity:

```cpp
// Take an artist-created template and procedurally create variations
void CreateBuildingVariations(pxr::UsdStage* stage, const pxr::SdfPath& templatePath, int variations) {
    // Get the template prim
    pxr::UsdPrim templatePrim = stage->GetPrimAtPath(templatePath);
    if (!templatePrim) return;
    
    // Create multiple variations based on the template
    for (int i = 0; i < variations; ++i) {
        // Create a new prim with reference to the template
        std::string variationName = templatePrim.GetName() + "_var" + std::to_string(i+1);
        pxr::SdfPath variationPath = templatePath.GetParentPath().AppendChild(pxr::TfToken(variationName));
        
        pxr::UsdPrim variationPrim = stage->DefinePrim(variationPath);
        variationPrim.GetReferences().AddInternalReference(templatePath);
        
        // Apply procedural variations
        float scaleVariation = std::uniform_real_distribution<float>(0.9f, 1.1f)(m_rng);
        pxr::UsdGeomXformable xformable(variationPrim);
        
        // Add scaling
        pxr::GfVec3f scale(scaleVariation, scaleVariation, scaleVariation);
        xformable.AddScaleOp().Set(scale);
        
        // Vary color or material
        pxr::UsdAttribute colorAttr = variationPrim.GetAttribute(pxr::TfToken("sparkle:material:color"));
        if (colorAttr) {
            pxr::GfVec3f baseColor;
            colorAttr.Get(&baseColor);
            
            // Subtle color variation
            pxr::GfVec3f newColor(
                baseColor[0] * std::uniform_real_distribution<float>(0.9f, 1.1f)(m_rng),
                baseColor[1] * std::uniform_real_distribution<float>(0.9f, 1.1f)(m_rng),
                baseColor[2] * std::uniform_real_distribution<float>(0.9f, 1.1f)(m_rng)
            );
            
            colorAttr.Set(newColor);
        }
    }
}
```

### 4. Landmark-Driven Layout

Use hand-placed landmarks to guide procedural generation:

```cpp
// Generate a city district around a landmark
void GenerateDistrictAroundLandmark(pxr::UsdStage* stage, const pxr::SdfPath& landmarkPath) {
    // Get the landmark prim
    pxr::UsdPrim landmarkPrim = stage->GetPrimAtPath(landmarkPath);
    if (!landmarkPrim) return;
    
    // Get landmark position
    pxr::GfVec3f landmarkPos(0, 0, 0);
    pxr::UsdGeomXformable landmarkXform(landmarkPrim);
    pxr::GfMatrix4d landmarkMatrix;
    bool resetsXformStack;
    if (landmarkXform.GetLocalTransformation(&landmarkMatrix, &resetsXformStack)) {
        landmarkPos = pxr::GfVec3f(landmarkMatrix.ExtractTranslation());
    }
    
    // Get landmark type/importance
    std::string landmarkType = "default";
    float importance = 1.0f;
    
    pxr::UsdAttribute typeAttr = landmarkPrim.GetAttribute(pxr::TfToken("sparkle:landmark:type"));
    pxr::UsdAttribute importanceAttr = landmarkPrim.GetAttribute(pxr::TfToken("sparkle:landmark:importance"));
    
    if (typeAttr) typeAttr.Get(&landmarkType);
    if (importanceAttr) importanceAttr.Get(&importance);
    
    // Determine district properties based on landmark
    std::string districtType;
    float radius;
    int buildingDensity;
    
    if (landmarkType == "government") {
        districtType = "civic";
        radius = 200.0f * importance;
        buildingDensity = 8;
    }
    else if (landmarkType == "religious") {
        districtType = "traditional";
        radius = 150.0f * importance;
        buildingDensity = 12;
    }
    else if (landmarkType == "commercial") {
        districtType = "market";
        radius = 120.0f * importance;
        buildingDensity = 15;
    }
    else {
        districtType = "residential";
        radius = 100.0f * importance;
        buildingDensity = 10;
    }
    
    // Generate district
    std::cout << "Generating " << districtType << " district around " << landmarkPath << std::endl;
    
    // Place buildings radiating from landmark
    for (int i = 0; i < buildingDensity; ++i) {
        // Determine position
        float angle = (float)i / buildingDensity * 2.0f * M_PI;
        float distance = std::uniform_real_distribution<float>(radius * 0.3f, radius * 0.9f)(m_rng);
        
        pxr::GfVec3f buildingPos(
            landmarkPos[0] + cos(angle) * distance,
            landmarkPos[1],
            landmarkPos[2] + sin(angle) * distance
        );
        
        // Determine building type based on district
        std::string buildingType;
        if (districtType == "civic") {
            buildingType = (rand() % 100 < 70) ? "office" : "house";
        }
        else if (districtType == "traditional") {
            buildingType = (rand() % 100 < 80) ? "house" : "shop";
        }
        else if (districtType == "market") {
            buildingType = (rand() % 100 < 75) ? "shop" : "house";
        }
        else {
            buildingType = (rand() % 100 < 85) ? "house" : "shop";
        }
        
        // Generate building
        GenerateBuilding(stage, buildingPos, buildingType);
        
        // Make buildings face the landmark
        pxr::SdfPath buildingPath = stage->GetPrimAtPath(buildingPath);
        if (buildingPath) {
            pxr::UsdGeomXformable buildingXform(buildingPath);
            
            // Calculate angle to face landmark
            float faceAngle = atan2(landmarkPos[2] - buildingPos[2], 
                                   landmarkPos[0] - buildingPos[0]);
            faceAngle = faceAngle * 180.0f / M_PI; // Convert to degrees
            
            buildingXform.AddRotateXYZOp().Set(pxr::GfVec3f(0, faceAngle, 0));
        }
    }
}
```

## Optimizing Procedural Systems

Procedural generation systems can become performance-intensive, especially for large worlds. Here are some optimization strategies when working with USD composition:

### 1. Hierarchical LOD Generation

Generate content with multiple levels of detail from the start:

```cpp
// Generate terrain with built-in LOD levels
void GenerateTerrainWithLOD(pxr::UsdStage* stage, const pxr::SdfPath& terrainPath) {
    // Create base terrain prim
    pxr::UsdPrim terrainPrim = stage->DefinePrim(terrainPath);
    
    // Generate different LOD meshes
    for (int lodLevel = 0; lodLevel < 4; ++lodLevel) {
        std::string lodName = "LOD" + std::to_string(lodLevel);
        pxr::SdfPath lodPath = terrainPath.AppendChild(pxr::TfToken(lodName));
        
        // Create LOD prim
        pxr::UsdPrim lodPrim = stage->DefinePrim(lodPath);
        
        // Different vertex counts based on LOD
        int resolution = 1024 >> lodLevel; // 1024, 512, 256, 128
        
        // Generate height data
        std::vector<float> heightData = GenerateHeightfield(resolution, resolution);
        
        // Create mesh for this LOD
        pxr::UsdGeomMesh mesh = pxr::UsdGeomMesh::Define(stage, lodPath.AppendChild(pxr::TfToken("Mesh")));
        
        // Set up points, faces, etc. with appropriate detail level
        std::vector<pxr::GfVec3f> points;
        std::vector<int> faceVertexCounts;
        std::vector<int> faceVertexIndices;
        
        // Fill mesh data from heightData...
        
        // Set mesh data
        mesh.GetPointsAttr().Set(pxr::VtArray<pxr::GfVec3f>(points));
        mesh.GetFaceVertexCountsAttr().Set(pxr::VtArray<int>(faceVertexCounts));
        mesh.GetFaceVertexIndicesAttr().Set(pxr::VtArray<int>(faceVertexIndices));
        
        // Configure LOD visibility
        pxr::UsdAttribute visibilityAttr = lodPrim.CreateAttribute(
            pxr::TfToken("visibility"), pxr::SdfValueTypeNames->Token);
        if (lodLevel > 0) {
            // Only LOD0 is initially visible
            visibilityAttr.Set(pxr::TfToken("invisible"));
        }
    }
}
```

### 2. Instance-Based Generation

Use instanceable prims for repeated elements:

```cpp
// Generate a forest using instancing
void GenerateForest(pxr::UsdStage* stage, const pxr::GfVec3f& center, float radius, int density) {
    // Create parent forest prim
    pxr::SdfPath forestPath = pxr::SdfPath("/World/Forest");
    pxr::UsdPrim forestPrim = stage->DefinePrim(forestPath);
    
    // Create a few tree prototypes
    std::vector<pxr::SdfPath> treePrototypes;
    for (int i = 0; i < 5; ++i) {
        std::string prototypeName = "TreePrototype_" + std::to_string(i);
        pxr::SdfPath prototypePath = forestPath.AppendChild(pxr::TfToken(prototypeName));
        
        // Create tree prototype
        pxr::UsdPrim prototypePrim = stage->DefinePrim(prototypePath);
        
        // Add reference to tree asset
        std::string treeAsset = "tree_" + std::to_string(i+1) + ".usda";
        prototypePrim.GetReferences().AddReference(treeAsset);
        
        treePrototypes.push_back(prototypePath);
    }
    
    // Create instance groups for each prototype
    for (const pxr::SdfPath& prototypePath : treePrototypes) {
        std::string groupName = "TreeInstances_" + prototypePath.GetName();
        pxr::SdfPath instancesPath = forestPath.AppendChild(pxr::TfToken(groupName));
        
        // Create instances group
        pxr::UsdPrim instancesPrim = stage->DefinePrim(instancesPath);
        instancesPrim.SetInstanceable(true);
        
        // Generate instances
        int instanceCount = density / treePrototypes.size();
        for (int i = 0; i < instanceCount; ++i) {
            std::string instanceName = "Instance_" + std::to_string(i);
            pxr::SdfPath instancePath = instancesPath.AppendChild(pxr::TfToken(instanceName));
            
            // Create instance
            pxr::UsdPrim instancePrim = stage->DefinePrim(instancePath);
            
            // Reference the prototype
            instancePrim.GetReferences().AddInternalReference(prototypePath);
            
            // Random position within radius
            float angle = std::uniform_real_distribution<float>(0.0f, 2.0f * M_PI)(m_rng);
            float distance = std::uniform_real_distribution<float>(0.0f, radius)(m_rng);
            
            pxr::GfVec3f position(
                center[0] + cos(angle) * distance,
                center[1],
                center[2] + sin(angle) * distance
            );
            
            // Apply random rotation and scale
            float rotation = std::uniform_real_distribution<float>(0.0f, 360.0f)(m_rng);
            float scale = std::uniform_real_distribution<float>(0.8f, 1.2f)(m_rng);
            
            pxr::UsdGeomXformable xformable(instancePrim);
            xformable.AddTranslateOp().Set(position);
            xformable.AddRotateXYZOp().Set(pxr::GfVec3f(0.0f, rotation, 0.0f));
            xformable.AddScaleOp().Set(pxr::GfVec3f(scale, scale, scale));
        }
    }
}
```

### 3. On-Demand Generation

Generate content only as needed based on player proximity:

```cpp
// World streaming manager that generates content on demand
class WorldStreamingManager {
public:
    WorldStreamingManager(pxr::UsdStage* stage) 
      : m_stage(stage), m_cityGenerator(stage), m_terrainGenerator(stage) {
        // Initialize
    }
    
    // Update based on player position
    void Update(const pxr::GfVec3f& playerPosition) {
        // Check each region
        for (auto& region : m_regions) {
            float distance = (region.center - playerPosition).GetLength();
            
            // If player is close enough and region isn't generated
            if (distance < region.streamingDistance && !region.isGenerated) {
                // Generate region content
                GenerateRegion(region);
                region.isGenerated = true;
            }
            // If player is far enough away and region should be unloaded
            else if (distance > region.unloadDistance && region.isGenerated && region.canUnload) {
                // Unload region
                UnloadRegion(region);
                region.isGenerated = false;
            }
        }
    }
    
    // Add a streaming region
    void AddRegion(const std::string& name, const pxr::GfVec3f& center,
                  float streamDist, float unloadDist, const std::string& type) {
        StreamingRegion region;
        region.name = name;
        region.center = center;
        region.streamingDistance = streamDist;
        region.unloadDistance = unloadDist;
        region.type = type;
        region.isGenerated = false;
        region.canUnload = true;
        
        m_regions.push_back(region);
    }
    
private:
    // Generate content for a region
    void GenerateRegion(StreamingRegion& region) {
        std::cout << "Generating region: " << region.name << std::endl;
        
        // Create region prim if it doesn't exist
        pxr::SdfPath regionPath = pxr::SdfPath("/World/Regions").AppendChild(
            pxr::TfToken(region.name));
        
        pxr::UsdPrim regionPrim = m_stage->GetPrimAtPath(regionPath);
        if (!regionPrim) {
            regionPrim = m_stage->DefinePrim(regionPath);
            
            // Set position
            pxr::UsdGeomXformable xformable(regionPrim);
            xformable.AddTranslateOp().Set(region.center);
        }
        
        // Generate content based on region type
        if (region.type == "city") {
            m_cityGenerator.GenerateCity(regionPrim.GetPath(), 3, "mixed");
        }
        else if (region.type == "forest") {
            GenerateForest(m_stage, region.center, 1000.0f, 500);
        }
        else if (region.type == "mountain") {
            m_terrainGenerator.GenerateMountainRegion(regionPrim.GetPath());
        }
        // Additional region types...
    }
    
    // Unload a region
    void UnloadRegion(StreamingRegion& region) {
        std::cout << "Unloading region: " << region.name << std::endl;
        
        pxr::SdfPath regionPath = pxr::SdfPath("/World/Regions").AppendChild(
            pxr::TfToken(region.name));
        
        // Option 1: Remove the prim entirely
        m_stage->RemovePrim(regionPath);
        
        // Option 2: Just deactivate the prim (faster if we'll load it again)
        // pxr::UsdPrim regionPrim = m_stage->GetPrimAtPath(regionPath);
        // if (regionPrim) {
        //     regionPrim.SetActive(false);
        // }
    }
    
    // Region tracking
    struct StreamingRegion {
        std::string name;
        pxr::GfVec3f center;
        float streamingDistance;
        float unloadDistance;
        std::string type;
        bool isGenerated;
        bool canUnload;
    };
    
    std::vector<StreamingRegion> m_regions;
    pxr::UsdStage* m_stage;
    ProceduralCityGenerator m_cityGenerator;
    TerrainGenerator m_terrainGenerator;
};
```

### 4. Background Processing

For complex generation, use background threads to generate content without blocking the main game loop:

```cpp
// Background generation system
class BackgroundContentGenerator {
public:
    BackgroundContentGenerator() : m_isRunning(false) {}
    
    // Start the background thread
    void Start() {
        m_isRunning = true;
        m_thread = std::thread(&BackgroundContentGenerator::ThreadMain, this);
    }
    
    // Stop the background thread
    void Stop() {
        m_isRunning = false;
        m_condVar.notify_one();
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }
    
    // Queue a generation task
    void QueueTask(const GenerationTask& task) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_taskQueue.push(task);
        }
        m_condVar.notify_one();
    }
    
    // Check if all tasks are done
    bool IsIdle() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_taskQueue.empty() && !m_currentTask.has_value();
    }
    
    // Get the current progress
    float GetProgress() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_currentTask.has_value()) {
            return m_currentProgress;
        }
        return 0.0f;
    }
    
private:
    // Generation task definition
    struct GenerationTask {
        std::string type;
        pxr::SdfPath targetPath;
        std::string outputFile;
        std::map<std::string, std::string> parameters;
    };
    
    // Background thread main function
    void ThreadMain() {
        while (m_isRunning) {
            GenerationTask task;
            
            // Wait for a task or shutdown
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_condVar.wait(lock, [this] { 
                    return !m_taskQueue.empty() || !m_isRunning; 
                });
                
                if (!m_isRunning) break;
                
                task = m_taskQueue.front();
                m_taskQueue.pop();
                m_currentTask = task;
                m_currentProgress = 0.0f;
            }
            
            // Process the task
            try {
                // Create a temporary stage
                pxr::UsdStageRefPtr tempStage = pxr::UsdStage::CreateNew(task.outputFile);
                
                if (task.type == "city") {
                    GenerateCity(tempStage, task.targetPath, task.parameters);
                }
                else if (task.type == "terrain") {
                    GenerateTerrain(tempStage, task.targetPath, task.parameters);
                }
                // Additional task types...
                
                // Save the stage
                tempStage->Save();
                
                // Mark task as completed
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_completedTasks.push_back(task);
                    m_currentTask.reset();
                }
            }
            catch (const std::exception& e) {
                std::cerr << "Error generating content: " << e.what() << std::endl;
                
                // Mark task as failed
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_failedTasks.push_back(task);
                    m_currentTask.reset();
                }
            }
        }
    }
    
    // Progress reporting from generation methods
    void UpdateProgress(float progress) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_currentProgress = progress;
    }
    
    // Generation methods
    void GenerateCity(pxr::UsdStageRefPtr stage, const pxr::SdfPath& targetPath,
                     const std::map<std::string, std::string>& params) {
        // City generation code...
        
        // Update progress periodically
        for (int i = 0; i < 100; ++i) {
            // Do some work...
            
            UpdateProgress(i / 100.0f);
            std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Simulated work
        }
    }
    
    void GenerateTerrain(pxr::UsdStageRefPtr stage, const pxr::SdfPath& targetPath,
                        const std::map<std::string, std::string>& params) {
        // Terrain generation code...
    }
    
    // Member variables
    std::thread m_thread;
    std::atomic<bool> m_isRunning;
    mutable std::mutex m_mutex;
    std::condition_variable m_condVar;
    std::queue<GenerationTask> m_taskQueue;
    std::vector<GenerationTask> m_completedTasks;
    std::vector<GenerationTask> m_failedTasks;
    std::optional<GenerationTask> m_currentTask;
    float m_currentProgress;
};
```

## Key Takeaways

- USD's composition system provides a powerful foundation for procedural content generation
- Modular code design mirrors USD's composition approach, enabling clear separation of concerns
- Layer-based generation allows for independent control of time, weather, events, and other environmental factors
- Procedural systems should be integrated with runtime game systems for dynamic environments
- Balance procedural generation with hand-authored content for the best results
- Optimization techniques like instancing, LOD generation, and on-demand creation are essential for large worlds
- Background processing can help manage complex generation tasks without impacting game performance

By leveraging USD's composition capabilities, game developers can create richly detailed, varied worlds that are both data-driven and procedurally generated. This approach scales well from small game environments to massive open worlds, providing a consistent framework for content creation across the entire development pipeline.