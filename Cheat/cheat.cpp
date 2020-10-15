#include "cheat.h"
#include <HookLib/HookLib.h>
#include <condition_variable>
#include <conio.h>
#include <filesystem>
#include <Xinput.h>


namespace fs = std::filesystem;

void Cheat::Renderer::Drawing::RenderText(char* text, FVector2D& pos, bool outlined = true, bool centered = true)
{
    if (!text) return;
    auto ImScreen = *reinterpret_cast<ImVec2*>(&pos);
    if (centered)
    {
        auto size = ImGui::CalcTextSize(text);
        ImScreen.x -= size.x * 0.5f;
        ImScreen.y -= size.y;
    }
    auto window = ImGui::GetCurrentWindow();

    // todo: change renderText outline 
    if (outlined)
    {
        window->DrawList->AddText(nullptr, 0.f, ImVec2(ImScreen.x - 1.f, ImScreen.y + 1.f), ImGui::GetColorU32(IM_COL32_BLACK), text);
    }

    window->DrawList->AddText(nullptr, 0.f, ImScreen, ImGui::GetColorU32(IM_COL32_WHITE), text);

}

void Cheat::Renderer::Drawing::Render2DBox(FVector2D& top, FVector2D& bottom, float height, float width, ImVec4& color)
{
    ImGui::GetCurrentWindow()->DrawList->AddRect({ top.X - width * 0.5f, top.Y}, { top.X + width * 0.5f, bottom.Y }, ImGui::GetColorU32(color), 0.f, 15, 1.5f);
}

bool Cheat::Renderer::Drawing::Render3DBox(AController*& controller, FVector& origin, FVector& extent, FRotator& rotation, ImVec4& color)
{
    FVector vertex[2][4];
    vertex[0][0] = { origin.X - extent.X, origin.Y - extent.Y, origin.Z - extent.Z };
    vertex[0][1] = { origin.X + extent.X, origin.Y - extent.Y, origin.Z - extent.Z };
    vertex[0][2] = { origin.X + extent.X, origin.Y + extent.Y, origin.Z - extent.Z };
    vertex[0][3] = { origin.X - extent.X, origin.Y + extent.Y, origin.Z - extent.Z };

    vertex[1][0] = { origin.X - extent.X, origin.Y - extent.Y, origin.Z + extent.Z };
    vertex[1][1] = { origin.X + extent.X, origin.Y - extent.Y, origin.Z + extent.Z };
    vertex[1][2] = { origin.X + extent.X, origin.Y + extent.Y, origin.Z + extent.Z };
    vertex[1][3] = { origin.X - extent.X, origin.Y + extent.Y, origin.Z + extent.Z };

    float theta = (int(rotation.Yaw + 450.f) % 360) * 0.0174533f;
    FVector2D screen[2][4];
    for (auto k = 0; k < 2; k++)
    {
        for (auto i = 0; i < 4; i++)
        {
            auto& vec = vertex[k][i];
            float x = vec.X - origin.X;
            float y = vec.Y - origin.Y;
            vertex[k][i] = { origin.X + (x * cos(theta) - y * sin(theta)), origin.Y + (x * sin(theta) + y * cos(theta)), vec.Z };
            if (!controller->ProjectWorldLocationToScreen(vertex[k][i], &screen[k][i])) return false;
        }

    }

    auto ImScreen = reinterpret_cast<ImVec2(&)[2][4]>(screen);
    
    auto window = ImGui::GetCurrentWindow();
    for (auto i = 0; i < 4; i++)
    {
        window->DrawList->AddLine(ImScreen[0][i], ImScreen[0][(i + 1) % 4], ImGui::GetColorU32(color));
        window->DrawList->AddLine(ImScreen[1][i], ImScreen[1][(i + 1) % 4], ImGui::GetColorU32(color));
        window->DrawList->AddLine(ImScreen[0][i], ImScreen[1][i], ImGui::GetColorU32(color));
    }

    return true;
}

bool Cheat::Renderer::Drawing::RenderSkeleton(AController* controller, ACharacter* actor, FMatrix& comp2world, const int* bones, int size, ImVec4& color)
{
    FVector2D previousBone;
    for (auto i = 0; i < size; i++)
    {

        FVector loc;
        if (!actor->GetBone(bones[i], loc, comp2world)) return false;
        FVector2D screen;
        if (!controller->ProjectWorldLocationToScreen(loc, &screen)) return false;
        if (previousBone.Length() == 0) {
            previousBone = screen;
        }
        else {
            auto ImScreen1 = *reinterpret_cast<ImVec2*>(&previousBone);
            auto ImScreen2 = *reinterpret_cast<ImVec2*>(&screen);
            ImGui::GetCurrentWindow()->DrawList->AddLine(ImScreen1, ImScreen2, ImGui::GetColorU32(color));
            previousBone = screen;
        }
    }
    return true;
}


extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI Cheat::Renderer::WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    // to get some input like open menu, etc. maybe it adds some delay to actual game input
    if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam) && !bGameInput) return true;
    if (bGameInput) return CallWindowProcA(WndProcOriginal, hwnd, uMsg, wParam, lParam);
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

void Cheat::Renderer::HookInput()
{
    RemoveInput();
    WndProcOriginal = reinterpret_cast<WNDPROC>(SetWindowLongPtrA(gameWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc)));
}

void Cheat::Renderer::RemoveInput()
{
    if (WndProcOriginal) 
    {
        SetWindowLongPtrA(gameWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProcOriginal));
        WndProcOriginal = nullptr;
    }
}

