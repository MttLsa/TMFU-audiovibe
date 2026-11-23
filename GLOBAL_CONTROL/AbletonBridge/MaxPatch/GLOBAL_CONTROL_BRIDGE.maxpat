{
 "patcher": {
  "fileversion": 1,
  "appversion": {
   "major": 8,
   "minor": 6,
   "revision": 0,
   "architecture": "x64",
   "modernui": 1
  },
  "classnamespace": "box",
  "rect": [
   80,
   80,
   640,
   320
  ],
  "openrect": [
   0,
   0,
   320,
   169
  ],
  "openinpresentation": 1,
  "default_fontsize": 10.0,
  "default_fontname": "Arial Bold",
  "gridsize": [
   8,
   8
  ],
  "boxes": [
   {
    "box": {
     "id": "obj-1",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 3,
     "patching_rect": [
      30,
      30,
      95,
      22
     ],
     "text": "live.thisdevice",
     "outlettype": [
      "bang",
      "bang",
      ""
     ]
    }
   },
   {
    "box": {
     "id": "obj-2",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      160,
      30,
      105,
      22
     ],
     "text": "udpreceive 47121",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "obj-3",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 3,
     "patching_rect": [
      30,
      120,
      150,
      22
     ],
     "text": "js gc_bridge.js",
     "outlettype": [
      "",
      "",
      ""
     ],
     "saved_object_attributes": {
      "filename": "gc_bridge.js",
      "parameter_enable": 0
     }
    }
   },
   {
    "box": {
     "id": "obj-4",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      30,
      200,
      145,
      22
     ],
     "text": "udpsend 127.0.0.1 47120"
    }
   },
   {
    "box": {
     "id": "obj-5",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      300,
      90,
      105,
      22
     ],
     "text": "prepend projectid",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "obj-6",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 3,
     "patching_rect": [
      300,
      200,
      230,
      22
     ],
     "text": "pattr gc_project_id @parameter_enable 1",
     "outlettype": [
      "",
      "",
      ""
     ],
     "saved_object_attributes": {
      "parameter_enable": 1,
      "parameter_mappable": 0
     },
     "saved_attribute_attributes": {
      "valueof": {
       "parameter_longname": "gc_project_id",
       "parameter_shortname": "gc_project_id",
       "parameter_type": 3,
       "parameter_invisible": 1,
       "parameter_initial_enable": 0
      }
     },
     "varname": "gc_project_id"
    }
   },
   {
    "box": {
     "id": "obj-7",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      190,
      160,
      260,
      22
     ],
     "text": "Waiting for Live...",
     "outlettype": [
      ""
     ],
     "presentation": 1,
     "presentation_rect": [
      8,
      8,
      280,
      22
     ]
    }
   },
   {
    "box": {
     "id": "obj-8",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      30,
      260,
      420,
      20
     ],
     "text": "GLOBAL CONTROL BRIDGE  -  one per Live Set, on a dedicated MIDI track",
     "presentation": 1,
     "presentation_rect": [
      8,
      36,
      300,
      20
     ]
    }
   },
   {
    "box": {
     "id": "obj-9",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      500,
      30,
      45,
      22
     ],
     "text": "midiin",
     "outlettype": [
      "int"
     ]
    }
   },
   {
    "box": {
     "id": "obj-10",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      500,
      70,
      52,
      22
     ],
     "text": "midiout"
    }
   }
  ],
  "lines": [
   {
    "patchline": {
     "source": [
      "obj-1",
      0
     ],
     "destination": [
      "obj-3",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-2",
      0
     ],
     "destination": [
      "obj-3",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-3",
      0
     ],
     "destination": [
      "obj-4",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-3",
      1
     ],
     "destination": [
      "obj-7",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-3",
      2
     ],
     "destination": [
      "obj-6",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-6",
      0
     ],
     "destination": [
      "obj-5",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-5",
      0
     ],
     "destination": [
      "obj-3",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-9",
      0
     ],
     "destination": [
      "obj-10",
      0
     ]
    }
   }
  ],
  "parameters": {
   "obj-6": [
    "gc_project_id",
    "gc_project_id",
    0
   ],
   "parameterbanks": {}
  },
  "dependency_cache": [
   {
    "name": "gc_bridge.js",
    "bootpath": ".",
    "type": "TEXT",
    "implicit": 1
   }
  ],
  "autosave": 0
 }
}