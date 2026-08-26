// procedural_generator.cpp
//
// Example implementation of a procedural generation system
// that leverages USD composition for game content

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/variantSet.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/gf/vec3f.h>
#include <random>
#include <string>
#include <vector>
#include <map>

// A basic procedural building generator class that creates instances
// from building templates and applies different variant configurations
class ProceduralBuildingGenerator {
public:
    ProceduralBuildingGenerator(pxr::UsdStage* stage) : m_stage(stage), m_seed(42) {
        // Initialize random number generator
        m_rng.seed(m_seed);
    }
    
    // Set the seed for the random number generator
    void SetSeed(int seed) {
        m_seed = seed;
        m_rng.seed(m_seed);
    }
    
    // Generate a building at a specific position with given parameters
    pxr::UsdPrim GenerateBuilding(const pxr::GfVec3f& position, 
                                  const std::string& buildingType = "house") {
        // Create a unique name for the building
        static int buildingCount = 0;
        std::string buildingName = "Building_" + buildingType + "_" + std::to_string(++buildingCount);
        
        // Create the building prim
        pxr::SdfPath buildingPath = pxr::SdfPath("/World/Buildings").AppendChild(pxr::TfToken(buildingName));
        pxr::UsdPrim buildingPrim = m_stage->DefinePrim(buildingPath);
        
        // Add a reference to the building template
        buildingPrim.GetReferences().AddReference("procedural_building.usda", pxr::SdfPath("/ProceduralBuilding"));
        
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
        
        // Set the LOD variant based on distance from center
        float distanceFromCenter = position.GetLength();
        std::string lodLevel = "high";
        
        if (distanceFromCenter > 200.0f) {
            lodLevel = "low";
        } else if (distanceFromCenter > 100.0f) {
            lodLevel = "medium";
        }
        
        pxr::UsdVariantSet lodVariant = buildingPrim.GetVariantSet("lodLevel");
        if (lodVariant && lodVariant.HasVariant(lodLevel)) {
            lodVariant.SetVariantSelection(lodLevel);
        }
        
        // Apply some random variation to the building dimensions
        std::uniform_real_distribution<float> variationDist(0.8f, 1.2f);
        
        // Width variation
        pxr::UsdAttribute widthAttr = buildingPrim.GetAttribute(pxr::TfToken("sparkle:building:width"));
        if (widthAttr) {
            float currentWidth;
            if (widthAttr.Get(&currentWidth)) {
                widthAttr.Set(currentWidth * variationDist(m_rng));
            }
        }
        
        // Height variation
        pxr::UsdAttribute heightAttr = buildingPrim.GetAttribute(pxr::TfToken("sparkle:building:height"));
        if (heightAttr) {
            float currentHeight;
            if (heightAttr.Get(&currentHeight)) {
                heightAttr.Set(currentHeight * variationDist(m_rng));
            }
        }
        
        // Add some random rotation
        std::uniform_real_distribution<float> rotationDist(0.0f, 360.0f);
        float rotation = rotationDist(m_rng);
        xformable.AddRotateYOp().Set(rotation);
        
        return buildingPrim;
    }
    