HRESULT Cheat::Renderer::PresentHook(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags)
{ 
    if (!device)
    {
        ID3D11Texture2D* surface = nullptr;
        if (FAILED(swapChain->GetBuffer(0, __uuidof(surface), reinterpret_cast<PVOID*>(&surface)))) 
        {
            return PresentOriginal(swapChain, syncInterval, flags);
        };

        if (FAILED(swapChain->GetDevice(__uuidof(device), reinterpret_cast<PVOID*>(&device))))
        {
            return PresentOriginal(swapChain, syncInterval, flags);
        }

        if (FAILED(device->CreateRenderTargetView(surface, nullptr, &renderTargetView))) 
        {
            surface->Release();
            device->Release();
            device = nullptr;
            return PresentOriginal(swapChain, syncInterval, flags);
        };

        surface->Release();

        device->GetImmediateContext(&context);

        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();

        
        ImFontConfig config;
        config.GlyphRanges = io.Fonts->GetGlyphRangesCyrillic();
        config.RasterizerMultiply = 1.125f;
        io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Arial.ttf", 16.0f, &config);
        io.IniFilename = nullptr;

        ImGui_ImplWin32_Init(gameWindow);
        ImGui_ImplDX11_Init(device, context);
        ImGui_ImplDX11_CreateDeviceObjects();

        HookInput();
    }
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    static struct Config {
        enum class EBox : int {
            ENone,
            E2DBoxes,
            E3DBoxes,
            EDebugBoxes
        };
        enum class EBar : int {
            ENone,
            ELeft,
            ERight,
            EBottom,
            ETop,
            ETriangle
        };
        enum class EAim {
            ENone, 
            EClosest,
            EFOV, 
        };
        struct {
            bool bEnable = false;
            struct {
                bool bEnable = false;
                bool bSkeleton = false;
                bool bDrawTeam = false;
                bool bHealth = false;
                bool bName = false;
                EBox boxType = EBox::ENone;
                EBar barType = EBar::ENone;
                ImVec4 enemyColorVis = { 1.f, 0.f, 0.f, 1.f };
                ImVec4 enemyColorInv = { 1.f, 1.f, 0.f, 1.f };
                ImVec4 teamColorVis = { 0.f, 1.f, 0.0f, 1.f };
                ImVec4 teamColorInv = { 0.f, 1.f, 1.f, 1.f };
            } players;
            struct {
                bool bEnable = false;
                bool bSkeleton = false;
                bool bName = false;
                EBox boxType = EBox::ENone;
                EBar barType = EBar::ENone;
                ImVec4 colorVis = { 0.f, 1.f, 0.5f, 1.f };
                ImVec4 colorInv = { 1.f, 0.f, 1.f, 1.f };
                
            } skeletons;
            struct {
                bool bEnable = false;
                bool bSkeleton = false;
                bool bHealth = false;
                bool bName = false;
                bool bDamage = false;
                ImVec4 damageColor = { 1.f, 1.f, 1.f, 1.f };
            } ships;
            struct {
                bool bEnable = false;
                bool bName = false;
                float fMaxDist = 3500.f;
            } islands;
            struct {
                bool bEnable = false;
                bool bName = false;
            } items;
            struct {
                bool bCrosshair = false;
                bool bOxygen = false;
                bool bCompass = false;
                bool bDebug = false;
                float fCrosshair = 7.f;
                float fDebug = 10.f;
                ImVec4 crosshairColor = { 1.f, 1.f, 1.f, 1.f };
            } client;
        } visuals;
        struct {
            bool bEnable = false;
            struct {
                bool bEnable = false;
                bool bVisibleOnly = false;
                bool bTeam = false;
                float fYaw = 20.f;
                float fPitch = 20.f;
                float fSmoothness = 5.f;
            } players;
            struct {
                bool bEnable = false;
                bool bVisibleOnly = false;
                float fYaw = 20.f;
                float fPitch = 20.f;
                float fSmoothness = 5.f;
            } skeletons;
        } aim;

        struct {
            bool bEnable = false;
            struct {
                
            } client;
        } misc;

    } cfg;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    
    ImGui::Begin("#1", nullptr, ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoTitleBar);
    auto& io = ImGui::GetIO();
    ImGui::SetWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.y), ImGuiCond_Always);
   
    auto drawList = ImGui::GetCurrentWindow()->DrawList;
    
    try 
    {
        do 
        {
            auto world = *UWorld::GWorld;
            if (!world) break;
            auto game = world->GameInstance;
            if (!game) break;
            auto gameState = world->GameState;
            if (!gameState) break;
            if (!game->LocalPlayers.Data) break;
            auto localPlayer = game->LocalPlayers[0];
            if (!localPlayer) break;
            auto localController = localPlayer->PlayerController;
            if (!localController) break;
            auto camera = localController->PlayerCameraManager;
            if (!camera) break;
            auto cameraLoc = camera->GetCameraLocation();
            auto cameraRot = camera->GetCameraRotation();
            auto localCharacter = localController->Character;
            if (!localCharacter) break;
            auto levels = world->Levels;
            if (!levels.Data) break;
            static FVector  localLoc_prev = {};
            auto localLoc = localCharacter->K2_GetActorLocation();
            
            

            bool isWieldedWeapon = false;
            auto item = localCharacter->GetWieldedItem();
            if (item) isWieldedWeapon = item->isWeapon();

           // check isWieldedWeapon before accessing!
           auto localWeapon = *reinterpret_cast<AProjectileWeapon**>(&item);
            
            static struct {
                ACharacter* target = nullptr;
                FVector location;
                FRotator delta;
                float best = FLT_MAX;
                float smoothness = 1.f;
            } aimBest;

            aimBest.target = nullptr;
            aimBest.best = FLT_MAX;

            for (auto l = 0; l < levels.Count; l++)
            {
                auto level = levels[l];
                if (!level) continue;
                auto actors = level->AActors;
                if (!actors.Data) continue;

                // todo: make functions for similar code 
                for (auto a = 0; a < actors.Count; a++)
                {
                    auto actor = actors[a];
                    if (!actor) continue;

                    if (cfg.aim.bEnable && isWieldedWeapon)
                    {
                        if (cfg.aim.players.bEnable && actor->isPlayer() && actor != localCharacter && !actor->IsDead())
                        {
                            do {

                                auto playerLoc = actor->K2_GetActorLocation();
                                auto dist = localLoc.DistTo(playerLoc);
                                if (dist > localWeapon->WeaponParameters.ProjectileMaximumRange) break;

                                if (cfg.aim.players.bVisibleOnly) if (!localController->LineOfSightTo(actor, cameraLoc, false)) break;
                                if (!cfg.aim.players.bTeam) if (UCrewFunctions::AreCharactersInSameCrew(actor, localCharacter)) break;

                                auto rotationDelta = UKismetMathLibrary::NormalizedDeltaRotator(UKismetMathLibrary::FindLookAtRotation(cameraLoc, playerLoc), cameraRot);

                                auto absYaw = abs(rotationDelta.Yaw);
                                auto absPitch = abs(rotationDelta.Pitch);
                                if (absYaw > cfg.aim.players.fYaw || absPitch > cfg.aim.players.fPitch) break;                                
                                auto sum = absYaw + absPitch;

                                if (sum < aimBest.best)
                                {
                                    aimBest.target = actor;
                                    aimBest.location = playerLoc;
                                    aimBest.delta = rotationDelta;
                                    aimBest.best = sum;
                                    aimBest.smoothness = cfg.aim.players.fSmoothness;
                                }

                            } while (false);
                        }
                        else if (cfg.aim.skeletons.bEnable && actor->isSkeleton() && !actor->IsDead())
                        {
                            do {
                                auto playerLoc = actor->K2_GetActorLocation();
                                auto dist = localLoc.DistTo(playerLoc);

                                if (dist > localWeapon->WeaponParameters.ProjectileMaximumRange) break;
                                if (cfg.aim.skeletons.bVisibleOnly) if (!localController->LineOfSightTo(actor, cameraLoc, false)) break;

                                auto rotationDelta = UKismetMathLibrary::NormalizedDeltaRotator(UKismetMathLibrary::FindLookAtRotation(cameraLoc, playerLoc), cameraRot);

                                auto absYaw = abs(rotationDelta.Yaw);
                                auto absPitch = abs(rotationDelta.Pitch);
                                if (absYaw > cfg.aim.skeletons.fYaw || absPitch > cfg.aim.skeletons.fPitch) break;
                                auto sum = absYaw + absPitch;

                                if (sum < aimBest.best)
                                {
                                    aimBest.target = actor;
                                    aimBest.location = playerLoc;
                                    aimBest.delta = rotationDelta;
                                    aimBest.best = sum;
                                    aimBest.smoothness = cfg.aim.skeletons.fSmoothness;
                                }

                            } while (false);
                        }
                    }
                   

                    if (cfg.visuals.bEnable)
                    {
                        if (cfg.visuals.client.bDebug)
                        {
                            auto location = actor->K2_GetActorLocation();
                            auto dist = localLoc.DistTo(location) * 0.01f;
                            if (dist < cfg.visuals.client.fDebug)
                            {
                                UClass* actorClass = actor->Class;
                                if (!actorClass) continue;
                                FVector2D screen;
                                if (localController->ProjectWorldLocationToScreen(location, &screen))
                                {
                                    auto name = actorClass->GetName();
                                    if (name.length())
                                    {
                                        char buf[0x64];
                                        sprintf_s(buf, "%s [%d]", name.data(), (int)dist);
                                        Drawing::RenderText(buf, screen);
                                    }
                                }
                            }
                        }
                        else {

                            if (cfg.visuals.items.bEnable && actor->isItem()) {

                                if (cfg.visuals.items.bName)
                                {
                                    auto location = actor->K2_GetActorLocation();
                                    FVector2D screen;
                                    if (localController->ProjectWorldLocationToScreen(location, &screen))
                                    {
                                        auto desc = actor->GetItemInfo()->Desc;
                                        if (!desc) continue;
                                        int dist = localLoc.DistTo(location) * 0.01f;
                                        char name[0x64];
                                        auto len = desc->Title->multi(name, 0x50);
                                        sprintf(name + len, " [%d]", dist);
                                        Drawing::RenderText(name, screen);
                                    };
                                }
                            

                                continue;
                            }

                            if (cfg.visuals.players.bEnable && actor->isPlayer() && actor != localCharacter && !actor->IsDead())
                            {

                                auto teammate = UCrewFunctions::AreCharactersInSameCrew(actor, localCharacter);
                                if (teammate && !cfg.visuals.players.bDrawTeam) continue;

                                FVector origin, extent;
                                actor->GetActorBounds(true, &origin, &extent);
                                auto location = actor->K2_GetActorLocation();

                                FVector2D headPos;
                                if (!localController->ProjectWorldLocationToScreen({ location.X, location.Y, location.Z + extent.Z }, &headPos)) continue;
                                FVector2D footPos;
                                if (!localController->ProjectWorldLocationToScreen({ location.X, location.Y, location.Z - extent.Z }, &footPos)) continue;

                                auto height = abs(footPos.Y - headPos.Y);
                                auto width = height * 0.4f;

                                if (cfg.visuals.players.bName)
                                {
                                
                                    auto playerState = actor->PlayerState;
                                    if (!playerState) continue;
                                    auto playerName = playerState->PlayerName;
                                    if (!playerName.Data) continue;
                                    char name[0x30];
                                    auto len = playerName.multi(name, 0x20);
                                    int dist = localLoc.DistTo(origin) * 0.01f;
                                    sprintf(name + len, " [%d]", dist);
                                    auto adjust = height * 0.05f;
                                    FVector2D pos = { headPos.X, headPos.Y - adjust };
                                    Drawing::RenderText(name, pos);
                                }

                                bool bVisible = localController->LineOfSightTo(actor, cameraLoc, false);
                                ImVec4 col;
                                if (teammate) col = bVisible ? cfg.visuals.players.teamColorVis : cfg.visuals.players.teamColorInv;
                                else  col = bVisible ? cfg.visuals.players.enemyColorVis : cfg.visuals.players.enemyColorInv;
                                
                           
                               
                                switch (cfg.visuals.players.boxType)
                                {
                                case Config::EBox::E2DBoxes: 
                                {
                                    Drawing::Render2DBox(headPos, footPos, height, width, col);
                                    break;
                                }
                                case Config::EBox::E3DBoxes: 
                                {
                                    FRotator rotation = actor->K2_GetActorRotation();
                                    FVector ext = { 35.f, 35.f, extent.Z };
                                    if (!Drawing::Render3DBox(localController, location, ext, rotation, col)) continue;
                                    break;
                                }
                                case Config::EBox::EDebugBoxes: 
                                {
                                    FVector ext = { 35.f, 35.f, extent.Z };
                                    UKismetMathLibrary::DrawDebugBox(actor, location, ext, *reinterpret_cast<FLinearColor*>(&col), actor->K2_GetActorRotation(), 0.0f);
                                    break;
                                }
                                }

                                if (cfg.visuals.players.barType != Config::EBar::ENone)
                                {
                                    auto healthComp = actor->HealthComponent;
                                    if (!healthComp) continue;
                                    float hp = healthComp->GetCurrentHealth();
                                    auto width2 = width * 0.5f;

                                    switch (cfg.visuals.players.barType)
                                    {
                                    case Config::EBar::ELeft: 
                                    {
                                        auto len = height * hp / 100.f;
                                        auto adjust = height * 0.025f;
                                        drawList->AddRectFilled({ headPos.X - width2 - adjust * 2.f, headPos.Y }, { headPos.X - width2 - adjust, footPos.Y - len }, ImGui::GetColorU32(IM_COL32(255, 0, 0, 255)));
                                        drawList->AddRectFilled({ headPos.X - width2 - adjust * 2.f, footPos.Y - len }, { headPos.X - width2 - adjust, footPos.Y }, ImGui::GetColorU32(IM_COL32(0, 255, 0, 255)));
                                        break;
                                    }
                                    case Config::EBar::ERight:
                                    {
                                        auto len = height * hp / 100.f;
                                        auto adjust = height * 0.025f;
                                        drawList->AddRectFilled({ headPos.X + width2 + adjust, headPos.Y }, { headPos.X + width2 + adjust * 2.f, footPos.Y - len }, ImGui::GetColorU32(IM_COL32(255, 0, 0, 255)));
                                        drawList->AddRectFilled({ headPos.X + width2 + adjust, footPos.Y - len }, { headPos.X + width2 + adjust * 2.f, footPos.Y }, ImGui::GetColorU32(IM_COL32(0, 255, 0, 255)));
                                        break;
                                    }
                                    case Config::EBar::EBottom:
                                    {
                                        auto len = width2 * hp / 50.f;
                                        auto adjust = height * 0.025f;
                                        drawList->AddRectFilled({ headPos.X - width2, footPos.Y + adjust }, { headPos.X - width2 + len, footPos.Y + adjust * 2.f }, ImGui::GetColorU32(IM_COL32(0, 255, 0, 255)));
                                        drawList->AddRectFilled({ headPos.X - width2 + len, footPos.Y + adjust }, { headPos.X + width2, footPos.Y + adjust * 2.f }, ImGui::GetColorU32(IM_COL32(255, 0, 0, 255)));
                                        break;
                                    }
                                    case Config::EBar::ETop:
                                    {
                                        auto len = width2 * hp / 50.f;
                                        auto adjust = height * 0.025f;
                                        drawList->AddRectFilled({ headPos.X - width2, headPos.Y - adjust * 2.f }, { headPos.X - width2 + len, headPos.Y - adjust }, ImGui::GetColorU32(IM_COL32(0, 255, 0, 255)));
                                        drawList->AddRectFilled({ headPos.X - width2 + len, headPos.Y - adjust * 2.f }, { headPos.X + width2, headPos.Y - adjust }, ImGui::GetColorU32(IM_COL32(255, 0, 0, 255)));
                                        break;
                                    }
                                    }
          
                                }
                            
                                if (cfg.visuals.players.bSkeleton)
                                {
                                    auto mesh = actor->Mesh;
                                    if (!actor->Mesh) continue;
                                    

                                    const int bodyHead[] = {4, 5, 6, 51, 7, 6, 80, 7, 8, 9};
                                    const int neckHandR[] = { 80, 81, 82, 83, 84 };
                                    const int neckHandL[] = { 51, 52, 53, 54, 55 };
                                    const int bodyFootR[] = { 4, 111, 112, 113, 114 };
                                    const int bodyFootL[] = { 4, 106, 107, 108, 109 };

                                    auto comp2world = mesh->K2_GetComponentToWorld().ToMatrixWithScale();

                                    if (!Drawing::RenderSkeleton(localController, actor, comp2world, bodyHead, 10, col)) continue;
                                    if (!Drawing::RenderSkeleton(localController, actor, comp2world, neckHandR, 5, col)) continue;
                                    if (!Drawing::RenderSkeleton(localController, actor, comp2world, neckHandL, 5, col)) continue;
                                    if (!Drawing::RenderSkeleton(localController, actor, comp2world, bodyFootR, 5, col)) continue;
                                    if (!Drawing::RenderSkeleton(localController, actor, comp2world, bodyFootL, 5, col)) continue;

                                   
                                }
                            

                                continue;
                            
                                
                            
                            }
                       
                            if (cfg.visuals.skeletons.bEnable && actor->isSkeleton() && !actor->IsDead()) {
                                // todo: make a function to draw both skeletons and players as they are similar
                                FVector origin, extent;
                                actor->GetActorBounds(true, &origin, &extent);
                            
                                auto location = actor->K2_GetActorLocation();
                                FVector2D headPos;
                                if (!localController->ProjectWorldLocationToScreen({ location.X, location.Y, location.Z + extent.Z }, &headPos)) continue;
                                FVector2D footPos;
                                if (!localController->ProjectWorldLocationToScreen({ location.X, location.Y, location.Z - extent.Z }, &footPos)) continue;

                                auto height = abs(footPos.Y - headPos.Y);
                                auto width = height * 0.4f;

                                bool bVisible = localController->LineOfSightTo(actor, cameraLoc, false);
                                ImVec4 col = bVisible ? cfg.visuals.skeletons.colorVis : cfg.visuals.skeletons.colorInv;
                               

                                switch (cfg.visuals.skeletons.boxType)
                                {
                                case Config::EBox::E2DBoxes:
                                {
                                    Drawing::Render2DBox(headPos, footPos, height, width, col);
                                    break;
                                }
                                case Config::EBox::E3DBoxes:
                                {
                                    FRotator rotation = actor->K2_GetActorRotation();
                                    if (!Drawing::Render3DBox(localController, origin, extent, rotation, col)) continue;
                                    break;
                                }
                                case Config::EBox::EDebugBoxes:
                                {
                                    UKismetMathLibrary::DrawDebugBox(actor, origin, extent, *reinterpret_cast<FLinearColor*>(&col), actor->K2_GetActorRotation(), 0.0f);
                                    break;
                                }
                                }

                                if (cfg.visuals.skeletons.bName)
                                {
                                    //auto location = actor->K2_GetActorLocation();
                                    int dist = localLoc.DistTo(location) * 0.01f;
                                    char name[0x20];
                                    sprintf_s(name, "Skeleton [%d]", dist);
                                    Drawing::RenderText(const_cast<char*>(name), headPos);
                                }

                                if (cfg.visuals.skeletons.barType != Config::EBar::ENone)
                                {
                                    auto healthComp = actor->HealthComponent;
                                    if (!healthComp) continue;
                                    float hp = healthComp->GetCurrentHealth() / 100.f;
                                    auto width2 = width * 0.5f;

                                    switch (cfg.visuals.skeletons.barType)
                                    {
                                    case Config::EBar::ELeft:
                                    {
                                        auto len = height * hp;
                                        auto adjust = height * 0.025f;
                                        drawList->AddRectFilled({ headPos.X - width2 - adjust * 2.f, headPos.Y }, { headPos.X - width2 - adjust, footPos.Y - len }, ImGui::GetColorU32(IM_COL32(255, 0, 0, 255)));
                                        drawList->AddRectFilled({ headPos.X - width2 - adjust * 2.f, footPos.Y - len }, { headPos.X - width2 - adjust, footPos.Y }, ImGui::GetColorU32(IM_COL32(0, 255, 0, 255)));
                                        break;
                                    }
                                    case Config::EBar::ERight:
                                    {
                                        auto len = height * hp;
                                        auto adjust = height * 0.025f;
                                        drawList->AddRectFilled({ headPos.X + width2 + adjust, headPos.Y }, { headPos.X + width2 + adjust * 2.f, footPos.Y - len }, ImGui::GetColorU32(IM_COL32(255, 0, 0, 255)));
                                        drawList->AddRectFilled({ headPos.X + width2 + adjust, footPos.Y - len }, { headPos.X + width2 + adjust * 2.f, footPos.Y }, ImGui::GetColorU32(IM_COL32(0, 255, 0, 255)));
                                        break;
                                    }
                                    case Config::EBar::EBottom:
                                    {
                                        auto len = width2 * hp * 2.f;
                                        auto adjust = height * 0.025f;
                                        drawList->AddRectFilled({ headPos.X - width2, footPos.Y + adjust }, { headPos.X - width2 + len, footPos.Y + adjust * 2.f }, ImGui::GetColorU32(IM_COL32(0, 255, 0, 255)));
                                        drawList->AddRectFilled({ headPos.X - width2 + len, footPos.Y + adjust }, { headPos.X + width2, footPos.Y + adjust * 2.f }, ImGui::GetColorU32(IM_COL32(255, 0, 0, 255)));
                                        break;
                                    }
                                    case Config::EBar::ETop:
                                    {
                                        auto len = width2 * hp * 2.f;
                                        auto adjust = height * 0.025f;
                                        drawList->AddRectFilled({ headPos.X - width2, headPos.Y - adjust * 2.f }, { headPos.X - width2 + len, headPos.Y - adjust }, ImGui::GetColorU32(IM_COL32(0, 255, 0, 255)));
                                        drawList->AddRectFilled({ headPos.X - width2 + len, headPos.Y - adjust * 2.f }, { headPos.X + width2, headPos.Y - adjust }, ImGui::GetColorU32(IM_COL32(255, 0, 0, 255)));
                                        break;
                                    }
                                    }



                                }

                                if (cfg.visuals.skeletons.bSkeleton)
                                {
                                    auto mesh = actor->Mesh;
                                    if (!actor->Mesh) continue;
                                    const int bodyHead[] = { 4, 5, 6, 7, 8, 9 };
                                    const int neckHandR[] = { 7, 41, 42, 43 };
                                    const int neckHandL[] = { 7, 12, 13, 14 };
                                    const int bodyFootR[] = { 4, 71, 72, 73, 74 };
                                    const int bodyFootL[] = { 4, 66, 67, 68, 69 };
                                    auto comp2world = mesh->K2_GetComponentToWorld().ToMatrixWithScale();
                                    if (!Drawing::RenderSkeleton(localController, actor, comp2world, bodyHead, 6, col)) continue;
                                    if (!Drawing::RenderSkeleton(localController, actor, comp2world, neckHandR, 4, col)) continue;
                                    if (!Drawing::RenderSkeleton(localController, actor, comp2world, neckHandL, 4, col)) continue;
                                    if (!Drawing::RenderSkeleton(localController, actor, comp2world, bodyFootR, 5, col)) continue;
                                    if (!Drawing::RenderSkeleton(localController, actor, comp2world, bodyFootL, 5, col)) continue;

                                    /*for (auto i = 0; i < 122; i++)
                                    {
                                        FVector pos;
                                        if (actor->GetBone(i, pos, comp2world))
                                        {
                                            FVector2D screen;
                                            if (!localController->ProjectWorldLocationToScreen(pos, &screen)) continue;
                                            char text[0x30];
                                            auto len = sprintf_s(text, "%d", i);
                                            Drawing::RenderText(text, screen);
                                        };
                                    }*/
                                }
                                continue;
                            }

                            if (cfg.visuals.ships.bEnable)
                            {
                            
                                if (actor->isShip() ) 
                                {
                                    auto location = actor->K2_GetActorLocation();
                                    int dist = localLoc.DistTo(location) * 0.01f;

                                    if (cfg.visuals.ships.bName && dist <= 1500)
                                    {
                                        FVector2D screen;
                                        if (localController->ProjectWorldLocationToScreen(location, &screen)) {
                                            int amount = 0;
                                            auto water = actor->GetInternalWater();
                                            if (water) amount = water->GetNormalizedWaterAmount() * 100.f;
                                            char name[0x30];
                                            sprintf_s(name, "Ship (%d%%) [%d]", amount, dist);
                                            Drawing::RenderText(const_cast<char*>(name), screen);
                                        };
                                    }

                                    if (cfg.visuals.ships.bDamage && dist <= 300)
                                    {
                                        auto damage = actor->GetHullDamage();
                                        if (!damage) continue;
                                        auto holes = damage->ActiveHullDamageZones;
                                        for (auto h = 0; h < holes.Count; h++)
                                        {
                                            auto hole = holes[h];
                                            auto location = hole->K2_GetActorLocation();
                                            FVector2D screen;
                                            if (localController->ProjectWorldLocationToScreen(location, &screen))
                                            {
                                                drawList->AddLine({ screen.X - 6.f, screen.Y + 6.f }, { screen.X + 6.f, screen.Y - 6.f }, ImGui::GetColorU32(IM_COL32_WHITE));
                                                drawList->AddLine({ screen.X - 6.f, screen.Y - 6.f }, { screen.X + 6.f, screen.Y + 6.f }, ImGui::GetColorU32(IM_COL32_WHITE));
                                            }
                                        }
                                    }

                                    continue;
                                }
                                else if (actor->isFarShip())
                                {
                                    auto location = actor->K2_GetActorLocation();
                                    int dist = localLoc.DistTo(location) * 0.01f;

                                    if (cfg.visuals.ships.bName && dist > 1500)
                                    {
                                        FVector2D screen;
                                        if (localController->ProjectWorldLocationToScreen(location, &screen)) {
                                            char name[0x30];
                                            sprintf_s(name, "Ship [%d]", dist);
                                            Drawing::RenderText(const_cast<char*>(name), screen);
                                        };
                                    }
                                    continue;
                                }

                                
                            }


                            /*if (cfg.bBuriedTreasure && actor->isBuriedTreasure()) {
                                auto location = actor->K2_GetActorLocation();
                                FVector2D screen;
                                if (localController->ProjectWorldLocationToScreen(location, &screen))
                                {
                                    int dist = localPos.DistTo(location) * 0.01f;
                                    char name[0x20] = "BuriedTreasure";
                                    Drawing::RenderText(dist, const_cast<char*>(name), 14, screen);
                                };
                                continue;
                            }*/
                        }
                    }
                }
            }
            
            if (cfg.visuals.bEnable)
            {

                if (cfg.visuals.islands.bEnable)
                {
                    if (cfg.visuals.islands.bName)
                    {
                        do {
                            auto islandService = gameState->IslandService;
                            if (!islandService) break;
                            auto islandDataAsset = islandService->IslandDataAsset;
                            if (!islandDataAsset) break;
                            auto islandDataEntries = islandDataAsset->IslandDataEntries;
                            if (!islandDataEntries.Data) break;
                            for (auto i = 0; i < islandDataEntries.Count; i++)
                            {
                                auto& island = islandDataEntries[i];
                                auto WorldMapData = island->WorldMapData;
                                if (!WorldMapData) continue;
                                
                                    
                                auto islandLoc = WorldMapData->WorldSpaceCameraPosition;
                                float distF = localLoc.DistTo(islandLoc) * 0.01f;
                                if (distF > cfg.visuals.islands.fMaxDist) continue;
                                FVector2D screen;
                                if (localController->ProjectWorldLocationToScreen(islandLoc, &screen))
                                {
                                    char name[0x64];
                                    auto len = island->LocalisedName->multi(name, 0x50);
                                    
                                    int dist = distF;
                                    sprintf(name + len, " [%d]", dist);
                                    Drawing::RenderText(name, screen);
                                    
                                    
                                }
                                
                            }

                        } while (false);
                    
                        
                    }
                }
                
                if (cfg.visuals.client.bCrosshair)
                {
                    drawList->AddLine({ io.DisplaySize.x * 0.5f - cfg.visuals.client.fCrosshair, io.DisplaySize.y * 0.5f }, { io.DisplaySize.x * 0.5f + cfg.visuals.client.fCrosshair, io.DisplaySize.y * 0.5f }, ImGui::GetColorU32(cfg.visuals.client.crosshairColor));
                    drawList->AddLine({ io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f - cfg.visuals.client.fCrosshair }, { io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f + cfg.visuals.client.fCrosshair }, ImGui::GetColorU32(cfg.visuals.client.crosshairColor));
                }

                if (cfg.visuals.client.bOxygen && localCharacter->IsInWater())
                {
                    auto drownComp = localCharacter->DrowningComponent;
                    if (!drownComp) break;
                    auto level = drownComp->GetOxygenLevel();
                    auto posX = io.DisplaySize.x * 0.5f;
                    auto posY = io.DisplaySize.y * 0.85f;
                    auto barWidth2 = io.DisplaySize.x * 0.05f;
                    auto barHeight2 = io.DisplaySize.y * 0.0030f;
                    drawList->AddRectFilled({ posX - barWidth2, posY - barHeight2 }, { posX + barWidth2, posY + barHeight2 }, ImGui::GetColorU32(IM_COL32(0, 0, 0, 255)));
                    drawList->AddRectFilled({ posX - barWidth2, posY - barHeight2 }, { posX - barWidth2 + barWidth2 * level * 2.f, posY + barHeight2 }, ImGui::GetColorU32(IM_COL32(0, 200, 255, 255)));
                }

                if (cfg.visuals.client.bCompass)
                {
               
                    const char* directions[] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" };
                    int yaw = ((int)cameraRot.Yaw + 450) % 360;
                    int index = int(yaw + 22.5f) % 360 * 0.0222222f;

                
                    FVector2D pos = { io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.02f };
                
                    Drawing::RenderText(const_cast<char*>(directions[index]), pos);
                    char buf[0x30];
                    int len = sprintf_s(buf, "%d", yaw);
                    pos.Y += 15.f;
                    Drawing::RenderText(buf, pos);
                
                
                }
            }

             
            if (aimBest.target != nullptr)
            {
                if (ImGui::IsMouseDown(1))
                {
                    // todo: do prediction to moving targets

                    auto dist = localLoc.DistTo(aimBest.location);

                    auto bulletV = localWeapon->WeaponParameters.AmmoParams.Velocity;
                    if (bulletV != 0.f)
                    {
                        auto BulletTime = dist / fabs(bulletV);
                        auto targetV = aimBest.target->GetVelocity(); // todo: somehow calculate world velocity
                        auto localV = (localLoc - localLoc_prev) * io.Framerate;
                        aimBest.location += (targetV - localV) * BulletTime;
                        aimBest.delta = UKismetMathLibrary::NormalizedDeltaRotator(UKismetMathLibrary::FindLookAtRotation(cameraLoc, aimBest.location), cameraRot);
                    }

                    auto smoothness = 1 / aimBest.smoothness;
                    localController->AddYawInput(aimBest.delta.Yaw * smoothness);
                    localController->AddPitchInput(aimBest.delta.Pitch * -smoothness);
                }

                  

               
            }
                
            
            
            if (cfg.misc.bEnable)
            {
                
            }

            localLoc_prev = localLoc;

        } while (false);


        
    }
    catch (...) 
    {
        // todo: somehow get the address where the error occurred
        Logger::Log("Exception\n");
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
   
    static bool bIsOpen = false;
    if (ImGui::IsKeyPressed(VK_INSERT)) {
        if (bIsOpen) {
            bGameInput = true;
            bIsOpen = false;
        }
        else {
            // todo: load cursor if it was removed by game
            bGameInput = false;
            bIsOpen = true;

        }
    }

    if (bIsOpen) {
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.7f), ImGuiCond_Once);
        ImGui::Begin("Menu", 0, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
        if (ImGui::BeginTabBar("Bars")) {
            if (ImGui::BeginTabItem("Visuals")) {

                ImGui::Text("Global Visuals");
                if (ImGui::BeginChild("Global", ImVec2(0.f, 38.f), true, 0))
                {
                    ImGui::Checkbox("Enable", &cfg.visuals.bEnable);
                }
                ImGui::EndChild();


                ImGui::Columns(2, "CLM1", false);
                const char* boxes[] = { "None", "2DBox", "3DBox", "DebugBox" };
                const char* bars[] = { "None", "2DRectLeft", "2DRectRight", "2DRectBottom", "2DRectTop", "2DTriangTop" };
                ImGui::Text("Players");
                if (ImGui::BeginChild("PlayersSettings", ImVec2(0.f, 365.f), true, 0))
                {
                    ImGui::Checkbox("Enable", &cfg.visuals.players.bEnable);
                    ImGui::Checkbox("Draw teammates", &cfg.visuals.players.bDrawTeam);
                    ImGui::Checkbox("Draw name", &cfg.visuals.players.bName);
                    ImGui::Checkbox("Draw skeleton", &cfg.visuals.players.bSkeleton);
                    ImGui::Combo("Box type", reinterpret_cast<int*>(&cfg.visuals.players.boxType), boxes, IM_ARRAYSIZE(boxes));
                    ImGui::Combo("Health bar type", reinterpret_cast<int*>(&cfg.visuals.players.barType), bars, IM_ARRAYSIZE(bars));
                    ImGui::ColorEdit4("Visible Enemy color", &cfg.visuals.players.enemyColorVis.x, 0);
                    ImGui::ColorEdit4("Invisible Enemy color", &cfg.visuals.players.enemyColorInv.x, 0);
                    ImGui::ColorEdit4("Visible Team color", &cfg.visuals.players.teamColorVis.x, 0);
                    ImGui::ColorEdit4("Invisible Team color", &cfg.visuals.players.teamColorInv.x, 0);
                }
                ImGui::EndChild();

                ImGui::NextColumn();

                ImGui::Text("Skeletons");
                if (ImGui::BeginChild("SkeletonsSettings", ImVec2(0.f, 365.f), true, 0))
                {
                    ImGui::Checkbox("Enable", &cfg.visuals.skeletons.bEnable);
                    ImGui::Combo("Box type", reinterpret_cast<int*>(&cfg.visuals.skeletons.boxType), boxes, IM_ARRAYSIZE(boxes));
                    ImGui::ColorEdit4("Visible Color", &cfg.visuals.skeletons.colorVis.x, 0);
                    ImGui::ColorEdit4("Invisible Color", &cfg.visuals.skeletons.colorInv.x, 0);
                    ImGui::Checkbox("Draw skeleton", &cfg.visuals.skeletons.bSkeleton);

                }
                ImGui::EndChild();
                ImGui::Columns();





                ImGui::Columns(2, "CLM2", false);

                ImGui::Text("Ships");
                if (ImGui::BeginChild("ShipsSettings", ImVec2(0.f, 220.f), true, 0)) {

                    ImGui::Checkbox("Enable", &cfg.visuals.ships.bEnable);
                    ImGui::Checkbox("Draw name", &cfg.visuals.ships.bName);
                    ImGui::Checkbox("Show holes", &cfg.visuals.ships.bDamage);

                }
                ImGui::EndChild();

                ImGui::NextColumn();

                ImGui::Text("Islands");
                if (ImGui::BeginChild("IslandsSettings", ImVec2(0.f, 220.f), true, 0)) {
                    ImGui::Checkbox("Enable", &cfg.visuals.islands.bEnable);
                    ImGui::Checkbox("Draw names", &cfg.visuals.islands.bName);
                    ImGui::SliderFloat("Max distance", &cfg.visuals.islands.fMaxDist, 100.f, 10000.f, "%.0f", ImGuiSliderFlags_AlwaysClamp);
                }
                ImGui::EndChild();
                ImGui::Columns();



                ImGui::Columns(2, "CLM3", false);
                ImGui::Text("Items");
                if (ImGui::BeginChild("ItemsSettings", ImVec2(0.f, 220.f), true, 0))
                {
                    ImGui::Checkbox("Enable", &cfg.visuals.items.bEnable);
                    ImGui::Checkbox("Draw name", &cfg.visuals.items.bName);

                }
                ImGui::EndChild();
                ImGui::NextColumn();
                ImGui::Text("Client");
                if (ImGui::BeginChild("ClientSettings", ImVec2(0.f, 220.f), true, 0))
                {

                    ImGui::Checkbox("Crosshair", &cfg.visuals.client.bCrosshair);
                    if (cfg.visuals.client.bCrosshair)
                    {
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(75.f);
                        ImGui::SliderFloat("Radius##1", &cfg.visuals.client.fCrosshair, 1.f, 100.f);
                    }

                    ImGui::ColorEdit4("Crosshair color", &cfg.visuals.client.crosshairColor.x, ImGuiColorEditFlags_DisplayRGB);

                    ImGui::Checkbox("Oxygen level", &cfg.visuals.client.bOxygen);
                    ImGui::Checkbox("Compass", &cfg.visuals.client.bCompass);

                    ImGui::Checkbox("Debug", &cfg.visuals.client.bDebug);
                    if (cfg.visuals.client.bDebug)
                    {
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(150.f);
                        ImGui::SliderFloat("Radius##2", &cfg.visuals.client.fDebug, 1.f, 1000.f);
                    }

                }
                ImGui::EndChild();
                ImGui::Columns();


                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Aim")) {

                ImGui::Text("Global Aim");
                if (ImGui::BeginChild("Global", ImVec2(0.f, 38.f), true, 0))
                {
                    ImGui::Checkbox("Enable", &cfg.aim.bEnable);
                }
                ImGui::EndChild();

                
                ImGui::Columns(2, "CLM1", false);
                ImGui::Text("Players");
                if (ImGui::BeginChild("PlayersSettings", ImVec2(0.f, 365.f), true, 0))
                {
                    // todo: add bones selection
                    ImGui::Checkbox("Enable", &cfg.aim.players.bEnable);
                    ImGui::Checkbox("Visible only", &cfg.aim.players.bVisibleOnly);
                    ImGui::Checkbox("Aim at teammates", &cfg.aim.players.bTeam);
                    ImGui::SliderFloat("Yaw", &cfg.aim.players.fYaw, 1.f, 180.f, "%.0f", ImGuiSliderFlags_AlwaysClamp);
                    ImGui::SliderFloat("Pitch", &cfg.aim.players.fPitch, 1.f, 180.f, "%.0f", ImGuiSliderFlags_AlwaysClamp);
                    ImGui::SliderFloat("Smoothness", &cfg.aim.players.fSmoothness, 1.f, 100.f, "%.0f", ImGuiSliderFlags_AlwaysClamp);
                }
                ImGui::EndChild();

                ImGui::NextColumn();

                ImGui::Text("Skeletons");
                if (ImGui::BeginChild("SkeletonsSettings", ImVec2(0.f, 365.f), true, 0))
                {
                    ImGui::Checkbox("Enable", &cfg.aim.skeletons.bEnable);
                    ImGui::Checkbox("Visible only", &cfg.aim.skeletons.bVisibleOnly);
                    ImGui::SliderFloat("Yaw", &cfg.aim.skeletons.fYaw, 1.f, 180.f, "%.0f", ImGuiSliderFlags_AlwaysClamp);
                    ImGui::SliderFloat("Pitch", &cfg.aim.skeletons.fPitch, 1.f, 180.f, "%.0f", ImGuiSliderFlags_AlwaysClamp);
                    ImGui::SliderFloat("Smoothness", &cfg.aim.skeletons.fSmoothness, 1.f, 100.f, "%.0f", ImGuiSliderFlags_AlwaysClamp);
                    

                }
                ImGui::EndChild();
                ImGui::Columns();



                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Misc")) {

                ImGui::Text("Global Misc");
                if (ImGui::BeginChild("Global", ImVec2(0.f, 38.f), true, 0))
                {
                    ImGui::Checkbox("Enable", &cfg.misc.bEnable);
                }
                ImGui::EndChild();

                ImGui::Columns(2, "CLM1", false);
                ImGui::Text("Client");
                if (ImGui::BeginChild("ClientSettings", ImVec2(0.f, 365.f), true, 0))
                {
                    if (ImGui::Button("Tests")) {
                        auto h = CreateThread(nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(Tests), nullptr, 0, nullptr);
                        if (h) CloseHandle(h);
                    }
                }
                ImGui::EndChild();


                ImGui::Columns();


                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        };
        ImGui::End();
    }
  
    context->OMSetRenderTargets(1, &renderTargetView, nullptr);
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    return PresentOriginal(swapChain, syncInterval, flags);
}

