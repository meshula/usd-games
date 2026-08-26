// update_npc_schedule.cpp
//
// Example function to update NPC schedules based on game state

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/variantSet.h>
#include <string>

// Forward declarations for GameState namespace
namespace GameState {
    bool IsEventActive(const std::string& eventName);
    std::string GetWeather();
    int GetDayOfWeek();
}

// Update NPC schedule based on day type and weather
void UpdateNPCSchedule(pxr::UsdStage* stage, const pxr::SdfPath& npcPath) {
    // Get the NPC prim
    pxr::UsdPrim npcPrim = stage->GetPrimAtPath(npcPath);
    if (!npcPrim) {
        return;
    }
    
    // Get schedule component
    pxr::SdfPath schedulePath = npcPath.AppendPath(pxr::SdfPath("Schedule"));
    pxr::UsdPrim schedulePrim = stage->GetPrimAtPath(schedulePath);
    if (!schedulePrim) {
        return;
    }
    
    // Get schedule variant set
    pxr::UsdVariantSet dayTypeVariant = schedulePrim.GetVariantSet("dayType");
    if (!dayTypeVariant) {
        return;
    }
    
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
        int dayOfWeek = GameState::GetDayOfWeek();
        if (dayOfWeek == 0 || dayOfWeek == 6) {  // Weekend
            scheduleType = "weekend";
        }
        else {  // Weekday
            scheduleType = "weekday";
        }
    }
    
    // Set the variant
    if (dayTypeVariant.HasVariant(scheduleType)) {
        dayTypeVariant.SetVariantSelection(scheduleType);
    }
}