// city_layer_generator.cpp
//
// Example of procedurally generating city variants through layer composition

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/gf/vec3f.h>
#include <string>
#include <vector>
#include <fstream>

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
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Lighting/SunLight"))
            .CreateAttribute(pxr::TfToken("sparkle:light:angle"), pxr::SdfValueTypeNames->Float)
            .Set(10.0f);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Lighting/AmbientLight"))
            .CreateAttribute(pxr::TfToken("sparkle:light:intensity"), pxr::SdfValueTypeNames->Float)
            .Set(5000.0f);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Lighting/AmbientLight"))
            .CreateAttribute(pxr::TfToken("sparkle:light:color"), pxr::SdfValueTypeNames->Color3f)
            .Set(pxr::GfVec3f(0.6f, 0.7f, 0.9f));
            
        // Street lights are still on at dawn
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/StreetLights"))
            .CreateAttribute(pxr::TfToken("sparkle:lights:enabled"), pxr::SdfValueTypeNames->Bool)
            .Set(true);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/StreetLights"))
            .CreateAttribute(pxr::TfToken("sparkle:lights:intensity"), pxr::SdfValueTypeNames->Float)
            .Set(0.7f);
            
        // Building windows - some lights on, some off
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Buildings"))
            .CreateAttribute(pxr::TfToken("sparkle:windows:litPercentage"), pxr::SdfValueTypeNames->Float)
            .Set(0.4f);
    }
    else if (timeOfDay == "day") {
        // Daytime lighting settings
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Lighting/SunLight"))
            .CreateAttribute(pxr::TfToken("sparkle:light:intensity"), pxr::SdfValueTypeNames->Float)
            .Set(100000.0f);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Lighting/SunLight"))
            .CreateAttribute(pxr::TfToken("sparkle:light:color"), pxr::SdfValueTypeNames->Color3f)
            .Set(pxr::GfVec3f(1.0f, 1.0f, 0.9f));
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Lighting/SunLight"))
            .CreateAttribute(pxr::TfToken("sparkle:light:angle"), pxr::SdfValueTypeNames->Float)
            .Set(60.0f);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Lighting/AmbientLight"))
            .CreateAttribute(pxr::TfToken("sparkle:light:intensity"), pxr::SdfValueTypeNames->Float)
            .Set(15000.0f);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Lighting/AmbientLight"))
            .CreateAttribute(pxr::TfToken("sparkle:light:color"), pxr::SdfValueTypeNames->Color3f)
            .Set(pxr::GfVec3f(1.0f, 1.0f, 1.0f));
            
        // Street lights are off during the day
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/StreetLights"))
            .CreateAttribute(pxr::TfToken("sparkle:lights:enabled"), pxr::SdfValueTypeNames->Bool)
            .Set(false);
            
        // Building windows - most lights off
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Buildings"))
            .CreateAttribute(pxr::TfToken("sparkle:windows:litPercentage"), pxr::SdfValueTypeNames->Float)
            .Set(0.1f);
    }
    else if (timeOfDay == "dusk") {
        // Dusk lighting settings
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Lighting/SunLight"))
            .CreateAttribute(pxr::TfToken("sparkle:light:intensity"), pxr::SdfValueTypeNames->Float)
            .Set(30000.0f);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Lighting/SunLight"))
            .CreateAttribute(pxr::TfToken("sparkle:light:color"), pxr::SdfValueTypeNames->Color3f)
            .Set(pxr::GfVec3f(1.0f, 0.5f, 0.2f));
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Lighting/SunLight"))
            .CreateAttribute(pxr::TfToken("sparkle:light:angle"), pxr::SdfValueTypeNames->Float)
            .Set(-10.0f);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Lighting/AmbientLight"))
            .CreateAttribute(pxr::TfToken("sparkle:light:intensity"), pxr::SdfValueTypeNames->Float)
            .Set(8000.0f);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Lighting/AmbientLight"))
            .CreateAttribute(pxr::TfToken("sparkle:light:color"), pxr::SdfValueTypeNames->Color3f)
            .Set(pxr::GfVec3f(0.6f, 0.4f, 0.6f));
            
        // Street lights are turning on at dusk
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/StreetLights"))
            .CreateAttribute(pxr::TfToken("sparkle:lights:enabled"), pxr::SdfValueTypeNames->Bool)
            .Set(true);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/StreetLights"))
            .CreateAttribute(pxr::TfToken("sparkle:lights:intensity"), pxr::SdfValueTypeNames->Float)
            .Set(0.8f);
            
        // Building windows - many lights on
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Buildings"))
            .CreateAttribute(pxr::TfToken("sparkle:windows:litPercentage"), pxr::SdfValueTypeNames->Float)
            .Set(0.7f);
    }
    else if (timeOfDay == "night") {
        // Night lighting settings
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Lighting/SunLight"))
            .CreateAttribute(pxr::TfToken("sparkle:light:intensity"), pxr::SdfValueTypeNames->Float)
            .Set(0.0f); // Sun is down
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Lighting/MoonLight"))
            .CreateAttribute(pxr::TfToken("sparkle:light:enabled"), pxr::SdfValueTypeNames->Bool)
            .Set(true);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Lighting/MoonLight"))
            .CreateAttribute(pxr::TfToken("sparkle:light:intensity"), pxr::SdfValueTypeNames->Float)
            .Set(5000.0f);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Lighting/MoonLight"))
            .CreateAttribute(pxr::TfToken("sparkle:light:color"), pxr::SdfValueTypeNames->Color3f)
            .Set(pxr::GfVec3f(0.7f, 0.7f, 1.0f));
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Lighting/AmbientLight"))
            .CreateAttribute(pxr::TfToken("sparkle:light:intensity"), pxr::SdfValueTypeNames->Float)
            .Set(1000.0f);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Lighting/AmbientLight"))
            .CreateAttribute(pxr::TfToken("sparkle:light:color"), pxr::SdfValueTypeNames->Color3f)
            .Set(pxr::GfVec3f(0.1f, 0.1f, 0.3f));
            
        // Street lights are fully on at night
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/StreetLights"))
            .CreateAttribute(pxr::TfToken("sparkle:lights:enabled"), pxr::SdfValueTypeNames->Bool)
            .Set(true);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/StreetLights"))
            .CreateAttribute(pxr::TfToken("sparkle:lights:intensity"), pxr::SdfValueTypeNames->Float)
            .Set(1.0f);
            
        // Building windows - varying levels of lights
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Buildings"))
            .CreateAttribute(pxr::TfToken("sparkle:windows:litPercentage"), pxr::SdfValueTypeNames->Float)
            .Set(0.5f);
            
        // Activate night-specific effects
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/NightEffects"))
            .CreateAttribute(pxr::TfToken("sparkle:effect:active"), pxr::SdfValueTypeNames->Bool)
            .Set(true);
    }
    
    // Add NPC activity variations based on time of day
    if (timeOfDay == "dawn") {
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/NPCs"))
            .CreateAttribute(pxr::TfToken("sparkle:npc:density"), pxr::SdfValueTypeNames->Float)
            .Set(0.3f);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/NPCs"))
            .CreateAttribute(pxr::TfToken("sparkle:npc:primaryActivity"), pxr::SdfValueTypeNames->Token)
            .Set(pxr::TfToken("waking_up"));
            
        // Shops are mostly closed
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Shops"))
            .CreateAttribute(pxr::TfToken("sparkle:shops:percentOpen"), pxr::SdfValueTypeNames->Float)
            .Set(0.2f);
    }
    else if (timeOfDay == "day") {
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/NPCs"))
            .CreateAttribute(pxr::TfToken("sparkle:npc:density"), pxr::SdfValueTypeNames->Float)
            .Set(1.0f);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/NPCs"))
            .CreateAttribute(pxr::TfToken("sparkle:npc:primaryActivity"), pxr::SdfValueTypeNames->Token)
            .Set(pxr::TfToken("working"));
            
        // Shops are open
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Shops"))
            .CreateAttribute(pxr::TfToken("sparkle:shops:percentOpen"), pxr::SdfValueTypeNames->Float)
            .Set(0.9f);
    }
    else if (timeOfDay == "dusk") {
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/NPCs"))
            .CreateAttribute(pxr::TfToken("sparkle:npc:density"), pxr::SdfValueTypeNames->Float)
            .Set(0.8f);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/NPCs"))
            .CreateAttribute(pxr::TfToken("sparkle:npc:primaryActivity"), pxr::SdfValueTypeNames->Token)
            .Set(pxr::TfToken("leisure"));
            
        // Shops are starting to close
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Shops"))
            .CreateAttribute(pxr::TfToken("sparkle:shops:percentOpen"), pxr::SdfValueTypeNames->Float)
            .Set(0.6f);
    }
    else if (timeOfDay == "night") {
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/NPCs"))
            .CreateAttribute(pxr::TfToken("sparkle:npc:density"), pxr::SdfValueTypeNames->Float)
            .Set(0.4f);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/NPCs"))
            .CreateAttribute(pxr::TfToken("sparkle:npc:primaryActivity"), pxr::SdfValueTypeNames->Token)
            .Set(pxr::TfToken("socializing"));
            
        // Only taverns are open
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Shops"))
            .CreateAttribute(pxr::TfToken("sparkle:shops:percentOpen"), pxr::SdfValueTypeNames->Float)
            .Set(0.2f);
            
        // Taverns are active at night
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Taverns"))
            .CreateAttribute(pxr::TfToken("sparkle:tavern:activity"), pxr::SdfValueTypeNames->Float)
            .Set(1.0f);
    }
    
    // Save the layer
    layer->Save();
}

