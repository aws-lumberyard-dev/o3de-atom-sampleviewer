/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <MeshAnalyzerComponent.h>

#include <Atom/Component/DebugCamera/ArcBallControllerComponent.h>
#include <Atom/Component/DebugCamera/NoClipControllerComponent.h>
#include <Atom/Component/DebugCamera/CameraComponent.h>

#include <Atom/RHI/Device.h>
#include <Atom/RHI/Factory.h>

#include <Atom/RPI.Public/View.h>

#include <Atom/RPI.Reflect/Model/ModelAsset.h>
#include <Atom/RPI.Reflect/Material/MaterialAsset.h>
#include <Atom/RPI.Reflect/Asset/AssetUtils.h>

#include <AzCore/Asset/AssetManagerBus.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/IO/IOUtils.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/smart_ptr/make_shared.h>
#include <AzCore/std/sort.h>

#include <AzFramework/Components/TransformComponent.h>
#include <AzFramework/Input/Devices/Mouse/InputDeviceMouse.h>

#include <SampleComponentManager.h>
#include <SampleComponentConfig.h>
#include <EntityUtilityFunctions.h>

#include <Automation/ScriptableImGui.h>
#include <Automation/ScriptRunnerBus.h>

#include <RHI/BasicRHIComponent.h>

namespace AtomSampleViewer
{
    MeshComparisonWindowScene::MeshComparisonWindowScene(MeshAnalyzerComponent* parent)
    {
        m_sceneName = "DiffScene";
        m_parent = parent;

        // Create a new EntityContext and AzFramework::Scene, and link them together via SetSceneForEntityContextId
        m_entityContext = AZStd::make_unique<AzFramework::EntityContext>();
        m_entityContext->InitContext();

        // Create the scene
        auto sceneSystem = AzFramework::SceneSystemInterface::Get();
        AZ_Assert(sceneSystem, "Unable to retrieve scene system.");
        Outcome<AZStd::shared_ptr<AzFramework::Scene>, AZStd::string> createSceneOutcome = sceneSystem->CreateScene(m_sceneName);
        AZ_Assert(createSceneOutcome, "%s", createSceneOutcome.GetError().data());
        m_frameworkScene = createSceneOutcome.TakeValue();
        m_frameworkScene->SetSubsystem<AzFramework::EntityContext::SceneStorageType>(m_entityContext.get());

        // Create a NativeWindow and WindowContext
        m_nativeWindow =
            AZStd::make_unique<AzFramework::NativeWindow>("Multi Scene: Second Window", AzFramework::WindowGeometry(0, 0, 1280, 720));
        m_nativeWindow->Activate();
        RHI::Ptr<RHI::Device> device = RHI::RHISystemInterface::Get()->GetDevice();
        m_windowContext = AZStd::make_shared<RPI::WindowContext>();
        m_windowContext->Initialize(*device, m_nativeWindow->GetWindowHandle());

        // Create the RPI::Scene with all feature processors enabled
        RPI::SceneDescriptor sceneDesc;
        sceneDesc.m_nameId = AZ::Name("DiffScene");
        m_scene = RPI::Scene::CreateScene(sceneDesc);
        m_scene->EnableAllFeatureProcessors();

        // Link our RPI::Scene to the AzFramework::Scene
        m_frameworkScene->SetSubsystem(m_scene);

        // Create a custom pipeline descriptor
        RPI::RenderPipelineDescriptor pipelineDesc;
        pipelineDesc.m_mainViewTagName = "MainCamera"; // Surface shaders render to the "MainCamera" tag
        pipelineDesc.m_name = "MeshComparisonPipeline"; // Sets the debug name for this pipeline
        pipelineDesc.m_rootPassTemplate = "MainPipeline"; // References a template in AtomSampleViewer\Passes\MainPipeline.pass
        pipelineDesc.m_renderSettings.m_multisampleState.m_samples = 4;
        m_pipeline = RPI::RenderPipeline::CreateRenderPipelineForWindow(pipelineDesc, *m_windowContext);

        m_scene->AddRenderPipeline(m_pipeline);
        m_scene->Activate();
        RPI::RPISystemInterface::Get()->RegisterScene(m_scene);

        // Create a camera entity, hook it up to the RenderPipeline
        m_cameraEntity = CreateEntity("WindowedSceneCamera", m_entityContext->GetContextId());
        Debug::CameraComponentConfig cameraConfig(m_windowContext);
        cameraConfig.m_fovY = Constants::HalfPi;
        m_cameraEntity->CreateComponent(azrtti_typeid<Debug::CameraComponent>())->SetConfiguration(cameraConfig);
        m_cameraEntity->CreateComponent(azrtti_typeid<AzFramework::TransformComponent>());
        m_cameraEntity->CreateComponent(azrtti_typeid<Debug::NoClipControllerComponent>());
        m_cameraEntity->Init();
        m_cameraEntity->Activate();
        m_pipeline->SetDefaultViewFromEntity(m_cameraEntity->GetId());

        AzFramework::WindowNotificationBus::Handler::BusConnect(m_nativeWindow->GetWindowHandle());
    }