HRESULT Cheat::Renderer::ResizeHook(IDXGISwapChain* swapChain, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags)
{

    if (renderTargetView)
    {
        ImGui_ImplDX11_Shutdown();
        ImGui::DestroyContext();
        renderTargetView->Release();
        renderTargetView = nullptr;
    }
    if (context)
    {
        context->Release();
        context = nullptr;
    }
    if (device)
    {
        device->Release();
        device = nullptr;
    }
    
    return ResizeOriginal(swapChain, bufferCount, width, height, newFormat, swapChainFlags);
}

inline bool Cheat::Renderer::Init()
{
    gameWindow = FindWindowA("UnrealWindow", "Sea of Thieves");
    Logger::Log("Window: %p\n", gameWindow);
    HMODULE dxgi = GetModuleHandleA("dxgi.dll");
    Logger::Log("dxgi: %p\n", dxgi);
    static BYTE PresentSig[] = { 0x55, 0x57, 0x41, 0x56, 0x48, 0x8d, 0x6c, 0x24, 0x90, 0x48, 0x81, 0xec, 0x70, 0x01 };
    //static BYTE PresentHead[] = { 0x48, 0x89, 0x5c, 0x24, 0x10 };
    //BYTE* fnPresent = Tools::PacthFn(dxgi, PresentSig, sizeof(PresentSig), PresentHead, sizeof(PresentHead));
    BYTE* fnPresent = Tools::FindFn(dxgi, PresentSig, sizeof(PresentSig));
    Logger::Log("IDXGISwapChain::Present: %p\n", fnPresent);
    if (!fnPresent) return false;
    

    static BYTE ResizeSig[] = { 0x48, 0x81, 0xec, 0xc0, 0x00, 0x00, 0x00, 0x48, 0xc7, 0x45, 0x1f };
    //static BYTE ResizeHead[] = { 0x48, 0x8b, 0xc4, 0x55, 0x41, 0x54 };  
    //BYTE* fnResize = Tools::PacthFn(dxgi, ResizeSig, sizeof(ResizeSig), ResizeHead, sizeof(ResizeHead));
    BYTE* fnResize = Tools::FindFn(dxgi, ResizeSig, sizeof(ResizeSig));
    Logger::Log("IDXGISwapChain::ResizeBuffers: %p\n", fnResize);
    if (!fnResize) return false;
    

    if (!SetHook(fnPresent, PresentHook, reinterpret_cast<void**>(&PresentOriginal)))
    {
        return false;
    };

    Logger::Log("PresentHook: %p\n", PresentHook);

    if (!SetHook(fnResize, ResizeHook, reinterpret_cast<void**>(&ResizeOriginal)))
    {
        return false;
    };

    Logger::Log("ResizeHook: %p\n", ResizeHook);

    return true;
}

