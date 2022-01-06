{
    "Source" : "MeshletPS.azsl",

    "DepthStencilState" : { 
        "Depth" : { "Enable" : false, "CompareFunc" : "GreaterEqual" }
    },

    "DrawList" : "forward",

    "CompilerHints" : { 
        "DisableOptimizations" : false,
        "GenerateDebugInfo" : true
    },

    "ProgramSettings":
    {
      "EntryPoints":
      [
        {
          "name": "MainMS",
          "type": "Vertex"
        },
        {
          "name": "MainMSPS",
          "type": "Fragment"
        }
      ]
    }    
}
 