// Generate a layer containing city variations for weather conditions
void GenerateCityWeatherLayer(const std::string& filename, const std::string& weather) {
    // Create a new layer
    pxr::SdfLayerRefPtr layer = pxr::SdfLayer::CreateNew(filename);
    if (!layer) {
        return;
    }
    
    // Set up the layer structure with city weather overrides
    layer->SetDefaultPrim(pxr::TfToken("World"));
    
    // Add overrides for the city
    std::string cityPath = "/World/City";
    
    // Weather-specific settings
    if (weather == "clear") {
        // Clear weather settings
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Weather"))
            .CreateAttribute(pxr::TfToken("sparkle:weather:type"), pxr::SdfValueTypeNames->Token)
            .Set(pxr::TfToken("clear"));
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Weather"))
            .CreateAttribute(pxr::TfToken("sparkle:weather:cloudCover"), pxr::SdfValueTypeNames->Float)
            .Set(0.1f);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Lighting/SunLight"))
            .CreateAttribute(pxr::TfToken("sparkle:light:shadowIntensity"), pxr::SdfValueTypeNames->Float)
            .Set(1.0f);
            
        // Effects for clear weather
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Weather/Effects/Rain"))
            .CreateAttribute(pxr::TfToken("sparkle:effect:active"), pxr::SdfValueTypeNames->Bool)
            .Set(false);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Weather/Effects/Fog"))
            .CreateAttribute(pxr::TfToken("sparkle:effect:active"), pxr::SdfValueTypeNames->Bool)
            .Set(true);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Weather/Effects/Fog"))
            .CreateAttribute(pxr::TfToken("sparkle:effect:intensity"), pxr::SdfValueTypeNames->Float)
            .Set(0.3f);
            
        // Surface conditions
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Ground"))
            .CreateAttribute(pxr::TfToken("sparkle:ground:snowCover"), pxr::SdfValueTypeNames->Float)
            .Set(1.0f);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Buildings"))
            .CreateAttribute(pxr::TfToken("sparkle:building:snowRoofs"), pxr::SdfValueTypeNames->Bool)
            .Set(true);
            
        // NPC behavior changes
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/NPCs"))
            .CreateAttribute(pxr::TfToken("sparkle:npc:density"), pxr::SdfValueTypeNames->Float)
            .Set(0.4f); // Fewer NPCs in the snow
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/NPCs"))
            .CreateAttribute(pxr::TfToken("sparkle:npc:moveSpeed"), pxr::SdfValueTypeNames->Float)
            .Set(0.7f); // NPCs move slower in snow
    }
    
    // Save the layer
    layer->Save();
}