inline bool Cheat::Renderer::Remove()
{
    Renderer::RemoveInput(); 
    if (!RemoveHook(PresentOriginal) || !RemoveHook(ResizeOriginal)) 
    {
        return false;
    }
    if (renderTargetView)
    {
        ImGui_ImplDX11_Shutdown();
        ImGui::DestroyContext();
        renderTargetView->Release();
        renderTargetView = nullptr;
    }
    if (context)
    {
        context->Release();
        context = nullptr;
    }
    if (device)
    {
        device->Release();
        device = nullptr;
    }
    return true;
}

inline bool Cheat::Tools::CompareByteArray(BYTE* data, BYTE* sig, SIZE_T size)
{
    for (SIZE_T i = 0; i < size; i++) {
        if (data[i] != sig[i]) {
            if (sig[i] == 0x00) continue;
            return false;
        }
    }
    return true;
}

inline BYTE* Cheat::Tools::FindSignature(BYTE* start, BYTE* end, BYTE* sig, SIZE_T size)
{
    for (BYTE* it = start; it < end - size; it++) {
        if (CompareByteArray(it, sig, size)) {
            return it;
        };
    }
    return 0;
}


void* Cheat::Tools::FindPointer(BYTE* sig, SIZE_T size, int addition = 0)
{
    auto base = static_cast<BYTE*>(gBaseMod.lpBaseOfDll);
    auto address = FindSignature(base, base + gBaseMod.SizeOfImage - 1, sig, size);
    if (!address) return nullptr;
    auto k = 0;
    for (; sig[k]; k++);
    auto offset = *reinterpret_cast<UINT32*>(address + k);
    return address + k + 4 + offset + addition;
}