    // Generate a city block with multiple buildings
    pxr::UsdPrim GenerateCityBlock(const pxr::GfVec3f& position, 
                                  const std::string& neighborhoodType = "residential") {
        // Create a unique name for the city block
        static int blockCount = 0;
        std::string blockName = "Block_" + std::to_string(++blockCount);
        
        // Create the city block prim
        pxr::SdfPath blockPath = pxr::SdfPath("/World/CityBlocks").AppendChild(pxr::TfToken(blockName));
        pxr::UsdPrim blockPrim = m_stage->DefinePrim(blockPath);
        
        // Add a reference to the city block template
        blockPrim.GetReferences().AddReference("procedural_city_block.usda", pxr::SdfPath("/CityBlock"));
        
        // Set transform
        pxr::UsdGeomXformable xformable(blockPrim);
        xformable.AddTranslateOp().Set(position);
        
        // Set the neighborhood type variant
        pxr::UsdVariantSet neighborhoodVariant = blockPrim.GetVariantSet("neighborhoodType");
        if (neighborhoodVariant && neighborhoodVariant.HasVariant(neighborhoodType)) {
            neighborhoodVariant.SetVariantSelection(neighborhoodType);
        }
        
        // Randomly select an era period
        std::vector<std::string> eraPeriods = {"medieval", "renaissance", "victorian"};
        std::uniform_int_distribution<> eraDist(0, eraPeriods.size() - 1);
        std::string eraPeriod = eraPeriods[eraDist(m_rng)];
        
        pxr::UsdVariantSet eraVariant = blockPrim.GetVariantSet("eraPeriod");
        if (eraVariant && eraVariant.HasVariant(eraPeriod)) {
            eraVariant.SetVariantSelection(eraPeriod);
        }
        
        // Randomly determine weather condition with bias toward normal
        std::vector<std::string> weatherConditions = {"normal", "rainy", "snowy"};
        std::uniform_int_distribution<> weatherDist(0, 9);
        int weatherRoll = weatherDist(m_rng);
        std::string weatherCondition = weatherConditions[0]; // Default to normal
        
        if (weatherRoll >= 8) {
            weatherCondition = weatherConditions[2]; // snowy
        } else if (weatherRoll >= 6) {
            weatherCondition = weatherConditions[1]; // rainy
        }
        
        pxr::UsdVariantSet weatherVariant = blockPrim.GetVariantSet("weatherCondition");
        if (weatherVariant && weatherVariant.HasVariant(weatherCondition)) {
            weatherVariant.SetVariantSelection(weatherCondition);
        }
        
        // Apply some random variation to the block dimensions
        std::uniform_real_distribution<float> variationDist(0.9f, 1.1f);
        
        // Width variation
        pxr::UsdAttribute widthAttr = blockPrim.GetAttribute(pxr::TfToken("sparkle:cityBlock:width"));
        if (widthAttr) {
            float currentWidth;
            if (widthAttr.Get(&currentWidth)) {
                widthAttr.Set(currentWidth * variationDist(m_rng));
            }
        }
        
        // Depth variation
        pxr::UsdAttribute depthAttr = blockPrim.GetAttribute(pxr::TfToken("sparkle:cityBlock:depth"));
        if (depthAttr) {
            float currentDepth;
            if (depthAttr.Get(&currentDepth)) {
                depthAttr.Set(currentDepth * variationDist(m_rng));
            }
        }
        
        // Apply a slight random rotation to break up grid patterns
        std::uniform_real_distribution<float> rotationDist(-5.0f, 5.0f);
        float rotation = rotationDist(m_rng);
        xformable.AddRotateYOp().Set(rotation);
        
        return blockPrim;
    }
    
