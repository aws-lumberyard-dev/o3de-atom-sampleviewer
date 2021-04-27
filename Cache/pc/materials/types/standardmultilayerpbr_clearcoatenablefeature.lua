--------------------------------------------------------------------------------------
--
-- All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
-- its licensors.
--
-- For complete copyright and license terms please see the LICENSE at the root of this
-- distribution (the "License"). All use of this software is governed by the License,
-- or, if provided, by the license below or the license accompanying this file. Do not
-- remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
-- WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
--
--
----------------------------------------------------------------------------------------------------

-- This functor controls the flag that enables the overall feature for the shader.
-- There are separate functors for 'handling the individual material layers' vs 'enabling the overall feature'.

function GetMaterialPropertyDependencies()
    return { 
        "layer1_clearCoat.enable",
        "layer2_clearCoat.enable",
        "layer3_clearCoat.enable" 
    }
end

function GetShaderOptionDependencies()
    return { "o_clearCoat_feature_enabled" }
end
 
function Process(context)
    local enable1 = context:GetMaterialPropertyValue_bool("layer1_clearCoat.enable")
    local enable2 = context:GetMaterialPropertyValue_bool("layer2_clearCoat.enable")
    local enable3 = context:GetMaterialPropertyValue_bool("layer3_clearCoat.enable")
    context:SetShaderOptionValue_bool("o_clearCoat_feature_enabled", enable1 or enable2 or enable3)
end