inline BYTE* Cheat::Tools::FindFn(HMODULE mod, BYTE* sig, SIZE_T sigSize)
{
    if (!mod || !sig || !sigSize) return 0;
    MODULEINFO modInfo;
    if (!K32GetModuleInformation(GetCurrentProcess(), mod, &modInfo, sizeof(MODULEINFO))) return 0;
    auto base = static_cast<BYTE*>(modInfo.lpBaseOfDll);
    auto fn = Tools::FindSignature(base, base + modInfo.SizeOfImage - 1, sig, sigSize);
    if (!fn) return 0;
    for (; *fn != 0xCC && *fn != 0xC3; fn--);
    fn++;
    return fn;
}

inline bool Cheat::Tools::PatchMem(void* address, void* bytes, SIZE_T size)
{
    DWORD oldProtection;
    if (VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &oldProtection))
    {
        memcpy(address, bytes, size);
        return VirtualProtect(address, size, oldProtection, &oldProtection);
    };
    return false;
}

/*inline bool Cheat::Tools::HookVT(void** vtable, UINT64 index, void* FuncH, void** FuncO)
{
    if (!vtable || !FuncH || !vtable[index]) return false;
    if (FuncO) { *FuncO = vtable[index]; }
    PatchMem(&vtable[index], &FuncH, 8);
    return FuncH == vtable[index];
}*/

