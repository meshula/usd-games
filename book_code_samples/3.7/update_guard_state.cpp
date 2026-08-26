// update_guard_state.cpp
//
// Example function to update guard behavior state through layer composition

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/base/tf/token.h>
#include <string>
#include <vector>

// Update guard behavior state
void UpdateGuardState(pxr::UsdStage* stage, const pxr::SdfPath& guardPath, const std::string& newState) {
    // Get root layer
    pxr::SdfLayerRefPtr rootLayer = stage->GetRootLayer();
    pxr::SdfSubLayerVector sublayers = rootLayer->GetSubLayerPaths();
    
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
    // Note: For "normal" state, we don't add any extra layers
    
    // Update sublayers
    rootLayer->SetSubLayerPaths(sublayers);
    
    // Update behavioral state property on the guard
    pxr::UsdPrim guardPrim = stage->GetPrimAtPath(guardPath);
    if (guardPrim) {
        pxr::UsdAttribute stateAttr = guardPrim.GetAttribute(pxr::TfToken("sparkle:character:behaviorState"));
        if (stateAttr) {
            stateAttr.Set(pxr::TfToken(newState));
        }
    }
}

// Update character animation based on behavior state
void UpdateCharacterAnimation(pxr::UsdStage* stage, const pxr::SdfPath& characterPath) {
    // Get character prim
    pxr::UsdPrim characterPrim = stage->GetPrimAtPath(characterPath);
    if (!characterPrim) {
        return;
    }
    
    // Get behavior state
    std::string behaviorState = "idle";
    pxr::UsdAttribute behaviorStateAttr = characterPrim.GetAttribute(pxr::TfToken("sparkle:character:behaviorState"));
    if (behaviorStateAttr) {
        pxr::TfToken state;
        if (behaviorStateAttr.Get(&state)) {
            behaviorState = state.GetString();
        }
    }
    
    // Get animation controller
    pxr::SdfPath animControllerPath = characterPath.AppendPath(pxr::SdfPath("AnimationController"));
    pxr::UsdPrim animControllerPrim = stage->GetPrimAtPath(animControllerPath);
    if (!animControllerPrim) {
        return;
    }
    
    // Update animation behavior state variant
    pxr::UsdVariantSet behaviorVariant = animControllerPrim.GetVariantSet("behaviorState");
    if (behaviorVariant && behaviorVariant.HasVariant(behaviorState)) {
        behaviorVariant.SetVariantSelection(behaviorState);
    }
    
    // Additional animation parameters based on movement
    float moveSpeed = 0.0f;
    pxr::UsdAttribute moveSpeedAttr = characterPrim.GetAttribute(pxr::TfToken("sparkle:movement:currentSpeed"));
    if (moveSpeedAttr) {
        moveSpeedAttr.Get(&moveSpeed);
    }
    
    // Update animation speed parameter
    pxr::UsdPrim stateParamsPrim = stage->GetPrimAtPath(
        animControllerPath.AppendPath(pxr::SdfPath("StateParameters"))
    );
    
    if (stateParamsPrim) {
        pxr::UsdAttribute speedMultAttr = stateParamsPrim.GetAttribute(pxr::TfToken("sparkle:animation:speedMultiplier"));
        if (speedMultAttr) {
            // Calculate appropriate speed multiplier
            float speedMult = 1.0f;
            if (moveSpeed > 0.0f) {
                // Normalize to character's base speed
                float baseSpeed = 3.0f;  // Default base speed
                pxr::UsdAttribute baseSpeedAttr = characterPrim.GetAttribute(pxr::TfToken("sparkle:movement:baseSpeed"));
                if (baseSpeedAttr) {
                    baseSpeedAttr.Get(&baseSpeed);
                }
                
                speedMult = moveSpeed / baseSpeed;
                
                // Clamp to reasonable range
                speedMult = std::max(0.5f, std::min(speedMult, 2.0f));
            }
            
            speedMultAttr.Set(speedMult);
        }
    }
}