    MeshComparisonWindowScene::~MeshComparisonWindowScene()
    {
        using namespace AZ;

        // Disconnect the busses
        AzFramework::WindowNotificationBus::Handler::BusDisconnect(m_nativeWindow->GetWindowHandle());

        DestroyEntity(m_cameraEntity);

        m_frameworkScene->UnsetSubsystem<RPI::Scene>();

        // Remove the scene
        m_scene->Deactivate();
        m_scene->RemoveRenderPipeline(m_pipeline->GetId());
        RPI::RPISystemInterface::Get()->UnregisterScene(m_scene);
        auto sceneSystem = AzFramework::SceneSystemInterface::Get();
        AZ_Assert(sceneSystem, "Scene system wasn't found to remove scene '%s' from.", m_sceneName.c_str());
        [[maybe_unused]] bool sceneRemovedSuccessfully = sceneSystem->RemoveScene(m_sceneName);
        AZ_Assert(sceneRemovedSuccessfully, "Unable to remove scene '%s'.", m_sceneName.c_str());
        m_scene = nullptr;

        m_windowContext->Shutdown();
    }

    
    void MeshComparisonWindowScene::OnWindowClosed()
    {
        m_parent->OnChildWindowClosed();
    }

    AzFramework::NativeWindowHandle MeshComparisonWindowScene::GetNativeWindowHandle()
    {
        if (m_nativeWindow)
        {
            return m_nativeWindow->GetWindowHandle();
        }
        else
        {
            return nullptr;
        }
    }

    const char* MeshAnalyzerComponent::CameraControllerNameTable[CameraControllerCount] =
    {
        "ArcBall",
        "NoClip"
    };

    void MeshAnalyzerComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<MeshAnalyzerComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    MeshAnalyzerComponent::MeshAnalyzerComponent()
        : m_materialBrowser("@user@/MeshAnalyzerComponent/material_browser.xml")
        , m_modelBrowserA("@user@/MeshAnalyzerComponent/model_browserA.xml")
        , m_modelBrowserB("@user@/MeshAnalyzerComponent/model_browserB.xml")
        , m_imguiSidebar("@user@/MeshAnalyzerComponent/sidebar.xml")
    {
        m_changedHandlerA = AZ::Render::MeshFeatureProcessorInterface::ModelChangedEvent::Handler
        {
            [&](AZ::Data::Instance<AZ::RPI::Model> model)
            {
                ScriptRunnerRequestBus::Broadcast(&ScriptRunnerRequests::ResumeScript);

                // This handler will be connected to the feature processor so that when the model is updated, the camera
                // controller will reset. This ensures the camera is a reasonable distance from the model when it resizes.
                ResetCameraController();

                UpdateGroundPlane();
            }
        };

        m_changedHandlerB = AZ::Render::MeshFeatureProcessorInterface::ModelChangedEvent::Handler{
            [&](AZ::Data::Instance<AZ::RPI::Model> model)
            {
                ScriptRunnerRequestBus::Broadcast(&ScriptRunnerRequests::ResumeScript);

                // This handler will be connected to the feature processor so that when the model is updated, the camera
                // controller will reset. This ensures the camera is a reasonable distance from the model when it resizes.
                ResetCameraController();

                UpdateGroundPlane();
            }
        };
    }