inline BYTE* Cheat::Tools::PacthFn(HMODULE mod, BYTE* sig, SIZE_T sigSize, BYTE* bytes, SIZE_T bytesSize)
{
    if (!mod || !sig || !sigSize || !bytes || !bytesSize) return 0;
    auto fn = FindFn(mod, sig, sigSize);
    if (!fn) return 0;
    return Tools::PatchMem(fn, bytes, bytesSize) ? fn : 0;
}

/*
void Cheat::Tools::ShowErrorMsg(const CHAR* lpszFunction)
{
    CHAR* lpMsgBuf;
    CHAR* lpDisplayBuf;
    DWORD dw = GetLastError();

    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPTSTR>(&lpMsgBuf),
        0, NULL);


    lpDisplayBuf = static_cast<CHAR*>(LocalAlloc(LMEM_ZEROINIT, strlen(lpMsgBuf) + strlen(lpszFunction) + 40));
    if (!lpDisplayBuf) return;
    sprintf(lpDisplayBuf, "%s failed with error %d: %s", lpszFunction, dw, lpMsgBuf);
    MessageBoxA(nullptr, lpDisplayBuf, "Error", 0);

    LocalFree(lpMsgBuf);
    LocalFree(lpDisplayBuf);
}
*/

inline bool Cheat::Tools::FindNameArray()
{
    static BYTE sig[] = { 0x48, 0x8b, 0x3d, 0x00, 0x00, 0x00, 0x00, 0x48, 0x85, 0xff, 0x75, 0x3c };
    FName::GNames = reinterpret_cast<decltype(FName::GNames)>(FindPointer(sig, sizeof(sig)));
    if (FName::GNames) (*FName::GNames)->Resolve();
    return FName::GNames;
}