    // Generate a complete procedural dungeon
    pxr::UsdPrim GenerateDungeon(const pxr::GfVec3f& position, 
                                const std::string& dungeonType = "dungeon",
                                const std::string& difficulty = "medium") {
        // Create a unique name for the dungeon
        static int dungeonCount = 0;
        std::string dungeonName = "Dungeon_" + std::to_string(++dungeonCount);
        
        // Create the dungeon prim
        pxr::SdfPath dungeonPath = pxr::SdfPath("/World/Dungeons").AppendChild(pxr::TfToken(dungeonName));
        pxr::UsdPrim dungeonPrim = m_stage->DefinePrim(dungeonPath);
        
        // Add a reference to the dungeon template
        dungeonPrim.GetReferences().AddReference("procedural_dungeon.usda", pxr::SdfPath("/ProceduralDungeon"));
        
        // Set transform
        pxr::UsdGeomXformable xformable(dungeonPrim);
        xformable.AddTranslateOp().Set(position);
        
        // Set the dungeon type variant
        pxr::UsdVariantSet dungeonTypeVariant = dungeonPrim.GetVariantSet("dungeonType");
        if (dungeonTypeVariant && dungeonTypeVariant.HasVariant(dungeonType)) {
            dungeonTypeVariant.SetVariantSelection(dungeonType);
        }
        
        // Set the difficulty variant
        pxr::UsdVariantSet difficultyVariant = dungeonPrim.GetVariantSet("difficulty");
        if (difficultyVariant && difficultyVariant.HasVariant(difficulty)) {
            difficultyVariant.SetVariantSelection(difficulty);
        }
        
        // Randomly determine environment theme with bias toward normal
        std::vector<std::string> environments = {"normal", "flooded", "corrupted"};
        std::uniform_int_distribution<> envDist(0, 9);
        int envRoll = envDist(m_rng);
        std::string environment = environments[0]; // Default to normal
        
        if (envRoll >= 8) {
            environment = environments[2]; // corrupted
        } else if (envRoll >= 6) {
            environment = environments[1]; // flooded
        }
        
        pxr::UsdVariantSet envVariant = dungeonPrim.GetVariantSet("environment");
        if (envVariant && envVariant.HasVariant(environment)) {
            envVariant.SetVariantSelection(environment);
        }
        
        // Apply some random variation to room count
        std::uniform_int_distribution<int> roomCountDist(8, 15);
        
        pxr::UsdAttribute roomCountAttr = dungeonPrim.GetAttribute(pxr::TfToken("sparkle:dungeon:roomCount"));
        if (roomCountAttr) {
            roomCountAttr.Set(roomCountDist(m_rng));
        }
        
        // Generate a random seed for the dungeon
        std::uniform_int_distribution<int> seedDist(1, 9999);
        
        pxr::UsdAttribute seedAttr = dungeonPrim.GetAttribute(pxr::TfToken("sparkle:dungeon:seed"));
        if (seedAttr) {
            seedAttr.Set(seedDist(m_rng));
        }
        
        return dungeonPrim;
    }
    
    // Generate a procedural quest
    pxr::UsdPrim GenerateQuest(const std::string& questType = "retrieval",
                              const std::string& difficulty = "medium") {
        // Create a unique name and ID for the quest
        static int questCount = 0;
        int questNum = ++questCount;
        std::string questName = "Quest_" + std::to_string(questNum);
        std::string questId = "proc_quest_" + std::to_string(questNum);
        
        // Create the quest prim
        pxr::SdfPath questPath = pxr::SdfPath("/World/Quests").AppendChild(pxr::TfToken(questName));
        pxr::UsdPrim questPrim = m_stage->DefinePrim(questPath);
        
        // Add a reference to the quest template
        questPrim.GetReferences().AddReference("procedural_quest.usda", pxr::SdfPath("/ProceduralQuest"));
        
        // Set a unique quest ID
        pxr::UsdAttribute idAttr = questPrim.GetAttribute(pxr::TfToken("sparkle:quest:id"));
        if (idAttr) {
            idAttr.Set(questId);
        }
        
        // Set the quest type variant
        pxr::UsdVariantSet questTypeVariant = questPrim.GetVariantSet("questType");
        if (questTypeVariant && questTypeVariant.HasVariant(questType)) {
            questTypeVariant.SetVariantSelection(questType);
        }
        
        // Set the difficulty variant
        pxr::UsdVariantSet difficultyVariant = questPrim.GetVariantSet("difficulty");
        if (difficultyVariant && difficultyVariant.HasVariant(difficulty)) {
            difficultyVariant.SetVariantSelection(difficulty);
        }
        
        // Randomly determine reputation requirement
        std::vector<std::string> reputationLevels = {"none", "friendly", "honored"};
        std::uniform_int_distribution<> repDist(0, reputationLevels.size() - 1);
        std::string repLevel = reputationLevels[repDist(m_rng)];
        
        pxr::UsdVariantSet reputationVariant = questPrim.GetVariantSet("reputationRequirement");
        if (reputationVariant && reputationVariant.HasVariant(repLevel)) {
            reputationVariant.SetVariantSelection(repLevel);
        }
        
        // Generate a random seed for the quest
        std::uniform_int_distribution<int> seedDist(1, 9999);
        
        pxr::UsdAttribute seedAttr = questPrim.GetAttribute(pxr::TfToken("sparkle:quest:seed"));
        if (seedAttr) {
            seedAttr.Set(seedDist(m_rng));
        }
        
        return questPrim;
    }

private:
    pxr::UsdStage* m_stage;
    int m_seed;
    std::mt19937 m_rng;
};

