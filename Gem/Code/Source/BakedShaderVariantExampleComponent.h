/*
 * All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
 * its licensors.
 *
 * For complete copyright and license terms please see the LICENSE at the root of this
 * distribution (the "License"). All use of this software is governed by the License,
 * or, if provided, by the license below or the license accompanying this file. Do not
 * remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 */

#pragma once

#include <Atom/RPI.Reflect/Material/MaterialAsset.h>
#include <Atom/RPI.Reflect/Model/ModelAsset.h>
#include <AzCore/Component/TickBus.h>
#include <AzFramework/Asset/AssetSystemTypes.h>
#include <CommonSampleComponentBase.h>
#include <Utils/FileIOErrorHandler.h>
#include <Utils/ImGuiSidebar.h>
#include <Utils/ImGuiAssetBrowser.h>
#include <Utils/ImGuiMaterialDetails.h>
#include <Utils/ImGuiHistogramQueue.h>

namespace AtomSampleViewer
{
    //! This test renders a simple material and exposes controls that can update the source data for that material and its shaders
    //! to demonstrate and test hot-reloading. It works by copying entire files from a test data folder into a material source folder
    //! and waiting for the Asset Processor to build the updates files.
    class BakedShaderVariantExampleComponent final
        : public CommonSampleComponentBase
        , public AZ::TickBus::Handler
        , public AZ::Data::AssetBus::Handler
    {
    public:
        AZ_COMPONENT(BakedShaderVariantExampleComponent, "{4986DD5D-347E-4E11-BBD5-E364061666A1}", CommonSampleComponentBase);
        BakedShaderVariantExampleComponent();

        static void Reflect(AZ::ReflectContext* context);

        // AZ::Component overrides...
        void Activate() override;
        void Deactivate() override;

    private:
        AZ_DISABLE_COPY_MOVE(BakedShaderVariantExampleComponent);

        // AZ::TickBus::Handler overrides...
        void OnTick(float deltaTime, AZ::ScriptTimePoint scriptTime) override;

        void MaterialChange();

        static constexpr uint32_t FrameTimeLogSize = 10;
        static constexpr uint32_t PassTimeLogSize = 10;
        ImGuiSidebar m_imguiSidebar;
        ImGuiMaterialDetails m_imguiMaterialDetails;
        ImGuiAssetBrowser m_materialBrowser;
        ImGuiHistogramQueue m_imGuiFrameTimer;
        ImGuiHistogramQueue m_imGuiPassTimer;

        AZ::Render::MeshFeatureProcessorInterface* m_meshFeatureProcessor = nullptr;

        AZ::Data::Asset<AZ::RPI::MaterialAsset> m_materialAsset;
        AZ::Data::Instance<AZ::RPI::Material> m_material;
        AZ::Data::Asset<AZ::RPI::ModelAsset> m_modelAsset;
        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_meshHandle;

        AZ::RHI::Ptr<AZ::RPI::ParentPass> rootPass;

        size_t m_selectedShaderIndex = 0;
    };
} // namespace AtomSampleViewer