inline bool Cheat::Tools::FindObjectsArray()
{
    static BYTE sig[] = { 0x89, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8B, 0xDF, 0x48, 0x89, 0x5C, 0x24 };
    UObject::GObjects = reinterpret_cast<decltype(UObject::GObjects)>(FindPointer(sig, sizeof(sig), 16));
    return UObject::GObjects;
}

inline bool Cheat::Tools::FindWorld()
{
    static BYTE sig[] = { 0x48, 0x8B, 0x05, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8B, 0x88, 0x00, 0x00, 0x00, 0x00, 0x48, 0x85, 0xC9, 0x74, 0x06, 0x48, 0x8B, 0x49, 0x70 };
    UWorld::GWorld = reinterpret_cast<decltype(UWorld::GWorld)>(FindPointer(sig, sizeof(sig)));
    return UWorld::GWorld;
}

inline bool Cheat::Tools::InitSDK()
{
    if (!UCrewFunctions::Init()) return false;
    if (!UKismetMathLibrary::Init()) return false;
    return true;
}

inline bool Cheat::Logger::Init()
{
    char buf[MAX_PATH];
    GetModuleFileNameA(hinstDLL, buf, MAX_PATH);
    fs::path log = fs::path(buf).remove_filename() / "log.txt";
    file = CreateFileA(log.string().c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    return file != INVALID_HANDLE_VALUE;
}

inline bool Cheat::Logger::Remove()
{
    return CloseHandle(file);
}

void Cheat::Logger::Log(const char* format, ...)
{
    mutex.lock();
    time_t rawtime;
    time(&rawtime);
    auto timeinfo = localtime(&rawtime);
    char buf[MAX_PATH];
    auto size = strftime(buf, MAX_PATH, "[%d-%m-%Y %H:%M:%S] ", timeinfo);
    va_list argptr;
    va_start(argptr, format);
    size += vsprintf(buf + size, format, argptr);
    WriteFile(file, buf, size, NULL, NULL);
    va_end(argptr);
    mutex.unlock();
}

bool Cheat::Init(HINSTANCE _hinstDLL)
{
    hinstDLL = _hinstDLL;
    if (!Logger::Init())
    {
        return false;
    };
    if (!K32GetModuleInformation(GetCurrentProcess(), GetModuleHandleA(nullptr), &gBaseMod, sizeof(MODULEINFO))) 
    {
        //Tools::ShowErrorMsg("GetModuleInformation");
        return false;
    };
    Logger::Log("SoTGame.exe base: %p\n", gBaseMod.lpBaseOfDll);
    if (!Tools::FindNameArray()) 
    {
        MessageBoxA(nullptr, "Couldn't find NameArray", "Error", 0);
        return false;
    }
    Logger::Log("NameArray: %p\n", FName::GNames);
    if (!Tools::FindObjectsArray()) 
    {
        MessageBoxA(nullptr, "Couldn't find ObjectsArray", "Error", 0);
        return false;
    } 
    Logger::Log("ObjectsArray: %p\n", UObject::GObjects);
    if (!Tools::FindWorld())
    {
        MessageBoxA(nullptr, "Couldn't find World", "Error", 0);
        return false;
    }
    Logger::Log("World: %p\n", UWorld::GWorld);
    if (!Tools::InitSDK())
    {
        MessageBoxA(nullptr, "Couldn't find important objects", "Error", 0);
        return false;
    };
    if (!Renderer::Init())
    {
        MessageBoxA(nullptr, "Couldn't init renderer", "Error", 0);
        return false;
    }

    auto t = CreateThread(nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(ClearingThread), nullptr, 0, nullptr);
    if (t) CloseHandle(t);

    return true;
}

void Cheat::ClearingThread()
{
    while (true) {
        if (GetAsyncKeyState(VK_END) & 1) {
            FreeLibraryAndExitThread(hinstDLL, 0);
        }
        Sleep(25);
    }
}

void Cheat::Tests()
{
    auto world = *UWorld::GWorld;
    if (!world) return;
    auto game = world->GameInstance;
    if (!game) return;
    auto localPlayers = game->LocalPlayers;
    if (!localPlayers.Data) return;
    auto localPlayer = localPlayers[0];
    auto localController = localPlayer->PlayerController;
    if (!localController) return;
    auto localCharacter = localController->Character;
    Logger::Log("localCharacter: %p\n", localCharacter);
    if (!localCharacter) return;
    auto localMesh = localCharacter->Mesh;
    if (!localMesh) return;
    auto comp2world = localMesh->K2_GetComponentToWorld();
    Logger::Log("%f %f %f", comp2world.Scale3D.X, comp2world.Scale3D.Y, comp2world.Scale3D.Z);
}


bool Cheat::Remove()
{
    if (!Renderer::Remove()) 
    {
        return false;
    };

    Logger::Remove();

    // some other stuff...


    return true;
}
