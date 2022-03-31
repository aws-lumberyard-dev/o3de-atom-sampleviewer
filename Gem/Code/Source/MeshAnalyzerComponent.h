/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <CommonSampleComponentBase.h>


#include <AzCore/Component/EntityBus.h>
#include <AzCore/Component/TickBus.h>

#include <AzFramework/Windowing/WindowBus.h>
#include <AzFramework/Windowing/NativeWindow.h>

#include <Utils/Utils.h>
#include <Utils/ImGuiSidebar.h>
#include <Utils/ImGuiMaterialDetails.h>
#include <Utils/ImGuiAssetBrowser.h>

#include <Atom/Bootstrap/DefaultWindowBus.h>
#include <Atom/Feature/ImGui/ImGuiUtils.h>
#include <Atom/Feature/SkyBox/SkyBoxFeatureProcessorInterface.h>

namespace AtomSampleViewer
{
    class MeshComparisonWindowScene
        : public AzFramework::WindowNotificationBus::Handler
    {
    public:
        MeshComparisonWindowScene(class MeshAnalyzerComponent* parent);
        ~MeshComparisonWindowScene();

        AzFramework::NativeWindowHandle GetNativeWindowHandle();

        // AzFramework::WindowNotificationBus::Handler overrides ...
        void OnWindowClosed() override;
    private:
        MeshAnalyzerComponent* m_parent = nullptr;
        AZStd::unique_ptr<AzFramework::NativeWindow> m_nativeWindow;
        AZStd::shared_ptr<AZ::RPI::WindowContext> m_windowContext;
        AZStd::unique_ptr<AzFramework::EntityContext> m_entityContext;
        AZStd::string m_sceneName;
        AZStd::shared_ptr<AzFramework::Scene> m_frameworkScene;
        AZ::RPI::ScenePtr m_scene;
        AZ::RPI::RenderPipelinePtr m_pipeline;
        AZ::Entity* m_cameraEntity = nullptr;
    };

    class MeshAnalyzerComponent final
        : public CommonSampleComponentBase
        , public AZ::Render::Bootstrap::DefaultWindowNotificationBus::Handler
        , public AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT(MeshAnalyzerComponent, "{5A093470-A884-4D97-8DB2-EA0AC9002421}", CommonSampleComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        MeshAnalyzerComponent();
        ~MeshAnalyzerComponent() override = default;

        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        void OnChildWindowClosed();

    private:
        // AZ::TickBus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        // AZ::EntityBus::MultiHandler
        void OnEntityDestruction(const AZ::EntityId& entityId) override;

        void ModelChange();

        void CreateGroundPlane();
        void UpdateGroundPlane();
        void RemoveGroundPlane();

        void UseArcBallCameraController();
        void UseNoClipCameraController();
        void RemoveController();

        void SetArcBallControllerParams();
        void ResetCameraController();

        void DefaultWindowCreated() override;
        void CreateDiffWindow();

        AZ::RPI::RenderPipelinePtr m_originalPipeline;

        AZStd::shared_ptr<AZ::RPI::WindowContext> m_windowContext;
        AZ::Render::ImGuiActiveContextScope m_imguiScope;

        enum class CameraControllerType : int32_t 
        {
            ArcBall = 0,
            NoClip,
            Count
        };
        static const uint32_t CameraControllerCount = static_cast<uint32_t>(CameraControllerType::Count);
        static const char* CameraControllerNameTable[CameraControllerCount];
        CameraControllerType m_currentCameraControllerType = CameraControllerType::ArcBall;

        AZ::Render::MeshFeatureProcessorInterface::ModelChangedEvent::Handler m_changedHandlerA;
        AZ::Render::MeshFeatureProcessorInterface::ModelChangedEvent::Handler m_changedHandlerB;
        
        static constexpr float ArcballRadiusMinModifier = 0.01f;
        static constexpr float ArcballRadiusMaxModifier = 4.0f;
        static constexpr float ArcballRadiusDefaultModifier = 2.0f;
        
        AZ::RPI::Cullable::LodConfiguration m_lodConfig;

        bool m_enableMaterialOverride = true;

        // If false, only azmaterials generated from ".material" files will be listed.
        // Otherwise, all azmaterials, regardless of its source (e.g ".fbx"), will
        // be shown in the material list.
        bool m_showModelMaterials = false;

        bool m_showGroundPlane = false;

        bool m_cameraControllerDisabled = false;

        bool m_renderModelA = true;

        AZ::Data::Instance<AZ::RPI::Material> m_materialOverrideInstance; //< Holds a copy of the material instance being used when m_enableMaterialOverride is true.
        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_meshHandleA;
        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_meshHandleB;

        AZ::Data::Asset<AZ::RPI::ModelAsset> m_modelAssetA;
        AZ::Data::Asset<AZ::RPI::ModelAsset> m_modelAssetB;

        AZ::Data::Asset<AZ::RPI::ModelAsset> m_groundPlaneModelAsset;
        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_groundPlandMeshHandle;
        AZ::Data::Instance<AZ::RPI::Material> m_groundPlaneMaterial;

        ImGuiSidebar m_imguiSidebar;
        ImGuiMaterialDetails m_imguiMaterialDetails;
        ImGuiAssetBrowser m_materialBrowser;
        ImGuiAssetBrowser m_modelBrowserA;
        ImGuiAssetBrowser m_modelBrowserB;

        
        AZStd::unique_ptr<MeshComparisonWindowScene> m_meshComparisonScene;
    };
} // namespace AtomSampleViewer
