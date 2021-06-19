{
  "targets": [
    {
      "target_name": "detection",
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ],
      "xcode_settings": { "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
        # "CLANG_CXX_LIBRARY": "libc++",
        "MACOSX_DEPLOYMENT_TARGET": "10.7",
      },
      "msvs_settings": {
        "VCCLCompilerTool": { "ExceptionHandling": 1 },
      },
      "sources": [
        "src/detection.cpp",
        "src/detection.h",
        "src/deviceList.cpp"
      ],
      "include_dirs" : [
        "<!@(node -p \"require('node-addon-api').include\")" 
      ],
      'conditions': [
        ['OS=="win"',
          {
            'sources': [
              "src/detection_win.cpp"
            ],
            'include_dirs+':
            [
              # Not needed now
            ]
          }
        ],
        ['OS=="mac"',
          {
            'sources': [
              "src/detection_mac.cpp"
            ],
            "libraries": [
              "-framework",
              "IOKit"
            ],
            'default_configuration': 'Debug',
            'configurations': {
              'Debug': {
                'defines': [ 'DEBUG', '_DEBUG' ],
              },
              'Release': {
                'defines': [ 'NDEBUG' ]
              }
            }
          }
        ],
        ['OS=="linux"',
          {
            'sources': [
              "src/detection_linux.cpp"
            ],
            'link_settings': {
              'libraries': [
                '-ludev'
              ]
            }
          }
        ]
      ]
    }
  ]
}