// A generator for procedural terrain
class ProceduralTerrainGenerator {
public:
    ProceduralTerrainGenerator(pxr::UsdStage* stage) : m_stage(stage), m_seed(42) {
        // Initialize random number generator
        m_rng.seed(m_seed);
    }
    
    // Set the seed for the random number generator
    void SetSeed(int seed) {
        m_seed = seed;
        m_rng.seed(m_seed);
    }
    
    // Generate terrain
    pxr::UsdPrim GenerateTerrain(const std::string& features = "default",
                               const std::string& climate = "temperate") {
        // Create the terrain prim
        pxr::SdfPath terrainPath = pxr::SdfPath("/World/Terrain");
        pxr::UsdPrim terrainPrim = m_stage->DefinePrim(terrainPath);
        
        // Add a reference to the terrain template
        terrainPrim.GetReferences().AddReference("terrain_generator.usda", pxr::SdfPath("/ProceduralTerrain"));
        
        // Set the features variant
        pxr::UsdVariantSet featuresVariant = terrainPrim.GetVariantSet("features");
        if (featuresVariant && featuresVariant.HasVariant(features)) {
            featuresVariant.SetVariantSelection(features);
        }
        
        // Set the climate variant
        pxr::UsdVariantSet climateVariant = terrainPrim.GetVariantSet("climate");
        if (climateVariant && climateVariant.HasVariant(climate)) {
            climateVariant.SetVariantSelection(climate);
        }
        
        // Generate a random seed for the terrain
        std::uniform_int_distribution<int> seedDist(1, 9999);
        
        pxr::UsdAttribute seedAttr = terrainPrim.GetAttribute(pxr::TfToken("sparkle:terrain:seed"));
        if (seedAttr) {
            seedAttr.Set(seedDist(m_rng));
        }
        
        // Apply some random variation to the terrain dimensions
        std::uniform_real_distribution<float> dimensionDist(800.0f, 1200.0f);
        
        pxr::UsdAttribute widthAttr = terrainPrim.GetAttribute(pxr::TfToken("sparkle:terrain:width"));
        if (widthAttr) {
            widthAttr.Set(dimensionDist(m_rng));
        }
        
        pxr::UsdAttribute lengthAttr = terrainPrim.GetAttribute(pxr::TfToken("sparkle:terrain:length"));
        if (lengthAttr) {
            lengthAttr.Set(dimensionDist(m_rng));
        }
        
        std::uniform_real_distribution<float> heightDist(150.0f, 250.0f);
        
        pxr::UsdAttribute heightAttr = terrainPrim.GetAttribute(pxr::TfToken("sparkle:terrain:maxHeight"));
        if (heightAttr) {
            heightAttr.Set(heightDist(m_rng));
        }
        
        return terrainPrim;
    }

private:
    pxr::UsdStage* m_stage;
    int m_seed;
    std::mt19937 m_rng;
};

