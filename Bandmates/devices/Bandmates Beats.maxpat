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
		"devicewidth": 860,
		"description": "Bandmates Beats 1.0.0 - TMFU audiovibe",
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
					"text": "js bm_beats.js",
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
						700,
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
					"text": "Kit elettronico: on/off e densita di ogni pezzo (il pad XY li sposta tutti)",
					"numinlets": 1,
					"numoutlets": 0,
					"fontsize": 9,
					"patching_rect": [
						306,
						2,
						390,
						14
					],
					"presentation": 1,
					"presentation_rect": [
						306,
						2,
						390,
						14
					]
				}
			},
			{
				"box": {
					"id": "obj-11",
					"maxclass": "live.comment",
					"text": "House · Tech House · Techno · Microhouse · Rominimal",
					"numinlets": 1,
					"numoutlets": 0,
					"fontsize": 9,
					"patching_rect": [
						306,
						96,
						270.40000000000003,
						14
					],
					"presentation": 1,
					"presentation_rect": [
						306,
						96,
						270.40000000000003,
						14
					]
				}
			},
			{
				"box": {
					"id": "obj-12",
					"maxclass": "live.comment",
					"text": "Sezioni dai nomi delle scene: Intro, Build, Drop, Break, Outro",
					"numinlets": 1,
					"numoutlets": 0,
					"fontsize": 9,
					"patching_rect": [
						306,
						112,
						322.40000000000003,
						14
					],
					"presentation": 1,
					"presentation_rect": [
						306,
						112,
						322.40000000000003,
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
							"parameter_longname": "Beatmaker",
							"parameter_shortname": "Player",
							"parameter_initial_enable": 1,
							"parameter_initial": [
								0
							],
							"parameter_type": 2,
							"parameter_mmax": 4,
							"parameter_enum": [
								"Volt · House",
								"Tess · Tech House",
								"Kern · Techno",
								"Mica · Microhouse",
								"Ilio · Rominimal"
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
					"varname": "on_kick",
					"patching_rect": [
						306,
						316,
						44,
						16
					],
					"presentation": 1,
					"presentation_rect": [
						306,
						16,
						44,
						16
					],
					"maxclass": "live.text",
					"mode": 1,
					"text": "Kick",
					"texton": "Kick",
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
							"parameter_longname": "Kick on",
							"parameter_shortname": "Kick",
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
					"id": "obj-32",
					"maxclass": "newobj",
					"text": "prepend param on_kick",
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
					"varname": "d_kick",
					"patching_rect": [
						306,
						338,
						44,
						48
					],
					"presentation": 1,
					"presentation_rect": [
						306,
						38,
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
							"parameter_longname": "Kick densita",
							"parameter_shortname": "KickD",
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
					"id": "obj-34",
					"maxclass": "newobj",
					"text": "prepend param d_kick",
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
					"varname": "on_clap",
					"patching_rect": [
						354,
						316,
						44,
						16
					],
					"presentation": 1,
					"presentation_rect": [
						354,
						16,
						44,
						16
					],
					"maxclass": "live.text",
					"mode": 1,
					"text": "Clap",
					"texton": "Clap",
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
							"parameter_longname": "Clap on",
							"parameter_shortname": "Clap",
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
					"id": "obj-36",
					"maxclass": "newobj",
					"text": "prepend param on_clap",
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
					"varname": "d_clap",
					"patching_rect": [
						354,
						338,
						44,
						48
					],
					"presentation": 1,
					"presentation_rect": [
						354,
						38,
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
							"parameter_longname": "Clap densita",
							"parameter_shortname": "ClapD",
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
					"id": "obj-38",
					"maxclass": "newobj",
					"text": "prepend param d_clap",
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
					"varname": "on_rim",
					"patching_rect": [
						402,
						316,
						44,
						16
					],
					"presentation": 1,
					"presentation_rect": [
						402,
						16,
						44,
						16
					],
					"maxclass": "live.text",
					"mode": 1,
					"text": "Rim",
					"texton": "Rim",
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
							"parameter_longname": "Rim on",
							"parameter_shortname": "Rim",
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
					"text": "prepend param on_rim",
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
					"varname": "d_rim",
					"patching_rect": [
						402,
						338,
						44,
						48
					],
					"presentation": 1,
					"presentation_rect": [
						402,
						38,
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
							"parameter_longname": "Rim densita",
							"parameter_shortname": "RimD",
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
					"id": "obj-42",
					"maxclass": "newobj",
					"text": "prepend param d_rim",
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
					"varname": "on_hat",
					"patching_rect": [
						450,
						316,
						44,
						16
					],
					"presentation": 1,
					"presentation_rect": [
						450,
						16,
						44,
						16
					],
					"maxclass": "live.text",
					"mode": 1,
					"text": "Hat",
					"texton": "Hat",
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
							"parameter_longname": "Hat on",
							"parameter_shortname": "Hat",
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
					"id": "obj-44",
					"maxclass": "newobj",
					"text": "prepend param on_hat",
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
					"varname": "d_hat",
					"patching_rect": [
						450,
						338,
						44,
						48
					],
					"presentation": 1,
					"presentation_rect": [
						450,
						38,
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
							"parameter_longname": "Hat densita",
							"parameter_shortname": "HatD",
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
					"id": "obj-46",
					"maxclass": "newobj",
					"text": "prepend param d_hat",
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
					"varname": "on_ohat",
					"patching_rect": [
						498,
						316,
						44,
						16
					],
					"presentation": 1,
					"presentation_rect": [
						498,
						16,
						44,
						16
					],
					"maxclass": "live.text",
					"mode": 1,
					"text": "Open",
					"texton": "Open",
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
							"parameter_longname": "Open on",
							"parameter_shortname": "Open",
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
					"id": "obj-48",
					"maxclass": "newobj",
					"text": "prepend param on_ohat",
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
					"varname": "d_ohat",
					"patching_rect": [
						498,
						338,
						44,
						48
					],
					"presentation": 1,
					"presentation_rect": [
						498,
						38,
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
							"parameter_longname": "Open densita",
							"parameter_shortname": "OpenD",
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
					"id": "obj-50",
					"maxclass": "newobj",
					"text": "prepend param d_ohat",
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
					"varname": "on_perc",
					"patching_rect": [
						546,
						316,
						44,
						16
					],
					"presentation": 1,
					"presentation_rect": [
						546,
						16,
						44,
						16
					],
					"maxclass": "live.text",
					"mode": 1,
					"text": "Perc",
					"texton": "Perc",
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
							"parameter_longname": "Perc on",
							"parameter_shortname": "Perc",
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
					"text": "prepend param on_perc",
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
					"varname": "d_perc",
					"patching_rect": [
						546,
						338,
						44,
						48
					],
					"presentation": 1,
					"presentation_rect": [
						546,
						38,
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
							"parameter_longname": "Perc densita",
							"parameter_shortname": "PercD",
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
					"id": "obj-54",
					"maxclass": "newobj",
					"text": "prepend param d_perc",
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
					"varname": "on_ride",
					"patching_rect": [
						594,
						316,
						44,
						16
					],
					"presentation": 1,
					"presentation_rect": [
						594,
						16,
						44,
						16
					],
					"maxclass": "live.text",
					"mode": 1,
					"text": "Ride",
					"texton": "Ride",
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
							"parameter_longname": "Ride on",
							"parameter_shortname": "Ride",
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
					"id": "obj-56",
					"maxclass": "newobj",
					"text": "prepend param on_ride",
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
					"varname": "d_ride",
					"patching_rect": [
						594,
						338,
						44,
						48
					],
					"presentation": 1,
					"presentation_rect": [
						594,
						38,
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
							"parameter_longname": "Ride densita",
							"parameter_shortname": "RideD",
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
					"id": "obj-58",
					"maxclass": "newobj",
					"text": "prepend param d_ride",
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
					"varname": "on_crash",
					"patching_rect": [
						642,
						316,
						44,
						16
					],
					"presentation": 1,
					"presentation_rect": [
						642,
						16,
						44,
						16
					],
					"maxclass": "live.text",
					"mode": 1,
					"text": "Crash",
					"texton": "Crash",
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
							"parameter_longname": "Crash on",
							"parameter_shortname": "Crash",
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
					"id": "obj-60",
					"maxclass": "newobj",
					"text": "prepend param on_crash",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						3010,
						150,
						120,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-61",
					"varname": "d_crash",
					"patching_rect": [
						642,
						338,
						44,
						48
					],
					"presentation": 1,
					"presentation_rect": [
						642,
						38,
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
							"parameter_longname": "Crash densita",
							"parameter_shortname": "CrasD",
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
					"id": "obj-62",
					"maxclass": "newobj",
					"text": "prepend param d_crash",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						3140,
						150,
						120,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-63",
					"varname": "micro",
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
							"parameter_longname": "Micro-timing",
							"parameter_shortname": "Micro",
							"parameter_initial_enable": 1,
							"parameter_initial": [
								10
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
					"id": "obj-64",
					"maxclass": "newobj",
					"text": "prepend param micro",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						3270,
						150,
						120,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-65",
					"varname": "generate",
					"patching_rect": [
						700,
						304,
						152,
						26
					],
					"presentation": 1,
					"presentation_rect": [
						700,
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
					"id": "obj-66",
					"maxclass": "message",
					"text": "generate",
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
					"varname": "newvar",
					"patching_rect": [
						700,
						334,
						152,
						18
					],
					"presentation": 1,
					"presentation_rect": [
						700,
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
					"id": "obj-68",
					"maxclass": "message",
					"text": "newvariation",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						3530,
						150,
						80,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-69",
					"varname": "allscenes",
					"patching_rect": [
						700,
						356,
						74,
						18
					],
					"presentation": 1,
					"presentation_rect": [
						700,
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
					"id": "obj-70",
					"maxclass": "message",
					"text": "generate_scenes",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						3660,
						150,
						80,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-71",
					"varname": "arrangement",
					"patching_rect": [
						778,
						356,
						74,
						18
					],
					"presentation": 1,
					"presentation_rect": [
						778,
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
					"id": "obj-72",
					"maxclass": "message",
					"text": "generate_arrangement",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						3790,
						150,
						80,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-73",
					"varname": "auto",
					"patching_rect": [
						700,
						378,
						74,
						18
					],
					"presentation": 1,
					"presentation_rect": [
						700,
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
					"id": "obj-74",
					"maxclass": "newobj",
					"text": "prepend param auto",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						3920,
						150,
						120,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-75",
					"varname": "followSections",
					"patching_rect": [
						778,
						378,
						74,
						18
					],
					"presentation": 1,
					"presentation_rect": [
						778,
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
					"id": "obj-76",
					"maxclass": "newobj",
					"text": "prepend param followSections",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						4050,
						150,
						120,
						22
					]
				}
			},
			{
				"box": {
					"id": "obj-77",
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
					"id": "obj-78",
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
					"id": "obj-79",
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
					"id": "obj-80",
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
					"id": "obj-81",
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
						"obj-5",
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
						"obj-5",
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
						"obj-76",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-76",
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
						"obj-77",
						0
					],
					"destination": [
						"obj-78",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-78",
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
						"obj-78",
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
						"obj-79",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-79",
						0
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
						"obj-15",
						0
					],
					"destination": [
						"obj-80",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-80",
						0
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
						"obj-7",
						0
					],
					"destination": [
						"obj-81",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-81",
						0
					],
					"destination": [
						"obj-77",
						0
					]
				}
			}
		],
		"dependency_cache": [
			{
				"name": "bm_beats.js",
				"bootpath": ".",
				"type": "TEXT",
				"implicit": 1
			},
			{
				"name": "bm_xy.js",
				"bootpath": ".",
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