    void MeshAnalyzerComponent::DefaultWindowCreated()
    {
        AZ::Render::Bootstrap::DefaultWindowBus::BroadcastResult(m_windowContext, &AZ::Render::Bootstrap::DefaultWindowBus::Events::GetDefaultWindowContext);
    }

    void MeshAnalyzerComponent::OnChildWindowClosed()
    {
        m_meshComparisonScene = nullptr;
    }

    void MeshAnalyzerComponent::Activate()
    {
        UseArcBallCameraController();

        m_materialBrowser.SetFilter([this](const AZ::Data::AssetInfo& assetInfo)
        {
            if (!AzFramework::StringFunc::Path::IsExtension(assetInfo.m_relativePath.c_str(), "azmaterial"))
            {
                return false;
            }
            if (m_showModelMaterials)
            {
                return true;
            }
            // Return true only if the azmaterial was generated from a ".material" file.
            // Materials with subid == 0, are 99.99% guaranteed to be generated from a ".material" file.
            // Without this assurance We would need to call  AzToolsFramework::AssetSystem::AssetSystemRequest::GetSourceInfoBySourceUUID()
            // to figure out what's the source of this azmaterial. But, Atom can not include AzToolsFramework.
            return assetInfo.m_assetId.m_subId == 0;
        });

        m_modelBrowserA.SetFilter([](const AZ::Data::AssetInfo& assetInfo)
        {
            return assetInfo.m_assetType == azrtti_typeid<AZ::RPI::ModelAsset>();
        });

        m_modelBrowserB.SetFilter(
            [](const AZ::Data::AssetInfo& assetInfo)
            {
                return assetInfo.m_assetType == azrtti_typeid<AZ::RPI::ModelAsset>();
            });

        m_materialBrowser.Activate();
        m_modelBrowserA.Activate();
        m_modelBrowserB.Activate();
        m_imguiSidebar.Activate();

        InitLightingPresets(true);

        AZ::Data::Asset<AZ::RPI::MaterialAsset> groundPlaneMaterialAsset = AZ::RPI::AssetUtils::LoadAssetByProductPath<AZ::RPI::MaterialAsset>(DefaultPbrMaterialPath, AZ::RPI::AssetUtils::TraceLevel::Error);
        m_groundPlaneMaterial = AZ::RPI::Material::FindOrCreate(groundPlaneMaterialAsset);
        m_groundPlaneModelAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::ModelAsset>("objects/plane.azmodel", AZ::RPI::AssetUtils::TraceLevel::Assert);

        AZ::TickBus::Handler::BusConnect();
        AZ::Render::Bootstrap::DefaultWindowNotificationBus::Handler::BusConnect();
        
        if (!m_meshComparisonScene)
        {
            m_meshComparisonScene = AZStd::make_unique<MeshComparisonWindowScene>(this);
        }
    }

    void MeshAnalyzerComponent::Deactivate()
    {
        if (m_meshComparisonScene)
        {
            m_meshComparisonScene = nullptr;
        }

        AZ::Render::Bootstrap::DefaultWindowNotificationBus::Handler::BusDisconnect();
        AZ::TickBus::Handler::BusDisconnect();

        m_imguiSidebar.Deactivate();

        m_materialBrowser.Deactivate();
        m_modelBrowserA.Deactivate();
        m_modelBrowserB.Deactivate();

        RemoveController();
        
        GetMeshFeatureProcessor()->ReleaseMesh(m_meshHandleA);
        GetMeshFeatureProcessor()->ReleaseMesh(m_meshHandleB);
        GetMeshFeatureProcessor()->ReleaseMesh(m_groundPlandMeshHandle);

        m_modelAssetA = {};
        m_modelAssetB = {};

        m_groundPlaneModelAsset = {};

        m_materialOverrideInstance = nullptr;

        ShutdownLightingPresets();
    }

    void MeshAnalyzerComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        bool modelNeedsUpdate = false;