// Generate a complete procedural world
void GenerateProceduralWorld(pxr::UsdStage* stage, int seed = 12345) {
    // Create the world root prim
    pxr::UsdPrim worldRoot = stage->DefinePrim(pxr::SdfPath("/World"));
    
    // Create containers for different entity types
    stage->DefinePrim(pxr::SdfPath("/World/Terrain"));
    stage->DefinePrim(pxr::SdfPath("/World/Buildings"));
    stage->DefinePrim(pxr::SdfPath("/World/CityBlocks"));
    stage->DefinePrim(pxr::SdfPath("/World/Dungeons"));
    stage->DefinePrim(pxr::SdfPath("/World/Quests"));
    
    // Create terrain generator and generate base terrain
    ProceduralTerrainGenerator terrainGen(stage);
    terrainGen.SetSeed(seed);
    terrainGen.GenerateTerrain("mountains", "temperate");
    
    // Create building generator
    ProceduralBuildingGenerator buildingGen(stage);
    buildingGen.SetSeed(seed + 1);
    
    // Generate some city blocks in a grid pattern
    std::vector<std::string> neighborhoodTypes = {"residential", "commercial", "mixed"};
    
    for (int x = -2; x <= 2; x++) {
        for (int z = -2; z <= 2; z++) {
            // Skip the center for a town square
            if (x == 0 && z == 0) continue;
            
            // Determine block type based on position
            std::string blockType;
            if (abs(x) <= 1 && abs(z) <= 1) {
                blockType = "commercial"; // Commercial center
            } else if ((abs(x) == 2 && abs(z) == 2) || (abs(x) == 2 && abs(z) == 1) || (abs(x) == 1 && abs(z) == 2)) {
                blockType = "mixed"; // Mixed zones on diagonals
            } else {
                blockType = "residential"; // Residential outskirts
            }
            
            // Generate a city block
            pxr::GfVec3f blockPos(x * 150.0f, 0, z * 150.0f);
            buildingGen.GenerateCityBlock(blockPos, blockType);
        }
    }
    
    // Generate some outlying buildings
    std::vector<std::string> buildingTypes = {"house", "shop", "tavern"};
    std::mt19937 rng(seed + 2);
    std::uniform_int_distribution<int> typeDist(0, buildingTypes.size() - 1);
    std::uniform_real_distribution<float> posDist(-500.0f, 500.0f);
    std::uniform_real_distribution<float> distFromCenter(350.0f, 450.0f);
    
    // Generate some scattered buildings
    for (int i = 0; i < 30; i++) {
        // Generate random position on a ring around the town
        float angle = (float)i / 30.0f * 2.0f * M_PI;
        float distance = distFromCenter(rng);
        pxr::GfVec3f buildingPos(cos(angle) * distance, 0, sin(angle) * distance);
        
        // Add some noise to position
        buildingPos[0] += posDist(rng) * 0.1f;
        buildingPos[2] += posDist(rng) * 0.1f;
        
        // Pick a random building type
        std::string buildingType = buildingTypes[typeDist(rng)];
        
        // Generate the building
        buildingGen.GenerateBuilding(buildingPos, buildingType);
    }
    
    // Generate a few dungeons
    std::vector<std::string> dungeonTypes = {"cave", "dungeon", "crypt"};
    std::vector<std::string> difficulties = {"easy", "medium", "hard"};
    std::uniform_int_distribution<int> dungeonTypeDist(0, dungeonTypes.size() - 1);
    std::uniform_int_distribution<int> difficultyDist(0, difficulties.size() - 1);
    std::uniform_real_distribution<float> dungeonDist(600.0f, 800.0f);
    
    for (int i = 0; i < 4; i++) {
        // Generate random position further out from town
        float angle = (float)i / 4.0f * 2.0f * M_PI + 0.3f; // Offset for variety
        float distance = dungeonDist(rng);
        pxr::GfVec3f dungeonPos(cos(angle) * distance, 0, sin(angle) * distance);
        
        // Pick a random dungeon type and difficulty
        std::string dungeonType = dungeonTypes[dungeonTypeDist(rng)];
        std::string difficulty = difficulties[difficultyDist(rng)];
        
        // Generate the dungeon
        buildingGen.GenerateDungeon(dungeonPos, dungeonType, difficulty);
    }
    
    // Generate some quests
    std::vector<std::string> questTypes = {"retrieval", "hunting", "escort"};
    std::uniform_int_distribution<int> questTypeDist(0, questTypes.size() - 1);
    
    for (int i = 0; i < 10; i++) {
        // Pick a random quest type and difficulty
        std::string questType = questTypes[questTypeDist(rng)];
        std::string difficulty = difficulties[difficultyDist(rng)];
        
        // Generate the quest
        buildingGen.GenerateQuest(questType, difficulty);
    }
}