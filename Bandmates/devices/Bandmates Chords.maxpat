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
			100,
			1100,
			700
		],
		"openrect": [
			0,
			0,
			0,
			169
		],
		"openinpresentation": 1,
		"default_fontsize": 10,
		"default_fontface": 0,
		"default_fontname": "Arial Bold",
		"gridonopen": 1,
		"gridsize": [
			8,
			8
		],
		"gridsnaponopen": 1,
		"objectsnaponopen": 1,
		"statusbarvisible": 2,
		"toolbarvisible": 1,
		"boxanimatetime": 200,
		"enablehscroll": 1,
		"enablevscroll": 1,
		"devicewidth": 640,
		"description": "Bandmates Chords 1.0.0 - TMFU audiovibe",
		"digest": "",
		"tags": "",
		"style": "",
		"subpatcher_template": "",
		"assistshowspatchername": 0,
		"boxes": [
			{
				"box": {
					"id": "obj-1",
					"maxclass": "newobj",
					"text": "midiin",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						"int"
					],
					"patching_rect": [
						20,
						20,
						40,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-2",
					"maxclass": "newobj",
					"text": "midiout",
					"numinlets": 1,
					"numoutlets": 0,
					"outlettype": [],
					"patching_rect": [
						20,
						60,
						50,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-3",
					"maxclass": "newobj",
					"text": "live.thisdevice",
					"numinlets": 1,
					"numoutlets": 3,
					"outlettype": [
						"bang",
						"int",
						"int"
					],
					"patching_rect": [
						100,
						20,
						90,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-4",
					"maxclass": "message",
					"text": "init",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						100,
						50,
						35,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-5",
					"maxclass": "newobj",
					"text": "js bm_chords.js",
					"numinlets": 1,
					"numoutlets": 2,
					"outlettype": [
						"",
						""
					],
					"patching_rect": [
						100,
						200,
						160,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-6",
					"maxclass": "comment",
					"text": "Bandmates 1.0.0",
					"linecount": 4,
					"fontsize": 9,
					"textcolor": [
						0.85,
						0.85,
						0.85,
						1
					],
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						300,
						200,
						200,
						50
					],
					"presentation": 1,
					"presentation_rect": [
						6,
						142,
						628,
						24
					]
				}
			},
			{
				"box": {
					"id": "obj-7",
					"maxclass": "newobj",
					"text": "route xy followmenu textedit",
					"numinlets": 1,
					"numoutlets": 4,
					"outlettype": [
						"",
						"",
						"",
						""
					],
					"patching_rect": [
						100,
						240,
						200,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-8",
					"maxclass": "live.comment",
					"text": "Progressione",
					"numinlets": 1,
					"numoutlets": 0,
					"fontsize": 9,
					"patching_rect": [
						6,
						2,
						62.400000000000006,
						14
					],
					"presentation": 1,
					"presentation_rect": [
						6,
						2,
						62.400000000000006,
						14
					]
				}
			},
			{
				"box": {
					"id": "obj-9",
					"maxclass": "live.comment",
					"text": "Tonalita",
					"numinlets": 1,
					"numoutlets": 0,
					"fontsize": 9,
					"patching_rect": [
						180,
						2,
						60,
						14
					],
					"presentation": 1,
					"presentation_rect": [
						180,
						2,
						60,
						14
					]
				}
			},
			{
				"box": {
					"id": "obj-10",
					"maxclass": "live.comment",
					"text": "Chord ID",
					"numinlets": 1,
					"numoutlets": 0,
					"fontsize": 9,
					"patching_rect": [
						296,
						2,
						60,
						14
					],
					"presentation": 1,
					"presentation_rect": [
						296,
						2,
						60,
						14
					]
				}
			},
			{
				"box": {
					"id": "obj-11",
					"maxclass": "live.comment",
					"text": "Accordi: battute separate da |, \"-\" prolunga, \"%\" ripete la battuta. Es: C G/B | Am7 - | F | G7sus4 G7",
					"numinlets": 1,
					"numoutlets": 0,
					"fontsize": 9,
					"patching_rect": [
						6,
						54,
						530.4,
						14
					],
					"presentation": 1,
					"presentation_rect": [
						6,
						54,
						530.4,
						14
					]
				}
			},
			{
				"box": {
					"id": "obj-12",
					"varname": "progPreset",
					"patching_rect": [
						6,
						315,
						170,
						16
					],
					"presentation": 1,
					"presentation_rect": [
						6,
						15,
						170,
						16
					],
					"maxclass": "live.menu",
					"numinlets": 1,
					"numoutlets": 3,
					"outlettype": [
						"",
						"",
						"float"
					],
					"parameter_enable": 1,
					"fontsize": 9,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Progressione",
							"parameter_shortname": "Prog",
							"parameter_initial_enable": 1,
							"parameter_initial": [
								0
							],
							"parameter_type": 2,
							"parameter_mmax": 19,
							"parameter_enum": [
								"Pop I-V-vi-IV",
								"Pop vi-IV-I-V",
								"Anni 50 I-vi-IV-V",
								"Ballata I-iii-IV-V",
								"J-Pop IVmaj7-V7-iii7-vi",
								"Canone (barocco)",
								"Jazz ii-V-I",
								"Turnaround I-vi-ii-V",
								"Blues 12 battute",
								"Minore i-bVI-bIII-bVII",
								"Andalusa i-bVII-bVI-V",
								"Minore i-iv-v-i",
								"Soul ii9-V9 (vamp)",
								"Gospel IV-iii-ii-I",
								"Rock I-bVII-IV-I",
								"Modale I-bVII (vamp)",
								"Dorico i7-IV7 (vamp)",
								"House vi7-ii7-V7-Imaj7",
								"Epico i-bVI-bVII-i",
								"Neo-soul IVmaj7-iii7-vi9"
							],
							"parameter_unitstyle": 9
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-13",
					"maxclass": "newobj",
					"text": "prepend param progPreset",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						20,
						150,
						120,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-14",
					"varname": "key",
					"patching_rect": [
						180,
						315,
						54,
						16
					],
					"presentation": 1,
					"presentation_rect": [
						180,
						15,
						54,
						16
					],
					"maxclass": "live.menu",
					"numinlets": 1,
					"numoutlets": 3,
					"outlettype": [
						"",
						"",
						"float"
					],
					"parameter_enable": 1,
					"fontsize": 9,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Tonalita",
							"parameter_shortname": "Key",
							"parameter_initial_enable": 1,
							"parameter_initial": [
								12
							],
							"parameter_type": 2,
							"parameter_mmax": 12,
							"parameter_enum": [
								"C",
								"C#",
								"D",
								"D#",
								"E",
								"F",
								"F#",
								"G",
								"G#",
								"A",
								"A#",
								"B",
								"Live"
							],
							"parameter_unitstyle": 9
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-15",
					"maxclass": "newobj",
					"text": "prepend param key",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						150,
						150,
						120,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-16",
					"varname": "flats",
					"patching_rect": [
						238,
						315,
						54,
						16
					],
					"presentation": 1,
					"presentation_rect": [
						238,
						15,
						54,
						16
					],
					"maxclass": "live.text",
					"mode": 1,
					"text": "Bemolli",
					"texton": "Bemolli",
					"numinlets": 1,
					"numoutlets": 2,
					"outlettype": [
						"",
						""
					],
					"parameter_enable": 1,
					"fontsize": 8.5,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Bemolli",
							"parameter_shortname": "Flats",
							"parameter_initial_enable": 1,
							"parameter_initial": [
								0
							],
							"parameter_type": 2,
							"parameter_mmax": 1,
							"parameter_enum": [
								"off",
								"on"
							],
							"parameter_unitstyle": 9
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-17",
					"maxclass": "newobj",
					"text": "prepend param flats",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						280,
						150,
						120,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-18",
					"varname": "idRes",
					"patching_rect": [
						296,
						315,
						120,
						16
					],
					"presentation": 1,
					"presentation_rect": [
						296,
						15,
						120,
						16
					],
					"maxclass": "live.tab",
					"num_lines_patching": 1,
					"num_lines_presentation": 1,
					"numinlets": 1,
					"numoutlets": 3,
					"outlettype": [
						"",
						"",
						"float"
					],
					"parameter_enable": 1,
					"fontsize": 8.5,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Risoluzione Chord ID",
							"parameter_shortname": "IDRes",
							"parameter_initial_enable": 1,
							"parameter_initial": [
								1
							],
							"parameter_type": 2,
							"parameter_mmax": 2,
							"parameter_enum": [
								"1/4",
								"1/2",
								"1 bar"
							],
							"parameter_unitstyle": 9
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-19",
					"maxclass": "newobj",
					"text": "prepend param idRes",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						410,
						150,
						120,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-20",
					"varname": "ext",
					"patching_rect": [
						420,
						302,
						44,
						48
					],
					"presentation": 1,
					"presentation_rect": [
						420,
						2,
						44,
						48
					],
					"maxclass": "live.dial",
					"numinlets": 1,
					"numoutlets": 2,
					"outlettype": [
						"",
						"float"
					],
					"parameter_enable": 1,
					"showname": 1,
					"fontsize": 9,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Estensioni nel clip",
							"parameter_shortname": "Ext",
							"parameter_initial_enable": 1,
							"parameter_initial": [
								0
							],
							"parameter_type": 0,
							"parameter_mmin": 0,
							"parameter_mmax": 100,
							"parameter_unitstyle": 5
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-21",
					"maxclass": "newobj",
					"text": "prepend param ext",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						540,
						150,
						120,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-22",
					"varname": "apply",
					"patching_rect": [
						6,
						334,
						170,
						16
					],
					"presentation": 1,
					"presentation_rect": [
						6,
						34,
						170,
						16
					],
					"maxclass": "live.text",
					"mode": 0,
					"text": "Usa questa progressione",
					"texton": "Usa questa progressione",
					"numinlets": 1,
					"numoutlets": 2,
					"outlettype": [
						"",
						""
					],
					"parameter_enable": 0,
					"fontsize": 9
				}
			},
			{
				"box": {
					"id": "obj-23",
					"maxclass": "message",
					"text": "use_preset",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						670,
						150,
						80,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-24",
					"varname": "write",
					"patching_rect": [
						470,
						304,
						164,
						24
					],
					"presentation": 1,
					"presentation_rect": [
						470,
						4,
						164,
						24
					],
					"maxclass": "live.text",
					"mode": 0,
					"text": "▶  Scrivi nel clip",
					"texton": "▶  Scrivi nel clip",
					"numinlets": 1,
					"numoutlets": 2,
					"outlettype": [
						"",
						""
					],
					"parameter_enable": 0,
					"fontsize": 9
				}
			},
			{
				"box": {
					"id": "obj-25",
					"maxclass": "message",
					"text": "write",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						800,
						150,
						80,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-26",
					"varname": "read",
					"patching_rect": [
						470,
						332,
						164,
						18
					],
					"presentation": 1,
					"presentation_rect": [
						470,
						32,
						164,
						18
					],
					"maxclass": "live.text",
					"mode": 0,
					"text": "Chord ID dal clip selezionato",
					"texton": "Chord ID dal clip selezionato",
					"numinlets": 1,
					"numoutlets": 2,
					"outlettype": [
						"",
						""
					],
					"parameter_enable": 0,
					"fontsize": 9
				}
			},
			{
				"box": {
					"id": "obj-27",
					"maxclass": "message",
					"text": "read",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						930,
						150,
						80,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-28",
					"varname": "transUp",
					"patching_rect": [
						296,
						334,
						58,
						16
					],
					"presentation": 1,
					"presentation_rect": [
						296,
						34,
						58,
						16
					],
					"maxclass": "live.text",
					"mode": 0,
					"text": "+1 st",
					"texton": "+1 st",
					"numinlets": 1,
					"numoutlets": 2,
					"outlettype": [
						"",
						""
					],
					"parameter_enable": 0,
					"fontsize": 9
				}
			},
			{
				"box": {
					"id": "obj-29",
					"maxclass": "message",
					"text": "transpose 1",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						1060,
						150,
						80,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-30",
					"varname": "transDown",
					"patching_rect": [
						358,
						334,
						58,
						16
					],
					"presentation": 1,
					"presentation_rect": [
						358,
						34,
						58,
						16
					],
					"maxclass": "live.text",
					"mode": 0,
					"text": "-1 st",
					"texton": "-1 st",
					"numinlets": 1,
					"numoutlets": 2,
					"outlettype": [
						"",
						""
					],
					"parameter_enable": 0,
					"fontsize": 9
				}
			},
			{
				"box": {
					"id": "obj-31",
					"maxclass": "message",
					"text": "transpose -1",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						1190,
						150,
						80,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-32",
					"maxclass": "textedit",
					"varname": "progText",
					"keymode": 1,
					"lines": 3,
					"wordwrap": 1,
					"fontsize": 11,
					"numinlets": 1,
					"numoutlets": 4,
					"outlettype": [
						"",
						"int",
						"",
						""
					],
					"patching_rect": [
						500,
						460,
						300,
						60
					],
					"text": "C G/B | Am7 - | Fmaj7 | G7sus4 G7",
					"presentation": 1,
					"presentation_rect": [
						6,
						68,
						628,
						70
					]
				}
			},
			{
				"box": {
					"id": "obj-33",
					"maxclass": "newobj",
					"text": "prepend text",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						500,
						530,
						80,
						22
					]
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
						"obj-2",
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
						"obj-4",
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
						"obj-6",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-5",
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
						"obj-12",
						0
					],
					"destination": [
						"obj-13",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-13",
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
						"obj-14",
						0
					],
					"destination": [
						"obj-15",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-15",
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
						"obj-16",
						0
					],
					"destination": [
						"obj-17",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-17",
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
						"obj-18",
						0
					],
					"destination": [
						"obj-19",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-19",
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
						"obj-20",
						0
					],
					"destination": [
						"obj-21",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-21",
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
						"obj-22",
						0
					],
					"destination": [
						"obj-23",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-23",
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
						"obj-24",
						0
					],
					"destination": [
						"obj-25",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-25",
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
						"obj-26",
						0
					],
					"destination": [
						"obj-27",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-27",
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
						"obj-28",
						0
					],
					"destination": [
						"obj-29",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-29",
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
						"obj-30",
						0
					],
					"destination": [
						"obj-31",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-31",
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
						"obj-7",
						2
					],
					"destination": [
						"obj-32",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-32",
						0
					],
					"destination": [
						"obj-33",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-33",
						0
					],
					"destination": [
						"obj-5",
						0
					]
				}
			}
		],
		"dependency_cache": [
			{
				"name": "bm_chords.js",
				"bootpath": ".",
				"patcherrelativepath": ".",
				"type": "TEXT",
				"implicit": 1
			}
		],
		"latency": 0,
		"is_mpe": 0,
		"minimum_live_version": "",
		"minimum_max_version": "",
		"platform_compatibility": 0,
		"autosave": 0
	}
}