        if (m_imguiSidebar.Begin())
        {
            ImGuiLightingPreset();

            ImGuiAssetBrowser::WidgetSettings assetBrowserSettings;

            bool modelVisibilityNeedsUpdate = false;
            modelVisibilityNeedsUpdate = ScriptableImGui::Checkbox("Show original model", &m_renderModelA);

            modelNeedsUpdate |= ScriptableImGui::Checkbox("Enable Material Override", &m_enableMaterialOverride);
           
            if (ScriptableImGui::Checkbox("Show Ground Plane", &m_showGroundPlane))
            {
                if (m_showGroundPlane)
                {
                    CreateGroundPlane();
                    UpdateGroundPlane();
                }
                else
                {
                    RemoveGroundPlane();
                }
            }

            if (ScriptableImGui::Checkbox("Show Model Materials", &m_showModelMaterials))
            {
                modelNeedsUpdate = true;
                m_materialBrowser.SetNeedsRefresh();
            }

            assetBrowserSettings.m_labels.m_root = "Materials";
            modelNeedsUpdate |= m_materialBrowser.Tick(assetBrowserSettings);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            assetBrowserSettings.m_labels.m_root = "Models";
            bool modelChanged = m_modelBrowserA.Tick(assetBrowserSettings);
            modelChanged |= m_modelBrowserB.Tick(assetBrowserSettings);
            modelNeedsUpdate |= modelChanged;

            if (modelChanged)
            {
                // Reset LOD override when the model changes.
                m_lodConfig.m_lodType = AZ::RPI::Cullable::LodType::Default;
            }

            AZ::Data::Instance<AZ::RPI::Model> modelA = GetMeshFeatureProcessor()->GetModel(m_meshHandleA);
            AZ::Data::Instance<AZ::RPI::Model> modelB = GetMeshFeatureProcessor()->GetModel(m_meshHandleB);
            if (modelA && modelB)
            {
                const char* NoLodOverrideText = "No LOD Override";
                const char* LodFormatString = "LOD %i";

                AZStd::string previewText = m_lodConfig.m_lodType == AZ::RPI::Cullable::LodType::Default ? 
                    NoLodOverrideText : 
                    AZStd::string::format(LodFormatString, m_lodConfig.m_lodOverride);

                if (ScriptableImGui::BeginCombo("", previewText.c_str()))
                {
                    if (ScriptableImGui::Selectable(NoLodOverrideText, m_lodConfig.m_lodType == AZ::RPI::Cullable::LodType::Default))
                    {
                        m_lodConfig.m_lodType = AZ::RPI::Cullable::LodType::Default;
                        GetMeshFeatureProcessor()->SetMeshLodConfiguration(m_meshHandleA, m_lodConfig);
                        GetMeshFeatureProcessor()->SetMeshLodConfiguration(m_meshHandleB, m_lodConfig);
                    }

                    for (uint32_t i = 0; i < modelA->GetLodCount(); ++i)
                    {
                        AZStd::string name = AZStd::string::format(LodFormatString, i);
                        if (ScriptableImGui::Selectable(name.c_str(), m_lodConfig.m_lodOverride == i))
                        {
                            m_lodConfig.m_lodType = AZ::RPI::Cullable::LodType::SpecificLod;
                            m_lodConfig.m_lodOverride = static_cast<AZ::RPI::Cullable::LodOverride>(i);
                            GetMeshFeatureProcessor()->SetMeshLodConfiguration(m_meshHandleA, m_lodConfig);
                        }
                    }
                    for (uint32_t i = 0; i < modelB->GetLodCount(); ++i)
                    {
                        AZStd::string name = AZStd::string::format(LodFormatString, i);
                        if (ScriptableImGui::Selectable(name.c_str(), m_lodConfig.m_lodOverride == i))
                        {
                            m_lodConfig.m_lodType = AZ::RPI::Cullable::LodType::SpecificLod;
                            m_lodConfig.m_lodOverride = static_cast<AZ::RPI::Cullable::LodOverride>(i);
                            GetMeshFeatureProcessor()->SetMeshLodConfiguration(m_meshHandleB, m_lodConfig);
                        }
                    }
                    ScriptableImGui::EndCombo();
                }

                if(modelVisibilityNeedsUpdate)
                {
                    GetMeshFeatureProcessor()->SetVisible(m_meshHandleA, m_renderModelA);
                    GetMeshFeatureProcessor()->SetVisible(m_meshHandleB, !m_renderModelA);
                }
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Camera controls
            {
                int32_t* currentControllerTypeIndex = reinterpret_cast<int32_t*>(&m_currentCameraControllerType);

                ImGui::LabelText("##CameraControllerLabel", "Camera Controller:");
                if (ScriptableImGui::Combo("##CameraController", currentControllerTypeIndex, CameraControllerNameTable, CameraControllerCount))
                {
                    ResetCameraController();
                }
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (m_materialOverrideInstance && ImGui::Button("Material Details..."))
            {
                m_imguiMaterialDetails.SetMaterial(m_materialOverrideInstance);
                m_imguiMaterialDetails.OpenDialog();
            }

            m_imguiSidebar.End();
        }

        m_imguiMaterialDetails.Tick();

        if (modelNeedsUpdate)
        {
            ModelChange();
        }
    }

    void MeshAnalyzerComponent::ModelChange()
    {
        if (!m_modelBrowserA.GetSelectedAssetId().IsValid() || !m_modelBrowserB.GetSelectedAssetId().IsValid())
        {
            m_modelAssetA = {};
            m_modelAssetB = {};

            GetMeshFeatureProcessor()->ReleaseMesh(m_meshHandleA);
            GetMeshFeatureProcessor()->ReleaseMesh(m_meshHandleB);
            return;
        }

        // If a material hasn't been selected, just choose the first one
        // If for some reason no materials are available log an error
        AZ::Data::AssetId selectedMaterialAssetId = m_materialBrowser.GetSelectedAssetId();
        if (!selectedMaterialAssetId.IsValid())
        {
            selectedMaterialAssetId = AZ::RPI::AssetUtils::GetAssetIdForProductPath(DefaultPbrMaterialPath, AZ::RPI::AssetUtils::TraceLevel::Error);

            if (!selectedMaterialAssetId.IsValid())
            {
                AZ_Error("MeshAnalyzerComponent", false, "Failed to select model, no material available to render with.");
                return;
            }
        }

        if (m_enableMaterialOverride && selectedMaterialAssetId.IsValid())
        {
            AZ::Data::Asset<AZ::RPI::MaterialAsset> materialAsset;
            materialAsset.Create(selectedMaterialAssetId);
            m_materialOverrideInstance = AZ::RPI::Material::FindOrCreate(materialAsset);
        }
        else
        {
            m_materialOverrideInstance = nullptr;
        }

        if (m_modelAssetA.GetId() != m_modelBrowserA.GetSelectedAssetId())
        {
            ScriptRunnerRequestBus::Broadcast(&ScriptRunnerRequests::PauseScript);

            m_modelAssetA.Create(m_modelBrowserA.GetSelectedAssetId());

            GetMeshFeatureProcessor()->ReleaseMesh(m_meshHandleA);
            m_meshHandleA =
                GetMeshFeatureProcessor()->AcquireMesh(AZ::Render::MeshHandleDescriptor{ m_modelAssetA }, m_materialOverrideInstance);
            GetMeshFeatureProcessor()->SetTransform(m_meshHandleA, AZ::Transform::CreateIdentity());
            GetMeshFeatureProcessor()->ConnectModelChangeEventHandler(m_meshHandleA, m_changedHandlerA);
            GetMeshFeatureProcessor()->SetMeshLodConfiguration(m_meshHandleA, m_lodConfig);
            GetMeshFeatureProcessor()->SetVisible(m_meshHandleA, m_renderModelA);
        }
        else
        {
            GetMeshFeatureProcessor()->SetMaterialAssignmentMap(m_meshHandleA, m_materialOverrideInstance);

        }

        if (m_modelAssetB.GetId() != m_modelBrowserB.GetSelectedAssetId())
        {
            m_modelAssetB.Create(m_modelBrowserB.GetSelectedAssetId());
            GetMeshFeatureProcessor()->ReleaseMesh(m_meshHandleB);
            m_meshHandleB =
                GetMeshFeatureProcessor()->AcquireMesh(AZ::Render::MeshHandleDescriptor{ m_modelAssetB }, m_materialOverrideInstance);
            GetMeshFeatureProcessor()->SetTransform(m_meshHandleB, AZ::Transform::CreateIdentity());
            GetMeshFeatureProcessor()->ConnectModelChangeEventHandler(m_meshHandleB, m_changedHandlerB);
            GetMeshFeatureProcessor()->SetMeshLodConfiguration(m_meshHandleB, m_lodConfig);
            GetMeshFeatureProcessor()->SetVisible(m_meshHandleA, !m_renderModelA);
        }
        else
        {
            GetMeshFeatureProcessor()->SetMaterialAssignmentMap(m_meshHandleB, m_materialOverrideInstance);
        }
    }
    
    void MeshAnalyzerComponent::CreateGroundPlane()
    {
        m_groundPlandMeshHandle = GetMeshFeatureProcessor()->AcquireMesh(AZ::Render::MeshHandleDescriptor{ m_groundPlaneModelAsset }, m_groundPlaneMaterial);
    }

    void MeshAnalyzerComponent::UpdateGroundPlane()
    {
        if (m_groundPlandMeshHandle.IsValid())
        {
            AZ::Transform groundPlaneTransform = AZ::Transform::CreateIdentity();

            if (m_modelAssetA)
            {
                AZ::Vector3 modelCenter;
                float modelRadius;
                m_modelAssetA->GetAabb().GetAsSphere(modelCenter, modelRadius);

                static const float GroundPlaneRelativeScale = 4.0f;
                static const float GroundPlaneOffset = 0.01f;

                groundPlaneTransform.SetUniformScale(GroundPlaneRelativeScale * modelRadius);
                groundPlaneTransform.SetTranslation(AZ::Vector3(0.0f, 0.0f, m_modelAssetA->GetAabb().GetMin().GetZ() - GroundPlaneOffset));
            }

            GetMeshFeatureProcessor()->SetTransform(m_groundPlandMeshHandle, groundPlaneTransform);
        }
    }

    void MeshAnalyzerComponent::RemoveGroundPlane()
    {
        GetMeshFeatureProcessor()->ReleaseMesh(m_groundPlandMeshHandle);
    }

    void MeshAnalyzerComponent::OnEntityDestruction(const AZ::EntityId& entityId)
    {
        OnLightingPresetEntityShutdown(entityId);
        AZ::EntityBus::MultiHandler::BusDisconnect(entityId);
    }

    void MeshAnalyzerComponent::UseArcBallCameraController()
    {
        AZ::Debug::CameraControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::CameraControllerRequestBus::Events::Enable,
            azrtti_typeid<AZ::Debug::ArcBallControllerComponent>());
    }

