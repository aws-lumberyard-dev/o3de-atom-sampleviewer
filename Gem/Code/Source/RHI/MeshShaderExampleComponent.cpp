/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Atom/RHI/CommandList.h>
#include <Atom/RHI/Factory.h>
#include <Atom/RHI/FrameScheduler.h>
#include <Atom/RHI/ScopeProducerFunction.h>
#include <Atom/RHI.Reflect/RenderAttachmentLayoutBuilder.h>

#include <Atom/RPI.Reflect/Shader/ShaderAsset.h>

#include <AzCore/Math/Matrix4x4.h>
#include <AzCore/Math/Vector4.h>
#include <AzCore/Serialization/SerializeContext.h>

#include <RHI/MeshShaderExampleComponent.h>
#include <SampleComponentConfig.h>
#include <SampleComponentManager.h>
#include <Utils/Utils.h>

namespace AtomSampleViewer
{
    namespace MeshShading
    {
        const char* const ShaderInputWorldViewProj{ "WorldViewProj" };
        const char* SampleName = "MeshShaderExample";
    }

    void MeshShaderExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<MeshShaderExampleComponent, AZ::Component>()->Version(0);
        }
    }

    MeshShaderExampleComponent::MeshShaderExampleComponent()
    {
        m_supportRHISamplePipeline = true;
    }

    void MeshShaderExampleComponent::FrameBeginInternal([[maybe_unused]] AZ::RHI::FrameGraphBuilder& frameGraphBuilder)
    {
    }

    void MeshShaderExampleComponent::OnTick(float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        m_time += deltaTime;

        // Move the triangle around.
        AZ::Vector3 translation(sinf(m_time) * 0.25f, cosf(m_time) * 0.25f, 0.0f);
        m_meshDrawSrg->SetConstant(m_worldViewProjMatIndex, AZ::Matrix4x4::CreateTranslation(translation));
    }

    void MeshShaderExampleComponent::Activate()
    {
        LoadMeshShader();
        CreateMeshShaderScope();
        AZ::TickBus::Handler::BusConnect();
        AZ::RHI::RHISystemNotificationBus::Handler::BusConnect();
    }

    void MeshShaderExampleComponent::Deactivate()
    {
        m_scopeProducers.clear();
        m_windowContext = nullptr;

        AZ::TickBus::Handler::BusDisconnect();
        AZ::RHI::RHISystemNotificationBus::Handler::BusDisconnect();
    }

    void MeshShaderExampleComponent::LoadMeshShader()
    {
        using namespace AZ;
        const char* shaderFilePath = "Shaders/RHI/MeshShaders/MeshletPS.azshader";

        const auto shader = LoadShader(shaderFilePath, MeshShading::SampleName);
        if (shader == nullptr)
        {
            return;
        }

        RHI::PipelineStateDescriptorForMeshShading pipelineDesc;
        shader->GetVariant(RPI::ShaderAsset::RootShaderVariantStableId).ConfigurePipelineState(pipelineDesc);
        pipelineDesc.m_renderStates.m_rasterState.m_cullMode = RHI::CullMode::None;

        RHI::RenderAttachmentLayoutBuilder attachmentsBuilder;
        attachmentsBuilder.AddSubpass()->RenderTargetAttachment(m_outputFormat);
        [[maybe_unused]] RHI::ResultCode result =
            attachmentsBuilder.End(pipelineDesc.m_renderAttachmentConfiguration.m_renderAttachmentLayout);
        AZ_Assert(result == RHI::ResultCode::Success, "Failed to create render attachment layout");

        m_drawPipelineState = shader->AcquirePipelineState(pipelineDesc);
        if (!m_drawPipelineState)
        {
            AZ_Error(MeshShading::SampleName, false, "Failed to acquire default pipeline state for shader '%s'", shaderFilePath);
            return;
        }

        m_meshDrawSrg = CreateShaderResourceGroup(shader, "MeshShaderSRG", MeshShading::SampleName);
        FindShaderInputIndex(&m_worldViewProjMatIndex, m_meshDrawSrg, AZ::Name{ MeshShading::ShaderInputWorldViewProj }, MeshShading::SampleName);
    }

    void MeshShaderExampleComponent::CreateMeshShaderScope()
    {
        using namespace AZ;

        struct ScopeData
        {
        };

        const auto prepareFunction = [this](RHI::FrameGraphInterface frameGraph, ScopeData& scopeData)
        {
            AZ_UNUSED(scopeData);
            // Binds the swap chain as a color attachment.
            {
                RHI::ImageScopeAttachmentDescriptor descriptor;
                descriptor.m_attachmentId = m_outputAttachmentId;
                descriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Load;
                frameGraph.UseColorAttachment(descriptor);
            }
            
            // We will submit a single draw item.
            frameGraph.SetEstimatedItemCount(1);
        };

        const auto compileFunction = [this]([[maybe_unused]] const RHI::FrameGraphCompileContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            float aspectRatio = static_cast<float>(m_outputWidth / m_outputHeight);
            AZ::Vector2 scale(AZStd::min(1.0f / aspectRatio, 1.0f), AZStd::min(aspectRatio, 1.0f));
            AZ::Matrix4x4 scaleTranslate = AZ::Matrix4x4::CreateTranslation(AZ::Vector3(-0.4f, -0.4f, 0)) *
                AZ::Matrix4x4::CreateScale(AZ::Vector3(scale.GetX() * 1.f, scale.GetY() * 1.f, 1.0f));
            //m_meshDrawSrg->SetConstant(m_worldViewProjMatIndex, scaleTranslate);
            m_meshDrawSrg->Compile();
        };

        const auto executeFunction = [this](const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            RHI::CommandList* commandList = context.GetCommandList();

            // Set persistent viewport and scissor state.
            commandList->SetViewports(&m_viewport, 1);
            commandList->SetScissors(&m_scissor, 1);

            RHI::DrawMesh drawMesh;
            RHI::ShaderResourceGroup* rhiSRGS[] = { m_meshDrawSrg->GetRHIShaderResourceGroup() };

            RHI::DrawItem drawItem;
            drawItem.m_arguments = drawMesh;
            drawItem.m_pipelineState = m_drawPipelineState.get();
            drawItem.m_indexBufferView = nullptr;
            drawItem.m_shaderResourceGroupCount = 1;
            drawItem.m_shaderResourceGroups = rhiSRGS;

            // Submit the draw item.
            commandList->Submit(drawItem);
        };

        m_scopeProducers.emplace_back(
            aznew RHI::ScopeProducerFunction<ScopeData, decltype(prepareFunction), decltype(compileFunction), decltype(executeFunction)>(
                RHI::ScopeId{ "MeshShaderScope" }, ScopeData{}, prepareFunction, compileFunction, executeFunction));
    }
}