// Generate a layer containing city variations for events/festivals
void GenerateCityEventLayer(const std::string& filename, const std::string& event) {
    // Create a new layer
    pxr::SdfLayerRefPtr layer = pxr::SdfLayer::CreateNew(filename);
    if (!layer) {
        return;
    }
    
    // Set up the layer structure with city event overrides
    layer->SetDefaultPrim(pxr::TfToken("World"));
    
    // Add overrides for the city
    std::string cityPath = "/World/City";
    
    // Common event settings
    layer->GetPrimAtPath(pxr::SdfPath(cityPath))
        .CreateAttribute(pxr::TfToken("sparkle:city:activeEvent"), pxr::SdfValueTypeNames->Token)
        .Set(pxr::TfToken(event));
        
    layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/NPCs"))
        .CreateAttribute(pxr::TfToken("sparkle:npc:density"), pxr::SdfValueTypeNames->Float)
        .Set(1.5f); // More NPCs during events
    
    // Event-specific settings
    if (event == "harvest_festival") {
        // Harvest festival decorations and activities
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Decorations/HarvestDecor"))
            .CreateAttribute(pxr::TfToken("sparkle:decor:active"), pxr::SdfValueTypeNames->Bool)
            .Set(true);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Activities/HarvestGames"))
            .CreateAttribute(pxr::TfToken("sparkle:activity:active"), pxr::SdfValueTypeNames->Bool)
            .Set(true);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/TownSquare/HarvestMarket"))
            .CreateAttribute(pxr::TfToken("sparkle:market:active"), pxr::SdfValueTypeNames->Bool)
            .Set(true);
            
        // Special lighting for the festival
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Decorations/FestivalLights"))
            .CreateAttribute(pxr::TfToken("sparkle:lights:active"), pxr::SdfValueTypeNames->Bool)
            .Set(true);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Decorations/FestivalLights"))
            .CreateAttribute(pxr::TfToken("sparkle:lights:color"), pxr::SdfValueTypeNames->Color3f)
            .Set(pxr::GfVec3f(1.0f, 0.8f, 0.2f));
            
        // NPC behavior changes
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/NPCs"))
            .CreateAttribute(pxr::TfToken("sparkle:npc:primaryActivity"), pxr::SdfValueTypeNames->Token)
            .Set(pxr::TfToken("celebrating"));
    }
    else if (event == "winter_solstice") {
        // Winter solstice decorations and activities
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Decorations/WinterDecor"))
            .CreateAttribute(pxr::TfToken("sparkle:decor:active"), pxr::SdfValueTypeNames->Bool)
            .Set(true);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Activities/WinterGames"))
            .CreateAttribute(pxr::TfToken("sparkle:activity:active"), pxr::SdfValueTypeNames->Bool)
            .Set(true);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/TownSquare/WinterMarket"))
            .CreateAttribute(pxr::TfToken("sparkle:market:active"), pxr::SdfValueTypeNames->Bool)
            .Set(true);
            
        // Special lighting for the festival
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Decorations/FestivalLights"))
            .CreateAttribute(pxr::TfToken("sparkle:lights:active"), pxr::SdfValueTypeNames->Bool)
            .Set(true);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Decorations/FestivalLights"))
            .CreateAttribute(pxr::TfToken("sparkle:lights:color"), pxr::SdfValueTypeNames->Color3f)
            .Set(pxr::GfVec3f(0.2f, 0.4f, 1.0f));
            
        // Add snow as part of the event
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Ground"))
            .CreateAttribute(pxr::TfToken("sparkle:ground:snowCover"), pxr::SdfValueTypeNames->Float)
            .Set(0.7f);
            
        // NPC behavior changes
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/NPCs"))
            .CreateAttribute(pxr::TfToken("sparkle:npc:primaryActivity"), pxr::SdfValueTypeNames->Token)
            .Set(pxr::TfToken("celebrating"));
    }
    else if (event == "victory_day") {
        // Victory celebration decorations and activities
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Decorations/BannerFlags"))
            .CreateAttribute(pxr::TfToken("sparkle:decor:active"), pxr::SdfValueTypeNames->Bool)
            .Set(true);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Activities/Parade"))
            .CreateAttribute(pxr::TfToken("sparkle:activity:active"), pxr::SdfValueTypeNames->Bool)
            .Set(true);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Activities/Speeches"))
            .CreateAttribute(pxr::TfToken("sparkle:activity:active"), pxr::SdfValueTypeNames->Bool)
            .Set(true);
            
        // Special lighting and effects
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Effects/Fireworks"))
            .CreateAttribute(pxr::TfToken("sparkle:effect:active"), pxr::SdfValueTypeNames->Bool)
            .Set(true);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Decorations/FestivalLights"))
            .CreateAttribute(pxr::TfToken("sparkle:lights:active"), pxr::SdfValueTypeNames->Bool)
            .Set(true);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Decorations/FestivalLights"))
            .CreateAttribute(pxr::TfToken("sparkle:lights:color"), pxr::SdfValueTypeNames->Color3f)
            .Set(pxr::GfVec3f(1.0f, 0.2f, 0.2f));
            
        // NPC behavior changes
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/NPCs"))
            .CreateAttribute(pxr::TfToken("sparkle:npc:primaryActivity"), pxr::SdfValueTypeNames->Token)
            .Set(pxr::TfToken("celebrating"));
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/NPCs/Soldiers"))
            .CreateAttribute(pxr::TfToken("sparkle:npc:visible"), pxr::SdfValueTypeNames->Bool)
            .Set(true);
    }
    
    // Save the layer
    layer->Save();
}

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