    void MeshAnalyzerComponent::UseNoClipCameraController()
    {
        AZ::Debug::CameraControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::CameraControllerRequestBus::Events::Enable,
            azrtti_typeid<AZ::Debug::NoClipControllerComponent>());
    }

    void MeshAnalyzerComponent::RemoveController()
    {
        AZ::Debug::CameraControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::CameraControllerRequestBus::Events::Disable);
    }

    void MeshAnalyzerComponent::SetArcBallControllerParams()
    {
        // Just use model A for the controls. We want the same camera controls on the views
        // of each model since we want the camera to be in sync
        if (!m_modelBrowserA.GetSelectedAssetId().IsValid() || !m_modelAssetA.IsReady())
        {
            return;
        }

        // Adjust the arc-ball controller so that it has bounds that make sense for the current model
        
        AZ::Vector3 center;
        float radius;
        m_modelAssetA->GetAabb().GetAsSphere(center, radius);

        const float startingDistance = radius * ArcballRadiusDefaultModifier;
        const float minDistance = radius * ArcballRadiusMinModifier;
        const float maxDistance = radius * ArcballRadiusMaxModifier;

        AZ::Debug::ArcBallControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::ArcBallControllerRequestBus::Events::SetCenter, center);
        AZ::Debug::ArcBallControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::ArcBallControllerRequestBus::Events::SetDistance, startingDistance);
        AZ::Debug::ArcBallControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::ArcBallControllerRequestBus::Events::SetMinDistance, minDistance);
        AZ::Debug::ArcBallControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::ArcBallControllerRequestBus::Events::SetMaxDistance, maxDistance);
    }
    void MeshAnalyzerComponent::ResetCameraController()
    {
        RemoveController();
        if (m_currentCameraControllerType == CameraControllerType::ArcBall)
        {
            UseArcBallCameraController();
            SetArcBallControllerParams();
        }
        else if (m_currentCameraControllerType == CameraControllerType::NoClip)
        {
            UseNoClipCameraController();
        }
    }
} // namespace AtomSampleViewer
