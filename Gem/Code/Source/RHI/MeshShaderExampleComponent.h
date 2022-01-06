/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <Atom/RHI/DrawItem.h>
#include <Atom/RHI/ScopeProducer.h>

#include <Atom/RHI/RHISystemInterface.h>
#include <Atom/RPI.Public/Shader/Shader.h>
#include <Atom/RPI.Public/Shader/ShaderResourceGroup.h>

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>

#include <RHI/BasicRHIComponent.h>

namespace AtomSampleViewer
{    

    class MeshShaderExampleComponent final
        : public BasicRHIComponent
        , public AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT(MeshShaderExampleComponent, "{D0846A4D-0FBC-4B39-96D6-B3CA6823270F}", AZ::Component);
        AZ_DISABLE_COPY(MeshShaderExampleComponent);

        static void Reflect(AZ::ReflectContext* context);

        MeshShaderExampleComponent();
        ~MeshShaderExampleComponent() = default;

    protected:

        // AZ::Component
        void Activate() override;
        void Deactivate() override;
        void FrameBeginInternal(AZ::RHI::FrameGraphBuilder& frameGraphBuilder) override;

        // TickBus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;
        void LoadMeshShader();
        void CreateMeshShaderScope();

        // ----------------------
        // Pipeline state and SRG
        // ----------------------

        // Meshshader pipeline
        AZ::RHI::ConstPtr<AZ::RHI::PipelineState> m_drawPipelineState;
        AZ::RHI::ShaderInputConstantIndex m_worldViewProjMatIndex;
        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> m_meshDrawSrg;

        // This is used to animate the hexagon.
        float m_time = 0.0f;
    };
}
