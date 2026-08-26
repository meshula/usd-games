// DialogController.h
#pragma once

#include "pxr/usd/usd/stage.h"
#include "pxr/usd/usdGeom/xform.h"
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

// Forward declarations for game systems
class GameState;
class UISystem;
class InventorySystem;
class QuestSystem;
class ReputationSystem;

// Dialog controller for SparkleCarrotPopper game
class DialogController {
public:
    // Constructor takes references to required game systems
    DialogController(pxr::UsdStageRefPtr stage, 
                     GameState* gameState,
                     UISystem* uiSystem,
                     InventorySystem* inventorySystem,
                     QuestSystem* questSystem,
                     ReputationSystem* reputationSystem);
    
    // Start a conversation with an NPC
    bool StartConversation(const pxr::SdfPath& npcPath);
    
    // Select a dialog response option
    void SelectResponse(int responseIndex);
    
    // End the current conversation
    void EndConversation();
    
    // Update dialog system (called per frame)
    void Update(float deltaTime);

private:
    // Internal methods
    void ApplyDialogVariants();
    void ShowCurrentDialog();
    void ExecuteDialogActions();
    bool EvaluateCondition(const pxr::UsdPrim& conditionPrim);
    
    // Specialized condition evaluators
    bool EvaluateQuestCondition(const pxr::UsdPrim& conditionPrim);
    bool EvaluateInventoryCondition(const pxr::UsdPrim& conditionPrim);
    bool EvaluateAttributeCondition(const pxr::UsdPrim& conditionPrim);
    bool EvaluateBlackboardCondition(const pxr::UsdPrim& conditionPrim);
    
    // Specialized action handlers
    void HandleQuestAction(const pxr::UsdPrim& actionPrim);
    void HandleInventoryAction(const pxr::UsdPrim& actionPrim);
    void HandleShopAction(const pxr::UsdPrim& actionPrim);
    void HandleReputationAction(const pxr::UsdPrim& actionPrim);
    void HandleDialogUnlockAction(const pxr::UsdPrim& actionPrim);
    
    // Helper methods
    std::string GetPlayerRelationship(const pxr::UsdPrim& npcPrim);
    std::string GetPlayerStatus();
    
    // Dialog state
    pxr::UsdStageRefPtr m_stage;
    pxr::UsdPrim m_currentDialog;
    pxr::UsdPrim m_currentNode;
    pxr::UsdPrim m_currentNPC;
    
    // Game system references
    GameState* m_gameState;
    UISystem* m_uiSystem;
    InventorySystem* m_inventorySystem;
    QuestSystem* m_questSystem;
    ReputationSystem* m_reputationSystem;
    
    // Dialog blackboard for temporary state
    std::unordered_map<std::string, bool> m_boolValues;
    std::unordered_map<std::string, int> m_intValues;
    std::unordered_map<std::string, float> m_floatValues;
    std::unordered_map<std::string, std::string> m_stringValues;
};