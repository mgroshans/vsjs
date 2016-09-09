{
    'conditions': [
        ['OS=="win"', {
            'variables': {
                'VS_SDK%': 'C:/Program Files/Vapoursynth/sdk'
            },
            'conditions': [
                ['target_arch=="ia32"', {
                    'variables': {
                        'VS_Lib%': '<(VS_SDK)/lib32'
                    }
                }],
                ['target_arch=="x64"', {
                    'variables': {
                        'VS_Lib%': '<(VS_SDK)/lib64'
                    }
                }]
            ]
        }]
    ],
    'targets': [
        {
            'target_name': 'vsjs',
            'sources': [ 'init.cc', 'vsjs.cc' ],
            'include_dirs': [
                '<(VS_SDK)/include/vapoursynth',
                '<!(node -e "require(\'nan\')\")'
            ],
            'conditions': [
                ['OS=="win"', {
                    'libraries': [
                        '-l<(VS_Lib)/vapoursynth.lib',
                        '-l<(VS_Lib)/vsscript.lib'
                    ]
                }, { # 'OS!="win"'
                    'libraries': [
                        '-lvapoursynth',
                        '-lvsscript'
                    ]
                }]
            ]
        }
    ]
}