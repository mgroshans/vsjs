{
    "targets": [
        {
            "target_name": "vsjs",
            "sources": [ "init.cc", "vsjs.cc" ],
            "include_dirs": [
                "include",
                "<!(node -e \"require('nan')\")"
            ],
            "libraries": [
                "../lib/vapoursynth.lib",
                "../lib/vsscript.lib"
            ]
        }
    ]
}