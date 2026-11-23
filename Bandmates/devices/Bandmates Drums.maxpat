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
		"devicewidth": 760,
		"description": "Bandmates Drums 1.0.2 - TMFU audiovibe",
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
					"text": "js bm_drums.js",
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
					"maxclass": "live.comment",
					"varname": "status",
					"text": "Bandmates 1.0.2 (script non avviato)",
					"linecount": 4,
					"fontsize": 9.5,
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
						600,
						100,
						152,
						64
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
					"text": "Player",
					"numinlets": 1,
					"numoutlets": 0,
					"fontsize": 9,
					"patching_rect": [
						162,
						2,
						60,
						14
					],
					"presentation": 1,
					"presentation_rect": [
						162,
						2,
						60,
						14
					]
				}
			},
			{
				"box": {
					"id": "obj-9",
					"maxclass": "live.comment",
					"text": "Preset",
					"numinlets": 1,
					"numoutlets": 0,
					"fontsize": 9,
					"patching_rect": [
						162,
						33,
						60,
						14
					],
					"presentation": 1,
					"presentation_rect": [
						162,
						33,
						60,
						14
					]
				}
			},
			{
				"box": {
					"id": "obj-10",
					"maxclass": "live.comment",
					"text": "Kit",
					"numinlets": 1,
					"numoutlets": 0,
					"fontsize": 9,
					"patching_rect": [
						306,
						2,
						60,
						14
					],
					"presentation": 1,
					"presentation_rect": [
						306,
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
					"text": "Pattern",
					"numinlets": 1,
					"numoutlets": 0,
					"fontsize": 9,
					"patching_rect": [
						510,
						2,
						60,
						14
					],
					"presentation": 1,
					"presentation_rect": [
						510,
						2,
						60,
						14
					]
				}
			},
			{
				"box": {
					"id": "obj-12",
					"maxclass": "live.comment",
					"text": "Cassa segue la traccia:",
					"numinlets": 1,
					"numoutlets": 0,
					"fontsize": 9,
					"patching_rect": [
						356,
						130,
						119.60000000000001,
						14
					],
					"presentation": 1,
					"presentation_rect": [
						356,
						130,
						119.60000000000001,
						14
					]
				}
			},
			{
				"box": {
					"id": "obj-13",
					"varname": "complexity",
					"patching_rect": [
						0,
						300,
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
							"parameter_longname": "Complessita",
							"parameter_shortname": "Complex",
							"parameter_initial_enable": 1,
							"parameter_initial": [
								50
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
					"id": "obj-14",
					"maxclass": "newobj",
					"text": "prepend param complexity",
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
					"id": "obj-15",
					"varname": "loudness",
					"patching_rect": [
						0,
						300,
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
							"parameter_longname": "Intensita",
							"parameter_shortname": "Intens",
							"parameter_initial_enable": 1,
							"parameter_initial": [
								50
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
					"id": "obj-16",
					"maxclass": "newobj",
					"text": "prepend param loudness",
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
					"id": "obj-17",
					"varname": "persona",
					"patching_rect": [
						162,
						315,
						132,
						16
					],
					"presentation": 1,
					"presentation_rect": [
						162,
						15,
						132,
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
							"parameter_longname": "Batterista",
							"parameter_shortname": "Player",
							"parameter_initial_enable": 1,
							"parameter_initial": [
								0
							],
							"parameter_type": 2,
							"parameter_mmax": 7,
							"parameter_enum": [
								"Gil · Soul",
								"Nina · Jazz",
								"Dalia · R&B",
								"Marco · Rock",
								"Rafa · Rock",
								"Leo · Pop",
								"Ines · Pop",
								"Teo · Funk"
							],
							"parameter_unitstyle": 9
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-18",
					"maxclass": "newobj",
					"text": "prepend param persona",
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
					"id": "obj-19",
					"varname": "preset",
					"patching_rect": [
						162,
						346,
						132,
						16
					],
					"presentation": 1,
					"presentation_rect": [
						162,
						46,
						132,
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
							"parameter_longname": "Preset",
							"parameter_shortname": "Preset",
							"parameter_initial_enable": 1,
							"parameter_initial": [
								0
							],
							"parameter_type": 2,
							"parameter_mmax": 5,
							"parameter_enum": [
								"Standard",
								"Strofa",
								"Ritornello",
								"Morbido",
								"Pieno",
								"Essenziale"
							],
							"parameter_unitstyle": 9
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-20",
					"maxclass": "newobj",
					"text": "prepend param preset",
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
					"id": "obj-21",
					"varname": "fills",
					"patching_rect": [
						160,
						366,
						44,
						48
					],
					"presentation": 1,
					"presentation_rect": [
						160,
						66,
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
							"parameter_longname": "Fill",
							"parameter_shortname": "Fill",
							"parameter_initial_enable": 1,
							"parameter_initial": [
								40
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
					"id": "obj-22",
					"maxclass": "newobj",
					"text": "prepend param fills",
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
					"id": "obj-23",
					"varname": "swing",
					"patching_rect": [
						206,
						366,
						44,
						48
					],
					"presentation": 1,
					"presentation_rect": [
						206,
						66,
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
							"parameter_longname": "Swing",
							"parameter_shortname": "Swing",
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
					"id": "obj-24",
					"maxclass": "newobj",
					"text": "prepend param swing",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						670,
						150,
						120,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-25",
					"varname": "feel",
					"patching_rect": [
						252,
						366,
						44,
						48
					],
					"presentation": 1,
					"presentation_rect": [
						252,
						66,
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
							"parameter_longname": "Feel (indietro/avanti)",
							"parameter_shortname": "Feel",
							"parameter_initial_enable": 1,
							"parameter_initial": [
								0
							],
							"parameter_type": 0,
							"parameter_mmin": -100,
							"parameter_mmax": 100,
							"parameter_unitstyle": 0
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-26",
					"maxclass": "newobj",
					"text": "prepend param feel",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						800,
						150,
						120,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-27",
					"varname": "humanize",
					"patching_rect": [
						160,
						416,
						44,
						48
					],
					"presentation": 1,
					"presentation_rect": [
						160,
						116,
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
							"parameter_longname": "Umanizza",
							"parameter_shortname": "Human",
							"parameter_initial_enable": 1,
							"parameter_initial": [
								35
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
					"id": "obj-28",
					"maxclass": "newobj",
					"text": "prepend param humanize",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						930,
						150,
						120,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-29",
					"varname": "swingGrid",
					"patching_rect": [
						206,
						432,
						88,
						16
					],
					"presentation": 1,
					"presentation_rect": [
						206,
						132,
						88,
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
							"parameter_longname": "Griglia swing",
							"parameter_shortname": "Grid",
							"parameter_initial_enable": 1,
							"parameter_initial": [
								1
							],
							"parameter_type": 2,
							"parameter_mmax": 1,
							"parameter_enum": [
								"8vi",
								"16mi"
							],
							"parameter_unitstyle": 9
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-30",
					"maxclass": "newobj",
					"text": "prepend param swingGrid",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						1060,
						150,
						120,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-31",
					"varname": "ghost",
					"patching_rect": [
						252,
						416,
						44,
						48
					],
					"presentation": 1,
					"presentation_rect": [
						252,
						116,
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
							"parameter_longname": "Ghost notes",
							"parameter_shortname": "Ghost",
							"parameter_initial_enable": 1,
							"parameter_initial": [
								30
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
					"id": "obj-32",
					"maxclass": "newobj",
					"text": "prepend param ghost",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						1190,
						150,
						120,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-33",
					"varname": "kitPerc",
					"patching_rect": [
						306,
						318,
						76,
						16
					],
					"presentation": 1,
					"presentation_rect": [
						306,
						18,
						76,
						16
					],
					"maxclass": "live.text",
					"mode": 1,
					"text": "Percussioni",
					"texton": "Percussioni",
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
							"parameter_longname": "Percussioni",
							"parameter_shortname": "Perc",
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
					"id": "obj-34",
					"maxclass": "newobj",
					"text": "prepend param kitPerc",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						1320,
						150,
						120,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-35",
					"varname": "percType",
					"patching_rect": [
						386,
						318,
						120,
						16
					],
					"presentation": 1,
					"presentation_rect": [
						386,
						18,
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
							"parameter_longname": "Tipo percussioni",
							"parameter_shortname": "PercT",
							"parameter_initial_enable": 1,
							"parameter_initial": [
								0
							],
							"parameter_type": 2,
							"parameter_mmax": 4,
							"parameter_enum": [
								"Tamb",
								"Shkr",
								"Clap",
								"Cowb",
								"Conga"
							],
							"parameter_unitstyle": 9
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-36",
					"maxclass": "newobj",
					"text": "prepend param percType",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						1450,
						150,
						120,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-37",
					"varname": "varPerc",
					"patching_rect": [
						510,
						318,
						84,
						16
					],
					"presentation": 1,
					"presentation_rect": [
						510,
						18,
						84,
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
							"parameter_longname": "Variazione percussioni",
							"parameter_shortname": "PercV",
							"parameter_initial_enable": 1,
							"parameter_initial": [
								0
							],
							"parameter_type": 2,
							"parameter_mmax": 4,
							"parameter_enum": [
								"1",
								"2",
								"3",
								"4",
								"5"
							],
							"parameter_unitstyle": 9
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-38",
					"maxclass": "newobj",
					"text": "prepend param varPerc",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						1580,
						150,
						120,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-39",
					"varname": "kitCymbals",
					"patching_rect": [
						306,
						340,
						76,
						16
					],
					"presentation": 1,
					"presentation_rect": [
						306,
						40,
						76,
						16
					],
					"maxclass": "live.text",
					"mode": 1,
					"text": "Piatti",
					"texton": "Piatti",
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
							"parameter_longname": "Piatti",
							"parameter_shortname": "Cymb",
							"parameter_initial_enable": 1,
							"parameter_initial": [
								1
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
					"id": "obj-40",
					"maxclass": "newobj",
					"text": "prepend param kitCymbals",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						1710,
						150,
						120,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-41",
					"varname": "cymbal",
					"patching_rect": [
						386,
						340,
						120,
						16
					],
					"presentation": 1,
					"presentation_rect": [
						386,
						40,
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
							"parameter_longname": "Hi-hat / Ride",
							"parameter_shortname": "HH/Ride",
							"parameter_initial_enable": 1,
							"parameter_initial": [
								0
							],
							"parameter_type": 2,
							"parameter_mmax": 1,
							"parameter_enum": [
								"Hi-hat",
								"Ride"
							],
							"parameter_unitstyle": 9
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-42",
					"maxclass": "newobj",
					"text": "prepend param cymbal",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						1840,
						150,
						120,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-43",
					"varname": "varCymbal",
					"patching_rect": [
						510,
						340,
						84,
						16
					],
					"presentation": 1,
					"presentation_rect": [
						510,
						40,
						84,
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
							"parameter_longname": "Variazione piatti",
							"parameter_shortname": "CymV",
							"parameter_initial_enable": 1,
							"parameter_initial": [
								0
							],
							"parameter_type": 2,
							"parameter_mmax": 4,
							"parameter_enum": [
								"1",
								"2",
								"3",
								"4",
								"5"
							],
							"parameter_unitstyle": 9
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-44",
					"maxclass": "newobj",
					"text": "prepend param varCymbal",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						1970,
						150,
						120,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-45",
					"varname": "kitKickSnare",
					"patching_rect": [
						306,
						362,
						76,
						16
					],
					"presentation": 1,
					"presentation_rect": [
						306,
						62,
						76,
						16
					],
					"maxclass": "live.text",
					"mode": 1,
					"text": "Cassa+Rull.",
					"texton": "Cassa+Rull.",
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
							"parameter_longname": "Cassa e rullante",
							"parameter_shortname": "K+S",
							"parameter_initial_enable": 1,
							"parameter_initial": [
								1
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
					"id": "obj-46",
					"maxclass": "newobj",
					"text": "prepend param kitKickSnare",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						2100,
						150,
						120,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-47",
					"varname": "timeMode",
					"patching_rect": [
						386,
						362,
						120,
						16
					],
					"presentation": 1,
					"presentation_rect": [
						386,
						62,
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
							"parameter_longname": "Tempo",
							"parameter_shortname": "Time",
							"parameter_initial_enable": 1,
							"parameter_initial": [
								1
							],
							"parameter_type": 2,
							"parameter_mmax": 2,
							"parameter_enum": [
								"1/2",
								"Normale",
								"x2"
							],
							"parameter_unitstyle": 9
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-48",
					"maxclass": "newobj",
					"text": "prepend param timeMode",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						2230,
						150,
						120,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-49",
					"varname": "varKick",
					"patching_rect": [
						510,
						362,
						84,
						16
					],
					"presentation": 1,
					"presentation_rect": [
						510,
						62,
						84,
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
							"parameter_longname": "Variazione cassa e rullante",
							"parameter_shortname": "KSV",
							"parameter_initial_enable": 1,
							"parameter_initial": [
								0
							],
							"parameter_type": 2,
							"parameter_mmax": 4,
							"parameter_enum": [
								"1",
								"2",
								"3",
								"4",
								"5"
							],
							"parameter_unitstyle": 9
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-50",
					"maxclass": "newobj",
					"text": "prepend param varKick",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						2360,
						150,
						120,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-51",
					"varname": "kitToms",
					"patching_rect": [
						306,
						384,
						76,
						16
					],
					"presentation": 1,
					"presentation_rect": [
						306,
						84,
						76,
						16
					],
					"maxclass": "live.text",
					"mode": 1,
					"text": "Tom",
					"texton": "Tom",
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
							"parameter_longname": "Tom",
							"parameter_shortname": "Toms",
							"parameter_initial_enable": 1,
							"parameter_initial": [
								1
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
					"id": "obj-52",
					"maxclass": "newobj",
					"text": "prepend param kitToms",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						2490,
						150,
						120,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-53",
					"varname": "hhAuto",
					"patching_rect": [
						386,
						384,
						60,
						16
					],
					"presentation": 1,
					"presentation_rect": [
						386,
						84,
						60,
						16
					],
					"maxclass": "live.text",
					"mode": 1,
					"text": "HH auto",
					"texton": "HH auto",
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
							"parameter_longname": "Hi-hat automatico",
							"parameter_shortname": "HHAuto",
							"parameter_initial_enable": 1,
							"parameter_initial": [
								1
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
					"id": "obj-54",
					"maxclass": "newobj",
					"text": "prepend param hhAuto",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						2620,
						150,
						120,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-55",
					"varname": "hhOpen",
					"patching_rect": [
						306,
						410,
						44,
						48
					],
					"presentation": 1,
					"presentation_rect": [
						306,
						110,
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
							"parameter_longname": "Apertura hi-hat",
							"parameter_shortname": "HHOpen",
							"parameter_initial_enable": 1,
							"parameter_initial": [
								30
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
					"id": "obj-56",
					"maxclass": "newobj",
					"text": "prepend param hhOpen",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						2750,
						150,
						120,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-57",
					"varname": "followOn",
					"patching_rect": [
						356,
						412,
						70,
						16
					],
					"presentation": 1,
					"presentation_rect": [
						356,
						112,
						70,
						16
					],
					"maxclass": "live.text",
					"mode": 1,
					"text": "Segui ritmo",
					"texton": "Segui ritmo",
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
							"parameter_longname": "Segui ritmo",
							"parameter_shortname": "Follow",
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
					"id": "obj-58",
					"maxclass": "newobj",
					"text": "prepend param followOn",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						2880,
						150,
						120,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-59",
					"varname": "generate",
					"patching_rect": [
						600,
						304,
						152,
						26
					],
					"presentation": 1,
					"presentation_rect": [
						600,
						4,
						152,
						26
					],
					"maxclass": "live.text",
					"mode": 0,
					"text": "▶  Genera",
					"texton": "▶  Genera",
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
					"id": "obj-60",
					"maxclass": "message",
					"text": "generate",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						3010,
						150,
						80,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-61",
					"varname": "newvar",
					"patching_rect": [
						600,
						334,
						152,
						18
					],
					"presentation": 1,
					"presentation_rect": [
						600,
						34,
						152,
						18
					],
					"maxclass": "live.text",
					"mode": 0,
					"text": "Nuova variazione",
					"texton": "Nuova variazione",
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
					"id": "obj-62",
					"maxclass": "message",
					"text": "newvariation",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						3140,
						150,
						80,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-63",
					"varname": "allscenes",
					"patching_rect": [
						600,
						356,
						74,
						18
					],
					"presentation": 1,
					"presentation_rect": [
						600,
						56,
						74,
						18
					],
					"maxclass": "live.text",
					"mode": 0,
					"text": "Tutte le scene",
					"texton": "Tutte le scene",
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
					"id": "obj-64",
					"maxclass": "message",
					"text": "generate_scenes",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						3270,
						150,
						80,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-65",
					"varname": "arrangement",
					"patching_rect": [
						678,
						356,
						74,
						18
					],
					"presentation": 1,
					"presentation_rect": [
						678,
						56,
						74,
						18
					],
					"maxclass": "live.text",
					"mode": 0,
					"text": "Arrangement",
					"texton": "Arrangement",
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
					"id": "obj-66",
					"maxclass": "message",
					"text": "generate_arrangement",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						3400,
						150,
						80,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-67",
					"varname": "auto",
					"patching_rect": [
						600,
						378,
						74,
						18
					],
					"presentation": 1,
					"presentation_rect": [
						600,
						78,
						74,
						18
					],
					"maxclass": "live.text",
					"mode": 1,
					"text": "Auto",
					"texton": "Auto",
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
							"parameter_longname": "Rigenera automatico",
							"parameter_shortname": "Auto",
							"parameter_initial_enable": 1,
							"parameter_initial": [
								1
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
					"id": "obj-68",
					"maxclass": "newobj",
					"text": "prepend param auto",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						3530,
						150,
						120,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-69",
					"varname": "followSections",
					"patching_rect": [
						678,
						378,
						74,
						18
					],
					"presentation": 1,
					"presentation_rect": [
						678,
						78,
						74,
						18
					],
					"maxclass": "live.text",
					"mode": 1,
					"text": "Segui sezioni",
					"texton": "Segui sezioni",
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
							"parameter_longname": "Segui sezioni",
							"parameter_shortname": "Sezioni",
							"parameter_initial_enable": 1,
							"parameter_initial": [
								1
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
					"id": "obj-70",
					"maxclass": "newobj",
					"text": "prepend param followSections",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						3660,
						150,
						120,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-71",
					"maxclass": "jsui",
					"filename": "bm_xy.js",
					"varname": "xy",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"parameter_enable": 0,
					"patching_rect": [
						20,
						400,
						150,
						150
					],
					"presentation": 1,
					"presentation_rect": [
						6,
						6,
						150,
						156
					]
				}
			},
			{
				"box": {
					"id": "obj-72",
					"maxclass": "newobj",
					"text": "unpack f f",
					"numinlets": 1,
					"numoutlets": 2,
					"outlettype": [
						"float",
						"float"
					],
					"patching_rect": [
						20,
						560,
						70,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-73",
					"maxclass": "newobj",
					"text": "prepend setx",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						200,
						560,
						80,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-74",
					"maxclass": "newobj",
					"text": "prepend sety",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						290,
						560,
						80,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-75",
					"maxclass": "newobj",
					"text": "prepend setxy",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						380,
						560,
						90,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-76",
					"maxclass": "umenu",
					"varname": "followTrack",
					"numinlets": 1,
					"numoutlets": 3,
					"outlettype": [
						"int",
						"",
						""
					],
					"items": [],
					"fontsize": 9,
					"patching_rect": [
						500,
						400,
						150,
						22
					],
					"presentation": 1,
					"presentation_rect": [
						356,
						144,
						150,
						18
					]
				}
			},
			{
				"box": {
					"id": "obj-77",
					"maxclass": "newobj",
					"text": "prepend followtrack",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						500,
						430,
						120,
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
						"obj-13",
						0
					],
					"destination": [
						"obj-14",
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
						"obj-5",
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
						"obj-16",
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
						"obj-5",
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
						"obj-18",
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
						"obj-5",
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
						"obj-20",
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
						"obj-5",
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
						"obj-22",
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
						"obj-5",
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
						"obj-24",
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
						"obj-5",
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
						"obj-26",
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
						"obj-5",
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
						"obj-28",
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
						"obj-5",
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
						"obj-30",
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
						"obj-5",
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
						"obj-5",
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
						"obj-34",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-34",
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
						"obj-35",
						0
					],
					"destination": [
						"obj-36",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-36",
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
						"obj-37",
						0
					],
					"destination": [
						"obj-38",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-38",
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
						"obj-39",
						0
					],
					"destination": [
						"obj-40",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-40",
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
						"obj-41",
						0
					],
					"destination": [
						"obj-42",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-42",
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
						"obj-43",
						0
					],
					"destination": [
						"obj-44",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-44",
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
						"obj-45",
						0
					],
					"destination": [
						"obj-46",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-46",
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
						"obj-47",
						0
					],
					"destination": [
						"obj-48",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-48",
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
						"obj-49",
						0
					],
					"destination": [
						"obj-50",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-50",
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
						"obj-51",
						0
					],
					"destination": [
						"obj-52",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-52",
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
						"obj-53",
						0
					],
					"destination": [
						"obj-54",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-54",
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
						"obj-55",
						0
					],
					"destination": [
						"obj-56",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-56",
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
						"obj-57",
						0
					],
					"destination": [
						"obj-58",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-58",
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
						"obj-59",
						0
					],
					"destination": [
						"obj-60",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-60",
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
						"obj-61",
						0
					],
					"destination": [
						"obj-62",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-62",
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
						"obj-63",
						0
					],
					"destination": [
						"obj-64",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-64",
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
						"obj-65",
						0
					],
					"destination": [
						"obj-66",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-66",
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
						"obj-67",
						0
					],
					"destination": [
						"obj-68",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-68",
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
						"obj-69",
						0
					],
					"destination": [
						"obj-70",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-70",
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
						"obj-71",
						0
					],
					"destination": [
						"obj-72",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-72",
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
						"obj-72",
						1
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
						"obj-13",
						0
					],
					"destination": [
						"obj-73",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-73",
						0
					],
					"destination": [
						"obj-71",
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
						"obj-74",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-74",
						0
					],
					"destination": [
						"obj-71",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-7",
						0
					],
					"destination": [
						"obj-75",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-75",
						0
					],
					"destination": [
						"obj-71",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-7",
						1
					],
					"destination": [
						"obj-76",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-76",
						1
					],
					"destination": [
						"obj-77",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-77",
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
				"name": "bm_drums.js",
				"bootpath": ".",
				"patcherrelativepath": ".",
				"type": "TEXT",
				"implicit": 1
			},
			{
				"name": "bm_xy.js",
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
