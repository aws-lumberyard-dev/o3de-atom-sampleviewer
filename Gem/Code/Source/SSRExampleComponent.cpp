/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <SSRExampleComponent.h>
#include <Atom/Component/DebugCamera/NoClipControllerComponent.h>
#include <Atom/RPI.Public/Scene.h>
#include <Atom/RPI.Public/RPISystemInterface.h>
#include <Atom/RPI.Public/Pass/PassFilter.h>
#include <Atom/RPI.Reflect/Asset/AssetUtils.h>
#include <Atom/RPI.Reflect/Model/ModelAsset.h>
#include <Atom/RPI.Reflect/Material/MaterialAsset.h>
#include <Automation/ScriptableImGui.h>
#include <Automation/ScriptRunnerBus.h>
#include <Utils/Utils.h>

#include <SSRExampleComponent_Traits_Platform.h>

#include <Atom/RPI.Public/Pass/VRSImageGenPass.h>
#include <Atom/RPI.Public/Pass/PassFilter.h>

namespace AtomSampleViewer
{
    void SSRExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class < SSRExampleComponent, AZ::Component>()
                ->Version(0)
            ;
        }
    }

    void SSRExampleComponent::Activate()
    {
        AZ::TickBus::Handler::BusConnect();

        m_imguiSidebar.Activate();

        // setup the camera
        Camera::CameraRequestBus::EventResult(m_originalFarClipDistance, GetCameraEntityId(), &Camera::CameraRequestBus::Events::GetFarClipDistance);
        Camera::CameraRequestBus::Event(GetCameraEntityId(), &Camera::CameraRequestBus::Events::SetFarClipDistance, 180.f);

        // create scene
        CreateModels();
        CreateGroundPlane();

        InitLightingPresets(true);

        // enable the SSR pass in the pipeline
        EnableSSR(true);
    }

    void SSRExampleComponent::Deactivate()
    {
        // disable the SSR pass in the pipeline
        EnableSSR(false);

        ShutdownLightingPresets();

        GetMeshFeatureProcessor()->ReleaseMesh(m_statueMeshHandle);
        GetMeshFeatureProcessor()->ReleaseMesh(m_boxMeshHandle);
        GetMeshFeatureProcessor()->ReleaseMesh(m_shaderBallMeshHandle);
        GetMeshFeatureProcessor()->ReleaseMesh(m_groundMeshHandle);
        GetMeshFeatureProcessor()->ReleaseMesh(m_groundMeshHandle1);

        Camera::CameraRequestBus::Event(GetCameraEntityId(), &Camera::CameraRequestBus::Events::SetFarClipDistance, m_originalFarClipDistance);
        AZ::Debug::CameraControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::CameraControllerRequestBus::Events::Disable);

        AZ::TickBus::Handler::BusDisconnect();
        m_imguiSidebar.Deactivate();
    }

    void SSRExampleComponent::CreateModels()
    {
        GetMeshFeatureProcessor();

        // statue
        {
            AZ::Data::Asset<AZ::RPI::MaterialAsset> materialAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::MaterialAsset>("objects/hermanubis/hermanubis_stone.azmaterial", AZ::RPI::AssetUtils::TraceLevel::Assert);
            AZ::Data::Asset<AZ::RPI::ModelAsset> modelAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::ModelAsset>(ATOMSAMPLEVIEWER_TRAIT_SSR_SAMPLE_HERMANUBIS_MODEL_NAME, AZ::RPI::AssetUtils::TraceLevel::Assert);
            AZ::Transform transform = AZ::Transform::CreateIdentity();
            transform.SetTranslation(0.0f, 0.0f, -0.05f);

            m_statueMeshHandle = GetMeshFeatureProcessor()->AcquireMesh(AZ::Render::MeshHandleDescriptor{ modelAsset }, AZ::RPI::Material::FindOrCreate(materialAsset));
            GetMeshFeatureProcessor()->SetTransform(m_statueMeshHandle, transform);
        }

        // cube
        {
            AZ::Data::Asset<AZ::RPI::MaterialAsset> materialAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::MaterialAsset>("materials/ssrexample/cube.azmaterial", AZ::RPI::AssetUtils::TraceLevel::Assert);
            AZ::Data::Asset<AZ::RPI::ModelAsset> modelAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::ModelAsset>("objects/cube.azmodel", AZ::RPI::AssetUtils::TraceLevel::Assert);
            AZ::Transform transform = AZ::Transform::CreateIdentity();
            transform.SetTranslation(-4.5f, 0.0f, 0.49f);

            m_boxMeshHandle = GetMeshFeatureProcessor()->AcquireMesh(AZ::Render::MeshHandleDescriptor{ modelAsset }, AZ::RPI::Material::FindOrCreate(materialAsset));
            GetMeshFeatureProcessor()->SetTransform(m_boxMeshHandle, transform);
        }

        // shader ball
        {
            AZ::Data::Asset<AZ::RPI::MaterialAsset> materialAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::MaterialAsset>("Materials/Presets/PBR/default_grid.azmaterial", AZ::RPI::AssetUtils::TraceLevel::Assert);
            AZ::Data::Asset<AZ::RPI::ModelAsset> modelAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::ModelAsset>("objects/ShaderBall_simple.azmodel", AZ::RPI::AssetUtils::TraceLevel::Assert);
            AZ::Transform transform = AZ::Transform::CreateIdentity();
            transform *= AZ::Transform::CreateRotationZ(AZ::Constants::Pi);
            transform.SetTranslation(4.5f, 0.0f, 0.89f);

            m_shaderBallMeshHandle = GetMeshFeatureProcessor()->AcquireMesh(AZ::Render::MeshHandleDescriptor{ modelAsset }, AZ::RPI::Material::FindOrCreate(materialAsset));
            GetMeshFeatureProcessor()->SetTransform(m_shaderBallMeshHandle, transform);
        }
    }

    void SSRExampleComponent::CreateGroundPlane()
    {
        AZ::Render::MeshFeatureProcessorInterface* meshFeatureProcessor = GetMeshFeatureProcessor();
        if (m_groundMeshHandle.IsValid())
        {
            meshFeatureProcessor->ReleaseMesh(m_groundMeshHandle);
        }

		if (m_groundMeshHandle1.IsValid())
		{
			meshFeatureProcessor->ReleaseMesh(m_groundMeshHandle1);
		}

        // load material
        AZStd::string materialName;
        switch (m_groundPlaneMaterial)
        {
        case 0:
            materialName = AZStd::string::format("materials/ssrexample/groundplanechrome.azmaterial");
            break;
        case 1:
            materialName = AZStd::string::format("materials/ssrexample/groundplanealuminum.azmaterial");
            break;
        case 2:
            materialName = AZStd::string::format("materials/presets/pbr/default_grid.azmaterial");
            break;
        default:
            materialName = AZStd::string::format("materials/ssrexample/groundplanemirror.azmaterial");
            break;
        }

        AZ::Data::AssetId groundMaterialAssetId = AZ::RPI::AssetUtils::GetAssetIdForProductPath(materialName.c_str(), AZ::RPI::AssetUtils::TraceLevel::Error);
        m_groundMaterialAsset.Create(groundMaterialAssetId);

		///m_groundMaterialAsset1.Create(groundMaterialAssetId);

        // load mesh
        AZ::Data::Asset<AZ::RPI::ModelAsset> planeModel = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::ModelAsset>("objects/plane.azmodel", AZ::RPI::AssetUtils::TraceLevel::Error);
        AZ::Data::Asset<AZ::RPI::ModelAsset> planeModel1 = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::ModelAsset>("objects/plane.azmodel", AZ::RPI::AssetUtils::TraceLevel::Error);
        m_groundMeshHandle = GetMeshFeatureProcessor()->AcquireMesh(AZ::Render::MeshHandleDescriptor{ planeModel }, AZ::RPI::Material::FindOrCreate(m_groundMaterialAsset));
		//m_groundMeshHandle1 = GetMeshFeatureProcessor()->AcquireMesh(AZ::Render::MeshHandleDescriptor{ planeModel1 }, AZ::RPI::Material::FindOrCreate(m_groundMaterialAsset1));

        AZ::Transform transform = AZ::Transform::CreateIdentity();
        const AZ::Vector3 nonUniformScale(50.0f, 50.0f, 1.0f);
        GetMeshFeatureProcessor()->SetTransform(m_groundMeshHandle, transform, nonUniformScale);

		//AZ::Transform transform1 = AZ::Transform::CreateIdentity();
        //transform1 *= AZ::Transform::CreateRotationY(AZ::Constants::Pi);
		//const AZ::Vector3 nonUniformScale1(15.0f, 15.0f, 1.0f);
		//GetMeshFeatureProcessor()->SetTransform(m_groundMeshHandle1, transform1, nonUniformScale1);
    }

    void SSRExampleComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint timePoint)
    {
        if (m_resetCamera)
        {
            AZ::Debug::CameraControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::CameraControllerRequestBus::Events::Reset);
            AZ::TransformBus::Event(GetCameraEntityId(), &AZ::TransformBus::Events::SetWorldTranslation, AZ::Vector3(7.5f, -10.5f, 3.0f));
            AZ::Debug::CameraControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::CameraControllerRequestBus::Events::Enable, azrtti_typeid<AZ::Debug::NoClipControllerComponent>());
            AZ::Debug::NoClipControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::NoClipControllerRequests::SetHeading, AZ::DegToRad(22.5f));
            AZ::Debug::NoClipControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::NoClipControllerRequests::SetPitch, AZ::DegToRad(-10.0f));
            m_resetCamera = false;
        }

        DrawSidebar();
    }

	void AddUiButton(const char* s, ImVec4 col, bool sameLine = true)
	{
		ImGui::ColorButton(s, col);
		// set tooltip
		if (ImGui::IsItemHovered())	ImGui::SetTooltip("Color Overlay for VRS rate of %s", s);
		// disable drag and drop
		if (ImGui::IsItemActive() && ImGui::BeginDragDropSource())ImGui::EndDragDropSource();

		// add text in same line
		ImGui::SameLine();
		ImGui::Text("%s", s);

		// convenience in case next item should be in same line, too:
		if (sameLine) ImGui::SameLine();
	}

    void SSRExampleComponent::DrawSidebar()
    {
        if (!m_imguiSidebar.Begin())
        {
            return;
        }

        ImGui::NewLine();
        if (ImGui::Checkbox("Enable SSR", &m_enableSSR))
        {
            EnableSSR(m_enableSSR);
        }

        ImGui::NewLine();
        ImGui::Text("Ground Plane Material");
        bool materialChanged = false;
        materialChanged |= ScriptableImGui::RadioButton("Chrome", &m_groundPlaneMaterial, 0);
        materialChanged |= ScriptableImGui::RadioButton("Aluminum", &m_groundPlaneMaterial, 1);
        materialChanged |= ScriptableImGui::RadioButton("Default Grid", &m_groundPlaneMaterial, 2);
        materialChanged |= ScriptableImGui::RadioButton("Mirror", &m_groundPlaneMaterial, 3);
        if (materialChanged)
        {
            CreateGroundPlane();
        }

        ImGui::NewLine();
        ImGuiLightingPreset();

		ImGui::Indent();
		AddUiButton("1x1", ImVec4(1.0f, 0.0f, 0, 0));
		AddUiButton("1x2", ImVec4(1.0f, 1.0f, 0, 0));
		AddUiButton("2x1", ImVec4(1.0f, 0.5f, 0, 0));
		AddUiButton("2x2", ImVec4(0.0f, 1.0f, 0, 0), false);

		//if (m_node->AdditionalShadingRates())
		{
			AddUiButton("2x4", ImVec4(0.5f, 0.5f, 1.0f, 0));
			AddUiButton("4x2", ImVec4(1.0f, 0.5f, 1.0f, 0));
			AddUiButton("4x4", ImVec4(0.0f, 1.0f, 1.0f, 0), false);
		}
		ImGui::Unindent();

        //const AZ::RPI::RenderPipelinePtr renderPipeline = m_scene->GetDefaultRenderPipeline();
		AZ::RPI::PassFilter vrsImageGenPassFilter = AZ::RPI::PassFilter::CreateWithPassName(AZ::Name{ "VRSImageGenPass" }, m_scene);
		AZ::RPI::Ptr<AZ::RPI::Pass> vrsImageGenPass = AZ::RPI::PassSystemInterface::Get()->FindFirstPass(vrsImageGenPassFilter);
        if (vrsImageGenPass)
        {
            if (ImGui::SliderFloat("Luminance Variance cut off", &m_varianceCutOff, 0.0f, 0.1f))
            {
                AZ::RPI::VRSImageGenPass* temp = static_cast<AZ::RPI::VRSImageGenPass*>(vrsImageGenPass.get());
                temp->m_VarianceCutoff = m_varianceCutOff;
            }
            if (ImGui::SliderFloat("Motion Variance cut off", &m_motionFactor, 0.0f, 0.1f))
            {
                AZ::RPI::VRSImageGenPass* temp = static_cast<AZ::RPI::VRSImageGenPass*>(vrsImageGenPass.get());
                temp->m_MotionFactor = m_motionFactor;
            }
        }
        m_imguiSidebar.End();
    }

    void SSRExampleComponent::EnableSSR(bool enabled)
    {
        // set screen space pass
        {
            AZ::RPI::PassFilter passFilter = AZ::RPI::PassFilter::CreateWithPassName(AZ::Name("ReflectionScreenSpacePass"), (AZ::RPI::Scene*) nullptr);
            AZ::RPI::PassSystemInterface::Get()->ForEachPass(passFilter, [enabled](AZ::RPI::Pass* pass) -> AZ::RPI::PassFilterExecutionFlow
                {
                    pass->SetEnabled(enabled);
                    return  AZ::RPI::PassFilterExecutionFlow::ContinueVisitingPasses;
                });
        }

        // set copy frame buffer pass
        {
            AZ::RPI::PassFilter passFilter = AZ::RPI::PassFilter::CreateWithPassName(AZ::Name("ReflectionCopyFrameBufferPass"), (AZ::RPI::Scene*) nullptr);
            AZ::RPI::PassSystemInterface::Get()->ForEachPass(passFilter, [enabled](AZ::RPI::Pass* pass) -> AZ::RPI::PassFilterExecutionFlow
                {
                    pass->SetEnabled(enabled);
                    return  AZ::RPI::PassFilterExecutionFlow::ContinueVisitingPasses;
                });
        }
    }
}