// Generate all city variation layers
void GenerateAllCityLayers() {
    // Generate time of day layers
    GenerateCityTimeOfDayLayer("city_time_dawn.usda", "dawn");
    GenerateCityTimeOfDayLayer("city_time_day.usda", "day");
    GenerateCityTimeOfDayLayer("city_time_dusk.usda", "dusk");
    GenerateCityTimeOfDayLayer("city_time_night.usda", "night");
    
    // Generate weather layers
    GenerateCityWeatherLayer("city_weather_clear.usda", "clear");
    GenerateCityWeatherLayer("city_weather_cloudy.usda", "cloudy");
    GenerateCityWeatherLayer("city_weather_rainy.usda", "rainy");
    GenerateCityWeatherLayer("city_weather_snowy.usda", "snowy");
    
    // Generate event layers
    GenerateCityEventLayer("city_event_harvest_festival.usda", "harvest_festival");
    GenerateCityEventLayer("city_event_winter_solstice.usda", "winter_solstice");
    GenerateCityEventLayer("city_event_victory_day.usda", "victory_day");
}("sparkle:effect:active"), pxr::SdfValueTypeNames->Bool)
            .Set(false);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Weather/Effects/Snow"))
            .CreateAttribute(pxr::TfToken("sparkle:effect:active"), pxr::SdfValueTypeNames->Bool)
            .Set(false);
            
        // Surface conditions
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Ground"))
            .CreateAttribute(pxr::TfToken("sparkle:ground:wetness"), pxr::SdfValueTypeNames->Float)
            .Set(0.0f);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Ground"))
            .CreateAttribute(pxr::TfToken("sparkle:ground:puddles"), pxr::SdfValueTypeNames->Bool)
            .Set(false);
    }
    else if (weather == "cloudy") {
        // Cloudy weather settings
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Weather"))
            .CreateAttribute(pxr::TfToken("sparkle:weather:type"), pxr::SdfValueTypeNames->Token)
            .Set(pxr::TfToken("cloudy"));
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Weather"))
            .CreateAttribute(pxr::TfToken("sparkle:weather:cloudCover"), pxr::SdfValueTypeNames->Float)
            .Set(0.7f);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Lighting/SunLight"))
            .CreateAttribute(pxr::TfToken("sparkle:light:intensity"), pxr::SdfValueTypeNames->Float)
            .Set(50000.0f); // Dimmer than clear day
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Lighting/SunLight"))
            .CreateAttribute(pxr::TfToken("sparkle:light:shadowIntensity"), pxr::SdfValueTypeNames->Float)
            .Set(0.7f);
            
        // Effects for cloudy weather
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Weather/Effects/CloudShadows"))
            .CreateAttribute(pxr::TfToken("sparkle:effect:active"), pxr::SdfValueTypeNames->Bool)
            .Set(true);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Weather/Effects/Fog"))
            .CreateAttribute(pxr::TfToken("sparkle:effect:active"), pxr::SdfValueTypeNames->Bool)
            .Set(true);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Weather/Effects/Fog"))
            .CreateAttribute(pxr::TfToken("sparkle:effect:intensity"), pxr::SdfValueTypeNames->Float)
            .Set(0.2f);
    }
    else if (weather == "rainy") {
        // Rainy weather settings
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Weather"))
            .CreateAttribute(pxr::TfToken("sparkle:weather:type"), pxr::SdfValueTypeNames->Token)
            .Set(pxr::TfToken("rainy"));
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Weather"))
            .CreateAttribute(pxr::TfToken("sparkle:weather:cloudCover"), pxr::SdfValueTypeNames->Float)
            .Set(0.9f);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Lighting/SunLight"))
            .CreateAttribute(pxr::TfToken("sparkle:light:intensity"), pxr::SdfValueTypeNames->Float)
            .Set(30000.0f); // Dimmer than cloudy
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Lighting/SunLight"))
            .CreateAttribute(pxr::TfToken("sparkle:light:shadowIntensity"), pxr::SdfValueTypeNames->Float)
            .Set(0.4f);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Lighting/AmbientLight"))
            .CreateAttribute(pxr::TfToken("sparkle:light:color"), pxr::SdfValueTypeNames->Color3f)
            .Set(pxr::GfVec3f(0.5f, 0.5f, 0.6f));
            
        // Effects for rainy weather
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Weather/Effects/Rain"))
            .CreateAttribute(pxr::TfToken("sparkle:effect:active"), pxr::SdfValueTypeNames->Bool)
            .Set(true);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Weather/Effects/Rain"))
            .CreateAttribute(pxr::TfToken("sparkle:effect:intensity"), pxr::SdfValueTypeNames->Float)
            .Set(0.7f);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Weather/Effects/Fog"))
            .CreateAttribute(pxr::TfToken("sparkle:effect:active"), pxr::SdfValueTypeNames->Bool)
            .Set(true);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Weather/Effects/Fog"))
            .CreateAttribute(pxr::TfToken("sparkle:effect:intensity"), pxr::SdfValueTypeNames->Float)
            .Set(0.4f);
            
        // Surface conditions
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Ground"))
            .CreateAttribute(pxr::TfToken("sparkle:ground:wetness"), pxr::SdfValueTypeNames->Float)
            .Set(1.0f);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Ground"))
            .CreateAttribute(pxr::TfToken("sparkle:ground:puddles"), pxr::SdfValueTypeNames->Bool)
            .Set(true);
            
        // NPC behavior changes
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/NPCs"))
            .CreateAttribute(pxr::TfToken("sparkle:npc:density"), pxr::SdfValueTypeNames->Float)
            .Set(0.5f); // Fewer NPCs in the rain
    }
    else if (weather == "snowy") {
        // Snowy weather settings
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Weather"))
            .CreateAttribute(pxr::TfToken("sparkle:weather:type"), pxr::SdfValueTypeNames->Token)
            .Set(pxr::TfToken("snowy"));
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Weather"))
            .CreateAttribute(pxr::TfToken("sparkle:weather:cloudCover"), pxr::SdfValueTypeNames->Float)
            .Set(0.8f);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Lighting/SunLight"))
            .CreateAttribute(pxr::TfToken("sparkle:light:intensity"), pxr::SdfValueTypeNames->Float)
            .Set(40000.0f);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Lighting/SunLight"))
            .CreateAttribute(pxr::TfToken("sparkle:light:shadowIntensity"), pxr::SdfValueTypeNames->Float)
            .Set(0.6f);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Lighting/AmbientLight"))
            .CreateAttribute(pxr::TfToken("sparkle:light:color"), pxr::SdfValueTypeNames->Color3f)
            .Set(pxr::GfVec3f(0.7f, 0.7f, 0.8f));
            
        // Effects for snowy weather
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Weather/Effects/Snow"))
            .CreateAttribute(pxr::TfToken("sparkle:effect:active"), pxr::SdfValueTypeNames->Bool)
            .Set(true);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Weather/Effects/Snow"))
            .CreateAttribute(pxr::TfToken("sparkle:effect:intensity"), pxr::SdfValueTypeNames->Float)
            .Set(0.6f);
            
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Weather/Effects/Fog"))
            .CreateAttribute(pxr::TfToken("sparkle:effect:active"), pxr::SdfValueTypeNames->Bool)
            .Set(true);
        layer->GetPrimAtPath(pxr::SdfPath(cityPath + "/Weather/Effects/Fog"))
            .CreateAttribute(pxr::TfToken("sparkle:effect:intensity"), pxr::SdfValueTypeNames->Float)
            .Set(0.5f);
