{
  "targets": [
    {
      "target_name": "detection",
      "sources": [
        "src/detection.cpp",
        "src/detection.h",
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
            ],
            'link_settings': 
            {
              "conditions" : 
              [
                ["target_arch=='ia32'", 
                  {
                    'libraries': 
                    [
                      '-lC:/Windows/System32/Setupapi.lib'
                    ]
                  }
                ],
                ["target_arch=='x64'", {
                  'libraries': [
                    '-lC:/Program Files (x86)/Microsoft SDKs/Windows/v7.0A/Lib/x64/Setupapi.lib',
                  ]
                  }
                ]
              ]
            }
          }
        ],
        ['OS=="mac"',
          {
            'sources': [
              "src/detection_mac.cpp"
            ]
          }
        ]
      ]
    }
  ]
}
