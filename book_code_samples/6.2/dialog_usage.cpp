#include "DialogController.h"
#include "GameState.h"
#include "UISystem.h"
#include "InventorySystem.h"
#include "QuestSystem.h"
#include "ReputationSystem.h"
#include "pxr/usd/usd/stage.h"

using namespace pxr;

// Example game class showing how to use the dialog system
class SparkleGame {
public:
    SparkleGame() {
        // Initialize systems
        m_gameState = std::make_unique<GameState>();
        m_uiSystem = std::make_unique<UISystem>();
        m_inventorySystem = std::make_unique<InventorySystem>();
        m_questSystem = std::make_unique<QuestSystem>();
        m_reputationSystem = std::make_unique<ReputationSystem>();
        
        // Load game world
        m_gameStage = UsdStage::Open("game_world.usda");
        
        // Initialize dialog controller
        m_dialogController = std::make_unique<DialogController>(
            m_gameStage,
            m_gameState.get(),
            m_uiSystem.get(),
            m_inventorySystem.get(),
            m_questSystem.get(),
            m_reputationSystem.get()
        );
        
        // Set up UI callbacks
        m_uiSystem->SetDialogResponseCallback([this](int responseIndex) {
            this->OnDialogResponseSelected(responseIndex);
        });
    }
    
    // Game update loop
    void Update(float deltaTime) {
        // Update all systems
        m_dialogController->Update(deltaTime);
        
        // Other game updates...
    }
    
    // Player interaction with an NPC
    void InteractWithNPC(const SdfPath& npcPath) {
        // Check if the NPC has dialog capability
        UsdPrim npcPrim = m_gameStage->GetPrimAtPath(npcPath);
        
        if (!npcPrim) {
            std::cerr << "NPC prim not found at path: " << npcPath << std::endl;
            return;
        }
        
        // Check if this NPC has a dialog tree
        UsdAttribute dialogTreeAttr = npcPrim.GetAttribute(TfToken("sparkle:dialog:dialogTree"));
        if (dialogTreeAttr) {
            // Start conversation with this NPC
            m_dialogController->StartConversation(npcPath);
        } else {
            // No dialog available, show a simple interaction message
            std::string npcName = "NPC";
            UsdAttribute nameAttr = npcPrim.GetAttribute(TfToken("sparkle:character:name"));
            if (nameAttr) {
                nameAttr.Get(&npcName);
            }
            
            m_uiSystem->ShowNotification(npcName + " has nothing to say.");
        }
    }
    
    // Callback for UI dialog response selection
    void OnDialogResponseSelected(int responseIndex) {
        m_dialogController->SelectResponse(responseIndex);
    }
    
    // Load dialog trees
    void LoadDialogTrees() {
        UsdPrim dialogTreesRoot = m_gameStage->DefinePrim(SdfPath("/DialogTrees"), TfToken("Xform"));
        dialogTreesRoot.SetMetadata(TfToken("kind"), VtValue(TfToken("group")));
        
        // Load the blacksmith dialog
        SdfLayerRefPtr blacksmithLayer = SdfLayer::FindOrOpen("dialogs/blacksmith_dialog.usda");
        if (blacksmithLayer) {
            m_gameStage->GetRootLayer()->GetSubLayerPaths().push_back(blacksmithLayer->GetIdentifier());
            std::cout << "Loaded blacksmith dialog tree." << std::endl;
        }
        
        // Load other dialog trees
        SdfLayerRefPtr villageElderLayer = SdfLayer::FindOrOpen("dialogs/village_elder_dialog.usda");
        if (villageElderLayer) {
            m_gameStage->GetRootLayer()->GetSubLayerPaths().push_back(villageElderLayer->GetIdentifier());
            std::cout << "Loaded village elder dialog tree." << std::endl;
        }
    }
    
    // Connect NPCs to their dialog trees
    void SetupNPCDialogs() {
        // Find the blacksmith NPC
        UsdPrim blacksmith = m_gameStage->GetPrimAtPath(SdfPath("/World/Village/NPCs/Blacksmith"));
        if (blacksmith) {
            // Set the dialog tree reference
            UsdAttribute dialogTreeAttr = blacksmith.CreateAttribute(
                TfToken("sparkle:dialog:dialogTree"), 
                SdfValueTypeNames->String
            );
            dialogTreeAttr.Set(SdfPath("/DialogTrees/BlacksmithDialog"));
        }
        
        // Find the village elder NPC
        UsdPrim villageElder = m_gameStage->GetPrimAtPath(SdfPath("/World/Village/NPCs/VillageElder"));
        if (villageElder) {
            // Set the dialog tree reference
            UsdAttribute dialogTreeAttr = villageElder.CreateAttribute(
                TfToken("sparkle:dialog:dialogTree"), 
                SdfValueTypeNames->String
            );
            dialogTreeAttr.Set(SdfPath("/DialogTrees/VillageElderDialog"));
        }
    }

private:
    // Game systems
    std::unique_ptr<GameState> m_gameState;
    std::unique_ptr<UISystem> m_uiSystem;
    std::unique_ptr<InventorySystem> m_inventorySystem;
    std::unique_ptr<QuestSystem> m_questSystem;
    std::unique_ptr<ReputationSystem> m_reputationSystem;
    
    // Dialog system
    std::unique_ptr<DialogController> m_dialogController;
    
    // Game world
    UsdStageRefPtr m_gameStage;
};

// Example main function showing initialization
int main() {
    // Create game instance
    SparkleGame game;
    
    // Load dialog trees
    game.LoadDialogTrees();
    
    // Setup NPC dialog connections
    game.SetupNPCDialogs();
    
    // Game loop (simplified)
    float deltaTime = 0.016f; // ~60 FPS
    bool running = true;
    
    while (running) {
        // Update game
        game.Update(deltaTime);
        
        // Example of player interaction with an NPC (triggered by UI)
        // In a real game, this would happen based on player input and proximity
        if (/* player is near blacksmith and presses interact key */) {
            game.InteractWithNPC(SdfPath("/World/Village/NPCs/Blacksmith"));
        }
        
        // Process other game events...
        
        // Check for exit condition
        if (/* exit condition */) {
            running = false;
        }
    }
    
    return 0;
}