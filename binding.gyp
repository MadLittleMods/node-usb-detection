{
  "targets": [
    {
      "target_name": "detection",
      "sources": [
        "src/detection.cpp",
        "src/detection.h"
      ],
      'conditions': [
        ['OS=="win"',
          {
            'sources': [
              "src/detection_win.cpp"
            ]
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
