/*
UI_WIDGET.C

symbols in this file:
000D32C0 0020:
	_code_000d32c0 (0000)
000D32E0 0020:
	_code_000d32e0 (0000)
000D3300 0030:
	_event_controller_index_compatible_with_widget (0000)
000D3330 0030:
	_set_ui_plasma_effect_color (0000)
000D3360 00a0:
	_ui_widgets_initialize (0000)
000D3400 0010:
	_ui_widgets_safe_to_load (0000)
000D3410 0010:
	_ui_widgets_set_fade_value (0000)
000D3420 0010:
	_ui_widget_debug_show_path (0000)
000D3430 0020:
	_widget_instance_count_children (0000)
000D3450 0050:
	_widget_instance_get_nth_child (0000)
000D34A0 0030:
	_ui_widget_realloc (0000)
000D34D0 0020:
	_widget_free (0000)
000D34F0 0030:
	_ui_widgets_active (0000)
000D3520 0070:
	_ui_widgets_active_for_local_player (0000)
000D3590 0050:
	_ui_widgets_inhibit_processing (0000)
000D35E0 0020:
	_code_000d35e0 (0000)
000D3600 0490:
	_draw_bitmap_in_rect (0000)
000D3A90 0020:
	_widget_instance_get_topmost_parent (0000)
000D3AB0 0040:
	_widget_instance_get_child_index_from_parent (0000)
000D3AF0 0060:
	_widget_instance_set_visibility_recursive (0000)
000D3B50 0010:
	_main_menu_active (0000)
000D3B60 0010:
	_main_menu_is_active (0000)
000D3B70 0030:
	_main_menu_screen_is_active (0000)
000D3BA0 0050:
	_ui_set_next_level (0000)
000D3BF0 0010:
	_ui_widget_load_progress_widget (0000)
000D3C00 0010:
	_filesystem_check_thread_is_active (0000)
000D3C10 0040:
	_code_000d3c10 (0000)
000D3C50 0030:
	_display_error_when_main_menu_loaded (0000)
000D3C80 0090:
	_display_error_deferred (0000)
000D3D10 0040:
	_display_error_abort_to_dashboard_deferred (0000)
000D3D50 0070:
	_ui_start_main_menu_music (0000)
000D3DC0 0060:
	_ui_stop_main_menu_music (0000)
000D3E20 0010:
	_ui_main_menu_music_active (0000)
000D3E30 0040:
	_ui_widgets_disable_pause_game (0000)
000D3E70 0080:
	_code_000d3e70 (0000)
000D3EF0 0060:
	_code_000d3ef0 (0000)
000D3F50 0030:
	_code_000d3f50 (0000)
000D3F80 0090:
	_code_000d3f80 (0000)
000D4010 0080:
	_code_000d4010 (0000)
000D4090 0050:
	_code_000d4090 (0000)
000D40E0 0020:
	_code_000d40e0 (0000)
000D4100 0080:
	_code_000d4100 (0000)
000D4180 0040:
	_code_000d4180 (0000)
000D41C0 0040:
	_code_000d41c0 (0000)
000D4200 0050:
	_code_000d4200 (0000)
000D4250 01a0:
	_code_000d4250 (0000)
000D43F0 0070:
	_code_000d43f0 (0000)
000D4460 0060:
	_code_000d4460 (0000)
000D44C0 0060:
	_code_000d44c0 (0000)
000D4520 0130:
	_remap_sticks_for_local_player (0000)
000D4650 0030:
	_code_000d4650 (0000)
000D4680 0010:
	_code_000d4680 (0000)
000D4690 0010:
	_code_000d4690 (0000)
000D46A0 0170:
	_code_000d46a0 (0000)
000D4810 00d0:
	_code_000d4810 (0000)
000D48E0 0020:
	_code_000d48e0 (0000)
000D4900 01d0:
	_code_000d4900 (0000)
000D4AD0 0030:
	_code_000d4ad0 (0000)
000D4B00 0040:
	_code_000d4b00 (0000)
000D4B40 0020:
	_code_000d4b40 (0000)
000D4B60 0060:
	_code_000d4b60 (0000)
000D4BC0 00a0:
	_code_000d4bc0 (0000)
000D4C60 0050:
	_get_ui_rgb_white (0000)
000D4CB0 0060:
	_get_ui_argb_white (0000)
000D4D10 0050:
	_code_000d4d10 (0000)
000D4D60 0040:
	_modulate_pixel32_by_real_alpha (0000)
000D4DA0 02a0:
	_ui_widget_delete (0000)
000D5040 0050:
	_ui_widgets_close_all (0000)
000D5090 0090:
	_ui_widgets_close_all_for_local_player (0000)
000D5120 0040:
	_ui_widgets_delete_history (0000)
000D5160 0060:
	_ui_widgets_pop_stack (0000)
000D51C0 0070:
	_main_screen_shell_begin_fade (0000)
000D5230 0060:
	_ui_play_audio_feedback_sound (0000)
000D5290 02a0:
	_code_000d5290 (0000)
000D5530 0030:
	_code_000d5530 (0000)
000D5560 0360:
	_draw_string_and_hack_in_icons (0000)
000D58C0 0310:
	_code_000d58c0 (0000)
000D5BD0 0440:
	_code_000d5bd0 (0000)
000D6010 0050:
	_code_000d6010 (0000)
000D6060 00a0:
	_code_000d6060 (0000)
000D6100 00e0:
	_code_000d6100 (0000)
000D61E0 0050:
	_ui_widgets_dispose (0000)
000D6230 0200:
	_widget_event_function_list_widget_goto_next_item (0000)
000D6430 0220:
	_widget_event_function_list_widget_goto_previous_item (0000)
000D6650 04f0:
	_code_000d6650 (0000)
000D6B40 02f0:
	_code_000d6b40 (0000)
000D6E30 00b0:
	_code_000d6e30 (0000)
000D6EE0 0180:
	_render_ui_widgets_postgame (0000)
000D7060 0230:
	_render_ui_widgets (0000)
000D7290 01f0:
	_code_000d7290 (0000)
000D7480 07e0:
	_code_000d7480 (0000)
000D7C60 0220:
	_ui_widget_load_by_name_or_tag (0000)
000D7E80 0130:
	_main_screen_shell_load (0000)
000D7FB0 00e0:
	_network_game_reset_to_pregame_ui (0000)
000D8090 0380:
	_display_error (0000)
000D8410 0090:
	_display_error_abort_to_dashboard (0000)
000D84A0 0090:
	_display_error_damaged_media (0000)
000D8530 0070:
	_display_errors_deferred_until_cinematic_stop (0000)
000D85A0 0260:
	_display_scenario_help (0000)
000D8800 02a0:
	_code_000d8800 (0000)
000D8AA0 0130:
	_code_000d8aa0 (0000)
000D8BD0 03c0:
	_process_ui_widgets (0000)
00271380 0012:
	??_C@_1BC@GEHLELKL@?$AAc?$AAu?$AAs?$AAt?$AAo?$AAm?$AA?9?$AA8?$AA?$AA@ (0000)
00271394 0012:
	??_C@_1BC@DMBIFLPN@?$AAc?$AAu?$AAs?$AAt?$AAo?$AAm?$AA?9?$AA7?$AA?$AA@ (0000)
002713A8 0012:
	??_C@_1BC@IEKEDMJI@?$AAc?$AAu?$AAs?$AAt?$AAo?$AAm?$AA?9?$AA6?$AA?$AA@ (0000)
002713BC 0012:
	??_C@_1BC@JGBBJDHG@?$AAc?$AAu?$AAs?$AAt?$AAo?$AAm?$AA?9?$AA5?$AA?$AA@ (0000)
002713D0 0012:
	??_C@_1BC@COKNPEBD@?$AAc?$AAu?$AAs?$AAt?$AAo?$AAm?$AA?9?$AA4?$AA?$AA@ (0000)
002713E4 0012:
	??_C@_1BC@LDHKMMKK@?$AAc?$AAu?$AAs?$AAt?$AAo?$AAm?$AA?9?$AA3?$AA?$AA@ (0000)
002713F8 0012:
	??_C@_1BC@LMGKLMP@?$AAc?$AAu?$AAs?$AAt?$AAo?$AAm?$AA?9?$AA2?$AA?$AA@ (0000)
0027140C 0012:
	??_C@_1BC@BJHDAECB@?$AAc?$AAu?$AAs?$AAt?$AAo?$AAm?$AA?9?$AA1?$AA?$AA@ (0000)
00271420 000a:
	??_C@_19MLOAAMON@?$AAl?$AAo?$AAo?$AAk?$AA?$AA@ (0000)
0027142C 000a:
	??_C@_19OACEEDLD@?$AAm?$AAo?$AAv?$AAe?$AA?$AA@ (0000)
00271438 000a:
	??_C@_19LHGBOOBH@?$AAb?$AAa?$AAc?$AAk?$AA?$AA@ (0000)
00271444 000e:
	??_C@_1O@CNPOHNDJ@?$AAa?$AAc?$AAc?$AAe?$AAp?$AAt?$AA?$AA@ (0000)
00271454 000a:
	??_C@_19MEDLKCJO@?$AAz?$AAo?$AAo?$AAm?$AA?$AA@ (0000)
00271460 000e:
	??_C@_1O@KCCHFBLA@?$AAc?$AAr?$AAo?$AAu?$AAc?$AAh?$AA?$AA@ (0000)
00271470 0020:
	??_C@_1CA@KFDCIJKK@?$AAr?$AAo?$AAt?$AAa?$AAt?$AAe?$AA?9?$AAg?$AAr?$AAe?$AAn?$AAa?$AAd?$AAe?$AAs?$AA?$AA@ (0000)
00271490 001e:
	??_C@_1BO@POELNCJG@?$AAr?$AAo?$AAt?$AAa?$AAt?$AAe?$AA?9?$AAw?$AAe?$AAa?$AAp?$AAo?$AAn?$AAs?$AA?$AA@ (0000)
002714B0 001c:
	??_C@_1BM@IKPPIMCO@?$AAu?$AAs?$AAe?$AA?9?$AAe?$AAq?$AAu?$AAi?$AAp?$AAm?$AAe?$AAn?$AAt?$AA?$AA@ (0000)
002714CC 000a:
	??_C@_19OBFFCANM@?$AAj?$AAu?$AAm?$AAp?$AA?$AA@ (0000)
002714D8 0022:
	??_C@_1CC@OGCINJG@?$AAi?$AAn?$AAt?$AAe?$AAg?$AAr?$AAa?$AAt?$AAe?$AAd?$AA?9?$AAl?$AAi?$AAg?$AAh?$AAt?$AA?$AA@ (0000)
002714FC 0020:
	??_C@_1CA@BEHOBGA@?$AAp?$AAr?$AAi?$AAm?$AAa?$AAr?$AAy?$AA?9?$AAt?$AAr?$AAi?$AAg?$AAg?$AAe?$AAr?$AA?$AA@ (0000)
0027151C 001c:
	??_C@_1BM@HECBOACM@?$AAt?$AAh?$AAr?$AAo?$AAw?$AA?9?$AAg?$AAr?$AAe?$AAn?$AAa?$AAd?$AAe?$AA?$AA@ (0000)
00271538 000e:
	??_C@_1O@HCAHPFFC@?$AAa?$AAc?$AAt?$AAi?$AAo?$AAn?$AA?$AA@ (0000)
00271548 0018:
	??_C@_1BI@IAFIHGKJ@?$AAr?$AAi?$AAg?$AAh?$AAt?$AA?9?$AAs?$AAt?$AAi?$AAc?$AAk?$AA?$AA@ (0000)
00271560 0016:
	??_C@_1BG@GNIACJFB@?$AAl?$AAe?$AAf?$AAt?$AA?9?$AAs?$AAt?$AAi?$AAc?$AAk?$AA?$AA@ (0000)
00271578 0018:
	??_C@_1BI@KFEEPENO@?$AAr?$AAi?$AAg?$AAh?$AAt?$AA?9?$AAt?$AAh?$AAu?$AAm?$AAb?$AA?$AA@ (0000)
00271590 0016:
	??_C@_1BG@EIJMKLCG@?$AAl?$AAe?$AAf?$AAt?$AA?9?$AAt?$AAh?$AAu?$AAm?$AAb?$AA?$AA@ (0000)
002715A8 0018:
	??_C@_1BI@KIILCNEB@?$AAb?$AAa?$AAc?$AAk?$AA?9?$AAb?$AAu?$AAt?$AAt?$AAo?$AAn?$AA?$AA@ (0000)
002715C0 001a:
	??_C@_1BK@CFIHHNDP@?$AAs?$AAt?$AAa?$AAr?$AAt?$AA?9?$AAb?$AAu?$AAt?$AAt?$AAo?$AAn?$AA?$AA@ (0000)
002715DC 0016:
	??_C@_1BG@CGEEKDJO@?$AAd?$AAp?$AAa?$AAd?$AA?9?$AAr?$AAi?$AAg?$AAh?$AAt?$AA?$AA@ (0000)
002715F4 0014:
	??_C@_1BE@LFENLMOB@?$AAd?$AAp?$AAa?$AAd?$AA?9?$AAl?$AAe?$AAf?$AAt?$AA?$AA@ (0000)
00271608 0014:
	??_C@_1BE@MAHEIGBI@?$AAd?$AAp?$AAa?$AAd?$AA?9?$AAd?$AAo?$AAw?$AAn?$AA?$AA@ (0000)
0027161C 0010:
	??_C@_1BA@CNADAJOL@?$AAd?$AAp?$AAa?$AAd?$AA?9?$AAu?$AAp?$AA?$AA@ (0000)
0027162C 001c:
	??_C@_1BM@DNLFOGBJ@?$AAr?$AAi?$AAg?$AAh?$AAt?$AA?9?$AAt?$AAr?$AAi?$AAg?$AAg?$AAe?$AAr?$AA?$AA@ (0000)
00271648 001a:
	??_C@_1BK@IONNGAAO@?$AAl?$AAe?$AAf?$AAt?$AA?9?$AAt?$AAr?$AAi?$AAg?$AAg?$AAe?$AAr?$AA?$AA@ (0000)
00271664 001a:
	??_C@_1BK@DHOCGHBB@?$AAw?$AAh?$AAi?$AAt?$AAe?$AA?9?$AAb?$AAu?$AAt?$AAt?$AAo?$AAn?$AA?$AA@ (0000)
00271680 001a:
	??_C@_1BK@JINDKLEA@?$AAb?$AAl?$AAa?$AAc?$AAk?$AA?9?$AAb?$AAu?$AAt?$AAt?$AAo?$AAn?$AA?$AA@ (0000)
0027169C 0012:
	??_C@_1BC@JDEDLPGE@?$AAy?$AA?9?$AAb?$AAu?$AAt?$AAt?$AAo?$AAn?$AA?$AA@ (0000)
002716B0 0012:
	??_C@_1BC@HMBBAJIF@?$AAx?$AA?9?$AAb?$AAu?$AAt?$AAt?$AAo?$AAn?$AA?$AA@ (0000)
002716C4 0012:
	??_C@_1BC@EADHJABC@?$AAb?$AA?9?$AAb?$AAu?$AAt?$AAt?$AAo?$AAn?$AA?$AA@ (0000)
002716D8 0012:
	??_C@_1BC@KKLBENHA@?$AAa?$AA?9?$AAb?$AAu?$AAt?$AAt?$AAo?$AAn?$AA?$AA@ (0000)
002716EC 000f:
	??_C@_0P@OHNLNAJA@levels?2d40?2d40?$AA@ (0000)
002716FC 000f:
	??_C@_0P@IAIGJJBI@levels?2d20?2d20?$AA@ (0000)
0027170C 000f:
	??_C@_0P@HAMJOBDA@levels?2c40?2c40?$AA@ (0000)
0027171C 000f:
	??_C@_0P@BHJEKILI@levels?2c20?2c20?$AA@ (0000)
0027172C 000f:
	??_C@_0P@CEDKAMHM@levels?2c10?2c10?$AA@ (0000)
0027173C 000f:
	??_C@_0P@ENPIGML@levels?2b40?2b40?$AA@ (0000)
0027174C 000f:
	??_C@_0P@MEDHKOMA@levels?2b30?2b30?$AA@ (0000)
0027175C 000f:
	??_C@_0P@DPFAEPEF@levels?2a50?2a50?$AA@ (0000)
0027176C 000f:
	??_C@_0P@FIANAGMN@levels?2a30?2a30?$AA@ (0000)
0027177C 000f:
	??_C@_0P@MMBGMDIK@levels?2a10?2a10?$AA@ (0000)
0027178C 0013:
	??_C@_0BD@CKKOGMCD@widget_memory_pool?$AA@ (0000)
002717A0 0008:
	__one_over_255 (0000)
	_SECONDS_PER_MILLISECOND (0004)
002717A8 0025:
	??_C@_0CF@BFKKDAFO@c?3?2halo?2SOURCE?2interface?2ui_widg@ (0000)
002717D0 0007:
	??_C@_06KLKLDLLI@widget?$AA@ (0000)
002717D8 0024:
	??_C@_0CE@JONHIOKA@expected?5a?5valid?5local_player_in@ (0000)
002717FC 001b:
	??_C@_0BL@IGOIGKAA@widget_globals?4initialized?$AA@ (0000)
00271818 0004:
	__real@38026b7e (0000)
0027181C 0004:
	__real@43490000 (0000)
00271820 0004:
	__real@38150d23 (0000)
00271824 0004:
	__real@37e0c670 (0000)
00271828 0004:
	__real@439b8000 (0000)
0027182C 0004:
	__real@3806dd76 (0000)
00271830 000e:
	??_C@_0O@CJGPHCNG@the_main_menu?$AA@ (0000)
00271840 000e:
	??_C@_0O@LGOJGHAO@unknown?5level?$AA@ (0000)
00271850 0048:
	??_C@_0EI@DFLNAEDH@the?5old?5loading?5progress?5screen?5@ (0000)
00271898 0059:
	??_C@_0FJ@HLIPNJGE@there?5is?5already?5an?5error?5messag@ (0000)
002718F8 0051:
	??_C@_0FB@JEKLNNGC@there?5is?5already?5a?5deferred?5erro@ (0000)
0027194C 0036:
	??_C@_0DG@OLDDPPAF@?$CIindex?$DO?$DN0?$CJ?5?$CG?$CG?5?$CIindex?$DMMAXIMUM_NUM@ (0000)
00271988 0047:
	??_C@_0EH@LLPKBLA@there?5is?5already?5a?5deferred?5dash@ (0000)
002719D0 001a:
	??_C@_0BK@LDNBEGIO@title?5music?5tag?5not?5found?$AA@ (0000)
002719EC 0019:
	??_C@_0BJ@DOPIPCPC@starting?5main?5menu?5music?$AA@ (0000)
00271A08 001a:
	??_C@_0BK@GFHNHPPO@sound?2music?2title1?2title1?$AA@ (0000)
00271A24 0019:
	??_C@_0BJ@OPCCAOJF@stopping?5main?5menu?5music?$AA@ (0000)
00271A40 0012:
	??_C@_0BC@MKLFKPBF@duration_ticks?$DO?$DN0?$AA@ (0000)
00271A54 0034:
	??_C@_0DE@HOKJOLHA@out?5of?5memory?$CB?5the?5UI?5screen?5his@ (0000)
00271A88 000c:
	??_C@_0M@EABMFPBO@top?5?$CG?$CG?5data?$AA@ (0000)
00271A94 0019:
	??_C@_0BJ@FOOJDKHD@tail_child?9?$DOnext?5?$DN?$DN?5NULL?$AA@ (0000)
00271AB0 0033:
	??_C@_0DD@MMOLGAOL@?$CIchild?9?$DOprevious?5?$DN?$DN?5NULL?$CJ?5?$CG?$CG?5?$CIch@ (0000)
00271AE4 0018:
	??_C@_0BI@LBDKLIFA@next?9?$DOprevious?5?$DN?$DN?5child?$AA@ (0000)
00271AFC 0018:
	??_C@_0BI@LBKIDJB@child?9?$DOprevious?5?$DN?$DN?5NULL?$AA@ (0000)
00271B18 0041:
	??_C@_0EB@BEJBEEHM@initial_indent?$DM0?5in?5render_state@ (0000)
00271B5C 001d:
	??_C@_0BN@EEMEPEOK@31?5?$DN?$DN?5get_icon_type?$CIL?$CClook?$CC?$CJ?$AA@ (0000)
00271B7C 0024:
	??_C@_0CE@DPIKLLNB@17?5?$DN?$DN?5get_icon_type?$CIL?$CCright?9stic@ (0000)
00271BA0 001d:
	??_C@_0BN@GOCJIAID@30?5?$DN?$DN?5get_icon_type?$CIL?$CCmove?$CC?$CJ?$AA@ (0000)
00271BC0 0023:
	??_C@_0CD@BJHDPKGJ@16?5?$DN?$DN?5get_icon_type?$CIL?$CCleft?9stick@ (0000)
00271BE8 0041:
	??_C@_0EB@PKGKIBFB@?$CD?$CD?$CDWARNING?3?5possible?5memory?5leak@ (0000)
00271C2C 0022:
	??_C@_0CC@MHEDEDIP@widget?5pause?5counter?5out?5of?5whac@ (0000)
00271C50 0025:
	??_C@_0CF@EFHCMBNF@event?5handler?5failed?5to?5spawn?5wi@ (0000)
00271C78 004b:
	??_C@_0EL@BONJAKAK@a?5?8widget?5deleted?8?5event?5handler@ (0000)
00271CC4 0025:
	??_C@_0CF@OIBDABJ@widget?5?$CG?$CG?5widget_globals?4initial@ (0000)
00271CEC 001a:
	??_C@_0BK@EFAHKEKA@sound?2sfx?2ui?2flag_failure?$AA@ (0000)
00271D08 0012:
	??_C@_0BC@LFDBPFAD@sound?2sfx?2ui?2back?$AA@ (0000)
00271D1C 0015:
	??_C@_0BF@FGMCPHMP@sound?2sfx?2ui?2forward?$AA@ (0000)
00271D34 0014:
	??_C@_0BE@EOGFPNHD@sound?2sfx?2ui?2cursor?$AA@ (0000)
00271D48 003a:
	??_C@_0DK@MJICNCDL@invalid?5controller?5index?5specifi@ (0000)
00271D88 005d:
	??_C@_0FN@LKLJLEJ@_list_items_generated_from_strin@ (0000)
00271DE8 0069:
	??_C@_0GJ@EGCLHEFN@no?5child?5widget?5references?5are?5n@ (0000)
00271E58 0063:
	??_C@_0GD@BFCMHBJK@_list_items_generated_from_strin@ (0000)
00271EBC 0026:
	??_C@_0CG@GCOFCMPN@icon_index?$DO?$DN0?5?$CG?$CG?5icon_index?$DMNUM_@ (0000)
00271EE4 0006:
	??_C@_05MAJJAKPI@FALSE?$AA@ (0000)
00271EEC 0004:
	??_C@_13EJFHHPOP@?$AA?$CF?$AA?$AA@ (0000)
00271EF0 0047:
	??_C@_0EH@BDEOCECI@failed?5to?5render?5text?5box?5widget@ (0000)
00271F38 0042:
	??_C@_0EC@OINFLBMA@failed?5to?5render?5text?5box?5widget@ (0000)
00271F7C 0020:
	??_C@_1CA@EBMNPDDP@?$AA?$DM?$AAo?$AAu?$AAt?$AA?5?$AAo?$AAf?$AA?5?$AAm?$AAe?$AAm?$AAo?$AAr?$AAy?$AA?$DO?$AA?$AA@ (0000)
00271FA0 0049:
	??_C@_0EJ@ICFFKBCK@failed?5to?5render?5spinner?5list?5it@ (0000)
00271FF0 0044:
	??_C@_0EE@GFHAOED@failed?5to?5render?5spinner?5list?5it@ (0000)
00272034 0029:
	??_C@_0CJ@CLCMGDOK@failed?5to?5find?5event?5focus?5targe@ (0000)
00272060 0046:
	??_C@_0EG@GAAKHNIE@failed?5to?5spawn?5thread?5for?5files@ (0000)
002720A8 003c:
	??_C@_0DM@DBODFMFA@begining?5filesystem?5checks?5?$CG?5sav@ (0000)
002720E4 002b:
	??_C@_0CL@DAIOFIBL@widget_globals?4initialization_th@ (0000)
00272110 003d:
	??_C@_0DN@CLOOAICL@failed?5to?5set?5focus?5to?5the?5next?5@ (0000)
00272150 006d:
	??_C@_0GN@CDIJNPDM@spinner?5lists?5with?5more?5that?51?5v@ (0000)
002721C0 0033:
	??_C@_0DD@MMCOEHPE@spinner?5lists?5must?5be?5either?51?9?5@ (0000)
002721F8 0041:
	??_C@_0EB@HAOLGJEB@failed?5to?5set?5focus?5to?5the?5?$CD?$CFd?5l@ (0000)
0027223C 0034:
	??_C@_0DE@PKDHGMBO@calling?5a?5list?5widget?5function?5o@ (0000)
00272270 0016:
	??_C@_0BG@ENJMIAKH@widget?9?$DOfocused_child?$AA@ (0000)
00272288 0048:
	??_C@_0EI@GEFPBJMP@couldn?8t?5load?5conditional?5widget@ (0000)
002722D0 0029:
	??_C@_0CJ@FPJLGLJO@condition?5handler?5failed?5to?5spaw@ (0000)
00272300 0045:
	??_C@_0EF@JLKENOEN@failed?5to?5open?5widget?5because?5th@ (0000)
00272348 0019:
	??_C@_0BJ@KIDHBPBC@previous?9?$DOnext?5?$DN?$DN?5widget?$AA@ (0000)
00272364 0019:
	??_C@_0BJ@FGBHJFEB@next?9?$DOprevious?5?$DN?$DN?5widget?$AA@ (0000)
00272380 0044:
	??_C@_0EE@NFLPKLKO@failed?5to?5close?5widget?5because?5e@ (0000)
002723C8 0045:
	??_C@_0EF@EEMBCNDP@failed?5to?5reload?5widget?5because?5@ (0000)
00272410 004e:
	??_C@_0EO@PGEAPKEN@failed?5to?5give?5focus?5to?5a?5widget@ (0000)
00272460 001e:
	??_C@_0BO@HFOFCHPP@event?5handler?5function?5failed?$AA@ (0000)
00272480 002a:
	??_C@_0CK@GOFLJEI@failed?5to?5run?5ui?5widget?5event?5sc@ (0000)
002724AC 000c:
	??_C@_0M@EJOMDMOI@ui?2small_ui?$AA@ (0000)
002724B8 0016:
	??_C@_0BG@ELECBMPG@window_bounds?5?$CB?$DN?5NULL?$AA@ (0000)
002724D0 001f:
	??_C@_0BP@JAOAINDB@failed?5to?5load?5widget?5children?$AA@ (0000)
002724F0 009a:
	??_C@_0JK@BLPAHHHK@if?5the?5_widget_pass_handled_even@ (0000)
0027258C 0021:
	??_C@_0CB@BADDKMAA@widget?9?$DOlocal_player_index?$DN?$DNNONE@ (0000)
002725B0 0037:
	??_C@_0DH@DCPBMEID@widget?5?$CG?$CG?5definition?5?$CG?$CG?5event?5?$CG?$CG@ (0000)
002725E8 002c:
	??_C@_0CM@OJPOGECC@ui_widget_definition?5tag?5?8?$CFs?8?1?$CFd@ (0000)
00272614 002c:
	??_C@_0CM@MEFFCNMB@failed?5to?5create?5new?5widget?$DL?5out@ (0000)
00272640 0035:
	??_C@_0DF@PBOMEHIM@?$CIwidget_stack?$DO?$DN0?$CJ?5?$CG?$CG?5?$CIwidget_sta@ (0000)
00272678 0026:
	??_C@_0CG@GBPNNCLI@?$CIname?5?$CB?$DN?5NULL?$CJ?5?$HM?$HM?5?$CItag_index?5?$CB?$DN?5@ (0000)
002726A0 002a:
	??_C@_0CK@OFOHOPCB@failed?5to?5initialize?5the?5virtual@ (0000)
002726CC 0028:
	??_C@_0CI@EBHLJEJP@failed?5to?5load?5main?5screen?5shell@ (0000)
002726F4 001d:
	??_C@_0BN@PLHBGNEF@ui?2shell?2main_menu?2main_menu?$AA@ (0000)
00272714 0018:
	??_C@_0BI@HFFEIMKP@xbox?5command?5line?$DN?5?8?$CFs?8?$AA@ (0000)
0027272C 0006:
	??_C@_05GLGMCIFH@xdemo?$AA@ (0000)
00272734 002f:
	??_C@_0CP@FMNMOLIB@failed?5to?5load?5networked?5pregame@ (0000)
00272768 005b:
	??_C@_0FL@CANKNMEF@ui?2shell?2main_menu?2multiplayer_t@ (0000)
002727C4 002a:
	??_C@_0CK@BADOGGKF@failed?5to?5load?5map?5select?5postga@ (0000)
002727F0 0060:
	??_C@_0GA@GFIHBLPH@ui?2shell?2main_menu?2multiplayer_t@ (0000)
00272850 0035:
	??_C@_0DF@GJABDHAE@failed?5to?5load?5pregame?5screen?5af@ (0000)
00272888 0063:
	??_C@_0GD@HIMIFBNM@ui?2shell?2main_menu?2multiplayer_t@ (0000)
002728EC 0025:
	??_C@_0CF@GLGHHELH@widget?5pause?5counter?5is?5out?5of?5w@ (0000)
00272914 002f:
	??_C@_0CP@BGKPJLCC@expected?5a?5text?5box?5widget?5in?5th@ (0000)
00272944 0032:
	??_C@_0DC@JNAKMNPN@error?5screen?5widget?5tag?5not?5laye@ (0000)
00272978 0020:
	??_C@_0CA@PLEKCLAA@failed?5to?5display?5error?5message?$AA@ (0000)
00272998 0048:
	??_C@_0EI@MDMAJGJE@there?5is?5already?5an?5error?5messag@ (0000)
002729E0 0032:
	??_C@_0DC@JLPDPOMG@aborting?5to?5the?5main?5menu?5root?0?5@ (0000)
00272A18 0044:
	??_C@_0EE@HGHDGOBG@?$CIwidget_stack?$DO?$DN0?$CJ?5?$CG?$CG?5?$CIwidget_sta@ (0000)
00272A5C 001b:
	??_C@_0BL@ILOCFJOB@invalid?5local?5player?5count?$AA@ (0000)
00272A78 0028:
	??_C@_0CI@NLLLFMIK@ui?2shell?2error?2error_nonmodal_qt@ (0000)
00272AA0 0025:
	??_C@_0CF@IPCJKMOA@ui?2shell?2error?2error_modal_qtrsc@ (0000)
00272AC8 0029:
	??_C@_0CJ@MDGMGIIG@ui?2shell?2error?2error_nonmodal_ha@ (0000)
00272AF4 0026:
	??_C@_0CG@GOFPHCDA@ui?2shell?2error?2error_modal_halfs@ (0000)
00272B1C 0029:
	??_C@_0CJ@COJPHONK@ui?2shell?2error?2error_nonmodal_fu@ (0000)
00272B48 0026:
	??_C@_0CG@IDKMGEGM@ui?2shell?2error?2error_modal_fulls@ (0000)
00272B70 0059:
	??_C@_0FJ@LCJINDPJ@there?5is?5already?5a?5deferred?9for?9@ (0000)
00272BCC 001b:
	??_C@_0BL@DHJKOIEB@failed?5to?5load?5?8?$CFs?8?5widget?$AA@ (0000)
00272BE8 001b:
	??_C@_0BL@PIJOAKFD@expected?5a?5text?5box?5widget?$AA@ (0000)
00272C04 003b:
	??_C@_0DL@MFGPNPO@ui?2shell?2error?2error_abort_to_da@ (0000)
00272C40 0028:
	??_C@_0CI@JFEBNJBP@ui?2shell?2error?2error_abort_to_da@ (0000)
00272C68 0017:
	??_C@_0BH@FIKKMDKG@Noooooooooooooooooo?$CB?$CB?$CB?$AA@ (0000)
00272C80 003a:
	??_C@_0DK@KGMPEFEN@can?8t?5display?5scenario?5help?5beca@ (0000)
00272CBC 003d:
	??_C@_0DN@BOLHGMPP@can?8t?5display?5scenario?5help?$DL?5unk@ (0000)
00272CFC 0023:
	??_C@_0CD@DGCLPNFD@failed?5to?5load?5in?9game?5help?5dial@ (0000)
00272D20 002f:
	??_C@_0CP@ODEPBIAI@expected?5text?5box?5widget?5in?5play@ (0000)
00272D50 0036:
	??_C@_0DG@EMGDHLEG@ui?2shell?2solo_game?2player_help?2p@ (0000)
00272D88 0004:
	??_C@_03JGNLDLAI@d40?$AA@ (0000)
00272D8C 0036:
	??_C@_0DG@EIOOAHPE@ui?2shell?2solo_game?2player_help?2p@ (0000)
00272DC4 0004:
	??_C@_03JCFGEHLK@d20?$AA@ (0000)
00272DC8 0036:
	??_C@_0DG@NBLEEDPP@ui?2shell?2solo_game?2player_help?2p@ (0000)
00272E00 0004:
	??_C@_03LAMADLB@c40?$AA@ (0000)
00272E04 0036:
	??_C@_0DG@NFDJDPEN@ui?2shell?2solo_game?2player_help?2p@ (0000)
00272E3C 0004:
	??_C@_03PIBHPAD@c20?$AA@ (0000)
00272E40 0036:
	??_C@_0DG@NHHPIBBE@ui?2shell?2solo_game?2player_help?2p@ (0000)
00272E78 0004:
	??_C@_03NMHMBFK@c10?$AA@ (0000)
00272E7C 0036:
	??_C@_0DG@GJAICEJK@ui?2shell?2solo_game?2player_help?2p@ (0000)
00272EB4 0004:
	??_C@_03LDLAGENE@b40?$AA@ (0000)
00272EB8 0036:
	??_C@_0DG@GMEHDCBP@ui?2shell?2solo_game?2player_help?2p@ (0000)
00272EF0 0004:
	??_C@_03LGPPHCFB@b30?$AA@ (0000)
00272EF4 0036:
	??_C@_0DG@HKHPOBED@ui?2shell?2solo_game?2player_help?2p@ (0000)
00272F2C 0004:
	??_C@_03KAMHKBAN@a50?$AA@ (0000)
00272F30 0036:
	??_C@_0DG@HOPCJNPB@ui?2shell?2solo_game?2player_help?2p@ (0000)
00272F68 0004:
	??_C@_03KEEKNNLP@a30?$AA@ (0000)
00272F6C 0036:
	??_C@_0DG@HNHGEJJP@ui?2shell?2solo_game?2player_help?2p@ (0000)
00272FA4 0004:
	??_C@_03KHMOAJNB@a10?$AA@ (0000)
00272FA8 0010:
	??_C@_0BA@CDPMFNHN@string_index?$DO?$DN0?$AA@ (0000)
00272FB8 002d:
	??_C@_0CN@FAHAPHCM@failed?5to?5load?5full?5screen?5pause@ (0000)
00272FE8 0029:
	??_C@_0CJ@OJHKNGAB@ui?2shell?2solo_game?2pause_game?2pa@ (0000)
00273018 0053:
	??_C@_0FD@MJAJHNPK@the?5ui?5seems?5to?5be?5confused?4?4?4?5a@ (0000)
0027306C 002e:
	??_C@_0CO@LCDPHEGN@failed?5to?5load?5split?5screen?5paus@ (0000)
0027309C 0036:
	??_C@_0DG@NEJLJPFI@ui?2shell?2solo_game?2pause_game?2pa@ (0000)
002730D4 0030:
	??_C@_0DA@JLINLCDA@invalid?5local?5player?5count?5for?5m@ (0000)
00273104 0033:
	??_C@_0DD@CLAJLOAB@ui?2shell?2multiplayer_game?2pause_@ (0000)
00273138 0033:
	??_C@_0DD@NDMLPFLH@ui?2shell?2multiplayer_game?2pause_@ (0000)
0027316C 002d:
	??_C@_0CN@BIDFDGIG@failed?5to?5load?5multiplayer?5pause@ (0000)
0027319C 0033:
	??_C@_0DD@KPKKNAGM@ui?2shell?2multiplayer_game?2pause_@ (0000)
002731D0 002a:
	??_C@_0CK@MAHDJCLA@invalid?5widget?5controller?5index?5@ (0000)
002731FC 0037:
	??_C@_0DH@EDIJCICF@waiting?5for?5?$CFd?5ticks?5before?5disp@ (0000)
00301DA0 413c:
	_data_00301da0 (0000)
	_widget_memory_pool (4034)
	_dashboard_abort_error (403c)
00454240 08b0:
	_bss_00454240 (0000)
*/

/* ---------- headers */

struct widget_instance;

#include "cseries.h"
#include "errors.h"
#include "bitmaps/bitmap_group.h"
#include "bitmaps/bitmaps.h"
#include "bink/bink_playback.h"
#include "bungie_net/common/thread.h"
#include "cache/cache_files.h"
#include "cache/texture_cache.h"
#include "cseries/cseries_windows.h"
#include "cutscene/cinematics.h"
#include "event_manager.h"
#include "game/game_engine.h"
#include "game/game_globals.h"
#include "game/players.h"
#include "hs/hs.h"
#include "input/input.h"
#include "input/input_abstraction.h"
#include "interface/attract_mode.h"
#include "interface/hud.h"
#include "interface/hud_definitions.h"
#include "interface/hud_draw.h"
#include "bitmaps/bitmap_color_conversion.h"
#include "interface/interface.h"
#include "interface/player_ui.h"
#include "interface/progress_bar.h"
#include "interface/ui_widget_game_data_input_functions.h"
#include "custom_edition_maps.h" /* port: port/linux/game/custom_edition_maps.c */
#include "interface/ui_widget_event_handler_functions.h"
#include "interface/ui_widget_text_search_and_replace_functions.h"
#include "interface/virtual_keyboard.h"
#include "main/main.h"
#include "memory/stack_memory_pool.h"
#include "networking/network_client_manager.h"
#include "networking/network_connection.h"
#include "networking/network_game_globals.h"
#include "networking/network_server_manager.h"
#include "network_coop.h" /* port: port/linux/game/network_coop.c */
/* port: the menus' redraw (port/linux/src/ui_redraw.c) */
void ui_redraw_begin_screen(char const *tag_name);
void ui_redraw_end_screen(void);
void halo_screen_menu_window(int active);
#include "menu_scale.h" /* port: shared content bounds and padding */
void halo_screen_menu_content(float top, float bottom);
void halo_screen_menu_visible_bounds(float *top, float *bottom);
static void ui_widget_menu_content(struct widget_instance *widget);
void ui_redraw_note(char const *tag_name, void const *hardware_format, long width, long height);
long ui_redraw_touch_target_units(void);
#include "rasterizer/rasterizer.h"
#include "saved games/player_profile.h"
#include "saved games/playlist_profile.h"
#include "saved games/saved_game_files.h"
#include "scenario/scenario.h"
#include "shell/shell_xbox.h"
#include "sound/game_sound.h"
#include "sound/sound_definitions.h"
#include "sound/sound_manager.h"
#include "tag_files/tag_files.h"
#include "tag_files/tag_groups.h"
#include "text/draw_string.h"
#include "text/font_group.h"
#include "text/text_group.h"
#include "text/unicode.h"
#include "ui_widget.h"
#include "interface/controller_glyphs.h"
#include "halo_menu_row.h" /* port: runtime settings control policy */
#include "halo_menu_dropdown.h"
#include "halo_menu_slider.h"
#include "halo_controller_glyphs.h"
#include "menu_geometry.h"
#include "halo_menu_meta.h"
#include "halo_menus.h"
#include "../../port/runtime/include/halo_ui_carousel.h" /* shared with the host and probes; not on the guest include path */
#include "rasterizer/xbox/rasterizer_xbox_hardware_bitmaps.h"

/* ---------- constants */

enum
{
	/* port: 16 KB on the Xbox; the PC version's screens (port/assets/menus)
	have many more widgets */
	WIDGET_MEMORY_POOL_SIZE = 0x40000,
	MAXIMUM_WIDGET_MEMORY_POOL_BLOCKS = 4096
};

enum
{
	_ui_widget_type_container,
	_ui_widget_type_text_box,
	_ui_widget_type_spinner_list,
	_ui_widget_type_column_list,
	_ui_widget_type_game_model,
	_ui_widget_type_movie,
	_ui_widget_type_custom,
	NUMBER_OF_UI_WIDGET_TYPES
};

enum
{
	_ui_audio_feedback_none,
	_ui_audio_feedback_cursor,
	_ui_audio_feedback_forward,
	_ui_audio_feedback_back,
	_ui_audio_feedback_flag_failure,
	NUMBER_OF_UI_AUDIO_FEEDBACK_SOUNDS
};

enum
{
	_icon_a_button,
	_icon_b_button,
	_icon_x_button,
	_icon_y_button,
	_icon_black_button,
	_icon_white_button,
	_icon_left_trigger,
	_icon_right_trigger,
	_icon_dpad_up,
	_icon_dpad_down,
	_icon_dpad_left,
	_icon_dpad_right,
	_icon_start_button,
	_icon_back_button,
	_icon_left_thumb,
	_icon_right_thumb,
	_icon_left_stick,
	_icon_right_stick,
	_icon_action,
	_icon_throw_grenade,
	_icon_primary_trigger,
	_icon_integrated_light,
	_icon_jump,
	_icon_use_equipment,
	_icon_rotate_weapons,
	_icon_rotate_grenades,
	_icon_crouch,
	_icon_zoom,
	_icon_accept,
	_icon_back,
	_icon_move,
	_icon_look,
	_icon_custom_1,
	_icon_custom_2,
	_icon_custom_3,
	_icon_custom_4,
	_icon_custom_5,
	_icon_custom_6,
	_icon_custom_7,
	_icon_custom_8,
	NUMBER_OF_ICON_TYPES
};

enum
{
	/* only the icon types below _icon_action name a button bitmap of their own;
	the rest are resolved through the local player's control preferences */
	NUM_ICONS = _icon_action
};

enum
{
	_hud_icon_use_text_bit,
	_hud_icon_use_color_bit,
	_hud_icon_absolute_width_bit,
	NUMBER_OF_HUD_ICON_FLAGS
};

enum
{
	UI_WIDGET_DEFINITION_TAG = 'DeLa'
};

enum
{
	_widget_controller0,
	_widget_controller1,
	_widget_controller2,
	_widget_controller3,
	_widget_controller_any,
	NUMBER_OF_WIDGET_CONTROLLERS
};

enum
{
	/* only the bits this file tests are named; when
	_widget_always_use_tag_controller_index_bit is set a definition asking for
	any player gets no controller at all, when it is clear the same request
	inherits the invoking widget's controller */
	_widget_pass_unhandled_events_to_children_bit = 0,
	_widget_pause_game_time_bit = 1,
	_widget_flash_background_bitmap_bit = 2,
	_widget_dpad_updown_tabs_thru_children_bit = 3,
	_widget_dpad_leftright_tabs_thru_children_bit = 4,
	_widget_dpad_updown_tabs_thru_list_items_bit = 5,
	_widget_dpad_leftright_tabs_thru_list_items_bit = 6,
	_widget_dont_focus_a_specific_child_bit = 7,
	_widget_pass_unhandled_events_to_all_children_bit = 8,
	_widget_render_regardless_of_controller_index_bit = 9,
	_widget_pass_handled_events_to_all_children_bit = 10,
	_widget_return_to_main_menu_if_no_history_bit = 11,
	_widget_always_use_tag_controller_index_bit = 12,
	_widget_always_render_with_nifty_fx_bit = 13,
	_widget_dont_push_history_data_bit = 14
};

enum
{
	_child_widget_use_custom_controller_index_bit = 0,
	NUMBER_OF_CHILD_WIDGET_FLAGS
};

enum
{
	_conditional_widget_load_if_event_handler_function_fails_bit = 0,
	NUMBER_OF_CONDITIONAL_WIDGET_FLAGS
};

enum
{
	_event_handler_close_current_widget_bit,
	_event_handler_close_other_widget_bit,
	_event_handler_close_all_widgets_bit,
	_event_handler_open_widget_bit,
	_event_handler_reload_self_bit,
	_event_handler_reload_widget_bit,
	_event_handler_give_focus_to_widget_bit,
	_event_handler_run_function_bit,
	_event_handler_replace_with_other_widget_bit,
	_event_handler_go_back_to_previous_widget_bit,
	_event_handler_run_scenario_script_bit,
	_event_handler_look_for_conditional_widget_on_failure_bit,
	NUMBER_OF_EVENT_HANDLER_FLAGS
};

enum
{
	_list_items_generated_in_code,
	_list_items_generated_from_string_list_tag,
	_list_items_only_one_tooltip_entry,
	_list_single_preview_box_no_scroll,
	NUMBER_OF_LIST_FLAGS
};

enum
{
	/* only the bit this file tests is named */
	_text_box_flashing_text_bit = 2,
	/* port: the text is broken at spaces to the widget's width */
	_text_box_port_wrap_bit = 4
};

enum
{
	_text_justification_left,
	_text_justification_right,
	_text_justification_center,
	NUMBER_OF_TEXT_JUSTIFICATIONS
};

enum
{
	/* EVENT_MANAGER.C owns this enumeration and publishes none of it */
	_event_type_null,
	_event_type_left_stick,
	_event_type_right_stick,
	_event_type_button,
	NUMBER_OF_EVENT_TYPES
};

enum
{
	/* the button event types are the gamepad button indices; the enumeration
	runs 0..33 and only the types this file names are listed */
	_widget_event_b_button = _gamepad_analog_button_b,
	_widget_event_dpad_up = _gamepad_binary_button_dpad_up,
	_widget_event_dpad_down = _gamepad_binary_button_dpad_down,
	_widget_event_dpad_left = _gamepad_binary_button_dpad_left,
	_widget_event_dpad_right = _gamepad_binary_button_dpad_right,
	_widget_event_back_button = _gamepad_binary_button_back,
	_widget_event_left_stick_up = NUMBER_OF_GAMEPAD_BUTTONS,
	_widget_event_left_stick_down,
	_widget_event_left_stick_left,
	_widget_event_left_stick_right,
	_widget_event_right_stick_up,
	_widget_event_right_stick_down,
	_widget_event_right_stick_left,
	_widget_event_right_stick_right,
	_widget_event_created,
	_widget_event_deleted
};

enum
{
	/* a held dpad direction repeats no faster than this */
	DPAD_EVENT_REPEAT_MILLISECONDS = 250,
	NUMBER_OF_DPAD_DIRECTIONS =
		_widget_event_dpad_right - _widget_event_dpad_up + 1
};

enum
{
	/* deferred errors wait this long into a level before they are shown */
	DEFERRED_ERROR_DELAY_TICKS = 30
};

enum
{
	WIDGET_DELETED_PLAYER_CONTROL_INHIBIT_FLAGS = 0x0FFF
};

enum
{
	/* named for the errors each result raises; SAVED_GAME_FILES.H publishes no
	enumeration of its own */
	_file_system_check_result_none,
	_file_system_check_result_not_enough_free_space,
	_file_system_check_result_too_many_saved_games,
	NUMBER_OF_FILE_SYSTEM_CHECK_RESULTS
};

/* ---------- macros */

#define SIGN(n) ((n) >= 0 ? 1 : -1)

/* The public definition header intentionally exposes only the shared prefix;
this TU owns the complete January layout used below. */
#define ui_widget_definition_get(tag_index) \
	((struct ui_widget_definition *)tag_get(UI_WIDGET_DEFINITION_TAG, (tag_index)))

/* ---------- structures */

struct stack_memory_pool_block;

struct stack_memory_pool
{
	char const *name;
	byte *base_address;
	long size;
	long maximum_block_count;
	long next_block_index;
	long bytes_used;
	long maximum_bytes_used;
	unsigned long block_count;
	unsigned long maximum_block_count_used;
	long largest_block_size;
	boolean disable_compaction;
	byte unused29[3];
	struct stack_memory_pool_block *first_block;
	struct stack_memory_pool_block *last_block;
	struct stack_memory_pool_block *blocks[1];
};

struct stack_memory_pool_medium
{
	struct stack_memory_pool pool;
	struct stack_memory_pool_block *blocks[MAXIMUM_WIDGET_MEMORY_POOL_BLOCKS - 1];
};

typedef char verify_icon_hud_element_definition_size[
	sizeof(struct icon_hud_element_definition) == 0x10 ? 1 : -1];
typedef char verify_hud_globals_button_icons_offset[
	offsetof(struct hud_globals_definition, messaging.button_icons) == 0xC4 ? 1 : -1];
typedef char verify_interface_tag_references_definition_size[
	sizeof(struct game_globals_interface_tag_references) == 0x130 ? 1 : -1];
/* narrow views of the 'DeLa' widget definition tag and of the three block
elements this file walks; only the members this file reaches are named and
every other span is left explicitly unknown */

struct ui_widget_event_handler_reference
{
	long flags;
	short event_type;
	short function;
	struct tag_reference widget_tag;
	struct tag_reference sound_effect;
	char script[32];
};

struct ui_widget_child_reference
{
	struct tag_reference widget_tag;
	char name[32];
	long flags;
	short custom_controller_index;
	short vertical_offset;
	short horizontal_offset;
	byte unknown03A[0x50 - 0x3A];
};

struct ui_widget_conditional_reference
{
	struct tag_reference widget_tag;
	char name[32];
	long flags;
	short custom_controller_index;
	byte unknown036[0x50 - 0x36];
};

struct ui_widget_game_data_input_reference
{
	short function;
	byte unknown002[0x24 - 0x02];
};

struct ui_widget_search_and_replace_reference
{
	char search_string[32];
	short replace_function;
};

struct ui_widget_definition
{
	short type;
	short controller_index;
	char name[32];
	rectangle2d bounds;
	long flags;
	long milliseconds_to_auto_close;
	long auto_close_fade_time;
	struct tag_reference background_bitmap;
	struct tag_block game_data_inputs;
	struct tag_block event_handlers;
	struct tag_block search_and_replace_functions;
	byte unknown06C[0xEC - 0x6C];
	struct tag_reference text_label_string_list;
	struct tag_reference text_font;
	real_argb_color text_color;
	short justification;
	word text_box_flags;
	byte unknown120[0x12E - 0x120];
	short string_list_index;
	short horizontal_offset;
	short vertical_offset;
	byte unknown134[0x150 - 0x134];
	long list_flags;
	struct tag_reference list_header_bitmap;
	struct tag_reference list_footer_bitmap;
	rectangle2d list_header_bounds;
	rectangle2d list_footer_bounds;
	byte unknown184[0x1A4 - 0x184];
	struct tag_reference extended_description_widget;
	byte unknown1B4[0x2D4 - 0x1B4];
	struct tag_block conditional_widgets;
	byte unknown2E0[0x3E0 - 0x2E0];
	struct tag_block child_widgets;
};

typedef char verify_ui_widget_game_data_input_reference_size[
	sizeof(struct ui_widget_game_data_input_reference) == 0x24 ? 1 : -1];
typedef char verify_ui_widget_search_and_replace_reference_size[
	sizeof(struct ui_widget_search_and_replace_reference) == 0x22 ? 1 : -1];
typedef char verify_ui_widget_child_reference_size[
	sizeof(struct ui_widget_child_reference) == 0x50 ? 1 : -1];
typedef char verify_ui_widget_conditional_reference_size[
	sizeof(struct ui_widget_conditional_reference) == 0x50 ? 1 : -1];
typedef char verify_ui_widget_event_handler_reference_size[
	sizeof(struct ui_widget_event_handler_reference) == 0x48 ? 1 : -1];
typedef char verify_ui_widget_definition_bounds_offset[
	offsetof(struct ui_widget_definition, bounds) == 0x24 ? 1 : -1];
typedef char verify_ui_widget_definition_flags_offset[
	offsetof(struct ui_widget_definition, flags) == 0x2C ? 1 : -1];
typedef char verify_ui_widget_definition_game_data_inputs_offset[
	offsetof(struct ui_widget_definition, game_data_inputs) == 0x48 ? 1 : -1];
typedef char verify_ui_widget_definition_search_and_replace_offset[
	offsetof(struct ui_widget_definition, search_and_replace_functions) == 0x60 ? 1 : -1];
typedef char verify_ui_widget_definition_text_font_offset[
	offsetof(struct ui_widget_definition, text_font) == 0xFC ? 1 : -1];
typedef char verify_ui_widget_definition_text_color_offset[
	offsetof(struct ui_widget_definition, text_color) == 0x10C ? 1 : -1];
typedef char verify_ui_widget_definition_justification_offset[
	offsetof(struct ui_widget_definition, justification) == 0x11C ? 1 : -1];
typedef char verify_ui_widget_definition_text_box_flags_offset[
	offsetof(struct ui_widget_definition, text_box_flags) == 0x11E ? 1 : -1];
typedef char verify_ui_widget_definition_string_list_index_offset[
	offsetof(struct ui_widget_definition, string_list_index) == 0x12E ? 1 : -1];
typedef char verify_ui_widget_definition_horizontal_offset_offset[
	offsetof(struct ui_widget_definition, horizontal_offset) == 0x130 ? 1 : -1];
typedef char verify_ui_widget_definition_list_header_bitmap_offset[
	offsetof(struct ui_widget_definition, list_header_bitmap) == 0x154 ? 1 : -1];
typedef char verify_ui_widget_definition_list_header_bounds_offset[
	offsetof(struct ui_widget_definition, list_header_bounds) == 0x174 ? 1 : -1];
typedef char verify_ui_widget_definition_event_handlers_offset[
	offsetof(struct ui_widget_definition, event_handlers) == 0x54 ? 1 : -1];
typedef char verify_ui_widget_definition_background_bitmap_offset[
	offsetof(struct ui_widget_definition, background_bitmap) == 0x38 ? 1 : -1];
typedef char verify_ui_widget_definition_text_label_string_list_offset[
	offsetof(struct ui_widget_definition, text_label_string_list) == 0xEC ? 1 : -1];
typedef char verify_ui_widget_definition_list_flags_offset[
	offsetof(struct ui_widget_definition, list_flags) == 0x150 ? 1 : -1];
typedef char verify_ui_widget_definition_extended_description_offset[
	offsetof(struct ui_widget_definition, extended_description_widget) == 0x1A4 ? 1 : -1];
typedef char verify_ui_widget_definition_conditional_widgets_offset[
	offsetof(struct ui_widget_definition, conditional_widgets) == 0x2D4 ? 1 : -1];
typedef char verify_ui_widget_definition_child_widgets_offset[
	offsetof(struct ui_widget_definition, child_widgets) == 0x3E0 ? 1 : -1];
typedef char verify_ui_widget_definition_size[
	sizeof(struct ui_widget_definition) == 0x3EC ? 1 : -1];

struct ui_widget_deferred_error
{
	short error_code;
	short local_player_index;
	boolean modal;
	boolean pause_game_time;
};

struct ui_widget_deferred_cinematic_error
{
	short error_code;
	boolean modal;
	boolean pause_game_time;
};

struct widget_stack_data
{
	long previous_widget_tag;
	long focused_child_parent_widget_tag;
	short focused_child_index;
	short local_player_index;
};

struct widget_stack_node
{
	struct widget_stack_data data;
	struct widget_stack_node *next;
};

struct ui_widget_runtime_globals_prefix
{
	struct widget_instance *active_widgets[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS];
	struct widget_stack_node *widget_stack[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS];
	unsigned long current_system_milliseconds;
	long pause_disabled_ticks;
	short main_menu_deferred_error_code;
	short pause_game_time_count;
	real fade_to_black;
	struct ui_widget_deferred_error deferred_errors[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS];
	short deferred_dashboard_error_code;
	boolean deferred_dashboard_optional;
	byte reserved004B;
	struct ui_widget_deferred_cinematic_error deferred_cinematic_errors[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS];
	struct thread_reference *initialization_thread;
	short filesystem_check_result;
	boolean initialized;
	boolean dont_load_children_recursive;
	boolean debug_show_path;
	boolean processing_inhibited;
	boolean main_menu_music_active;
	boolean sound_paused;
};

struct ui_widget_bss_prefix
{
	wchar_t string_data[1024];
	struct ui_widget_runtime_globals_prefix widget_globals;
	boolean we_are_at_the_main_menu;
	byte unknown869[0x870 - 0x869];
	unsigned long dpad_event_times[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS][NUMBER_OF_DPAD_DIRECTIONS];
};

typedef char verify_ui_widget_fade_to_black_offset[
	offsetof(
		struct ui_widget_runtime_globals_prefix,
		fade_to_black) == 0x2C ? 1 : -1];
typedef char verify_ui_widget_pause_disabled_ticks_offset[
	offsetof(
		struct ui_widget_runtime_globals_prefix,
		pause_disabled_ticks) == 0x24 ? 1 : -1];
typedef char verify_ui_widget_main_menu_deferred_error_code_offset[
	offsetof(
		struct ui_widget_runtime_globals_prefix,
		main_menu_deferred_error_code) == 0x28 ? 1 : -1];
typedef char verify_ui_widget_deferred_dashboard_error_code_offset[
	offsetof(
		struct ui_widget_runtime_globals_prefix,
		deferred_dashboard_error_code) == 0x48 ? 1 : -1];
typedef char verify_ui_widget_deferred_dashboard_optional_offset[
	offsetof(
		struct ui_widget_runtime_globals_prefix,
		deferred_dashboard_optional) == 0x4A ? 1 : -1];
typedef char verify_ui_widget_initialization_thread_offset[
	offsetof(
		struct ui_widget_runtime_globals_prefix,
		initialization_thread) == 0x5C ? 1 : -1];
typedef char verify_ui_widget_initialized_offset[
	offsetof(
		struct ui_widget_runtime_globals_prefix,
		initialized) == 0x62 ? 1 : -1];
typedef char verify_ui_widget_debug_show_path_offset[
	offsetof(
		struct ui_widget_runtime_globals_prefix,
		debug_show_path) == 0x64 ? 1 : -1];
typedef char verify_ui_widget_processing_inhibited_offset[
	offsetof(
		struct ui_widget_runtime_globals_prefix,
		processing_inhibited) == 0x65 ? 1 : -1];
typedef char verify_ui_widget_main_menu_music_active_offset[
	offsetof(
		struct ui_widget_runtime_globals_prefix,
		main_menu_music_active) == 0x66 ? 1 : -1];
typedef char verify_ui_widget_runtime_globals_prefix_size[
	sizeof(struct ui_widget_runtime_globals_prefix) == 0x68 ? 1 : -1];
typedef char verify_ui_widget_globals_offset[
	offsetof(
		struct ui_widget_bss_prefix,
		widget_globals) == 0x800 ? 1 : -1];
typedef char verify_ui_widget_main_menu_active_offset[
	offsetof(
		struct ui_widget_bss_prefix,
		we_are_at_the_main_menu) == 0x868 ? 1 : -1];
typedef char verify_ui_widget_dpad_event_times_offset[
	offsetof(
		struct ui_widget_bss_prefix,
		dpad_event_times) == 0x870 ? 1 : -1];

struct widget_animation_data
{
	short current_frame_index;
	short first_frame_index;
	short last_frame_index;
	short number_of_sprite_frames;
};

struct widget_instance
{
	long definition_tag_index;
	char const *name;
	short local_player_index;
	short horizontal_offset;
	short vertical_offset;
	short type;
	boolean visible;
	boolean render_regardless_of_controller_index;
	boolean disabled;
	boolean pause_game_time;
	boolean delete_recursion_lock;
	boolean widget_is_error_dialog;
	boolean close_if_local_player_controller_present;
	byte pad17;
	long creation_time;
	unsigned long milliseconds_to_auto_close;
	unsigned long auto_close_fade_time;
	real alpha_modifier;
	struct widget_instance *previous;
	struct widget_instance *next;
	struct widget_instance *parent;
	struct widget_instance *child;
	struct widget_instance *focused_child;
	union
	{
		struct
		{
			wchar_t *text;
			short string_list_index;
		} text_box;
		struct
		{
			short selected_index;
			/* counted back toward zero one step per rendered frame; the two
			tab functions start it at +15 and -15 and the column list renderer
			clears it */
			short last_list_tab_direction;
			void *list_items;
			word number_of_items;
			struct widget_instance *extended_description;
			wchar_t *item_text;
		} list;
	} parameters;
	struct widget_animation_data animation;
};

typedef char verify_widget_instance_size[
	sizeof(struct widget_instance) == 0x58 ? 1 : -1];
typedef char verify_widget_instance_animation_offset[
	offsetof(struct widget_instance, animation) == 0x50 ? 1 : -1];

typedef char verify_widget_instance_creation_time_offset[
	offsetof(struct widget_instance, creation_time) == 0x18 ? 1 : -1];
typedef char verify_widget_instance_alpha_modifier_offset[
	offsetof(struct widget_instance, alpha_modifier) == 0x24 ? 1 : -1];
typedef char verify_widget_instance_next_offset[
	offsetof(struct widget_instance, next) == 0x2C ? 1 : -1];
typedef char verify_widget_instance_focused_child_offset[
	offsetof(struct widget_instance, focused_child) == 0x38 ? 1 : -1];
typedef char verify_widget_instance_text_box_string_list_index_offset[
	offsetof(struct widget_instance, parameters.text_box.string_list_index) == 0x40 ? 1 : -1];

/* ---------- prototypes */

static boolean transition_to_game_in_progress(
	void);
static __inline real compute_offset_coordinate(
	long time,
	real delta_per_second);
static short get_icon_type(
	wchar_t const *string);
static void render_state_text(
	rectangle2d *bounds,
	rectangle2d *cursor_bounds,
	wchar_t const *text);
static void render_state_bitmap(
	rectangle2d *bounds,
	rectangle2d *cursor_bounds,
	pixel32 color,
	struct icon_hud_element_definition *icon,
	short physical, short semantic, pixel32 glyph_color);
static boolean should_flip_sticks_for_local_player(
	short local_player_index);
static unsigned long __stdcall filesystem_initialization_thread_proc(
	void *input);
static void perform_filesystem_initialization(
	void);
static void ui_widget_delete_children_recursive(
	struct widget_instance *widget);
static struct widget_instance *ui_widget_launch_widget(
	struct widget_instance *widget,
	long new_widget_tag_index);
static __inline boolean widget_instance_can_handle_events(
	struct widget_instance *widget);
static struct widget_instance *widget_instance_find_by_tag_index_recursive(
	struct widget_instance *widget,
	long tag_index);
static void widget_instance_give_focus_directly(
	struct widget_instance *widget,
	struct widget_instance *new_focus);
static void widget_instance_give_focus_by_tag(
	struct widget_instance *widget,
	long tag_index,
	short local_player_index);

static __inline struct widget_instance *widget_instance_get_tail_child_widget(
	struct widget_instance *widget);
static void ui_widget_add_child(
	struct widget_instance *parent,
	struct widget_instance *child);
static void push_widget(
	struct widget_stack_node **top,
	struct widget_stack_data *data);
static boolean widget_instance_can_receive_events(
	struct widget_instance *widget);
static void widget_instance_set_focused_child_by_index(
	long tag_index,
	struct widget_instance *widget,
	short child_index);
static void widget_instance_go_back_to_previous(
	struct widget_instance *widget);
static __inline struct widget_instance *widget_instance_find_by_tag_index(
	long tag_index);
static void widget_instance_reload_recursive(
	struct widget_instance *widget);
static void ui_widget_reload_by_tag(
	long tag_index);
static void event_handler_dispatch(
	struct widget_instance *widget,
	struct ui_widget_definition *definition,
	struct event_record *event,
	struct ui_widget_event_handler_reference *handler,
	boolean *calling_widget_deleted);
static boolean ui_widget_load_children_recursive(
	struct widget_instance *widget,
	struct ui_widget_definition *definition);
/* port: a standard tall row, as the menus' files or a patch said (menu_tags.c) */
boolean pc_menu_row_tall(
	long tag_index);
/* port: whether the tag is one of the menus' (port/linux/game/menu_tags.c) */
boolean pc_menu_tag(
	long tag_index);
/* port: how many rows of a menu file's column list show at once
(visible_rows=), else 0 (port/linux/game/menu_tags.c) */
short pc_menu_list_window(
	long tag_index);
/* port: whether the list is one the menus' patch table changed, and then its
row's description number (port/linux/game/menu_tags.c) */
boolean pc_menu_patch_list_index(
	long list_tag,
	long row_tag,
	short *description);
/* port: unavailable browser rows stay focusable for their reason text
(port/linux/game/menu_functions.c), and draw in gray. */
boolean ui_widget_port_browser_row_unavailable(
	struct widget_instance *widget);
/* port: a disabled row (grayed INTERNET or SPLIT SCREEN) refuses an activation (menu_functions.c) */
boolean ui_widget_port_disabled_row_activation(
	struct widget_instance const *widget,
	short event_type);
/* port: a graphics-mode override locks editing without disabling the row (menu_functions.c). */
boolean ui_widget_port_graphics_locked(
	struct widget_instance *widget);
/* port: a text field is being typed in (menu_functions.c) */
boolean ui_widget_port_text_editing(
	void);
/* port: whether the screen's bar action on the event's button waits while typing (its key is a letter then) */
static boolean ui_bar_key_is_letter(
	struct widget_instance *screen,
	struct event_record const *event);
/* port: where in its widget, and how large, the menus draw a frame of
ui.map's that they scale (port/linux/game/menu_tags.c) */
boolean pc_menu_frame_placement(
	struct bitmap_data const *bitmap,
	short *x,
	short *y,
	short *width,
	short *height);
static void widget_instance_initialize(
	struct widget_instance *widget,
	struct widget_instance *parent,
	struct ui_widget_definition *definition,
	long tag_index,
	short local_player_index,
	short widget_stack);
static __inline real widget_instance_get_cumulative_alpha_modifier(
	struct widget_instance *widget);
static boolean widget_instance_text_box_is_focused(
	struct widget_instance *widget);
static boolean string_has_icons_to_draw(
	wchar_t *string);
static long search_and_replace(
	wchar_t *search,
	wchar_t *replace,
	wchar_t **string);
static short ui_mouse_key_button(struct widget_instance *widget);
static void widget_instance_render_text_box(
	struct widget_instance *widget,
	struct ui_widget_definition *definition,
	rectangle2d *clip_rect,
	point2d offset,
	boolean focus);
static void widget_instance_render_spinner_list(
	struct widget_instance *widget,
	struct ui_widget_definition *definition,
	rectangle2d *clip_rect,
	point2d offset,
	boolean focus);
static void widget_instance_render_column_list(
	struct widget_instance *widget,
	struct ui_widget_definition *definition,
	rectangle2d *clip_rect,
	point2d offset,
	boolean focus);
static void widget_instance_render_recursive(
	struct widget_instance *widget,
	rectangle2d *clip_rect,
	point2d offset,
	boolean focus,
	boolean use_nifty_plasma_fx);
static __inline void widget_instance_update_animation_parameters(
	struct widget_instance *widget);
static __inline void spinner_list_update(
	struct widget_instance *widget);
static void column_list_update(
	struct widget_instance *widget,
	struct ui_widget_definition *definition);
static void widget_instance_tab_to_next_valid_widget(
	struct widget_instance *widget);
static void widget_instance_tab_to_previous_valid_widget(
	struct widget_instance *widget);
static void widget_instance_process_one_event_recursive(
	struct widget_instance *widget,
	struct ui_widget_definition *definition,
	struct event_record *event,
	boolean *return_widget_deleted);
static boolean ui_check_for_pause_game(
	void);
static long spinner_string_list_extra_count(
	long string_list_index);
static wchar_t *spinner_string_list_get_string(
	long string_list_index,
	short string_index);

/* ---------- globals */

/* port: text boxes' string list indices from here are the descriptions of
spinners' extra items (kills_to_win_extra_descriptions) */
#define SPINNER_EXTRA_DESCRIPTION_BASE 0x5000
/* ... and the pixels a spinner with extra items is wider (for three digits) */
#define SPINNER_EXTRA_WIDTH 12

/* port: the strings a spinner's string list has past the tag's own, as more
items: the higher kills to win of the Slayer game type editor
(ui_widget_event_handler_functions.c saves and loads them) */
static wchar_t const *const kills_to_win_extra_strings[] =
{
	L"75", L"100", L"150", L"200", L"250", L"500",
};

/* ... their descriptions, as a text box's string list index of
SPINNER_EXTRA_DESCRIPTION_BASE and up (ui_widget_spinner_extra_description) */
static wchar_t const *const kills_to_win_extra_descriptions[] =
{
	L"Seventy-five kills to win. Settle in for a long\r\nfight.",
	L"A hundred kills to win. Made for big games.",
	L"A hundred and fifty kills to win. Only a crowded\r\nserver gets there.",
	L"Two hundred kills to win. Bring friends. Lots of\r\nthem.",
	L"Two hundred and fifty kills to win.",
	L"Five hundred kills to win. You'll be here a while.",
};

/* port: an error message of the port's own text (display_error_text_deferred):
the text waiting for its dialog, then the dialog's text box showing it */
static wchar_t const *ui_widget_port_error_pending_text = NULL;
static wchar_t const *ui_widget_port_error_text = NULL;
static struct widget_instance *ui_widget_port_error_text_box = NULL;
/* port: the main menu's version label (set_textbox_to_build_number) */
struct widget_instance *ui_widget_port_build_label = NULL;

static struct ui_widget_bss_prefix ui_widget_globals_storage;

#define string_data ui_widget_globals_storage.string_data
#define widget_globals ui_widget_globals_storage.widget_globals
#define we_are_at_the_main_menu ui_widget_globals_storage.we_are_at_the_main_menu
#define dpad_event_times ui_widget_globals_storage.dpad_event_times
real_argb_color ui_plasma_effect_color;
short local_player_index_for_draw_string_and_hack_in_icons;

/* January defines this and never references it, as we do not */
real const _one_over_255 = 1.0f / 255.0f;
real const SECONDS_PER_MILLISECOND = 0.001f;

static struct stack_memory_pool_medium __medium_widget_memory_pool =
{
	{
		"widget_memory_pool",
		NULL,
		0,
		MAXIMUM_WIDGET_MEMORY_POOL_BLOCKS
	}
};

struct stack_memory_pool *widget_memory_pool = &__medium_widget_memory_pool.pool;

static boolean main_screen_shell_first_load = TRUE;

short dashboard_abort_error = NONE;

static char const *scenario_paths[10] =
{
	"levels\\a10\\a10",
	"levels\\a30\\a30",
	"levels\\a50\\a50",
	"levels\\b30\\b30",
	"levels\\b40\\b40",
	"levels\\c10\\c10",
	"levels\\c20\\c20",
	"levels\\c40\\c40",
	"levels\\d20\\d20",
	"levels\\d40\\d40"
};

static boolean icon_is_special[NUM_ICONS] =
{
	FALSE,	/* a-button */
	FALSE,	/* b-button */
	FALSE,	/* x-button */
	FALSE,	/* y-button */
	FALSE,	/* black-button */
	FALSE,	/* white-button */
	TRUE,	/* left-trigger */
	TRUE,	/* right-trigger */
	FALSE,	/* dpad-up */
	FALSE,	/* dpad-down */
	FALSE,	/* dpad-left */
	FALSE,	/* dpad-right */
	FALSE,	/* start-button */
	FALSE,	/* back-button */
	TRUE,	/* left-thumb */
	TRUE,	/* right-thumb */
	TRUE,	/* left-stick */
	TRUE	/* right-stick */
};

static wchar_t const *icon_names[NUMBER_OF_ICON_TYPES] =
{
	L"a-button",
	L"b-button",
	L"x-button",
	L"y-button",
	L"black-button",
	L"white-button",
	L"left-trigger",
	L"right-trigger",
	L"dpad-up",
	L"dpad-down",
	L"dpad-left",
	L"dpad-right",
	L"start-button",
	L"back-button",
	L"left-thumb",
	L"right-thumb",
	L"left-stick",
	L"right-stick",
	L"action",
	L"throw-grenade",
	L"primary-trigger",
	L"integrated-light",
	L"jump",
	L"use-equipment",
	L"rotate-weapons",
	L"rotate-grenades",
	L"crouch",
	L"zoom",
	L"accept",
	L"back",
	L"move",
	L"look",
	L"custom-1",
	L"custom-2",
	L"custom-3",
	L"custom-4",
	L"custom-5",
	L"custom-6",
	L"custom-7",
	L"custom-8"
};

/* indexed by icon type - _icon_action.  The first ten entries are game control
indices for game_input_preferences.game_control_to_xbox_buttons (that enumeration
is private to INPUT_ABSTRACTION.C); the last four are icon types used directly
when the icon does not depend on the local player's control preferences. */
static char button_mappings[_icon_custom_1 - _icon_action] =
{
	2,		/* action -> action */
	6,		/* throw-grenade -> grenade */
	7,		/* primary-trigger -> primary trigger */
	5,		/* integrated-light -> flashlight */
	0,		/* jump -> jump */
	4,		/* use-equipment -> melee */
	3,		/* rotate-weapons -> switch weapons */
	1,		/* rotate-grenades -> switch grenades */
	11,		/* crouch -> zoom */
	10,		/* zoom -> crouch */
	_icon_a_button,		/* accept */
	_icon_b_button,		/* back */
	_icon_left_stick,	/* move */
	_icon_right_stick	/* look */
};

static real global_ui_white_red = 0.8f;
static real global_ui_white_green = 0.8f;
static real global_ui_white_blue = 0.8f;


/* port: independently owned bitmaps allow two players to draw different
symbols for the same original atlas frame during one render pass. */
#ifdef HALO_IOS
#define CONTROLLER_GLYPH_CACHE_SIZE 64
static struct
{
	int kind, width, height;
	char label[96];
	struct bitmap_data *bitmap;
} controller_glyph_cache[CONTROLLER_GLYPH_CACHE_SIZE];
static short controller_glyph_cache_count, controller_glyph_cache_next;
#endif

void controller_glyphs_dispose(void)
{
#ifdef HALO_IOS
	short index;
	for (index = 0; index < controller_glyph_cache_count; index++)
		bitmap_delete(controller_glyph_cache[index].bitmap);
	memset(controller_glyph_cache, 0, sizeof(controller_glyph_cache));
	controller_glyph_cache_count = controller_glyph_cache_next = 0;
#endif
}

/* port: clipping must crop texture coordinates rather than stretch the
remaining glyph. Also used for the keyboard backdrop's two legend holes. */
void controller_glyph_blit(struct bitmap_data *bitmap, rectangle2d const *bounds,
	rectangle2d const *source, rectangle2d const *clip, pixel32 color, short blend)
{
	struct dynamic_screen_vertex vertices[4];
	struct rasterizer_dynamic_screen_geometry_parameters parameters;
	rectangle2d visible = *bounds;
	real u0, v0, u1, v1, source_x, source_y, source_width, source_height;
	int index;

	if (!bitmap || bounds->x1 <= bounds->x0 || bounds->y1 <= bounds->y0) return;
	if (clip)
	{
		visible.x0 = MAX(visible.x0, clip->x0); visible.x1 = MIN(visible.x1, clip->x1);
		visible.y0 = MAX(visible.y0, clip->y0); visible.y1 = MIN(visible.y1, clip->y1);
	}
	if (visible.x1 <= visible.x0 || visible.y1 <= visible.y0) return;
	source_x = source ? source->x0 : 0; source_y = source ? source->y0 : 0;
	source_width = source ? source->x1 - source->x0 : bitmap->width;
	source_height = source ? source->y1 - source->y0 : bitmap->height;
	u0 = (source_x + (visible.x0-bounds->x0)*source_width/(bounds->x1-bounds->x0))/bitmap->width;
	u1 = (source_x + (visible.x1-bounds->x0)*source_width/(bounds->x1-bounds->x0))/bitmap->width;
	v0 = (source_y + (visible.y0-bounds->y0)*source_height/(bounds->y1-bounds->y0))/bitmap->height;
	v1 = (source_y + (visible.y1-bounds->y0)*source_height/(bounds->y1-bounds->y0))/bitmap->height;
	memset(vertices, 0, sizeof(vertices));
	for (index = 0; index < 4; index++)
	{
		vertices[index].position.x = index == 0 || index == 3 ? visible.x0 : visible.x1;
		vertices[index].position.y = index < 2 ? visible.y0 : visible.y1;
		vertices[index].texture_coordinates.x = index == 0 || index == 3 ? u0 : u1;
		vertices[index].texture_coordinates.y = index < 2 ? v0 : v1;
		vertices[index].color = color;
	}
	memset(&parameters, 0, sizeof(parameters));
	parameters.map[0] = bitmap;
	parameters.framebuffer_blend_function = blend;
	/* port: HUD blend uses ONE, so its white coverage must weight RGB by
	alpha; menu SRCALPHA already does that multiplication at blending. */
	parameters.alpha_weighted = blend == _controller_glyph_blend_hud;
	parameters.map_scale[0].i = parameters.map_scale[0].j = 1.0f;
	parameters.map_texture_scale[0].i = parameters.map_texture_scale[0].j = 1.0f;
	rasterizer_psuedo_dynamic_screen_quad_draw(&parameters, vertices);
}

int controller_glyph_draw(short player, short semantic, short physical,
	rectangle2d const *bounds, rectangle2d const *clip, pixel32 color, short blend)
{
#ifdef HALO_IOS
	char label[96];
	int kind, width, height, padded_width = 8, padded_height = 8, index;
	real scale;
	struct bitmap_data *bitmap;
	rectangle2d source;
	int host_controller_glyph_raster(int kind, char const *label, int width, int height, unsigned char *pixels);
	float halo_screen_pixel_scale(void);

	if (!bounds || bounds->x1 <= bounds->x0 || bounds->y1 <= bounds->y0) return 0;
	kind = halo_controller_glyph_request(player, semantic, physical, label, sizeof(label));
	if (kind == HALO_GLYPH_HIDDEN) return 1;
	if (kind != HALO_GLYPH_SYMBOL && kind != HALO_GLYPH_KEYCAP) return 0;
	scale = PIN(halo_screen_pixel_scale(), 1.0f, 8.0f);
	width = PIN((int)((bounds->x1 - bounds->x0) * scale + 0.999f), 8, 512);
	height = PIN((int)((bounds->y1 - bounds->y0) * scale + 0.999f), 8, 256);
	while (padded_width < width) padded_width *= 2;
	while (padded_height < height) padded_height *= 2;
	for (index = 0; index < controller_glyph_cache_count; index++)
	{
		if (controller_glyph_cache[index].kind == kind && controller_glyph_cache[index].width == width &&
			controller_glyph_cache[index].height == height && !strcmp(controller_glyph_cache[index].label, label)) break;
	}
	if (index == controller_glyph_cache_count)
	{
		int replacing = index == CONTROLLER_GLYPH_CACHE_SIZE;
		unsigned char *pixels = malloc(width * height * 4);
		int row;
		if (!pixels) return 0;
		if (!host_controller_glyph_raster(kind, label, width, height, pixels))
		{
			free(pixels);
			return 0;
		}
		bitmap = bitmap_2d_new((short)padded_width, (short)padded_height, 0, 11);
		if (!bitmap || !bitmap->base_address)
		{
			if (bitmap) bitmap_delete(bitmap);
			free(pixels);
			return 0;
		}
		memset(bitmap->base_address, 0, padded_width * padded_height * 4);
		for (row = 0; row < height; row++)
			memcpy((unsigned char *)bitmap->base_address + row * padded_width * 4, pixels + row * width * 4, width * 4);
		free(pixels);
		if (!rasterizer_bitmap_new(bitmap))
		{
			bitmap_delete(bitmap);
			return 0;
		}
		rasterizer_bitmap_changed(bitmap);
		if (replacing)
		{
			index = controller_glyph_cache_next;
			controller_glyph_cache_next = (controller_glyph_cache_next + 1) % CONTROLLER_GLYPH_CACHE_SIZE;
			bitmap_delete(controller_glyph_cache[index].bitmap);
		}
		controller_glyph_cache[index].bitmap = bitmap;
		controller_glyph_cache[index].kind = kind;
		controller_glyph_cache[index].width = width;
		controller_glyph_cache[index].height = height;
		strcpy(controller_glyph_cache[index].label, label);
		if (!replacing) controller_glyph_cache_count++;
	}
	bitmap = controller_glyph_cache[index].bitmap;
	source.x0 = source.y0 = 0; source.x1 = (short)width; source.y1 = (short)height;
	if (blend >= 0) controller_glyph_blit(bitmap, bounds, &source, clip, color, blend);
	return 1;
#else
	(void)player; (void)semantic; (void)physical; (void)bounds; (void)clip; (void)color; (void)blend;
	return 0;
#endif
}

/* port: prepare before cutting baked legend art out of a background, so
missing symbols and Xbox fallback leave the original picture untouched. */
int controller_glyph_hidden(short player, short physical)
{
	char label[96];
	return halo_controller_glyph_request(player, NONE, physical, label, sizeof(label)) == HALO_GLYPH_HIDDEN;
}

int controller_glyph_ready(short player, short semantic, short physical, rectangle2d const *bounds)
{
	return controller_glyph_draw(player, semantic, physical, bounds, NULL, 0, -1);
}

/* port: three reusable widgets bind a nonwrapping strip. Keep its motion out of
widget_instance, whose layout is shared with the original game. */
static void widget_instance_give_focus_directly(
	struct widget_instance *widget, struct widget_instance *new_focus);
#define CAROUSEL_STATE_COUNT 16
struct ui_carousel_state
{
	struct widget_instance *widget;
	long creation_time;
	int first, binding_first;
	unsigned long last_tick, last_input;
	struct halo_ui_drag drag;
	rectangle2d frame;
};
static struct ui_carousel_state ui_carousels[CAROUSEL_STATE_COUNT];
static unsigned int ui_carousel_next;
static struct ui_carousel_state *ui_carousel_capture;
static boolean ui_carousel_pointer_was_down;
/* Absolute menu-space clipping for descendants whose bitmap clips otherwise
move with their own offsets. Active only during the carousel's render passes. */
static rectangle2d *ui_carousel_clip;

static boolean ui_widget_is_carousel(struct widget_instance *widget)
{
	return widget && widget->type == _ui_widget_type_spinner_list &&
		ui_widget_definition_get(widget->definition_tag_index)->child_widgets.count == 3;
}

static struct ui_carousel_state *ui_carousel_state(struct widget_instance *widget)
{
	unsigned int index;
	struct ui_carousel_state *state;
	for (index = 0; index < CAROUSEL_STATE_COUNT; index++)
		if (ui_carousels[index].widget == widget &&
			ui_carousels[index].creation_time == widget->creation_time)
			return &ui_carousels[index];
	state = &ui_carousels[ui_carousel_next++ % CAROUSEL_STATE_COUNT];
	if (ui_carousel_capture == state)
		ui_carousel_capture = NULL;
	csmemset(state, 0, sizeof(*state));
	state->widget = widget;
	state->frame.x1 = 640;
	state->frame.y1 = 480;
	state->creation_time = widget->creation_time;
	state->first = halo_ui_carousel_clamp_first(widget->parameters.list.selected_index,
		widget->parameters.list.number_of_items);
	state->binding_first = state->first;
	state->last_tick = widget_globals.current_system_milliseconds;
	state->last_input = system_milliseconds();
	halo_ui_drag_reset(&state->drag, state->first * HALO_UI_CAROUSEL_PITCH);
	return state;
}

/* Game-data inputs use this even while a render pass temporarily rebinds the
three cards. Focus never determines their contents. */
long ui_widget_port_carousel_first(struct widget_instance *widget)
{
	return ui_carousel_state(widget)->binding_first;
}

static void ui_carousel_forget(void *widget)
{
	unsigned int index;
	for (index = 0; index < CAROUSEL_STATE_COUNT; index++)
		if (ui_carousels[index].widget == widget)
		{
			if (ui_carousel_capture == &ui_carousels[index])
				ui_carousel_capture = NULL;
			ui_carousels[index].widget = NULL;
		}
}

static void ui_carousel_focus(struct widget_instance *widget, int item, int first)
{
	struct widget_instance *child = widget_instance_get_nth_child(widget, item - first);
	if (child)
		widget_instance_give_focus_directly(widget, child);
	widget->parameters.list.selected_index = (short)item;
}

static boolean ui_carousel_controller_move(struct widget_instance *widget, int direction)
{
	struct ui_carousel_state *state = ui_carousel_state(widget);
	int focus = widget->parameters.list.selected_index;
	if (!halo_ui_carousel_controller_move(&state->drag, &focus, &state->first,
		widget->parameters.list.number_of_items, direction))
	{
		ui_play_audio_feedback_sound(_ui_audio_feedback_flag_failure);
		return TRUE;
	}
	state->binding_first = state->first;
	ui_carousel_focus(widget, focus, state->first);
	return TRUE;
}

static void ui_carousel_tick(struct widget_instance *widget)
{
	struct ui_carousel_state *state = ui_carousel_state(widget);
	unsigned long now = widget_globals.current_system_milliseconds;
	float dt = MIN((float)(now - state->last_tick) * 0.001f, 0.1f);
	int count = widget->parameters.list.number_of_items;
	state->last_tick = now;
	state->first = halo_ui_carousel_clamp_first(state->first, count);
	if (state->drag.state == HALO_UI_DRAG_IDLE)
		state->drag.offset = halo_ui_drag_snap(state->drag.offset, HALO_UI_CAROUSEL_PITCH,
			0, halo_ui_carousel_max_first(count) * HALO_UI_CAROUSEL_PITCH);
	state->drag.target = halo_ui_drag_snap(state->drag.target, HALO_UI_CAROUSEL_PITCH,
		0, halo_ui_carousel_max_first(count) * HALO_UI_CAROUSEL_PITCH);
	if (dt > 0)
		halo_ui_drag_step(&state->drag, dt);
	if (state->drag.state == HALO_UI_DRAG_IDLE && count > 0)
	{
		int focus;
		state->first = halo_ui_carousel_settled_first(state->drag.offset, count);
		focus = halo_ui_carousel_focus_in_window(widget->parameters.list.selected_index, state->first, count);
		if (focus != widget->parameters.list.selected_index ||
			widget->focused_child != widget_instance_get_nth_child(widget, focus - state->first))
			ui_carousel_focus(widget, focus, state->first);
	}
	state->binding_first = state->first;
}

/* port: carousel state helpers end. */

/* ---------- public code */

void set_ui_plasma_effect_color(
	real alpha,
	real red,
	real green,
	real blue)
{
	ui_plasma_effect_color.alpha = alpha;
	ui_plasma_effect_color.red = red;
	ui_plasma_effect_color.green = green;
	ui_plasma_effect_color.blue = blue;

	return;
}

boolean event_controller_index_compatible_with_widget(
	struct event_record const *event,
	struct widget_instance const *widget)
{
	short widget_controller_index;

	widget_controller_index = widget->local_player_index;
	return widget_controller_index == NONE ||
		widget_controller_index == event->controller_index;
}

void ui_widgets_safe_to_load(
	boolean safe)
{
	return;
}

void ui_widgets_inhibit_processing(
	boolean inhibit)
{
	match_assert(
		"c:\\halo\\SOURCE\\interface\\ui_widget.c",
		1174,
		widget_globals.initialized);
	widget_globals.processing_inhibited = inhibit;

	return;
}

static __inline real compute_offset_coordinate(
	long time,
	real delta_per_second)
{
	real scaled_delta = delta_per_second * 0.001f;

	return (real)fmod(
		scaled_delta * time,
		1.0);
}

/* port: the carousel's edges fade its peeks out instead of cutting them with
a hard line. The clipped quad is split where the fade starts and ends, and each
piece's corner alphas carry the linear fade, using the game's own alpha blend. */
static void ui_carousel_faded_quad_draw(
	struct rasterizer_dynamic_screen_geometry_parameters *parameters,
	struct dynamic_screen_vertex const *clipped,
	pixel32 argb,
	short rectangle_x0, short rectangle_y0, short rectangle_width, short rectangle_height,
	real texture_width, real texture_height)
{
	/* port: the plasma pixel shader ignores vertex alpha (its alpha comes from the map 2 fade constant),
	   so plasma draws are cut into narrow strips that each carry a constant fade. */
	enum { MAXIMUM_CUTS = 64 };
	real frame_x0 = (real)ui_carousel_clip->x0;
	real frame_x1 = (real)ui_carousel_clip->x1;
	real left = clipped[0].position.x;
	real right = clipped[1].position.x;
	real cuts[MAXIMUM_CUTS];
	short cut_count = 0;
	short cut_index;
	unsigned long alpha = (unsigned long)argb >> 24;
	boolean strips = parameters->doing_plasma_effect ? TRUE : FALSE;
	real strip_fade = 1.0f;

	cuts[cut_count++] = left;
	if (strips)
	{
		real x;
		for (x = left + 4.0f; x < right && cut_count < MAXIMUM_CUTS - 1; x += 4.0f)
			cuts[cut_count++] = x;
	}
	else
	{
		if (frame_x0 + HALO_UI_CAROUSEL_FADE_WIDTH > left && frame_x0 + HALO_UI_CAROUSEL_FADE_WIDTH < right)
			cuts[cut_count++] = frame_x0 + HALO_UI_CAROUSEL_FADE_WIDTH;
		if (frame_x1 - HALO_UI_CAROUSEL_FADE_WIDTH > cuts[cut_count - 1] && frame_x1 - HALO_UI_CAROUSEL_FADE_WIDTH < right)
			cuts[cut_count++] = frame_x1 - HALO_UI_CAROUSEL_FADE_WIDTH;
	}
	cuts[cut_count++] = right;
	if (strips)
		parameters->map_fade[2] = &strip_fade;
	for (cut_index = 0; cut_index + 1 < cut_count; cut_index++)
	{
		struct dynamic_screen_vertex piece[NUMBER_OF_POINTS_PER_RECTANGLE];
		short vertex_index;
		boolean visible = FALSE;

		if (strips)
		{
			strip_fade = halo_ui_carousel_fade(0.5f * (cuts[cut_index] + cuts[cut_index + 1]), frame_x0, frame_x1);
			if (strip_fade > 0.0f)
				visible = TRUE;
		}
		for (vertex_index = 0; vertex_index < NUMBER_OF_POINTS_PER_RECTANGLE; vertex_index++)
		{
			real x = (vertex_index % 3) ? cuts[cut_index + 1] : cuts[cut_index];
			real fade = strips ? 1.0f : halo_ui_carousel_fade(x, frame_x0, frame_x1);
			unsigned long faded = (unsigned long)(alpha * fade + 0.5f);

			piece[vertex_index] = clipped[vertex_index];
			piece[vertex_index].position.x = x;
			piece[vertex_index].texture_coordinates.x = texture_width * (x - rectangle_x0) / rectangle_width;
			piece[vertex_index].texture_coordinates.y = texture_height *
				(piece[vertex_index].position.y - rectangle_y0) / rectangle_height;
			piece[vertex_index].color = (pixel32)(((unsigned long)argb & 0x00FFFFFFul) | (faded << 24));
			if (faded && !strips)
				visible = TRUE;
		}
		if (visible)
			rasterizer_psuedo_dynamic_screen_quad_draw(parameters, piece);
	}
	if (strips)
		parameters->map_fade[2] = NULL;
}

void draw_bitmap_in_rect(
	struct bitmap_data *bitmap,
	rectangle2d *rect,
	rectangle2d *bitmap_rect,
	rectangle2d *clip_rect,
	pixel32 argb,
	struct rasterizer_dynamic_screen_geometry_parameters *multitexture_params,
	boolean no_plasma)
{
	rectangle2d carousel_clip;
	if (ui_carousel_clip)
	{
		carousel_clip = clip_rect ? *clip_rect : *ui_carousel_clip;
		carousel_clip.x0 = MAX(carousel_clip.x0, ui_carousel_clip->x0);
		carousel_clip.x1 = MIN(carousel_clip.x1, ui_carousel_clip->x1);
		carousel_clip.y0 = MAX(carousel_clip.y0, ui_carousel_clip->y0);
		carousel_clip.y1 = MIN(carousel_clip.y1, ui_carousel_clip->y1);
		if (!rect || carousel_clip.x0 >= rect->x1 || carousel_clip.x1 <= rect->x0 ||
			carousel_clip.y0 >= rect->y1 || carousel_clip.y1 <= rect->y0)
			return;
		clip_rect = &carousel_clip;
	}

	if (bitmap && rect)
	{
		real_argb_color plasma_fade = ui_plasma_effect_color;
		real_rgb_color map_tint = { 0.9f, 0.9f, 0.9f };
		real map_fade = 0.9f;
		rectangle2d temp;
		real_point2d points[NUMBER_OF_POINTS_PER_RECTANGLE];
		struct dynamic_screen_vertex vertices[NUMBER_OF_POINTS_PER_RECTANGLE];
		struct rasterizer_dynamic_screen_geometry_parameters parameters;
		real bitmap_width;
		real bitmap_height;
		real texture_width;
		real texture_height;
		real_point2d map0_offset;
		real_point2d map1_offset;
		short rectangle_x0;
		short rectangle_y0;
		short rectangle_width;
		short rectangle_height;
		short source_width;
		short source_height;
		short vertex_index;

		if (!bitmap_rect)
		{
			temp.x0 = 0;
			temp.y0 = 0;
			temp.x1 = bitmap->width;
			temp.y1 = bitmap->height;
			bitmap_rect = &temp;
		}

		rectangle_width = rect->x1 - rect->x0;
		rectangle_height = rect->y1 - rect->y0;
		rectangle_x0 = rect->x0;
		rectangle_y0 = rect->y0;
		source_width = bitmap_rect->x1 - bitmap_rect->x0;
		source_height = bitmap_rect->y1 - bitmap_rect->y0;
		points[0].x = (real)rectangle_x0;
		points[0].y = (real)rectangle_y0;
		points[1].x = (real)(rectangle_x0 + rectangle_width);
		points[1].y = (real)rectangle_y0;
		points[2].x = (real)(rectangle_x0 + rectangle_width);
		points[2].y = (real)(rectangle_y0 + rectangle_height);
		points[3].x = (real)rectangle_x0;
		points[3].y = (real)(rectangle_y0 + rectangle_height);

		if (clip_rect)
		{
			if (clip_rect->x0 > rect->x0)
			{
				points[0].x = points[3].x = (real)clip_rect->x0;
			}
			if (clip_rect->x1 < rect->x1)
			{
				points[1].x = points[2].x = (real)clip_rect->x1;
			}
			if (clip_rect->y0 > rect->y0)
			{
				points[0].y = points[1].y = (real)clip_rect->y0;
			}
			if (clip_rect->y1 < rect->y1)
			{
				points[2].y = points[3].y = (real)clip_rect->y1;
			}
		}

		bitmap_width = MAX(1.0f, (real)bitmap->width);
		texture_width = MIN(
			(real)source_width / bitmap_width,
			1.0f);
		bitmap_height = MAX(1.0f, (real)bitmap->height);
		texture_height = MIN(
			(real)source_height / bitmap_height,
			1.0f);

		for (vertex_index = 0;
			vertex_index < NUMBER_OF_POINTS_PER_RECTANGLE;
			vertex_index++)
		{
			vertices[vertex_index].color = argb;
			vertices[vertex_index].texture_coordinates.x =
				(vertex_index % 3) ? texture_width : 0.0f;
			vertices[vertex_index].texture_coordinates.y =
				(vertex_index > 1) ? texture_height : 0.0f;
			vertices[vertex_index].position = points[vertex_index];
		}

		/* port: crop the moving card's texture instead of squeezing it into
		the visible peek. Other menu quads retain their original behavior. */
		if (ui_carousel_clip && rectangle_width > 0 && rectangle_height > 0)
			for (vertex_index = 0; vertex_index < NUMBER_OF_POINTS_PER_RECTANGLE; vertex_index++)
			{
				vertices[vertex_index].texture_coordinates.x = texture_width *
					(points[vertex_index].x - rectangle_x0) / rectangle_width;
				vertices[vertex_index].texture_coordinates.y = texture_height *
					(points[vertex_index].y - rectangle_y0) / rectangle_height;
			}

		csmemset(&parameters, 0, sizeof(parameters));
		if (no_plasma)
		{
			parameters.map_texture_scale[0].j = 1.0f;
			parameters.map_texture_scale[0].i = 1.0f;
			parameters.map_scale[0].j = 1.0f;
			parameters.map_scale[0].i = 1.0f;
			parameters.map[0] = bitmap;
		}
		else
		{
			struct bitmap_data *plasma_bitmap = TAG_BLOCK_GET_ELEMENT(
				&bitmap_group_get(
					interface_get_tag_index(_interface_bitmap_iface_map3))->bitmaps,
				0,
				struct bitmap_data);
			long time = system_milliseconds();

			map0_offset.x =
				compute_offset_coordinate(time, 0.03215434f) * 311.0f;
			map0_offset.y =
				compute_offset_coordinate(time, 0.026795285f) * 311.0f;
			map1_offset.x =
				-compute_offset_coordinate(time, 0.035536603f);
			map1_offset.x *= 201.0f;
			map1_offset.y =
				-compute_offset_coordinate(time, 0.031094525f);
			map1_offset.y *= 201.0f;

			parameters.map[0] = plasma_bitmap;
			parameters.map0_to_1_blend_function = 5;
			parameters.map_scale[0].i = 1.0f;
			parameters.map_scale[0].j = 1.0f;
			parameters.map_wrapped[0] = TRUE;
			parameters.map_anchor_screen[0] = TRUE;
			parameters.map_texture_scale[0].i = 1.0f / 311.0f;
			parameters.map_texture_scale[0].j = 1.0f / 311.0f;
			parameters.map_tint[0] = &map_tint;
			parameters.map_fade[0] = &map_fade;
			parameters.map_offset[0] = &map0_offset;
			parameters.map[1] = plasma_bitmap;
			parameters.map1_to_2_blend_function = 0;
			parameters.map_scale[1].i = 1.0f;
			parameters.map_scale[1].j = 1.0f;
			parameters.map_wrapped[1] = TRUE;
			parameters.map_anchor_screen[1] = TRUE;
			parameters.map_texture_scale[1].i = 1.0f / 201.0f;
			parameters.map_texture_scale[1].j = 1.0f / 201.0f;
			parameters.map_tint[1] = &map_tint;
			parameters.map_fade[1] = &map_fade;
			parameters.map_offset[1] = &map1_offset;
			parameters.plasma_fade = plasma_fade;
			parameters.map_fade[2] = NULL;
			parameters.map_texture_scale[2].j = 1.0f;
			parameters.map_texture_scale[2].i = 1.0f;
			parameters.map_scale[2].j = 1.0f;
			parameters.map_scale[2].i = 1.0f;
			parameters.map[2] = bitmap;
			parameters.doing_plasma_effect = TRUE;
		}

		parameters.meter_parameters = NULL;
		parameters.point_sampled = FALSE;
		/* port: (rasterizer.h) */
		parameters.alpha_weighted = FALSE;
		parameters.framebuffer_blend_function = 0;
		if (ui_carousel_clip && rectangle_width > 0 && rectangle_height > 0)
			ui_carousel_faded_quad_draw(&parameters, vertices, argb,
				rectangle_x0, rectangle_y0, rectangle_width, rectangle_height, texture_width, texture_height);
		else
			rasterizer_psuedo_dynamic_screen_quad_draw(&parameters, vertices);
	}

	return;
}

void ui_widgets_set_fade_value(
	real value)
{
	widget_globals.fade_to_black = value;

	return;
}

void ui_widget_debug_show_path(
	boolean show)
{
	widget_globals.debug_show_path = show;

	return;
}

int widget_instance_count_children(
	struct widget_instance *widget)
{
	int count;
	struct widget_instance *child;

	count = 0;
	if (widget)
	{
		for (child = widget->child; child; child = child->next)
			count++;
	}
	return count;
}

struct widget_instance *widget_instance_get_nth_child(
	struct widget_instance *widget,
	int n)
{
	int i;
	struct widget_instance *result;

	match_assert(
		"c:\\halo\\SOURCE\\interface\\ui_widget.c",
		1050,
		widget);
	result = widget->child;
	for (i = 0; i < n && result; i++)
		result = result->next;

	return result;
}

void widget_instance_set_visibility_recursive(
	struct widget_instance *widget,
	boolean visible)
{
	struct widget_instance *child;

	match_assert(
		"c:\\halo\\SOURCE\\interface\\ui_widget.c",
		1859,
		widget);
	widget->visible = visible;
	for (child = widget->child; child; child = child->next)
		widget_instance_set_visibility_recursive(child, visible);

	return;
}

void *ui_widget_realloc(
	void *pointer,
	word size,
	char const *file,
	unsigned long line)
{
	return pool_resize_pointer(
		widget_memory_pool,
		pointer,
		size,
		file,
		line);
}

void widget_free(
	void *ptr)
{
	ui_carousel_forget(ptr); /* port */
	dispose_pointer(widget_memory_pool, ptr);

	return;
}

void main_menu_active(
	boolean active)
{
	we_are_at_the_main_menu = active;

	return;
}

boolean main_menu_is_active(
	void)
{
	return we_are_at_the_main_menu;
}

void ui_widget_load_progress_widget(
	void)
{
	error(
		_error_silent,
		"the old loading progress screen has been replaced with glowy halo gravy");

	return;
}

boolean filesystem_check_thread_is_active(
	void)
{
	return widget_globals.initialization_thread != NULL;
}

void display_error_when_main_menu_loaded(
	short error_code)
{
	if (widget_globals.main_menu_deferred_error_code == NONE)
	{
		widget_globals.main_menu_deferred_error_code = error_code;
		return;
	}

	error(
		_error_silent,
		"there is already an error message queued for display at the main menu; ignoring this one");
	return;
}

/* port: an error of the port's own text (as display_error_text_deferred's)
shown when the main menu is next loaded, as display_error_when_main_menu_loaded
shows one of the game's: a network game left for a reason the game has no
error for (a map missing: cache_files.c). The text is copied, when no error
is queued already. */
void display_error_text_when_main_menu_loaded(
	wchar_t const *text)
{
	static wchar_t queued_text[512];

	if (widget_globals.main_menu_deferred_error_code != NONE)
	{
		error(_error_silent, "there is already an error message queued for display at the main menu; ignoring this one");
		return;
	}
	ustrncpy(queued_text, text, NUMBEROF(queued_text) - 1);
	queued_text[NUMBEROF(queued_text) - 1] = 0;
	ui_widget_port_error_pending_text = queued_text;
	widget_globals.main_menu_deferred_error_code = _error_cannot_create_saved_game_file_with_empty_name;

	return;
}

void display_error_abort_to_dashboard_deferred(
	short error_code,
	boolean optional)
{
	if (widget_globals.deferred_dashboard_error_code == NONE)
	{
		widget_globals.deferred_dashboard_error_code = error_code;
		widget_globals.deferred_dashboard_optional = optional;
		return;
	}

	error(
		_error_silent,
		"there is already a deferred dashbaord error queued; ignoring this one!");
	return;
}

boolean ui_main_menu_music_active(
	void)
{
	return widget_globals.main_menu_music_active;
}

void ui_widgets_disable_pause_game(
	long duration_ticks)
{
	match_assert(
		"c:\\halo\\SOURCE\\interface\\ui_widget.c",
		2519,
		duration_ticks>=0);
	widget_globals.pause_disabled_ticks = duration_ticks;

	return;
}

struct widget_instance *widget_instance_get_topmost_parent(
	struct widget_instance *widget)
{
	while (widget->parent)
		widget = widget->parent;

	return widget;
}

int widget_instance_get_child_index_from_parent(
	struct widget_instance *widget)
{
	int result = NONE;

	if (widget->parent)
	{
		struct widget_instance *child = widget->parent->child;
		int index = 0;

		while (child)
		{
			if (child == widget)
			{
				result = index;
				break;
			}
			child = child->next;
			index++;
		}
	}

	return result;
}

pixel32 modulate_pixel32_by_real_alpha(
	pixel32 argb,
	real alpha)
{
	real modulated_alpha = (argb >> 24) * alpha;

	return (fast_ftol(modulated_alpha) << 24) | (argb & 0x00FFFFFF);
}

void ui_set_next_level(
	short level)
{
	long level_index = level;

	if (level_index != NONE)
	{
		if (level_index >= 0 && level_index <= 9)
		{
			main_set_map_name(main_get_solo_level_name(level));
			main_disallow_persistent_storage();
		}
		else
		{
			error(_error_silent, "unknown level");
			main_goto_main_menu();
		}
	}
	else
	{
		main_roll_credits();
	}

	return;
}

boolean ui_widgets_active(
	void)
{
	boolean result = FALSE;

	if (widget_globals.initialized)
	{
		long local_player_index;

		for (local_player_index = 0;
			local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS;
			local_player_index++)
		{
			if (widget_globals.active_widgets[local_player_index])
			{
				result = TRUE;
				break;
			}
		}
	}

	return result;
}

boolean main_menu_screen_is_active(
	void)
{
	if (we_are_at_the_main_menu == TRUE &&
		widget_globals.active_widgets[0] &&
		strcmp(widget_globals.active_widgets[0]->name, "the_main_menu") == 0)
	{
		return TRUE;
	}

	return FALSE;
}

static void *pool_alloc(
	unsigned long size)
{
	return match_malloc("c:\\halo\\SOURCE\\interface\\ui_widget.c", 117, size);
}

static void pool_free(
	void *pointer)
{
	match_free("c:\\halo\\SOURCE\\interface\\ui_widget.c", 118, pointer);

	return;
}

void ui_widgets_initialize(
	void)
{
	boolean success = TRUE;
	byte *base_address;
	long local_player_index;

	base_address = pool_alloc(WIDGET_MEMORY_POOL_SIZE);
	if (base_address)
	{
		widget_memory_pool->base_address = base_address;
		widget_memory_pool->size = WIDGET_MEMORY_POOL_SIZE;
	}
	else
	{
		success = FALSE;
	}
	stack_memory_pool_reset(widget_memory_pool);

	memset(&widget_globals, 0, sizeof(widget_globals));
	widget_globals.main_menu_deferred_error_code = NONE;
	widget_globals.deferred_dashboard_error_code = NONE;
	for (local_player_index = 0;
		local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS;
		local_player_index++)
	{
		widget_globals.deferred_errors[local_player_index].error_code = NONE;
		widget_globals.deferred_cinematic_errors[local_player_index].error_code = NONE;
	}
	widget_globals.initialized = success;
	widget_globals.fade_to_black = -1.0f;

	return;
}

void ui_widgets_dispose(
	void)
{
	ui_widgets_close_all();
	controller_glyphs_dispose(); /* port */
	if (widget_memory_pool->base_address)
		pool_free(widget_memory_pool->base_address);
	widget_memory_pool->base_address = NULL;
	widget_memory_pool->size = 0;
	memset(&widget_globals, 0, sizeof(widget_globals));

	return;
}

static void pop_widget(
	struct widget_stack_node **top,
	struct widget_stack_data *data)
{
	struct widget_stack_node *node;

	match_assert(
		"c:\\halo\\SOURCE\\interface\\ui_widget.c",
		2556,
		top && data);
	node = *top;
	*data = node->data;
	*top = node->next;
	dispose_pointer(widget_memory_pool, node);

	return;
}

static void dispose_widget_stack(
	struct widget_stack_node **top)
{
	while (*top)
	{
		struct widget_stack_node *node = *top;

		*top = node->next;
		dispose_pointer(widget_memory_pool, node);
	}

	return;
}

/* port: the pointer's remembered targets (pointing at widgets), and what it
has pending, are dropped with a widget, so they cannot land on freed pool
memory the next screen now uses (ui_widget.c, "pointer") */
static void ui_mouse_forget_targets(
	void);

void ui_widget_delete(
	struct widget_instance *widget)
{
	struct ui_widget_definition *definition;
	long handler_index;
	long widget_index;

	ui_mouse_forget_targets();

	match_assert(
		"c:\\halo\\SOURCE\\interface\\ui_widget.c",
		469,
		widget && widget_globals.initialized);
	if (widget->delete_recursion_lock)
		return;
	widget->delete_recursion_lock = TRUE;
	if (widget->local_player_index != NONE && !widget->parent)
		player_control_inhibit_buttons(
			widget->local_player_index,
			WIDGET_DELETED_PLAYER_CONTROL_INHIBIT_FLAGS,
			TRUE);
	definition = ui_widget_definition_get(widget->definition_tag_index);
	for (handler_index = 0;
		handler_index < definition->event_handlers.count;
		handler_index++)
	{
		struct ui_widget_event_handler_reference *handler =
			(struct ui_widget_event_handler_reference *)definition->event_handlers.address + handler_index;

		if (handler->event_type == _widget_event_deleted &&
			TEST_FLAG(handler->flags, _event_handler_run_function_bit))
		{
			/* port: an event of nothing, not none: a map's widget may name a
			function for this handler that reads its event */
			static struct event_record no_event;
			boolean widget_deleted = FALSE;
			boolean handled;

			csmemset(&no_event, 0, sizeof(no_event));
			no_event.controller_index = NONE;
			handled = ui_widget_event_handler_function_invoke(
				widget,
				&no_event,
				handler->function,
				&widget_deleted);

			match_vassert(
				"c:\\halo\\SOURCE\\interface\\ui_widget.c",
				520,
				!widget_deleted,
				"a 'widget deleted' event handler tried to delete the widget being deleted!");
			if (handled == TRUE &&
				TEST_FLAG(handler->flags, _event_handler_open_widget_bit))
			{
				long new_widget_tag_index = handler->widget_tag.index;

				if (new_widget_tag_index != NONE &&
					!ui_widget_launch_widget(widget, new_widget_tag_index))
				{
					error(_error_silent, "event handler failed to spawn widget");
				}
			}
		}
	}
	if (widget->pause_game_time == TRUE)
	{
		match_vassert(
			"c:\\halo\\SOURCE\\interface\\ui_widget.c",
			544,
			widget_globals.pause_game_time_count > 0,
			"widget pause counter out of whack");
		if (--widget_globals.pause_game_time_count == 0)
		{
			if (game_time_get_paused())
			{
				game_time_set_paused(FALSE);
				if (we_are_at_the_main_menu)
				{
					main_menu_ensure_player_queues_exist();
					game_time_dispose_from_old_map();
					game_time_initialize_for_new_map();
					game_time_start();
				}
			}
			if (widget_globals.sound_paused == TRUE)
			{
				sound_pause(FALSE);
				widget_globals.sound_paused = FALSE;
			}
		}
	}
	ui_widget_delete_children_recursive(widget);
	if (widget->previous)
		widget->previous->next = widget->next;
	if (widget->next)
		widget->next->previous = widget->previous;
	if (widget->parent && widget->parent->child == widget)
		widget->parent->child = widget->next;
	switch (widget->type)
	{
	case _ui_widget_type_text_box:
		if (widget->parameters.text_box.text)
			dispose_pointer(widget_memory_pool, widget->parameters.text_box.text);
		if (widget == ui_widget_port_build_label)
			ui_widget_port_build_label = NULL;
		if (widget == ui_widget_port_error_text_box)
		{
			ui_widget_port_error_text_box = NULL;
			ui_widget_port_error_text = NULL;
		}
		break;
	case _ui_widget_type_spinner_list:
	case _ui_widget_type_column_list:
		if (widget->parameters.list.list_items)
		{
			error(
				_error_silent,
				"###WARNING: possible memory leak disposing of a list widget (%s)",
				definition->name ? definition->name : "<unknown>");
		}
		if (widget->parameters.list.item_text)
			dispose_pointer(widget_memory_pool, widget->parameters.list.item_text);
		if (widget->parameters.list.extended_description)
			ui_widget_delete(widget->parameters.list.extended_description);
		break;
	}
	dispose_pointer(widget_memory_pool, widget);
	for (widget_index = 0;
		widget_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS;
		widget_index++)
	{
		if (widget_globals.active_widgets[widget_index] == widget)
		{
			widget_globals.active_widgets[widget_index] = NULL;
			break;
		}
	}

	return;
}

static void ui_widget_delete_children_recursive(
	struct widget_instance *widget)
{
	struct widget_instance *child = widget->child;

	while (child)
	{
		struct widget_instance *next = child->next;

		match_assert(
			"c:\\halo\\SOURCE\\interface\\ui_widget.c",
			2750,
			child->previous == NULL);
		if (next)
		{
			match_assert(
				"c:\\halo\\SOURCE\\interface\\ui_widget.c",
				2754,
				next->previous == child);
		}
		ui_widget_delete(child);
		if (next)
			next->previous = NULL;
		child = next;
	}

	return;
}

static struct widget_instance *ui_widget_launch_widget(
	struct widget_instance *widget,
	long new_widget_tag_index)
{
	struct ui_widget_definition *definition = ui_widget_definition_get(new_widget_tag_index);
	struct widget_instance *root;
	struct widget_instance *new_widget;
	short local_player_index;

	/* port: the menus of multiplayer with other machines (not split screen's
	or co-op's) open only on maps of a build that plays multiplayer with the
	others (cache_files.c, cache_files_multiplayer_region); otherwise the
	player is told why, and the menu stays */
	{
		static char const multiplayer_menus[] = "ui\\shell\\main_menu\\multiplayer_type_select\\connected\\";
		char const *name = tag_get_name(new_widget_tag_index);
		char build[0x20];

		if (name &&
			!csstrncmp(name, multiplayer_menus, sizeof(multiplayer_menus) - 1) &&
			!cache_files_multiplayer_region(build))
		{
			cache_files_show_multiplayer_unavailable(NULL, build);

			return NULL;
		}
	}

	if (TEST_FLAG(definition->flags, _widget_always_use_tag_controller_index_bit))
	{
		switch (definition->controller_index)
		{
		case _widget_controller0:
			local_player_index = 0;
			break;
		case _widget_controller1:
			local_player_index = 1;
			break;
		case _widget_controller2:
			local_player_index = 2;
			break;
		case _widget_controller3:
			local_player_index = 3;
			break;
		case _widget_controller_any:
			local_player_index = NONE;
			break;
		default:
			match_vassert(
				"c:\\halo\\SOURCE\\interface\\ui_widget.c",
				5380,
				FALSE,
				"invalid widget controller index specified");
			break;
		}
	}
	else
	{
		switch (definition->controller_index)
		{
		case _widget_controller0:
			local_player_index = 0;
			break;
		case _widget_controller1:
			local_player_index = 1;
			break;
		case _widget_controller2:
			local_player_index = 2;
			break;
		case _widget_controller3:
			local_player_index = 3;
			break;
		case _widget_controller_any:
			local_player_index = widget->local_player_index;
			break;
		default:
			match_vassert(
				"c:\\halo\\SOURCE\\interface\\ui_widget.c",
				5392,
				FALSE,
				"invalid widget controller index specified");
			break;
		}
	}
	root = widget_instance_get_topmost_parent(widget);
	new_widget = ui_widget_load_by_name_or_tag(
		NULL,
		new_widget_tag_index,
		NULL,
		local_player_index,
		root->definition_tag_index,
		widget->parent ? widget->parent->definition_tag_index : NONE,
		(short)widget_instance_get_child_index_from_parent(widget));
	if (!new_widget)
		error(_error_silent, "event handler failed to spawn widget");

	return new_widget;
}

static __inline boolean widget_instance_can_handle_events(
	struct widget_instance *widget)
{
	struct ui_widget_definition *definition = ui_widget_definition_get(widget->definition_tag_index);

	if (!widget->disabled &&
		(definition->event_handlers.count > 0 ||
		widget->type == _ui_widget_type_spinner_list ||
		widget->type == _ui_widget_type_column_list))
	{
		return TRUE;
	}

	return FALSE;
}

/* port: a label in the PC version's lists (port/assets/menus): an item that
takes no events and has nothing in it that does, which its game passes over
(its rows of settings take none themselves, but their spinners do), or one
hidden (a list's rows past its items: port/linux/game/menu_functions.c) or
disabled (the server browser's column titles, which sort nothing) */
static boolean widget_instance_port_is_label(
	struct widget_instance *widget)
{
	return (!widget_instance_can_handle_events(widget) && !widget->child) || !widget->visible || widget->disabled;
}

static struct widget_instance *widget_instance_find_by_tag_index_recursive(
	struct widget_instance *widget,
	long tag_index)
{
	struct widget_instance *result = NULL;

	if (widget->definition_tag_index == tag_index)
	{
		result = widget;
	}
	else
	{
		struct widget_instance *child;

		for (child = widget->child; child && !result; child = child->next)
		{
			if (child->definition_tag_index == tag_index)
				result = child;
			else
				result = widget_instance_find_by_tag_index_recursive(child, tag_index);
		}
	}

	return result;
}

static void widget_instance_give_focus_directly(
	struct widget_instance *widget,
	struct widget_instance *new_focus)
{
	struct widget_instance *focused_child =
		widget_instance_get_topmost_parent(widget)->focused_child;

	if (new_focus->disabled == TRUE)
	{
		struct widget_instance *substitute;

		for (substitute = new_focus->next; substitute; substitute = substitute->next)
		{
			if (widget_instance_can_handle_events(substitute))
				break;
		}
		if (!substitute && new_focus->parent)
		{
			for (substitute = new_focus->parent->child; substitute; substitute = substitute->next)
			{
				if (widget_instance_can_handle_events(substitute))
					break;
			}
			if (substitute == new_focus->parent->focused_child)
			{
				for (substitute = new_focus->previous; substitute; substitute = substitute->previous)
				{
					if (widget_instance_can_handle_events(substitute))
						break;
				}
			}
		}
		if (substitute)
			new_focus = substitute;
	}
	if (focused_child)
	{
		if (new_focus &&
			focused_child->parent == new_focus->parent &&
			focused_child->parent)
		{
			new_focus->parent->focused_child = new_focus;

			return;
		}
		while (focused_child)
		{
			focused_child->parent->focused_child = NULL;
			focused_child = focused_child->focused_child;
		}
	}
	while (new_focus->parent)
	{
		new_focus->parent->focused_child = new_focus;
		new_focus = new_focus->parent;
	}

	return;
}

static void widget_instance_give_focus_by_tag(
	struct widget_instance *widget,
	long tag_index,
	short local_player_index)
{
	struct widget_instance *root = widget_instance_get_topmost_parent(widget);
	struct widget_instance *new_focus = widget_instance_find_by_tag_index_recursive(root, tag_index);

	if (new_focus)
		widget_instance_give_focus_directly(root, new_focus);
	else
		error(_error_silent, "failed to find event focus target widget");

	return;
}

/* port: a locked row still receives focus and help, but its edit presses must stop before the hidden spinner's
handlers run. Mouse clicks and taps arrive as A or left/right button events through ui_widgets_process_mouse. */
static boolean widget_instance_port_refuse_graphics_edit(
	struct widget_instance *widget,
	struct event_record *event)
{
	boolean edit = event->type == _event_type_button && event->data.button.value == 1 &&
		(event->data.button.index == _gamepad_analog_button_a ||
		 event->data.button.index == _widget_event_dpad_left || event->data.button.index == _widget_event_dpad_right);

	edit = edit || (event->type == _event_type_left_stick && event->data.stick.x != 0);
	if (!edit)
		return FALSE;
	/* a disabled row (grayed INTERNET or SPLIT SCREEN) is refused by A, whichever input made it (controller,
	keyboard, mouse click or touch tap) */
	if (!(event->type == _event_type_button && event->data.button.index == _gamepad_analog_button_a &&
		ui_widget_port_disabled_row_activation(widget, _gamepad_analog_button_a)) &&
		!ui_widget_port_graphics_locked(widget))
		return FALSE;
	ui_play_audio_feedback_sound(_ui_audio_feedback_flag_failure);
	return TRUE;
}

/* port: the native Xbox controller pages normally copy their spinners to
player_ui only on exit. Run that same copy after each input step, then save.
Generated settings have their own changed handlers in menu_functions.c. */
static void widget_profile_spinner_save(struct widget_instance *spinner)
{
	extern boolean pc_menu_profile_auto_save_enabled(void);
	extern boolean pc_menu_profile_save_on_back(void);
	struct widget_instance *list;

	if (spinner->type != _ui_widget_type_spinner_list || !main_menu_is_active() ||
		!pc_menu_profile_auto_save_enabled())
		return;
	for (list = spinner->parent; list; list = list->parent)
	{
		struct ui_widget_definition *definition = ui_widget_definition_get(list->definition_tag_index);
		struct ui_widget_event_handler_reference *handlers = definition->event_handlers.address;
		long index;

		for (index = 0; index < definition->event_handlers.count; index++)
		{
			/* Native controller and advanced-controller save functions. */
			if (TEST_FLAG(handlers[index].flags, _event_handler_run_function_bit) &&
				(handlers[index].function == 70 || handlers[index].function == 71))
			{
				boolean deleted = FALSE;

				if (ui_widget_event_handler_function_invoke(list, NULL, handlers[index].function, &deleted))
					pc_menu_profile_save_on_back();
				return;
			}
		}
	}
}

boolean widget_event_function_list_widget_goto_next_item(
	struct widget_instance *widget,
	struct event_record *event,
	boolean *widget_deleted)
{
	struct ui_widget_definition *definition;
	struct widget_instance *child;
	long item_index;
	boolean result = TRUE;

	match_assert(
		"c:\\halo\\SOURCE\\interface\\ui_widget.c",
		1230,
		widget);
	/* port: direct cycle handlers must obey the same lock as input dispatch. */
	if (ui_widget_port_graphics_locked(widget))
	{
		ui_play_audio_feedback_sound(_ui_audio_feedback_flag_failure);
		return TRUE;
	}
	if (ui_widget_is_carousel(widget))
		return ui_carousel_controller_move(widget, 1);
	definition = ui_widget_definition_get(widget->definition_tag_index);
	match_vassert(
		"c:\\halo\\SOURCE\\interface\\ui_widget.c",
		1233,
		widget->type == _ui_widget_type_spinner_list || widget->type == _ui_widget_type_column_list,
		"calling a list widget function on a non-list widget");
	if (widget->parameters.list.list_items && widget->parameters.list.number_of_items > 0)
	{
		item_index = widget->parameters.list.selected_index + 1;
		if (item_index >= widget->parameters.list.number_of_items)
			item_index = 0;
		if (widget->type == _ui_widget_type_column_list)
		{
			child = widget_instance_get_nth_child(widget, item_index);
			if (child)
			{
				widget_instance_give_focus_by_tag(
					widget,
					child->definition_tag_index,
					widget->local_player_index);
				widget->parameters.list.selected_index = (short)item_index;
			}
			else
			{
				error(
					_error_silent,
					"failed to set focus to the #%d list item of a column list widget",
					item_index);
				result = FALSE;
			}
		}
		else if (widget->type == _ui_widget_type_spinner_list)
		{
			if (definition->child_widgets.count > 1)
			{
				match_vassert(
					"c:\\halo\\SOURCE\\interface\\ui_widget.c",
					1268,
					definition->child_widgets.count == 3,
					"spinner lists must be either 1- or 3-wide... sorry");
				if (widget->focused_child == widget->child ||
					widget->focused_child == widget->child->next)
				{
					if (widget->focused_child->next)
						widget_instance_give_focus_directly(widget, widget->focused_child->next);
				}
			}
			widget->parameters.list.selected_index = (short)item_index;
		}
	}
	else
	{
		if (widget->type == _ui_widget_type_spinner_list)
		{
			match_vassert(
				"c:\\halo\\SOURCE\\interface\\ui_widget.c",
				1288,
				definition->child_widgets.count <= 1,
				"spinner lists with more that 1 visible item need to have code-generated lists associated with them... sorry.");
		}
		if (widget->type == _ui_widget_type_spinner_list &&
			TEST_FLAG(definition->list_flags, _list_items_generated_from_string_list_tag) &&
			definition->child_widgets.count == 0)
		{
			widget->parameters.list.selected_index++;
			if (widget->parameters.list.selected_index == widget->parameters.list.number_of_items)
				widget->parameters.list.selected_index = 0;
		}
		else
		{
			child = NULL;
			if (widget->focused_child)
			{
				item_index = widget->parameters.list.selected_index + 1;
				child = widget->focused_child->next;
				if (item_index == widget->parameters.list.number_of_items)
					child = NULL;
			}
			if (!child)
			{
				child = widget->child;
				item_index = 0;
			}
			/* port: the PC version's lists (port/assets/menus) pass over the
			children that take no events (their labels and lines), as its
			game does */
			if (child && widget->type == _ui_widget_type_column_list &&
				pc_menu_tag(widget->definition_tag_index))
			{
				long tries = 0;

				while (widget_instance_port_is_label(child) && tries++ < 256)
				{
					child = child->next;
					item_index++;
					if (!child)
					{
						child = widget->child;
						item_index = 0;
					}
				}
			}
			if (child)
			{
				widget_instance_give_focus_by_tag(
					widget,
					child->definition_tag_index,
					widget->local_player_index);
				widget->parameters.list.selected_index = (short)item_index;
			}
			else
			{
				error(
					_error_silent,
					"failed to set focus to the next list item of a column widget");
				result = FALSE;
			}
		}
	}
	if (result)
	{
		widget->parameters.list.last_list_tab_direction = 15;
		widget_profile_spinner_save(widget); /* port */
	}

	return result;
}

boolean widget_event_function_list_widget_goto_previous_item(
	struct widget_instance *widget,
	struct event_record *event,
	boolean *widget_deleted)
{
	struct ui_widget_definition *definition;
	struct widget_instance *child;
	long item_index;

	match_assert(
		"c:\\halo\\SOURCE\\interface\\ui_widget.c",
		1359,
		widget);
	/* port: direct cycle handlers must obey the same lock as input dispatch. */
	if (ui_widget_port_graphics_locked(widget))
	{
		ui_play_audio_feedback_sound(_ui_audio_feedback_flag_failure);
		return TRUE;
	}
	if (ui_widget_is_carousel(widget))
		return ui_carousel_controller_move(widget, -1);
	definition = ui_widget_definition_get(widget->definition_tag_index);
	match_vassert(
		"c:\\halo\\SOURCE\\interface\\ui_widget.c",
		1362,
		widget->type == _ui_widget_type_spinner_list || widget->type == _ui_widget_type_column_list,
		"calling a list widget function on a non-list widget");
	if (widget->parameters.list.list_items && widget->parameters.list.number_of_items > 0)
	{
		item_index = widget->parameters.list.selected_index - 1;
		if (item_index < 0)
			item_index = widget->parameters.list.number_of_items - 1;
		if (widget->type == _ui_widget_type_column_list)
		{
			child = widget_instance_get_nth_child(widget, item_index);
			if (child)
			{
				widget_instance_give_focus_by_tag(
					widget,
					child->definition_tag_index,
					widget->local_player_index);
				widget->parameters.list.selected_index = (short)item_index;
			}
			else
			{
				error(
					_error_silent,
					"failed to set focus to the #%d list item of a column list widget",
					item_index);

				return FALSE;
			}
		}
		else if (widget->type == _ui_widget_type_spinner_list)
		{
			if (definition->child_widgets.count > 1)
			{
				match_vassert(
					"c:\\halo\\SOURCE\\interface\\ui_widget.c",
					1396,
					definition->child_widgets.count == 3,
					"spinner lists must be either 1- or 3-wide... sorry");
				if (widget->focused_child != widget->child)
				{
					match_assert(
						"c:\\halo\\SOURCE\\interface\\ui_widget.c",
						1405,
						widget->focused_child);
					if (widget->focused_child->previous)
						widget_instance_give_focus_directly(widget, widget->focused_child->previous);
				}
			}
			widget->parameters.list.selected_index = (short)item_index;
		}
	}
	else
	{
		if (widget->type == _ui_widget_type_spinner_list)
		{
			match_vassert(
				"c:\\halo\\SOURCE\\interface\\ui_widget.c",
				1420,
				definition->child_widgets.count <= 1,
				"spinner lists with more that 1 visible item need to have code-generated lists associated with them... sorry.");
		}
		if (widget->type == _ui_widget_type_spinner_list &&
			TEST_FLAG(definition->list_flags, _list_items_generated_from_string_list_tag) &&
			definition->child_widgets.count == 0)
		{
			widget->parameters.list.selected_index--;
			if (widget->parameters.list.selected_index < 0)
				widget->parameters.list.selected_index = widget->parameters.list.number_of_items - 1;
		}
		else
		{
			child = NULL;
			if (widget->focused_child)
			{
				item_index = widget->parameters.list.selected_index - 1;
				child = widget->focused_child->previous;
			}
			if (!child)
			{
				child = widget->child;
				item_index = 0;
				while (child->next)
				{
					child = child->next;
					item_index++;
				}
			}
			/* port: as goto_next_item, over the PC version's labels */
			if (widget->type == _ui_widget_type_column_list && pc_menu_tag(widget->definition_tag_index))
			{
				long tries = 0;

				while (widget_instance_port_is_label(child) && tries++ < 256)
				{
					child = child->previous;
					item_index--;
					if (!child)
					{
						child = widget->child;
						item_index = 0;
						while (child->next)
						{
							child = child->next;
							item_index++;
						}
					}
				}
			}
			widget_instance_give_focus_by_tag(
				widget,
				child->definition_tag_index,
				widget->local_player_index);
			widget->parameters.list.selected_index = (short)item_index;
		}
	}
	widget->parameters.list.last_list_tab_direction = -15;
	widget_profile_spinner_save(widget); /* port */

	return TRUE;
}

void ui_widgets_close_all(
	void)
{
	long local_player_index;

	/* port: the virtual keyboard goes with the widgets (while the widget
	whose text it edits is still there): left open, it drew on after a game
	loaded, with the menu map's font, which the game's tags no longer have
	(a player typing when the host started the game) */
	if (virtual_keyboard_active())
		virtual_keyboard_close();
	for (local_player_index = 0;
		local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS;
		local_player_index++)
	{
		if (widget_globals.active_widgets[local_player_index])
			ui_widget_delete(widget_globals.active_widgets[local_player_index]);
		if (widget_globals.widget_stack[local_player_index])
			dispose_widget_stack(&widget_globals.widget_stack[local_player_index]);
	}

	return;
}

void ui_widgets_close_all_for_local_player(
	short local_player_index)
{
	long widget_index;

	match_vassert(
		"c:\\halo\\SOURCE\\interface\\ui_widget.c",
		1154,
		local_player_index>=0 && local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS,
		"expected a valid local_player_index");
	for (widget_index = 0;
		widget_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS;
		widget_index++)
	{
		struct widget_instance *widget = widget_globals.active_widgets[widget_index];

		if (widget && widget->local_player_index == local_player_index)
		{
			ui_widget_delete(widget);
			if (widget_globals.widget_stack[widget_index])
				dispose_widget_stack(&widget_globals.widget_stack[widget_index]);
		}
	}

	return;
}

void ui_widgets_delete_history(
	void)
{
	long local_player_index;

	for (local_player_index = 0;
		local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS;
		local_player_index++)
	{
		if (widget_globals.widget_stack[local_player_index])
			dispose_widget_stack(&widget_globals.widget_stack[local_player_index]);
	}

	return;
}

void ui_widgets_pop_stack(
	short local_player_index)
{
	struct widget_stack_data data;

	if (local_player_index == NONE)
	{
		local_player_index = 0;
	}
	else
	{
		match_assert(
			"c:\\halo\\SOURCE\\interface\\ui_widget.c",
			1204,
			(local_player_index>=0) && (local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS));
	}
	if (widget_globals.widget_stack[local_player_index])
		pop_widget(&widget_globals.widget_stack[local_player_index], &data);

	return;
}

void main_screen_shell_begin_fade(
	unsigned long fade_duration_milliseconds)
{
	long local_player_index;

	ui_stop_main_menu_music();
	for (local_player_index = 0;
		local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS;
		local_player_index++)
	{
		if (widget_globals.active_widgets[local_player_index] &&
			!widget_globals.active_widgets[local_player_index]->widget_is_error_dialog)
		{
			widget_globals.active_widgets[local_player_index]->auto_close_fade_time = fade_duration_milliseconds;
			widget_globals.active_widgets[local_player_index]->milliseconds_to_auto_close =
				widget_globals.current_system_milliseconds -
				widget_globals.active_widgets[local_player_index]->creation_time + 100;
			if (widget_globals.widget_stack[local_player_index])
				dispose_widget_stack(&widget_globals.widget_stack[local_player_index]);
		}
	}

	return;
}

static void play_sound_tag(
	long sound_tag_index)
{
	if (sound_tag_index != NONE)
		unspatialized_impulse_sound_new(sound_tag_index, 1.0f);

	return;
}

void ui_play_audio_feedback_sound(
	short audio_feedback)
{
	switch (audio_feedback)
	{
	case _ui_audio_feedback_cursor:
		play_sound_tag(tag_loaded(SOUND_DEFINITION_TAG, "sound\\sfx\\ui\\cursor"));
		break;
	case _ui_audio_feedback_forward:
		play_sound_tag(tag_loaded(SOUND_DEFINITION_TAG, "sound\\sfx\\ui\\forward"));
		break;
	case _ui_audio_feedback_back:
		play_sound_tag(tag_loaded(SOUND_DEFINITION_TAG, "sound\\sfx\\ui\\back"));
		break;
	case _ui_audio_feedback_flag_failure:
		play_sound_tag(tag_loaded(SOUND_DEFINITION_TAG, "sound\\sfx\\ui\\flag_failure"));
		break;
	}

	return;
}

static __inline struct widget_instance *widget_instance_get_tail_child_widget(
	struct widget_instance *widget)
{
	struct widget_instance *child = widget->child;

	if (child)
	{
		while (child->next)
			child = child->next;
	}

	return child;
}

static void ui_widget_add_child(
	struct widget_instance *parent,
	struct widget_instance *child)
{
	struct widget_instance *tail_child = NULL;
	struct widget_instance *sibling = parent->child;

	match_assert(
		"c:\\halo\\SOURCE\\interface\\ui_widget.c",
		2719,
		(child->previous == NULL) && (child->next == NULL));
	while (sibling)
	{
		tail_child = sibling;
		sibling = sibling->next;
	}
	if (tail_child)
	{
		match_assert(
			"c:\\halo\\SOURCE\\interface\\ui_widget.c",
			2729,
			tail_child->next == NULL);
		tail_child->next = child;
		child->previous = tail_child;
	}
	else
	{
		parent->child = child;
	}

	return;
}

static void push_widget(
	struct widget_stack_node **top,
	struct widget_stack_data *data)
{
	struct widget_stack_node *node = pool_new_pointer(
		widget_memory_pool,
		sizeof(struct widget_stack_node),
		"c:\\halo\\SOURCE\\interface\\ui_widget.c",
		2532);

	match_assert(
		"c:\\halo\\SOURCE\\interface\\ui_widget.c",
		2534,
		top && data);
	if (node)
	{
		node->data = *data;
		node->next = *top;
		*top = node;
	}
	else
	{
		match_vwarn(
			"c:\\halo\\SOURCE\\interface\\ui_widget.c",
			2544,
			FALSE,
			"out of memory! the UI screen history will be hosed.");
	}

	return;
}

static boolean widget_instance_can_receive_events(
	struct widget_instance *widget)
{
	if (widget->disabled)
		return FALSE;
	if (widget->parent)
	{
		/* NOTE January carries the parent's definition over from one ancestor to
		the next: the flags it tests belong to the ancestor BELOW the one whose
		type it tests, from the second iteration onward.  Reproduced, not
		repaired - the June 2011 rebuild of this function decompiles to the same
		carried-over definition pointer */
		struct ui_widget_definition *definition =
			ui_widget_definition_get(widget->parent->definition_tag_index);
		struct widget_instance *parent;
		boolean result = TRUE;

		for (parent = widget->parent; parent && result; parent = parent->parent)
		{
			struct ui_widget_definition *parent_definition =
				ui_widget_definition_get(parent->definition_tag_index);

			result = TEST_FLAG(definition->flags, _widget_pass_unhandled_events_to_children_bit) ||
				parent->type == _ui_widget_type_spinner_list ||
				parent->type == _ui_widget_type_column_list;
			definition = parent_definition;
		}

		return result;
	}

	return TRUE;
}

static void widget_instance_set_focused_child_by_index(
	long tag_index,
	struct widget_instance *widget,
	short child_index)
{
	struct widget_instance *parent;

	if (tag_index == NONE)
		return;
	parent = widget_instance_find_by_tag_index_recursive(widget, tag_index);
	if (!parent)
		return;
	if (child_index >= 0)
	{
		struct widget_instance *child;
		long index;

		for (child = parent->child, index = 0; child; child = child->next, index++)
		{
			if (child->type == _ui_widget_type_spinner_list &&
				ui_widget_definition_get(child->definition_tag_index)->child_widgets.count > 1)
			{
				return;
			}
			if (index == child_index)
			{
				widget_instance_give_focus_directly(widget, child);
				if (child->parent &&
					(child->parent->type == _ui_widget_type_spinner_list ||
					child->parent->type == _ui_widget_type_column_list))
				{
					child->parent->parameters.list.selected_index = (short)index;
				}

				return;
			}
		}
	}
	else
	{
		/* with no index the found widget takes the focus itself, unless it is a
		multiple-item spinner list, which owns the focus of its own items */
		if (widget_instance_can_receive_events(parent) &&
			!(parent->type == _ui_widget_type_spinner_list &&
			ui_widget_definition_get(parent->definition_tag_index)->child_widgets.count > 1))
		{
			widget_instance_give_focus_directly(widget, parent);
		}
	}

	return;
}

static void widget_instance_go_back_to_previous(
	struct widget_instance *widget)
{
	struct widget_stack_data data;
	short widget_stack = (widget->local_player_index == NONE) ? 0 : widget->local_player_index;
	short previous_local_player_index = NONE;
	long previous_widget_tag;

	if (widget_globals.widget_stack[widget_stack])
	{
		pop_widget(&widget_globals.widget_stack[widget_stack], &data);
		previous_local_player_index = data.local_player_index;
		previous_widget_tag = data.previous_widget_tag;
	}
	else
	{
		previous_widget_tag = NONE;
	}
	ui_widget_delete(widget_instance_get_topmost_parent(widget));
	if (previous_widget_tag != NONE)
	{
		struct widget_instance *new_widget = ui_widget_load_by_name_or_tag(
			NULL,
			previous_widget_tag,
			NULL,
			previous_local_player_index,
			NONE,
			NONE,
			NONE);

		if (new_widget)
		{
			widget_instance_set_focused_child_by_index(
				data.focused_child_parent_widget_tag,
				new_widget,
				data.focused_child_index);
		}
	}

	return;
}

static __inline struct widget_instance *widget_instance_find_by_tag_index(
	long tag_index)
{
	struct widget_instance *result = NULL;
	long widget_index;

	for (widget_index = 0;
		widget_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS && !result;
		widget_index++)
	{
		if (widget_globals.active_widgets[widget_index])
		{
			result = widget_instance_find_by_tag_index_recursive(
				widget_globals.active_widgets[widget_index],
				tag_index);
		}
	}

	return result;
}

static void event_handler_dispatch(
	struct widget_instance *widget,
	struct ui_widget_definition *definition,
	struct event_record *event,
	struct ui_widget_event_handler_reference *handler,
	boolean *calling_widget_deleted)
{
	boolean widget_deleted = FALSE;
	boolean success = TRUE;
	boolean function_failed = FALSE;
	long audio_feedback = _ui_audio_feedback_none;
	boolean close_widget_after = FALSE;
	boolean close_current = FALSE;
	boolean close_all = FALSE;

	/* port: a disabled row does nothing when chosen (the press is normally refused earlier, with the same sound) */
	if (ui_widget_port_disabled_row_activation(widget, handler->event_type))
	{
		ui_play_audio_feedback_sound(_ui_audio_feedback_flag_failure);
		return;
	}
	/* port: a bar action declared while_typing="pointer" (the password's X): its key is a letter while a field is
	being typed in, so only a tap or a click on the action runs it */
	if (ui_bar_key_is_letter(widget, event))
		return;
	if (TEST_FLAG(handler->flags, _event_handler_run_scenario_script_bit) &&
		handler->script[0])
	{
		if (!hs_evaluate_by_name(handler->script))
			error(_error_silent, "failed to run ui widget event script '%s'", handler->script);
	}
	if (TEST_FLAG(handler->flags, _event_handler_run_function_bit) &&
		!widget_deleted &&
		!ui_widget_event_handler_function_invoke(
			widget,
			event,
			handler->function,
			&widget_deleted))
	{
		error(_error_silent, "event handler function failed");
		function_failed = TRUE;
	}
	else
	{
		if (TEST_FLAG(handler->flags, _event_handler_give_focus_to_widget_bit) &&
			!widget_deleted)
		{
			if (handler->widget_tag.index != NONE)
			{
				widget_instance_give_focus_by_tag(
					widget,
					handler->widget_tag.index,
					widget->local_player_index);
				audio_feedback = _ui_audio_feedback_cursor;
			}
			else
			{
				error(
					_error_silent,
					"failed to give focus to a widget because event_handler->ui_widget_tag == NONE");
				success = FALSE;
			}
		}
		if (TEST_FLAG(handler->flags, _event_handler_reload_self_bit) &&
			!widget_deleted)
		{
			widget_instance_reload_recursive(widget);
		}
		if (TEST_FLAG(handler->flags, _event_handler_reload_widget_bit) &&
			!widget_deleted)
		{
			if (handler->widget_tag.index != NONE)
			{
				ui_widget_reload_by_tag(handler->widget_tag.index);
			}
			else
			{
				error(
					_error_silent,
					"failed to reload widget because event_handler->ui_widget_tag == NONE");
				success = FALSE;
			}
		}
		if (TEST_FLAG(handler->flags, _event_handler_close_current_widget_bit) &&
			!widget_deleted)
		{
			close_widget_after = TRUE;
		}
		if (TEST_FLAG(handler->flags, _event_handler_close_other_widget_bit) &&
			!widget_deleted &&
			handler->widget_tag.index != NONE)
		{
			struct widget_instance *other_widget =
				widget_instance_find_by_tag_index(handler->widget_tag.index);

			if (other_widget)
			{
				if (other_widget == widget)
					close_current = TRUE;
				else
					ui_widget_delete(other_widget);
			}
			else
			{
				error(
					_error_silent,
					"failed to close widget because event_handler->ui_widget_tag == NONE");
				success = FALSE;
			}
		}
		if (TEST_FLAG(handler->flags, _event_handler_close_all_widgets_bit) &&
			!widget_deleted)
		{
			close_all = TRUE;
		}
		/* port: not from a widget its function deleted (it went back:
		menu_functions.c's profile_save_changes), which the Xbox's opened
		from regardless */
		if (TEST_FLAG(handler->flags, _event_handler_open_widget_bit) &&
			!widget_deleted &&
			handler->widget_tag.index != NONE)
		{
			if (!ui_widget_launch_widget(widget, handler->widget_tag.index))
			{
				error(_error_silent, "event handler failed to spawn widget");
				success = FALSE;
			}
			else
			{
				if (audio_feedback == _ui_audio_feedback_none)
					audio_feedback = _ui_audio_feedback_forward;
				widget_deleted = TRUE;
			}
		}
		if (TEST_FLAG(handler->flags, _event_handler_replace_with_other_widget_bit) &&
			!widget_deleted &&
			handler->widget_tag.index != NONE)
		{
			struct widget_instance *new_widget = ui_widget_load_by_name_or_tag(
				NULL,
				handler->widget_tag.index,
				widget,
				widget->local_player_index,
				NONE,
				NONE,
				NONE);

			if (new_widget)
			{
				struct widget_instance *parent = widget->parent;
				struct widget_instance *next = widget->next;
				struct widget_instance *previous = widget->previous;
				long widget_index;

				if (new_widget->previous)
					new_widget->previous->next = NULL;
				new_widget->previous = NULL;
				new_widget->parent = NULL;
				new_widget->horizontal_offset += widget->horizontal_offset;
				new_widget->vertical_offset += widget->vertical_offset;
				if (parent)
				{
					new_widget->parent = parent;
					if (parent->child == widget)
						parent->child = new_widget;
					if (parent->focused_child == widget)
						parent->focused_child = new_widget;
				}
				if (next)
				{
					match_assert(
						"c:\\halo\\SOURCE\\interface\\ui_widget.c",
						3721,
						next->previous == widget);
					next->previous = new_widget;
				}
				new_widget->next = next;
				if (previous)
				{
					match_assert(
						"c:\\halo\\SOURCE\\interface\\ui_widget.c",
						3728,
						previous->next == widget);
					previous->next = new_widget;
				}
				new_widget->previous = previous;
				for (widget_index = 0;
					widget_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS;
					widget_index++)
				{
					if (widget_globals.active_widgets[widget_index] == new_widget)
					{
						widget_globals.active_widgets[widget_index] = NULL;
						break;
					}
				}
				if (audio_feedback == _ui_audio_feedback_none)
					audio_feedback = _ui_audio_feedback_forward;
				widget->previous = NULL;
				widget->next = NULL;
				widget->parent = NULL;
				close_current = TRUE;
			}
			else
			{
				error(
					_error_silent,
					"failed to open widget because the specified widget tag was not found");
				success = FALSE;
			}
		}
		if (TEST_FLAG(handler->flags, _event_handler_go_back_to_previous_widget_bit))
		{
			widget_instance_go_back_to_previous(widget);
			if (audio_feedback == _ui_audio_feedback_none)
				audio_feedback = _ui_audio_feedback_back;
			widget_deleted = TRUE;
		}
		if (handler->sound_effect.index != NONE)
			unspatialized_impulse_sound_new(handler->sound_effect.index, 1.0f);
		if (close_all)
		{
			long widget_index;

			for (widget_index = 0;
				widget_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS;
				widget_index++)
			{
				if (widget_globals.active_widgets[widget_index])
					ui_widget_delete(widget_globals.active_widgets[widget_index]);
				while (widget_globals.widget_stack[widget_index])
				{
					struct widget_stack_data data;

					pop_widget(&widget_globals.widget_stack[widget_index], &data);
				}
			}
			widget_deleted = TRUE;
		}
		else if (close_widget_after)
		{
			ui_widget_delete(widget_instance_get_topmost_parent(widget));
			widget_deleted = TRUE;
		}
		else if (close_current)
		{
			ui_widget_delete(widget);
			widget_deleted = TRUE;
		}
	}
	if (!success || function_failed)
	{
		if (TEST_FLAG(handler->flags, _event_handler_look_for_conditional_widget_on_failure_bit))
		{
			long conditional_index;

			for (conditional_index = 0;
				conditional_index < definition->conditional_widgets.count;
				conditional_index++)
			{
				struct ui_widget_conditional_reference *conditional =
					(struct ui_widget_conditional_reference *)definition->conditional_widgets.address +
					conditional_index;

				if (function_failed == TRUE &&
					TEST_FLAG(
						conditional->flags,
						_conditional_widget_load_if_event_handler_function_fails_bit))
				{
					if (!widget_deleted)
					{
						if (conditional->widget_tag.index != NONE)
						{
							if (!ui_widget_launch_widget(widget, conditional->widget_tag.index))
								error(_error_silent, "condition handler failed to spawn widget");
							else
								widget_deleted = TRUE;
						}
					}
					else
					{
						error(
							_error_silent,
							"couldn't load conditional widget because the calling widget was deleted");
					}
				}
			}
		}
	}
	ui_play_audio_feedback_sound(audio_feedback);
	*calling_widget_deleted = widget_deleted;

	return;
}

static boolean ui_widget_load_children_recursive(
	struct widget_instance *widget,
	struct ui_widget_definition *definition)
{
	boolean result = TRUE;
	long child_index;

	if (TEST_FLAG(definition->list_flags, _list_items_generated_from_string_list_tag))
	{
		struct string_list *string_list;
		long string_index;

		match_vassert(
			"c:\\halo\\SOURCE\\interface\\ui_widget.c",
			2592,
			widget->type == _ui_widget_type_spinner_list,
			"_list_items_generated_from_string_list_tag flag should only be set for 1-wide spinner list widgets");
		match_vassert(
			"c:\\halo\\SOURCE\\interface\\ui_widget.c",
			2594,
			definition->child_widgets.count == 0,
			"no child widget references are needed to define list items when generating a list from a string list tag");
		match_vassert(
			"c:\\halo\\SOURCE\\interface\\ui_widget.c",
			2596,
			definition->text_label_string_list.index != NONE,
			"_list_items_generated_from_string_list_tag flag was set but no string list tag was specified");
		string_list = unicode_string_list_definition_get(definition->text_label_string_list.index);
		widget_globals.dont_load_children_recursive = TRUE;
		for (string_index = 0;
			string_index < string_list->strings.count +
				spinner_string_list_extra_count(definition->text_label_string_list.index);
			string_index++)
		{
			struct widget_instance *child = ui_widget_load_by_name_or_tag(
				NULL,
				widget->definition_tag_index,
				widget,
				widget->local_player_index,
				NONE,
				NONE,
				NONE);

			if (!child)
			{
				result = FALSE;
				break;
			}
			ui_widget_add_child(widget, child);
			widget->parameters.list.number_of_items++;
		}
		widget_globals.dont_load_children_recursive = FALSE;
	}
	for (child_index = 0;
		child_index < definition->child_widgets.count;
		child_index++)
	{
		struct ui_widget_child_reference *reference =
			(struct ui_widget_child_reference *)definition->child_widgets.address + child_index;
		short controller_index = widget->local_player_index;

		if (TEST_FLAG(reference->flags, _child_widget_use_custom_controller_index_bit))
		{
			if (reference->custom_controller_index >= 0 &&
				reference->custom_controller_index < MAXIMUM_GAMEPADS)
			{
				controller_index = reference->custom_controller_index;
			}
			else
			{
				error(
					_error_silent,
					"invalid controller index specified for child widget (#%d)",
					reference->custom_controller_index);
			}
		}
		if (reference->widget_tag.index != NONE)
		{
			struct widget_instance *child = ui_widget_load_by_name_or_tag(
				NULL,
				reference->widget_tag.index,
				widget,
				controller_index,
				NONE,
				NONE,
				NONE);

			if (!child)
			{
				result = FALSE;
				break;
			}
			child->horizontal_offset = reference->horizontal_offset + widget->horizontal_offset;
			child->vertical_offset = reference->vertical_offset + widget->vertical_offset;
			ui_widget_add_child(widget, child);
		}
	}
	if (widget->type == _ui_widget_type_column_list &&
		definition->extended_description_widget.index != NONE)
	{
		widget->parameters.list.extended_description = ui_widget_load_by_name_or_tag(
			NULL,
			definition->extended_description_widget.index,
			widget,
			widget->local_player_index,
			NONE,
			NONE,
			NONE);
		if (widget->parameters.list.extended_description)
		{
			if (widget->parameters.list.extended_description->previous)
				widget->parameters.list.extended_description->previous->next = NULL;
			widget->parameters.list.extended_description->previous = NULL;
			widget->parameters.list.extended_description->parent = NULL;
		}
	}
	if (!TEST_FLAG(definition->flags, _widget_dont_focus_a_specific_child_bit))
	{
		boolean focus_a_child = FALSE;

		if (widget->type == _ui_widget_type_spinner_list ||
			widget->type == _ui_widget_type_column_list)
		{
			widget->parameters.list.selected_index = 0;
			widget->parameters.list.last_list_tab_direction = 0;
			focus_a_child = TRUE;
		}
		else if (TEST_FLAG(definition->flags, _widget_pass_unhandled_events_to_children_bit))
		{
			focus_a_child = TRUE;
		}
		if (focus_a_child)
		{
			struct widget_instance *child;
			/* port: the PC version's lists (port/assets/menus) start on their
			first child that takes events, past their labels */
			boolean skip_labels = widget->type == _ui_widget_type_column_list &&
				pc_menu_tag(widget->definition_tag_index);
			short index = 0;

			for (child = widget->child; child; child = child->next, index++)
			{
				if (((widget->type == _ui_widget_type_spinner_list ||
					widget->type == _ui_widget_type_column_list) &&
					(!skip_labels || !widget_instance_port_is_label(child))) ||
					widget_instance_can_handle_events(child))
				{
					widget->focused_child = child;
					if (skip_labels)
						widget->parameters.list.selected_index = index;
					break;
				}
			}
			if (!widget->focused_child && skip_labels)
				widget->focused_child = widget->child;
		}
	}

	return result;
}

static void widget_instance_initialize(
	struct widget_instance *widget,
	struct widget_instance *parent,
	struct ui_widget_definition *definition,
	long tag_index,
	short local_player_index,
	short widget_stack)
{
	long handler_index;

	memset(widget, 0, sizeof(struct widget_instance));
	if (TEST_FLAG(definition->list_flags, _list_items_generated_from_string_list_tag) &&
		parent &&
		tag_index == parent->definition_tag_index)
	{
		widget->type = _ui_widget_type_text_box;
	}
	widget->definition_tag_index = tag_index;
	widget->name = definition->name;
	widget->local_player_index = local_player_index;
	widget->type = definition->type;
	widget->visible = TRUE;
	widget->render_regardless_of_controller_index =
		TEST_FLAG(definition->flags, _widget_render_regardless_of_controller_index_bit);
	/* port: a network co-op game never pauses (it opens the campaign's pause
	screen, which would), and neither does the main menu: its child screens
	(settings and the like carry the in-game pause flag) keep the animated
	backdrop running */
	widget->pause_game_time = TEST_FLAG(definition->flags, _widget_pause_game_time_bit) &&
		!network_coop_active() && !we_are_at_the_main_menu;
	ui_carousel_forget(widget); /* port: a pool address can be reused in the same millisecond. */
	widget->creation_time = widget_globals.current_system_milliseconds;
	widget->milliseconds_to_auto_close = MAX(definition->milliseconds_to_auto_close, 0);
	widget->auto_close_fade_time = MAX(definition->auto_close_fade_time, 0);
	widget->alpha_modifier = 1.0f;
	widget->parent = parent;
	switch (widget->type)
	{
	case _ui_widget_type_text_box:
		widget->parameters.text_box.string_list_index = NONE;
		break;
	}
	if (definition->background_bitmap.index != NONE)
	{
		widget->animation.number_of_sprite_frames = TAG_BLOCK_GET_ELEMENT(
			&bitmap_group_get(definition->background_bitmap.index)->sequences,
			0,
			struct bitmap_group_sequence)->bitmap_count;
	}
	if (!widget_globals.dont_load_children_recursive)
	{
		if (!ui_widget_load_children_recursive(widget, definition))
			error(_error_silent, "failed to load widget children");
	}
	for (handler_index = 0;
		handler_index < definition->event_handlers.count;
		handler_index++)
	{
		struct ui_widget_event_handler_reference *handler =
			(struct ui_widget_event_handler_reference *)definition->event_handlers.address + handler_index;

		if (handler->event_type == _widget_event_created)
		{
			struct event_record event = {0};
			boolean widget_deleted;

			event.controller_index = widget->local_player_index;
			event_handler_dispatch(widget, definition, &event, handler, &widget_deleted);
		}
	}
	if (!widget->focused_child)
	{
		struct widget_instance *child;

		for (child = widget->child; child; child = child->next)
		{
			if (widget_instance_can_handle_events(child))
				widget_instance_give_focus_directly(widget, child);
		}
	}
	if (widget->pause_game_time == TRUE)
	{
		widget_globals.pause_game_time_count++;
		if (!game_time_get_paused())
			game_time_set_paused(TRUE);
		if (!widget_globals.sound_paused && !we_are_at_the_main_menu)
		{
			sound_pause(TRUE);
			widget_globals.sound_paused = TRUE;
		}
	}

	return;
}

/* port: the PC version's events that this engine never sends (its custom
activation), for the menus' functions (port/linux/game/menu_functions.c):
runs the widget's handlers for the event, else the first descendant's that
has any (depth first); FALSE if none has */
boolean ui_widget_port_dispatch_event(
	struct widget_instance *widget,
	short event_type,
	short controller_index,
	boolean *deleted)
{
	struct ui_widget_definition *definition = ui_widget_definition_get(widget->definition_tag_index);
	struct widget_instance *child;
	boolean found = FALSE;
	long handler_index;

	/* (deleted: a handler deleted the widget's screen, opening another or
	going back; the callers' widgets are gone with it) */
	*deleted = FALSE;
	if (ui_widget_port_disabled_row_activation(widget, event_type))
	{
		ui_play_audio_feedback_sound(_ui_audio_feedback_flag_failure);
		return TRUE;
	}

	for (handler_index = 0; handler_index < definition->event_handlers.count; handler_index++)
	{
		struct ui_widget_event_handler_reference *handler =
			(struct ui_widget_event_handler_reference *)definition->event_handlers.address + handler_index;

		if (handler->event_type == event_type)
		{
			struct event_record event = {0};
			boolean widget_deleted = FALSE;

			event.controller_index = controller_index;
			event_handler_dispatch(widget, definition, &event, handler, &widget_deleted);
			found = TRUE;
			if (widget_deleted)
			{
				*deleted = TRUE;
				return TRUE;
			}
		}
	}
	for (child = widget->child; child && !found; child = child->next)
	{
		found = ui_widget_port_dispatch_event(child, event_type, controller_index, deleted);
		if (*deleted)
			break;
	}
	return found;
}

/* port: the number of a list's focused item: among all its children, but
in the PC version's lists (port/assets/menus) among those that take events,
past their labels, as its game counts them; in a list of the map's that the
patch table changed, the number its description is shown by (the row's
place before the patch, or a patch row's own), NONE for a row without one */
short ui_widget_port_list_index(
	struct widget_instance *list_widget)
{
	boolean skip_labels = pc_menu_tag(list_widget->definition_tag_index);
	struct widget_instance *child;
	short index = 0;

	/* a nested list in focus (a button bar, which the mouse can focus) is not
	one of the list's items, so it has no description of its own: NONE leaves
	the description of the item before it up */
	if (list_widget->focused_child && list_widget->focused_child->type == _ui_widget_type_column_list)
		return NONE;

	if (pc_menu_patch_list_index(list_widget->definition_tag_index,
		list_widget->focused_child ? list_widget->focused_child->definition_tag_index : NONE, &index))
	{
		return index;
	}
	index = 0;

	for (child = list_widget->child; child && child != list_widget->focused_child; child = child->next)
	{
		if (!skip_labels || !widget_instance_port_is_label(child))
			index++;
	}
	return index;
}

/* port: back to the screen before, for the menus' functions */
void ui_widget_port_go_back(
	struct widget_instance *widget)
{
	widget_instance_go_back_to_previous(widget);
}

/* port: the delete confirmation must skip the editor of the deleted item without reloading it. */
void ui_widget_port_go_back_past_editor(
	struct widget_instance *widget)
{
	short stack = widget->local_player_index == NONE ? 0 : widget->local_player_index;
	struct widget_stack_data editor;

	if (widget_globals.widget_stack[stack])
		pop_widget(&widget_globals.widget_stack[stack], &editor);
	widget_instance_go_back_to_previous(widget);
}

/* port: the screen a local player's menus show, or NULL in play
(menu_functions.c's invite jump) */
struct widget_instance *ui_widget_port_active_screen(
	short local_player_index)
{
	return widget_globals.active_widgets[local_player_index];
}

struct widget_instance *ui_widget_load_by_name_or_tag(
	char const *name,
	long tag_index,
	struct widget_instance *parent,
	short local_player_index,
	long invoking_widget_tag,
	long focused_child_parent_widget_tag,
	short focused_child_index)
{
	struct ui_widget_definition *definition;
	struct widget_instance *widget = NULL;
	short widget_stack = (local_player_index == NONE) ? 0 : local_player_index;

	match_assert(
		"c:\\halo\\SOURCE\\interface\\ui_widget.c",
		377,
		widget_globals.initialized);
	match_assert(
		"c:\\halo\\SOURCE\\interface\\ui_widget.c",
		378,
		(name != NULL) || (tag_index != NONE));
	match_assert(
		"c:\\halo\\SOURCE\\interface\\ui_widget.c",
		379,
		(widget_stack>=0) && (widget_stack<MAXIMUM_GAMEPADS));
	if (tag_index == NONE)
		tag_index = tag_loaded(UI_WIDGET_DEFINITION_TAG, name);
	/* port: map screen opens and embedded save warnings (menu_tags.c). */
	if (tag_index != NONE || name)
	{
		extern long pc_menu_screen_open(long tag_index, char const *name, boolean root);
		long screen_tag = pc_menu_screen_open(tag_index, name, parent == NULL);
		if (screen_tag != tag_index)
		{
			focused_child_parent_widget_tag = NONE;
			focused_child_index = NONE;
		}
		tag_index = screen_tag;
	}
	if (tag_index != NONE)
	{
		definition = ui_widget_definition_get(tag_index);
		widget = pool_new_pointer(
			widget_memory_pool,
			sizeof(struct widget_instance),
			"c:\\halo\\SOURCE\\interface\\ui_widget.c",
			395);
		if (widget)
		{
			if (!parent)
			{
				short previous_local_player_index;

				if (widget_globals.active_widgets[widget_stack])
				{
					previous_local_player_index =
						widget_globals.active_widgets[widget_stack]->local_player_index;
					ui_widget_delete(widget_globals.active_widgets[widget_stack]);
				}
				else
				{
					previous_local_player_index = NONE;
				}
				widget_globals.active_widgets[widget_stack] = widget;
				if (invoking_widget_tag != NONE &&
					!TEST_FLAG(
						ui_widget_definition_get(invoking_widget_tag)->flags,
						_widget_dont_push_history_data_bit))
				{
					struct widget_stack_data data;

					data.previous_widget_tag = invoking_widget_tag;
					data.focused_child_parent_widget_tag = focused_child_parent_widget_tag;
					data.focused_child_index = focused_child_index;
					data.local_player_index = previous_local_player_index;
					push_widget(&widget_globals.widget_stack[widget_stack], &data);
				}
			}
			if (local_player_index == NONE)
			{
				switch (definition->controller_index)
				{
				case _widget_controller0:
					local_player_index = 0;
					break;
				case _widget_controller1:
					local_player_index = 1;
					break;
				case _widget_controller2:
					local_player_index = 2;
					break;
				case _widget_controller3:
					local_player_index = 3;
					break;
				case _widget_controller_any:
					local_player_index = NONE;
					break;
				}
			}
			widget_instance_initialize(
				widget,
				parent,
				definition,
				tag_index,
				local_player_index,
				widget_stack);
		}
		else
		{
			error(_error_silent, "failed to create new widget; out of memory!");
		}
	}
	else
	{
		error(_error_silent, "ui_widget_definition tag '%s'/%d not loaded", name, NONE);
	}

	return widget;
}

static void render_state_text(
	rectangle2d *bounds,
	rectangle2d *cursor_bounds,
	wchar_t const *text)
{
	rectangle2d text_bounds;
	short initial_indent = cursor_bounds->x0 - bounds->x0;

	if (initial_indent < 0)
		error(_error_silent, "initial_indent<0 in render_state_text() and was about to explode");
	initial_indent = MAX(0, initial_indent);
	draw_string_set_indents(initial_indent, 0);
	draw_unicode_string_compute_bounds(bounds, text, &text_bounds, cursor_bounds);
	cursor_bounds->x0 -= 3;
	text_bounds.x0 = bounds->x0;
	rasterizer_draw_unicode_string(&text_bounds, NULL, NULL, 0, text);
	bounds->y0 = cursor_bounds->y0;

	return;
}

static void render_state_bitmap(
	rectangle2d *bounds,
	rectangle2d *cursor_bounds,
	pixel32 color,
	struct icon_hud_element_definition *icon,
	short physical, short semantic, pixel32 glyph_color)
{
	struct game_globals *game_globals;
	struct game_globals_interface_tag_references *interface_tag_references;
	long bitmap_group_index;
	long frame_index;
	struct bitmap_data const *bitmap;
	real_rectangle2d const *clip;
	real scale;
	point2d point;

	global_scenario_get();
	game_globals = scenario_get_game_globals();
	interface_tag_references = game_globals->interface_tag_references.count
		? TAG_BLOCK_GET_ELEMENT(
			&game_globals->interface_tag_references,
			0,
			struct game_globals_interface_tag_references)
		: NULL;
	bitmap_group_index = interface_tag_references->interface_tag_references[_interface_bitmap_iface_map2].index;
	frame_index = 0;
	bitmap = NULL;
	clip = NULL;
	if (icon->frame_rate)
		frame_index = system_milliseconds() * 30 / 1000 / icon->frame_rate;
	hud_retrieve_bitmap_and_bounding_rect(
		bitmap_group_index,
		icon->sequence_index,
		frame_index,
		&bitmap,
		&clip);
	if (bitmap && _texture_cache_bitmap_get_hardware_format(
		(struct bitmap_data *)bitmap, FALSE, TRUE))
	{
		scale = hud_globals_get_scale(local_player_count() > 1);
		point.x = (short)(icon->offset.x * scale + cursor_bounds->x0 + 1.0f);
		point.y = (short)(cursor_bounds->y1 - icon->offset.y * scale - 2.0f);
		rectangle2d glyph_bounds;
		glyph_bounds.x0 = point.x;
		glyph_bounds.x1 = point.x + (short)fast_ftol((clip ? clip->x1 - clip->x0 : 1.0f) * bitmap->width * scale);
		glyph_bounds.y1 = point.y;
		glyph_bounds.y0 = point.y + (short)fast_ftol(-(clip ? clip->y1 - clip->y0 : 1.0f) * bitmap->height * scale);
		if (!controller_glyph_draw(local_player_index_for_draw_string_and_hack_in_icons,
			semantic, physical, &glyph_bounds, NULL, glyph_color, _controller_glyph_blend_hud))
			hud_draw_bitmap_direct(
			bitmap,
			_hud_anchor_bottom_left,
			&point,
			clip,
			scale,
			0.0f,
			TEST_FLAG(icon->flags, _hud_icon_use_color_bit) ? icon->color : color,
			FALSE);
		if (TEST_FLAG(icon->flags, _hud_icon_absolute_width_bit))
			cursor_bounds->x0 = icon->width_offset + point.x;
		else if (clip)
			cursor_bounds->x0 = (short)((clip->x1 - clip->x0) * bitmap->width + icon->width_offset + point.x);
		else
			cursor_bounds->x0 = bitmap->width + icon->width_offset + point.x;
	}

	return;
}

void draw_string_and_hack_in_icons(
	rectangle2d *bounds,
	rectangle2d *clip,
	point2d *cursor_reference,
	short height_adjust,
	wchar_t const *instring,
	boolean ignore_icon_color)
{
	wchar_t *current = string_data;
	rectangle2d cursor_bounds = *bounds;
	unsigned long length;

	/* port: no more than the buffer holds (the text is the map's: a string
	list's or a hud message's; retail strings are up to 398 characters of
	1024). A longer one is cut, said once. */
	for (length = 0; length < NUMBEROF(string_data) - 1 && instring[length]; length++)
		string_data[length] = instring[length];
	string_data[length] = 0;
	if (instring[length])
	{
		static boolean long_string_reported = FALSE;

		if (!long_string_reported)
		{
			long_string_reported = TRUE;
			error(
				_error_silent,
				"string of more than %d characters cut",
				(long)NUMBEROF(string_data) - 1);
		}
	}
	while (current)
	{
		wchar_t *icon_spec = wcschr(current, L'%');
		short icon_type;

		if (!icon_spec)
			break;
		*icon_spec = 0;
		icon_spec++;
		render_state_text(bounds, &cursor_bounds, current);
		current = icon_spec;
		icon_type = get_icon_type(icon_spec);
		if (icon_type == NONE)
		{
			render_state_text(bounds, &cursor_bounds, L"%");
		}
		else
		{
			short remapped_icon_type;
			short icon_index;

			current = icon_spec + wcslen(icon_names[icon_type]);
			remapped_icon_type = remap_sticks_for_local_player(
				icon_type,
				local_player_index_for_draw_string_and_hack_in_icons);
			icon_index = NONE;
			if (remapped_icon_type > _icon_right_stick)
			{
				if (remapped_icon_type <= _icon_look)
				{
					if (remapped_icon_type <= _icon_accept)
					{
						struct game_input_preferences preferences;

						input_abstraction_get_local_player_preferences(
							local_player_index_for_draw_string_and_hack_in_icons,
							&preferences);
						icon_index = preferences.game_control_to_xbox_buttons[
							button_mappings[remapped_icon_type - _icon_action]];
					}
					else
					{
						icon_index = button_mappings[remapped_icon_type - _icon_action];
						switch (remapped_icon_type)
						{
						case _icon_accept:
							icon_index = _icon_start_button;
							break;
						case _icon_back:
							icon_index = _icon_back_button;
							break;
						case _icon_move:
							icon_index = _icon_left_stick;
							break;
						case _icon_look:
							icon_index = _icon_right_stick;
							break;
						}
					}
				}
				else
				{
					match_assert(
						"c:\\halo\\SOURCE\\interface\\ui_widget.c",
						4341,
						FALSE);
				}
			}
			else
			{
				icon_index = remapped_icon_type;
			}

			{
				struct icon_hud_element_definition *icon = TAG_BLOCK_GET_ELEMENT(
					&hud_globals->messaging.button_icons,
					icon_index,
					struct icon_hud_element_definition);
				byte saved_flags = icon->flags;
				short saved_width_offset = icon->width_offset;
				real_argb_color icon_color;
				real_argb_color text_color;
				long alpha;

				pixel32_to_real_argb_color(icon->color, &icon_color);
				icon->flags &= ~FLAG(_hud_icon_use_color_bit);
				match_assert(
					"c:\\halo\\SOURCE\\interface\\ui_widget.c",
					4362,
					icon_index>=0 && icon_index<NUM_ICONS);
				if (icon_is_special[icon_index])
				{
					icon->flags &= ~FLAG(_hud_icon_absolute_width_bit);
					icon->width_offset = -5;
				}
				draw_string_get_color(&text_color);
				alpha = (long)(text_color.alpha * 255.0f) << 24;
				if (icon->color == 0 || ignore_icon_color)
					icon_color = text_color;
				icon_color.red *= text_color.alpha;
				icon_color.green *= text_color.alpha;
				icon_color.blue *= text_color.alpha;
				render_state_bitmap(
					bounds,
					&cursor_bounds,
					(real_argb_color_to_pixel32(&icon_color) & 0x00ffffff) | alpha,
					icon, icon_index, icon_type, real_argb_color_to_pixel32(&text_color));
				bounds->x0++;
				icon->flags = saved_flags;
				icon->width_offset = saved_width_offset;
			}
		}
	}
	if (current)
		render_state_text(bounds, &cursor_bounds, current);
	draw_string_set_indents(0, 0);

	return;
}

void ui_start_main_menu_music(
	void)
{
	if (!widget_globals.main_menu_music_active && !main_menu_fade_active())
	{
		long sound_definition_index = tag_loaded(LOOPING_SOUND_DEFINITION_TAG, "sound\\music\\title1\\title1");

		if (sound_definition_index != NONE)
		{
			error(_error_silent, "starting main menu music");
			scripted_looping_sound_start(sound_definition_index, NONE, 1.0f);
			widget_globals.main_menu_music_active = TRUE;
		}
		else
		{
			error(_error_silent, "title music tag not found");
		}
	}

	return;
}

void ui_stop_main_menu_music(
	void)
{
	if (widget_globals.main_menu_music_active == TRUE)
	{
		long sound_definition_index = tag_loaded(LOOPING_SOUND_DEFINITION_TAG, "sound\\music\\title1\\title1");

		if (sound_definition_index != NONE)
		{
			error(_error_silent, "stopping main menu music");
			scripted_looping_sound_stop(sound_definition_index);
		}
		else
		{
			error(_error_silent, "title music tag not found");
		}
		widget_globals.main_menu_music_active = FALSE;
	}

	return;
}

void display_error_deferred(
	short error_code,
	short local_player_index,
	boolean modal,
	boolean pause_game_time)
{
	long index;

	if (local_player_index == NONE)
	{
		index = 0;
	}
	else
	{
		index = local_player_index;
		match_assert(
			"c:\\halo\\SOURCE\\interface\\ui_widget.c",
			2288,
			(index>=0) && (index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS));
	}
	if (widget_globals.deferred_errors[index].error_code == NONE)
	{
		widget_globals.deferred_errors[index].error_code = error_code;
		widget_globals.deferred_errors[index].local_player_index = local_player_index;
		widget_globals.deferred_errors[index].modal = modal;
		widget_globals.deferred_errors[index].pause_game_time = pause_game_time;
	}
	else
	{
		error(
			_error_silent,
			"there is already a deferred error message for local player %d; ignoring this one",
			index);
	}

	return;
}

/* port: an error message of the port's own text (the maps have only the
Xbox's), in the dialog of an error whose text it takes the place of. The text is
copied: the dialog reads it on every frame it draws, long after a caller's
formatted buffer is gone */
void display_error_text_deferred(
	wchar_t const *text,
	short local_player_index)
{
	static wchar_t deferred_text[512];
	short index = local_player_index == NONE ? 0 : local_player_index;

	if (!VALID_INDEX(index, MAXIMUM_NUMBER_OF_LOCAL_PLAYERS) || widget_globals.deferred_errors[index].error_code != NONE)
		return;
	ustrncpy(deferred_text, text, NUMBEROF(deferred_text) - 1);
	deferred_text[NUMBEROF(deferred_text) - 1] = 0;
	ui_widget_port_error_pending_text = deferred_text;
	display_error_deferred(_error_cannot_create_saved_game_file_with_empty_name, local_player_index, TRUE, FALSE);

	return;
}

void display_errors_deferred_until_cinematic_stop(
	void)
{
	short local_player_index;

	match_vassert(
		"c:\\halo\\SOURCE\\interface\\ui_widget.c",
		2367,
		!cinematic_in_progress(),
		"Noooooooooooooooooo!!!");
	for (local_player_index = 0;
		local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS;
		local_player_index++)
	{
		struct ui_widget_deferred_cinematic_error *deferred_error =
			&widget_globals.deferred_cinematic_errors[local_player_index];

		if (deferred_error->error_code >= 0 && deferred_error->error_code < NUMBER_OF_ERROR_CODES)
		{
			display_error(
				deferred_error->error_code,
				local_player_index,
				deferred_error->modal,
				deferred_error->pause_game_time);
		}
		deferred_error->error_code = NONE;
	}

	return;
}

/* port: whether an open menu holds this local player's input: a menu opened
for them, or one that belongs to no player (a dialog). Another player's pause
menu in split screen does not stop them. */
boolean ui_widgets_hold_player_input(
	short local_player_index)
{
	boolean result = FALSE;

	if (widget_globals.initialized)
	{
		long widget_index;

		for (widget_index = 0; widget_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; widget_index++)
		{
			struct widget_instance *widget = widget_globals.active_widgets[widget_index];

			if (widget && (widget->local_player_index == NONE || widget->local_player_index == local_player_index))
			{
				result = TRUE;
				break;
			}
		}
	}

	return result;
}

boolean ui_widgets_active_for_local_player(
	short local_player_index)
{
	boolean result = FALSE;

	match_vassert(
		"c:\\halo\\SOURCE\\interface\\ui_widget.c",
		1110,
		local_player_index>=0 && local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS,
		"expected a valid local_player_index");
	if (widget_globals.initialized)
	{
		long widget_index;

		for (widget_index = 0;
			widget_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS;
			widget_index++)
		{
			if (widget_globals.active_widgets[widget_index] &&
				widget_globals.active_widgets[widget_index]->local_player_index == local_player_index)
			{
				result = TRUE;
				break;
			}
		}
	}

	return result;
}

void display_error(
	short error_code,
	short local_player_index,
	boolean modal,
	boolean pause_game_time)
{
	if (cinematic_in_progress())
	{
		{ extern void platform_log(char const *, ...); platform_log("menus: display_error %d deferred for a cinematic", (int)error_code); }
		if (local_player_index == NONE)
			local_player_index = 0;
		match_assert(
			"c:\\halo\\SOURCE\\interface\\ui_widget.c",
			2077,
			local_player_index>=0 && local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
		if (widget_globals.deferred_cinematic_errors[local_player_index].error_code == NONE)
		{
			widget_globals.deferred_cinematic_errors[local_player_index].error_code = error_code;
			widget_globals.deferred_cinematic_errors[local_player_index].modal = modal;
			widget_globals.deferred_cinematic_errors[local_player_index].pause_game_time = pause_game_time;
		}
		else
		{
			error(_error_silent, "there is already a deferred-for-cinematic error queued for player #%d; ignoring this one", local_player_index);
		}
	}
	else
	{
		char const *widget_name = NULL;
		short error_local_player_index;
		short local_player_count;
		short widget_stack;
		boolean first_local_player = TRUE;
		struct widget_instance *top_widget;
		long top_widget_tag_index;
		struct widget_instance *widget;
		/* (port: the port's own text for this error, taken whether or not its
		dialog opens, so that it is never another's) */
		wchar_t const *port_text = NULL;

		if (error_code == _error_cannot_create_saved_game_file_with_empty_name)
		{
			port_text = ui_widget_port_error_pending_text;
			ui_widget_port_error_pending_text = NULL;
		}
		if (local_player_index != NONE)
		{
			short index;

			local_player_count = 0;
			error_local_player_index = NONE;
			for (index = local_player_get_next(NONE); index != NONE; index = local_player_get_next(index))
			{
				if (index == local_player_index)
				{
					error_local_player_index = local_player_index;
					if (local_player_count > 0)
						first_local_player = FALSE;
				}
				local_player_count++;
			}
		}
		else
		{
			error_local_player_index = NONE;
			local_player_count = 0;
		}
		if (error_local_player_index == NONE && !we_are_at_the_main_menu)
			local_player_index = NONE;
		switch (local_player_count)
		{
		case 0:
		case 1:
			widget_name = modal
				? "ui\\shell\\error\\error_modal_fullscreen"
				: "ui\\shell\\error\\error_nonmodal_fullscreen";
			break;
		case 2:
			widget_name = modal
				? "ui\\shell\\error\\error_modal_halfscreen"
				: "ui\\shell\\error\\error_nonmodal_halfscreen";
			break;
		case 3:
			if (first_local_player == TRUE)
			{
				widget_name = modal
					? "ui\\shell\\error\\error_modal_halfscreen"
					: "ui\\shell\\error\\error_nonmodal_halfscreen";
			}
			else
			{
				widget_name = modal
					? "ui\\shell\\error\\error_modal_qtrscreen"
					: "ui\\shell\\error\\error_nonmodal_qtrscreen";
			}
			break;
		case 4:
			widget_name = modal
				? "ui\\shell\\error\\error_modal_qtrscreen"
				: "ui\\shell\\error\\error_nonmodal_qtrscreen";
			break;
		default:
			match_vassert(
				"c:\\halo\\SOURCE\\interface\\ui_widget.c",
				2161,
				FALSE,
				"invalid local player count");
			break;
		}
		/* port: force a pane only during the development screenshot call. */
		{
			extern char const *pc_menu_debug_error_variant(char const *name, boolean modal);
			widget_name = pc_menu_debug_error_variant(widget_name, modal);
		}
		if (widget_name)
		{
			widget_stack = local_player_index == NONE ? 0 : local_player_index;
			match_assert(
				"c:\\halo\\SOURCE\\interface\\ui_widget.c",
				2168,
				(widget_stack>=0) && (widget_stack<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS));
			if (transition_to_game_in_progress())
			{
				error(_error_silent, "aborting to the main menu root, for safety's sake");
				main_screen_shell_load();
				main_defer_map_map_change();
				ui_widgets_set_fade_value(-1.0f);
			}
			top_widget = widget_globals.active_widgets[widget_stack];
			top_widget_tag_index = top_widget ? top_widget->definition_tag_index : NONE;
			if (top_widget && top_widget->widget_is_error_dialog == TRUE)
			{
				error(_error_silent, "there is already an error message displayed for this local player index");
				widget = NULL;
			}
			else
			{
				widget = ui_widget_load_by_name_or_tag(
					widget_name,
					NONE,
					NULL,
					local_player_index,
					top_widget_tag_index,
					NONE,
					NONE);
			}
			if (widget)
			{
				struct widget_instance *text_box;

				match_vassert(
					"c:\\halo\\SOURCE\\interface\\ui_widget.c",
					2206,
					widget->child && widget->child->child,
					"error screen widget tag not layed out as expected");
				text_box = widget->child->child;
				match_vassert(
					"c:\\halo\\SOURCE\\interface\\ui_widget.c",
					2208,
					text_box->type == _ui_widget_type_text_box,
					"expected a text box widget in the error widget");
				text_box->parameters.text_box.string_list_index = PIN(error_code, 0, NUMBER_OF_ERROR_CODES - 1);
				/* (port: the port's own text in place of the error's) */
				ui_widget_port_error_text_box = text_box;
				ui_widget_port_error_text = port_text;
				widget->widget_is_error_dialog = TRUE;
				if (!widget->pause_game_time)
				{
					widget->pause_game_time = pause_game_time;
					if (widget->pause_game_time == TRUE)
					{
						match_vassert(
							"c:\\halo\\SOURCE\\interface\\ui_widget.c",
							2217,
							widget_globals.pause_game_time_count>=0,
							"widget pause counter is out of whack");
						widget_globals.pause_game_time_count++;
						if (!game_time_get_paused())
							game_time_set_paused(TRUE);
						if (!widget_globals.sound_paused && !we_are_at_the_main_menu)
						{
							sound_pause(TRUE);
							widget_globals.sound_paused = TRUE;
						}
					}
				}
				switch (error_code)
				{
				case _error_controller_unplugged_start_to_continue:
					widget->milliseconds_to_auto_close = 0;
					widget->auto_close_fade_time = 0;
					break;
				case _error_controller_unplugged:
					widget->close_if_local_player_controller_present = TRUE;
					widget->milliseconds_to_auto_close = 0;
					widget->auto_close_fade_time = 0;
					break;
				default:
					widget->close_if_local_player_controller_present = FALSE;
					break;
				}
			}
			else
			{
				{ extern void platform_log(char const *, ...); platform_log("menus: display_error %d failed to open %s", (int)error_code, widget_name); }
				error(_error_silent, "failed to display error message");
			}
		}
	}

	return;
}

void display_error_abort_to_dashboard(
	short error_code,
	boolean optional)
{
	char const *widget_name;
	struct widget_instance *widget;

	if (optional == TRUE)
		widget_name = "ui\\shell\\error\\error_abort_to_dashboard";
	else
		widget_name = "ui\\shell\\error\\error_abort_to_dashboard_you_have_no_choice";
	if (!optional)
		ui_widgets_close_all();
	widget = ui_widget_load_by_name_or_tag(widget_name, NONE, NULL, NONE, NONE, NONE, NONE);
	if (widget)
	{
		match_vassert(
			"c:\\halo\\SOURCE\\interface\\ui_widget.c",
			2319,
			widget->type == _ui_widget_type_text_box,
			"expected a text box widget");
		widget->parameters.text_box.string_list_index = error_code;
		/* port: our text-box root holds the index; its visible text follows
		the backdrop in draw order (tools/xbox_menus.py: dialog_document). */
		if (widget->child && widget->child->child && widget->child->child->type == _ui_widget_type_text_box)
		{
			widget->child->child->parameters.text_box.string_list_index = error_code;
			/* the same wrap to the dialog as every other error text (the game's own string) */
			ui_widget_port_error_text_box = widget->child->child;
			ui_widget_port_error_text = NULL;
		}
		widget->widget_is_error_dialog = TRUE;
		dashboard_abort_error = error_code;
	}
	else
	{
		error(_error_silent, "failed to load '%s' widget", widget_name);
	}

	return;
}

void display_error_damaged_media(
	void)
{
	display_error_abort_to_dashboard(_error_media_damaged, FALSE);
	input_frame_end();
	main_loop_of_death();

	return;
}

void network_game_reset_to_pregame_ui(
	void)
{
	ui_widgets_close_all();
	if (network_game_is_splitscreen_local())
	{
		if (network_game_is_quickstart_local())
		{
			if (!ui_widget_load_by_name_or_tag(
				"ui\\shell\\main_menu\\multiplayer_type_select\\split_screen\\pregame\\splitscreen_pregame_wrapper_normal",
				NONE, NULL, NONE, NONE, NONE, NONE))
			{
				error(_error_silent, "failed to load pregame screen after quickstart match");
			}
		}
		else
		{
			if (!ui_widget_load_by_name_or_tag(
				"ui\\shell\\main_menu\\multiplayer_type_select\\split_screen\\splitscreen_map_select_postgame_wrapper",
				NONE, NULL, NONE, NONE, NONE, NONE))
			{
				error(_error_silent, "failed to load map select postgame screen");
			}
		}
	}
	else
	{
		if (global_network_game_server_get())
		{
			network_game_server_pause_countdown(global_network_game_server_get(), TRUE);
			/* port: with the PC version's menus, theirs (port/linux/game/menu_tags.c) */
			if (!ui_widget_load_by_name_or_tag(
				pc_menus_screen("ui\\shell\\main_menu\\multiplayer_type_select\\connected\\connected_map_select_postgame_wrapper"),
				NONE, NULL, NONE, NONE, NONE, NONE))
			{
				error(_error_silent, "failed to load map select postgame screen");
			}
		}
		else
		{
			if (!ui_widget_load_by_name_or_tag(
				pc_menus_screen("ui\\shell\\main_menu\\multiplayer_type_select\\connected\\pregame\\connected_pregame_screen"),
				NONE, NULL, NONE, NONE, NONE, NONE))
			{
				error(_error_silent, "failed to load networked pregame status screen");
			}
		}
	}

	return;
}

void display_scenario_help(
	short string_index)
{
	char scenario_name[256];

	match_assert(
		"c:\\halo\\SOURCE\\interface\\ui_widget.c",
		2407,
		string_index>=0);
	/* port: noted in the recorded time (input_replay.c), where the level's script asks for the dialog, whether or not
	it shows */
	{
		extern void input_replay_note_event(char const *kind, char const *name);
		extern char const *hs_runtime_get_executing_thread_name(void);

		input_replay_note_event("scenario_help", hs_runtime_get_executing_thread_name());
	}
	/* port: shown by no automatic take, replay of one or run with debug.scenario_help off: the dialog stops the game
	until a button is pressed, which such a take has no press for (input_replay.c) */
	{
		extern int input_replay_scenario_help_suppressed(void);

		if (input_replay_scenario_help_suppressed())
			return;
	}
	if (global_scenario_index != NONE)
	{
		char const *widget_name;
		short local_player_index;
		struct widget_instance *widget;

		csstrncpy(scenario_name, tag_get_name(global_scenario_index), sizeof(scenario_name) - 1);
		scenario_name[sizeof(scenario_name) - 1] = 0;
		strlwr(scenario_name);
		if (strstr(scenario_name, "a10"))
			widget_name = "ui\\shell\\solo_game\\player_help\\player_help_screen_a10";
		else if (strstr(scenario_name, "a30"))
			widget_name = "ui\\shell\\solo_game\\player_help\\player_help_screen_a30";
		else if (strstr(scenario_name, "a50"))
			widget_name = "ui\\shell\\solo_game\\player_help\\player_help_screen_a50";
		else if (strstr(scenario_name, "b30"))
			widget_name = "ui\\shell\\solo_game\\player_help\\player_help_screen_b30";
		else if (strstr(scenario_name, "b40"))
			widget_name = "ui\\shell\\solo_game\\player_help\\player_help_screen_b40";
		else if (strstr(scenario_name, "c10"))
			widget_name = "ui\\shell\\solo_game\\player_help\\player_help_screen_c10";
		else if (strstr(scenario_name, "c20"))
			widget_name = "ui\\shell\\solo_game\\player_help\\player_help_screen_c20";
		else if (strstr(scenario_name, "c40"))
			widget_name = "ui\\shell\\solo_game\\player_help\\player_help_screen_c40";
		else if (strstr(scenario_name, "d20"))
			widget_name = "ui\\shell\\solo_game\\player_help\\player_help_screen_d20";
		else if (strstr(scenario_name, "d40"))
			widget_name = "ui\\shell\\solo_game\\player_help\\player_help_screen_d40";
		else
		{
			error(_error_silent, "can't display scenario help; unknown scenario is active '%s'", scenario_name);

			return;
		}
		local_player_index = player_ui_get_single_player_local_player_controller(0);
		/* port: the player's profile decides, except while a recording or a replay runs: there the dialog shows as
		its recording had it, whatever the profile of the device replaying, since showing one or not changes when the
		game stops (input_replay.c) */
		{
			extern int input_replay_scenario_help_fixed(void);

			if (!input_replay_scenario_help_fixed() && player_ui_ingame_help_disabled(local_player_index))
				return;
		}

		widget = ui_widget_load_by_name_or_tag(
			widget_name,
			NONE,
			NULL,
			local_player_index,
			NONE,
			NONE,
			NONE);
		if (widget)
		{
			struct widget_instance *text_box;

			for (text_box = widget->child; text_box; text_box = text_box->next)
			{
				if (text_box->type == _ui_widget_type_text_box)
					break;
			}
			match_vassert(
				"c:\\halo\\SOURCE\\interface\\ui_widget.c",
				2438,
				text_box && text_box->type == _ui_widget_type_text_box,
				"expected text box widget in player help screen");
			text_box->parameters.text_box.string_list_index = string_index;
		}
		else
		{
			error(_error_silent, "failed to load in-game help dialog");
		}
	}
	else
	{
		error(_error_silent, "can't display scenario help because no scenario is loaded");
	}

	return;
}

static boolean transition_to_game_in_progress(
	void)
{
	if (
		we_are_at_the_main_menu &&
		widget_globals.fade_to_black <= 1.0f &&
		widget_globals.fade_to_black >= 0.0f)
	{
		return TRUE;
	}

	return FALSE;
}

static short get_icon_type(
	wchar_t const *string)
{
	short icon_index;

	for (icon_index = 0; icon_index < NUMBEROF(icon_names); icon_index++)
	{
		if (_wcsnicmp(string, icon_names[icon_index], wcslen(icon_names[icon_index])) == 0)
			break;
	}

	if (icon_index == NUMBEROF(icon_names))
		icon_index = NONE;

	return icon_index;
}

static boolean should_flip_sticks_for_local_player(
	short local_player_index)
{
	struct game_input_preferences preferences;

	if (local_player_index == NONE)
		local_player_index = local_player_get_next(NONE);
	memset(&preferences, 0, sizeof(preferences));
	if (local_player_index != NONE)
		input_abstraction_get_local_player_preferences(local_player_index, &preferences);
	switch (preferences.joystick_controls)
	{
	case _joystick_preset_south_paw:
	case _joystick_preset_legacy_south_paw:
		return TRUE;
	}

	return FALSE;
}

short remap_sticks_for_local_player(
	short icon,
	short local_player_index)
{
	switch (icon)
	{
	case _icon_left_stick:
	case _icon_move:
		match_assert(
			"c:\\halo\\SOURCE\\interface\\ui_widget.c",
			4243,
			16 == get_icon_type(L"left-stick"));
		match_assert(
			"c:\\halo\\SOURCE\\interface\\ui_widget.c",
			4244,
			30 == get_icon_type(L"move"));
		icon = should_flip_sticks_for_local_player(local_player_index) ? _icon_right_stick : _icon_left_stick;
		break;
	case _icon_right_stick:
	case _icon_look:
		match_assert(
			"c:\\halo\\SOURCE\\interface\\ui_widget.c",
			4250,
			17 == get_icon_type(L"right-stick"));
		match_assert(
			"c:\\halo\\SOURCE\\interface\\ui_widget.c",
			4251,
			31 == get_icon_type(L"look"));
		icon = should_flip_sticks_for_local_player(local_player_index) ? _icon_left_stick : _icon_right_stick;
		break;
	}

	return icon;
}

static unsigned long __stdcall filesystem_initialization_thread_proc(
	void *input)
{
	widget_globals.filesystem_check_result = saved_game_perform_file_system_checks();
	if (!widget_globals.filesystem_check_result)
	{
		word number_of_profiles = 1;
		long profile_index;

		playlist_profiles_enumerate_available_to_local_player_index(NONE, &number_of_profiles, &profile_index);
		player_profiles_enumerate_available_to_local_player_index(NONE, &number_of_profiles, &profile_index, TRUE);
		player_ui_get_player1_last_used_profile_index();
	}

	return 0;
}

static void perform_filesystem_initialization(
	void)
{
	match_assert(
		"c:\\halo\\SOURCE\\interface\\ui_widget.c",
		5439,
		widget_globals.initialization_thread==NULL);
	error(_error_silent, "begining filesystem checks & saved game file enumeration...");
	ui_widgets_inhibit_processing(TRUE);
	widget_globals.filesystem_check_result = 0;
	if (!create_thread(0, filesystem_initialization_thread_proc, NULL, &widget_globals.initialization_thread))
	{
		error(_error_silent, "failed to spawn thread for filesystem checks - running synchronously!");
		widget_globals.initialization_thread = NULL;
		filesystem_initialization_thread_proc(NULL);
		ui_widgets_inhibit_processing(FALSE);
	}

	return;
}

void main_screen_shell_load(
	void)
{
	boolean load_main_menu = TRUE;

	ui_widgets_inhibit_processing(FALSE);
	if (main_screen_shell_first_load == TRUE)
	{
		char const *command_line = shell_get_command_line();

		if (command_line && _stricmp(command_line, "xdemo") == 0)
		{
			error(_error_silent, "xbox command line= '%s'", command_line);
		}
		else
		{
			bink_playback_start(
				attract_mode_get_localized_movie_path(_bink_intro_movie),
				FLAG(_bink_playback_button_click_stops_movie_bit) |
					FLAG(_bink_playback_prevent_events_to_ui_bit) |
					FLAG(_bink_playback_return_to_main_menu_when_finished_bit) |
					FLAG(_bink_playback_dont_allow_skipping_if_filesystem_check_thread_is_active_bit) |
					FLAG(_bink_playback_eat_up_memory_like_a_goddamn_beaver_bit));
			load_main_menu = FALSE;
			if (!bink_playback_active())
				load_main_menu = TRUE;
		}
		perform_filesystem_initialization();
		input_abstraction_reset_controller_detection_timer();
	}
	if (load_main_menu)
	{
		attract_mode_reset_timer();
		ui_widgets_close_all();
		/* port: the menus' main menu, when they are there (port/linux/game/menu_tags.c) */
		{
			extern char const *pc_menus_root_name(void);

			if (!ui_widget_load_by_name_or_tag(pc_menus_root_name(), NONE, NULL, NONE, NONE, NONE, NONE))
				error(_error_silent, "failed to load main screen shell window");
		}
		if (widget_globals.main_menu_deferred_error_code != NONE)
		{
			display_error(widget_globals.main_menu_deferred_error_code, NONE, TRUE, FALSE);
			widget_globals.main_menu_deferred_error_code = NONE;
		}
		if (!widget_globals.main_menu_music_active)
			ui_start_main_menu_music();
		reset_last_player1_profile_index();
		/* port: exercise real error APIs only after the menu has loaded. */
		{
			extern void pc_menu_debug_error_ready(void);
			pc_menu_debug_error_ready();
		}
	}
	if (!virtual_keyboard_initialize())
		error(_error_silent, "failed to initialize the virtual keyboard");
	main_screen_shell_first_load = FALSE;

	return;
}

static void widget_instance_reload_recursive(
	struct widget_instance *widget)
{
	return;
}

static void ui_widget_reload_by_tag(
	long tag_index)
{
	return;
}

real_rgb_color get_ui_rgb_white(
	void)
{
	real_rgb_color result;

	result = *global_real_rgb_white;
	result.red = global_ui_white_red;
	result.green = global_ui_white_green;
	result.blue = global_ui_white_blue;

	return result;
}

real_argb_color get_ui_argb_white(
	void)
{
	real_argb_color result;

	result = *global_real_argb_white;
	result.red = global_ui_white_red;
	result.green = global_ui_white_green;
	result.blue = global_ui_white_blue;

	return result;
}

static __inline real widget_instance_get_cumulative_alpha_modifier(
	struct widget_instance *widget)
{
	real alpha_modifier = widget->alpha_modifier;

	widget = widget->parent;
	while (widget)
	{
		alpha_modifier *= widget->alpha_modifier;
		widget = widget->parent;
	}

	return alpha_modifier;
}

/* port: the one rule for drawing focus (a row's selected frame, white text, the plasma, a card's outline): whether a
focused item of a kind (halo_menu_meta.h) draws as focused. A list's rows draw their focus on every input, touch
too, so a touch player sees which row the panel describes. A carousel card draws no focus outline while the last
input is touch: a finger does not move focus and a tap acts on the card it lands on. Every place that draws focus
asks this (ui_list_child_draws_focus for a list's child, the carousel's cards, and the port's own lists in
menu_functions.c); tools/test_focus_draw_rule.py fails on one that does not. */
int ui_widget_port_draws_focus(
	enum halo_menu_focus_item item,
	int focused)
{
	return focused && (item == HALO_MENU_FOCUS_ROW || halo_menu_pointer_controls() != 2);
}

/* port: the kind of item a list's children are (halo_menu_meta.h): a carousel's are cards, any other list's rows.
The list updaters and the carousel both ask this, so one widget never has two rules. */
static enum halo_menu_focus_item ui_list_focus_item(
	struct widget_instance *list)
{
	return ui_widget_is_carousel(list) ? HALO_MENU_FOCUS_CARD : HALO_MENU_FOCUS_ROW;
}

/* port: whether a list draws its child as focused (ui_widget_port_draws_focus) */
static boolean ui_list_child_draws_focus(
	struct widget_instance *list,
	struct widget_instance *child)
{
	return ui_widget_port_draws_focus(ui_list_focus_item(list), child == list->focused_child);
}

static boolean widget_instance_text_box_is_focused(
	struct widget_instance *widget)
{
	struct widget_instance *parent = widget->parent;
	boolean focused = parent ? parent->focused_child == widget : TRUE;

	if (!focused && parent)
	{
		struct widget_instance *ancestor;

		do
		{
			ancestor = parent->parent;
			if (ancestor)
			{
				if (ancestor->focused_child != parent)
					break;
				focused = (ancestor->type == _ui_widget_type_spinner_list ||
					ancestor->type == _ui_widget_type_column_list) &&
					ancestor->focused_child == parent;
			}
			parent = ancestor;
		}
		while (ancestor);
	}

	return focused;
}

static boolean string_has_icons_to_draw(
	wchar_t *string)
{
	match_assert(
		"c:\\halo\\SOURCE\\interface\\ui_widget.c",
		4181,
		string);
	while (string)
	{
		wchar_t *icon_spec = wcschr(string, L'%');

		if (!icon_spec)
			break;
		icon_spec++;
		if (get_icon_type(icon_spec) != NONE)
			return TRUE;
		string = icon_spec;
	}

	return FALSE;
}

static long search_and_replace(
	wchar_t *search,
	wchar_t *replace,
	wchar_t **string)
{
	long replacements = 0;

	if (string && *string)
	{
		wchar_t *buffer = *string;
		long search_length = ustrlen(search);
		long replace_length = ustrlen(replace);
		long length = ustrlen(buffer) + 1;
		long delta;
		wchar_t *match;

		if (replace_length <= search_length)
		{
			delta = search_length - replace_length;
			match = ustrstr(buffer, search);
			if (match)
			{
				do
				{
					replacements++;
					csmemcpy(match, replace, 2 * replace_length);
					if (delta > 0)
					{
						csmemmove(
							&match[replace_length],
							&match[search_length],
							2 * (length - (match - buffer) - replace_length));
						length -= delta;
					}
					match = ustrstr(buffer, search);
				}
				while (match);
			}
		}
		else
		{
			delta = replace_length - search_length;
			for (match = ustrstr(buffer, search);
				match;
				match = ustrstr(&match[search_length], search))
			{
				replacements++;
			}
			if (replacements > 0)
			{
				buffer = pool_resize_pointer(
					widget_memory_pool,
					buffer,
					2 * (delta * replacements + length),
					"c:\\halo\\SOURCE\\interface\\ui_widget.c",
					4994);
				if (!buffer)
				{
					replacements = NONE;
				}
				else
				{
					for (match = ustrstr(buffer, search);
						match;
						match = ustrstr(buffer, search))
					{
						csmemmove(
							&match[replace_length],
							&match[search_length],
							2 * (length - (match - buffer) - search_length));
						csmemcpy(match, replace, 2 * replace_length);
						length += delta;
					}
					*string = buffer;
				}
			}
		}
	}

	return replacements;
}

/* port: measure without the cell's clipping, including glyph overhang. */
static short ui_widget_port_text_width(wchar_t const *text)
{
	rectangle2d bounds = {0, 0, SHORT_MAX, SHORT_MAX};
	rectangle2d ink, cursor;

	draw_unicode_string_compute_bounds(&bounds, text, &ink, &cursor);
	return MAX(ink.x1, cursor.x0);
}

/* port: a string's width in a font, for the menus' layout (menu_tags.c
sizes the settings lists' value columns with it as the menus are made). */
short ui_widget_port_font_text_width(long font_index, void const *text)
{
	long old_font = draw_string_get_font();
	short width;

	if (font_index == NONE || !text)
		return 0;
	draw_string_set_font(font_index);
	draw_string_set_format(NONE, 0, 0);
	draw_string_set_indents(0, 0);
	width = ui_widget_port_text_width((wchar_t const *)text);
	/* (the font in force may not be one of this map's: before anything is
	drawn it is the zeroed global's 0, and while a game map's menus are made
	it is still the previous map's index, which draw_string_set_font would
	look up as a font in the new map) */
	draw_string_restore_font(old_font);
	return width;
}

/* port: browser cells share this path for both menu looks. Use the rendered
font's metrics and glyph table so a cut name ends visibly in an ellipsis. */
void ui_widget_port_text_ellipsize(struct widget_instance *widget, wchar_t *text, short capacity)
{
	struct ui_widget_definition *definition;
	wchar_t const *suffix;
	long old_font;
	short width, length = 0, suffix_length, index;

	if (!widget || capacity < 1 || !text[0])
		return;
	definition = ui_widget_definition_get(widget->definition_tag_index);
	if (definition->text_font.index == NONE)
		return;
	/* draw_unicode_string requires a glyph to end strictly before x1, and the last dot of the suffix was clipped at
	the edge (rows showed ".." for "..."), so keep a 3 pixel margin. */
	width = definition->bounds.x1 - definition->bounds.x0 - definition->horizontal_offset - 1 - 3;
	old_font = draw_string_get_font();
	draw_string_set_font(definition->text_font.index);
	draw_string_set_format(NONE, 0, 0);
	draw_string_set_indents(0, 0);
	if (ui_widget_port_text_width(text) > width)
	{
		/* the font's U+2026 slot draws as two dots, so always use three plain periods */
		suffix = L"...";
		suffix_length = suffix[1] ? 3 : 1;
		while (text[length] && length < capacity - 1)
			length++;
		length = MIN(length, capacity - suffix_length - 1);
		if (length < 0)
			text[0] = 0;
		else
		{
			for (;;)
			{
				for (index = 0; index <= suffix_length; index++)
					text[length + index] = suffix[index];
				if (ui_widget_port_text_width(text) <= width)
					break;
				if (!length)
				{
					text[0] = 0;
					break;
				}
				length--;
			}
		}
	}
	draw_string_restore_font(old_font);
}

/* port: the port's own error text (display_error_text_deferred), which may be
longer than a line of its dialog: broken at spaces (the strings' line break,
'\r') where a line would be wider than `width` in the font drawn with.
FALSE when a line is still wider (a word longer than a line). */
static boolean ui_widget_port_text_wrap(
	wchar_t *text,
	rectangle2d const *bounds,
	short width)
{
	long line_start = 0;
	long last_space = NONE;
	long index;
	boolean fits = TRUE;

	/* the strings' line breaks (a \r\n pair, from "\n") were made for the original box's width: a single break is a
	space now (this box wraps to its own width); a blank line (paragraph break) stays */
	for (index = 1; text[index]; index++)
	{
		long run_end = index;

		if (text[index] != L'\r' && text[index] != L'\n')
			continue;
		while (text[run_end] == L'\r' || text[run_end] == L'\n')
			run_end++;
		if (run_end - index <= 2 && !(run_end - index == 2 && text[index] == text[index + 1]) && text[run_end])
		{
			long copy_index;

			text[index] = L' ';
			for (copy_index = index + 1; ; copy_index++)
			{
				text[copy_index] = text[copy_index - index - 1 + run_end];
				if (!text[copy_index])
					break;
			}
		}
		else
			index = run_end - 1;
	}

	for (index = 0; ; index++)
	{
		wchar_t character = text[index];

		/* a hard line break ends a word like a space does: the last word of each authored line is measured too */
		if (character == L' ' || character == 0 || character == L'\r' || character == L'\n')
		{
			wchar_t line[256];
			long length = MIN(index - line_start, (long)NUMBEROF(line) - 1);
			rectangle2d text_bounds;
			rectangle2d cursor_bounds;

			csmemcpy(line, text + line_start, length * sizeof(wchar_t));
			line[length] = 0;
			draw_unicode_string_compute_bounds(bounds, line, &text_bounds, &cursor_bounds);
			/* the glyphs of a line reach past its cursor: measure what is drawn */
			if (MAX(cursor_bounds.x0, text_bounds.x1) - bounds->x0 > width && last_space != NONE)
			{
				text[last_space] = L'\r';
				line_start = last_space + 1;
				/* (the line begun, the word just measured: wider alone?) */
				length = MIN(index - line_start, (long)NUMBEROF(line) - 1);
				csmemcpy(line, text + line_start, length * sizeof(wchar_t));
				line[length] = 0;
				draw_unicode_string_compute_bounds(bounds, line, &text_bounds, &cursor_bounds);
			}
			if (MAX(cursor_bounds.x0, text_bounds.x1) - bounds->x0 > width)
				fits = FALSE;
			if (character == 0)
				break;
			if (character == L'\r' || character == L'\n')
			{
				line_start = index + 1;
				last_space = NONE;
				continue;
			}
			last_space = index;
		}
	}

	return fits;
}

/* port: text drawn in place of a text box's own for one render (the commit page's SAVE
caption). The widget's text stays a widget_memory_pool block: a text box whose text comes
from a string list resizes that block every render, and pool_resize_pointer halts on any
other pointer. */
static wchar_t *ui_text_box_render_override = NULL;

/* draw_string's flag to wrap a line too wide for its box at a space (_draw_text_wrap_horizontally_bit, draw_string.c) */
#define UI_DRAW_TEXT_WRAP_HORIZONTALLY FLAG(0)

/* port: on touch, the disc's menu strings that name a controller button ("Press %a-button or
%y-button to start your own game") draw that button as an empty gap, since touch has no glyph for it:
each such string reachable on touch reads in words for a tap instead. The SELECT PROFILE slots'
press_start_to_join strings depend on the slot and have their own touch text. NULL keeps the disc's
string (any other string, or not touch). tools/test_menu_standard.py checks the table against the map. */
static wchar_t const *ui_touch_disc_text(
	long string_list_tag_index,
	short string_list_index)
{
	static struct
	{
		char const *list;
		short index;
		wchar_t const *text;
	} const touch_strings[] =
	{
		{ "ui\\shell\\main_menu\\multiplayer_type_select\\connected\\server_list\\looking_for_servers_message", 1,
			L"Looking for games...\r\n\r\nTap CREATE GAME to start\r\nyour own game" },
	};
	char const *name;
	long index;

	if (halo_menu_pointer_controls() != 2 || string_list_tag_index == NONE)
		return NULL;
	name = tag_get_name(string_list_tag_index);
	for (index = 0; name && index < (long)NUMBEROF(touch_strings); index++)
	{
		if (touch_strings[index].index == string_list_index && !strcmp(name, touch_strings[index].list))
			return touch_strings[index].text;
	}
	return NULL;
}

static void widget_instance_render_text_box(
	struct widget_instance *widget,
	struct ui_widget_definition *definition,
	rectangle2d *clip_rect,
	point2d offset,
	boolean focus)
{
	wchar_t **text;
	wchar_t *override_text = ui_text_box_render_override;
	long search_index;
	long font_index;
	short justification;
	real alpha_modifier;
	real color_alpha;
	real_argb_color color;
	rectangle2d bounds;
	rectangle2d clip;

	if (definition->text_label_string_list.index != NONE)
	{
		short string_list_index;
		wchar_t *string;
		unsigned long length;

		if (widget->parameters.text_box.string_list_index == NONE)
			string_list_index = definition->string_list_index;
		else
			string_list_index = widget->parameters.text_box.string_list_index;
		string = widget == ui_widget_port_error_text_box && ui_widget_port_error_text ?
			(wchar_t *)ui_widget_port_error_text :
			unicode_string_list_get_string(definition->text_label_string_list.index, string_list_index);
		/* port: on touch, a disc string that names a button in words a tap can act on instead */
		{
			wchar_t const *touch_text = ui_touch_disc_text(definition->text_label_string_list.index, string_list_index);

			if (touch_text)
				string = (wchar_t *)touch_text;
		}
		/* port: the description of a spinner's extra item */
		if (string_list_index >= SPINNER_EXTRA_DESCRIPTION_BASE &&
			string_list_index < SPINNER_EXTRA_DESCRIPTION_BASE + (short)NUMBEROF(kills_to_win_extra_descriptions))
		{
			string = (wchar_t *)kills_to_win_extra_descriptions[string_list_index - SPINNER_EXTRA_DESCRIPTION_BASE];
		}
		length = ustrlen(string);
		widget->parameters.text_box.text = pool_resize_pointer(
			widget_memory_pool,
			widget->parameters.text_box.text,
			2 * length + 2,
			"c:\\halo\\SOURCE\\interface\\ui_widget.c",
			4421);
		if (widget->parameters.text_box.text)
		{
			csmemcpy(widget->parameters.text_box.text, string, 2 * length);
			widget->parameters.text_box.text[length] = 0;
		}
		else
		{
			widget->parameters.text_box.text = L"<out of memory>";
		}
	}
	text = override_text ? &override_text : &widget->parameters.text_box.text;
	if (!*text || !**text)
		return;
	/* (an override is not a pool block: search and replace may resize its text) */
	for (search_index = 0;
		!override_text && search_index < definition->search_and_replace_functions.count;
		search_index++)
	{
		struct ui_widget_search_and_replace_reference *reference =
			(struct ui_widget_search_and_replace_reference *)
				definition->search_and_replace_functions.address +
			search_index;

		if (reference && reference->search_string[0])
		{
			wchar_t search_string[32];
			wchar_t *replace = ui_widget_search_and_replace_invoke(
				widget,
				reference->replace_function);

			search_and_replace(
				ascii_to_wide(
					reference->search_string,
					search_string,
					sizeof(search_string)),
				replace,
				text);
		}
	}
	font_index = definition->text_font.index;
	if (font_index == NONE)
	{
		error(
			_error_silent,
			"failed to render text box widget because the font tag was invalid");

		return;
	}
	justification = definition->justification;
	if (justification < 0 || justification >= NUMBER_OF_TEXT_JUSTIFICATIONS)
	{
		error(
			_error_silent,
			"failed to render text box widget because the justification was invalid");

		return;
	}
	if (!widget->visible)
		return;
	alpha_modifier = widget_instance_get_cumulative_alpha_modifier(widget);
	bounds = definition->bounds;
	clip = clip_rect ? *clip_rect : definition->bounds;
	if (ui_carousel_clip)
	{
		clip.x0 = MAX(clip.x0, ui_carousel_clip->x0);
		clip.x1 = MIN(clip.x1, ui_carousel_clip->x1);
		clip.y0 = MAX(clip.y0, ui_carousel_clip->y0);
		clip.y1 = MIN(clip.y1, ui_carousel_clip->y1);
	}

	bounds.x1 += offset.x;
	bounds.y1 += offset.y;
	bounds.x0 += offset.x;
	bounds.y0 += offset.y;
	bounds.x0 += definition->horizontal_offset;
	bounds.y0 += definition->vertical_offset;
#ifdef HALO_IOS
	/* port: the version label keeps its whole text, including a development build's commit; it is
	right-aligned to the menu's right edge, which the safe-area zoom keeps inside the camera cutout */
	if (widget == ui_widget_port_build_label)
	{
		justification = _text_justification_right;
		bounds.x0 = 0;
		bounds.x1 = 636;
		clip = bounds;
	}
#endif
	if (focus)
	{
		color = get_ui_argb_white();
		color_alpha = definition->text_color.alpha;
	}
	else
	{
		color = definition->text_color;
		if (1.0f == color.red && 1.0f == color.green && 1.0f == color.blue)
		{
			color = get_ui_argb_white();
			color_alpha = definition->text_color.alpha;
		}
		else
		{
			color_alpha = color.alpha;
		}
	}
	/* port: unsupported modes and incompatible versions in the browser. */
	if (ui_widget_port_browser_row_unavailable(widget))
		color.red = color.green = color.blue = 0.5f;
	color.alpha = alpha_modifier * color_alpha;
	if (TEST_FLAG(definition->text_box_flags, _text_box_flashing_text_bit))
	{
		color.alpha = (((real)cos(
			widget_globals.current_system_milliseconds *
				SECONDS_PER_MILLISECOND * 3.0f) + 1.5f) * 0.4f) * color.alpha;
	}
	/* port: text inside the carousel's edge fade takes the fade at the middle
	of its visible span, matching the faded card art around it. */
	if (ui_carousel_clip)
	{
		real visible_x0 = (real)MAX(bounds.x0, clip.x0);
		real visible_x1 = (real)MIN(bounds.x1, clip.x1);
		color.alpha *= halo_ui_carousel_fade((visible_x0 + visible_x1) * 0.5f,
			(real)ui_carousel_clip->x0, (real)ui_carousel_clip->x1);
	}
	/* port: a text box with room for two lines wraps a line too wide for it at a space, so no
	description or panel text ever loses its end; draw_string would otherwise stop drawing a line at
	the first letter past the box (MULTIPLAYER's panel showed "needs a g"). A box one line tall keeps
	its line as it is (its text is sized to fit: tools/test_menu_standard.py). */
	{
		struct font_header *font_header = font_definition_get(font_index);
		short line_height = (short)(font_header->ascending_height + font_header->descending_height +
			font_header->leading_height);

		draw_string_set_draw_mode(font_index, NONE, justification,
			line_height > 0 && bounds.y1 - bounds.y0 - definition->vertical_offset >= 2 * line_height ?
			UI_DRAW_TEXT_WRAP_HORIZONTALLY : 0, &color);
	}
	/* port: the port's own error text, wrapped to its dialog: the box its
	background draws (narrower than the text box; its picture is padded to a
	power of two, so a margin of the text's offset on each side and as much
	again), and as tall as the text box less its offset above and as much
	below (the dialog's footer). Text that would be taller, or wider (a word
	longer than a line), is drawn in the menus' smaller font. */
	if (widget == ui_widget_port_error_text_box)
	{
		struct bitmap_data *background = definition->background_bitmap.index != NONE ?
			bitmap_group_get_bitmap_from_sequence(definition->background_bitmap.index, 0, 0) : NULL;
		short width = (short)(definition->bounds.x1 - definition->bounds.x0);
		short height = (short)(definition->bounds.y1 - definition->bounds.y0 - 2 * definition->vertical_offset);
		wchar_t wrapped[512];
		rectangle2d text_bounds;
		rectangle2d cursor_bounds;

		if (background && background->width > 0 && background->width < width)
			width = background->width;
		/* the frame's border takes about 4 units beyond the offset on each side: wrap to what is left */
		width = (short)(width - 2 * (definition->horizontal_offset + 4));
		ustrncpy(wrapped, *text, NUMBEROF(wrapped) - 1);
		wrapped[NUMBEROF(wrapped) - 1] = 0;
		if (!ui_widget_port_text_wrap(wrapped, &bounds, width) ||
			(draw_unicode_string_compute_bounds(&bounds, wrapped, &text_bounds, &cursor_bounds),
			cursor_bounds.y1 - bounds.y0 > height))
		{
			long small_font_index = tag_loaded('font', "ui\\small_ui");

			if (small_font_index != NONE)
			{
				font_index = small_font_index;
				draw_string_set_draw_mode(font_index, NONE, justification, 0, &color);
			}
		}
		ui_widget_port_text_wrap(*text, &bounds, width);
	}
	if (TEST_FLAG(definition->text_box_flags, _text_box_port_wrap_bit))
		ui_widget_port_text_wrap(*text, &bounds, (short)(definition->bounds.x1 - definition->bounds.x0));
	wchar_t const *display_text = *text;
	if (string_has_icons_to_draw(display_text))
	{
		short saved_player = local_player_index_for_draw_string_and_hack_in_icons;
		if (widget->local_player_index >= 0 && widget->local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS)
			local_player_index_for_draw_string_and_hack_in_icons = widget->local_player_index;
		draw_string_and_hack_in_icons(&bounds, &clip, NULL, 0, display_text, FALSE);
		local_player_index_for_draw_string_and_hack_in_icons = saved_player;
	}
	else
		rasterizer_draw_unicode_string(&bounds, &clip, NULL, 0, display_text);

	return;
}

/* port: read the live tag strings, including values appended by the port. */
static wchar_t const *ui_widget_row_string(void *context, int index)
{
	struct ui_widget_definition *definition = context;
	return spinner_string_list_get_string(definition->text_label_string_list.index, (short)index);
}

/* port: whether a spinner is a standard row's value, drawn as a switch, a dropdown or a slider under touch and
mouse (halo_menu_row.h decides which from its values): any spinner of the menus' own files, and the map's own
spinner in a row that was made a standard tall row (pc_menu_row_tall: the disc's option lists). Said by the menus'
data, never read off a tag's name. */
static boolean ui_widget_row_is_standard(struct widget_instance *widget)
{
	return pc_menu_tag(widget->definition_tag_index) ||
		(widget->parent && pc_menu_row_tall(widget->parent->definition_tag_index));
}

static struct halo_menu_row ui_widget_row_control(struct widget_instance *widget)
{
	struct halo_menu_row row = { HALO_ROW_ENUM, -1 };
	struct ui_widget_definition *definition = ui_widget_definition_get(widget->definition_tag_index);
	struct string_list *strings;
	/* (carousels, which hold their items as children, keep their own behavior) */
	if (!ui_widget_row_is_standard(widget) ||
		definition->child_widgets.count || definition->text_label_string_list.index == NONE)
		return row;
	strings = unicode_string_list_definition_get(definition->text_label_string_list.index);
	if (!strings) return row;
	return halo_menu_row_classify(strings->strings.count +
		spinner_string_list_extra_count(definition->text_label_string_list.index),
		ui_widget_row_string, definition);
}

/* port: only string-backed Xbox settings rows own a dropdown. The classifier's
fallback enum also covers PC lists and carousels, which retain their behavior. */
static boolean ui_widget_row_has_dropdown(struct widget_instance *widget)
{
	struct ui_widget_definition *definition = ui_widget_definition_get(widget->definition_tag_index);
	return ui_widget_row_is_standard(widget) &&
		!definition->child_widgets.count && definition->text_label_string_list.index != NONE &&
		widget->parameters.list.number_of_items > 1 && ui_widget_row_control(widget).kind == HALO_ROW_ENUM;
}

static struct halo_menu_dropdown ui_dropdown;
static struct widget_instance *ui_dropdown_widget;
static long ui_dropdown_creation_time;
static int ui_dropdown_selection = -1;
static int ui_dropdown_last_selection = -1;

/* port: bitmap controls share the game's font/bitmap pipeline, with no host widgets. */
static void ui_dropdown_bitmap(struct widget_instance *widget, char const *name,
	short frame, rectangle2d *bounds, rectangle2d *clip)
{
	long tag = tag_loaded('bitm', name);
	struct bitmap_data *bitmap;
	struct rasterizer_dynamic_screen_geometry_parameters parameters;
	rectangle2d source;
	if (tag == NONE) return;
	bitmap = bitmap_group_get_bitmap_from_sequence(tag, 0, frame);
	if (!bitmap) return;
	source.x0 = source.y0 = 0;
	source.x1 = bounds->x1 - bounds->x0;
	source.y1 = bounds->y1 - bounds->y0;
	csmemset(&parameters, 0, sizeof(parameters));
	draw_bitmap_in_rect(bitmap, bounds, &source, clip,
		(fast_ftol(widget_instance_get_cumulative_alpha_modifier(widget) * 255.0f) << 24) | 0x00FFFFFF,
		&parameters, TRUE);
}

static void ui_widget_row_draw_chevron(struct widget_instance *widget,
	struct ui_widget_definition *definition, point2d offset, rectangle2d *clip)
{
	rectangle2d bounds;
	bounds.x1 = definition->bounds.x1 + offset.x - MENU_CHEVRON_INSET;
	bounds.x0 = bounds.x1 - MENU_CHEVRON_WIDTH;
	bounds.y0 = definition->bounds.y0 + offset.y + (definition->bounds.y1 - definition->bounds.y0 - 8) / 2;
	bounds.y1 = bounds.y0 + 8;
	ui_dropdown_bitmap(widget, "pc\\xbox\\bitmaps\\dropdown_chevron", 0, &bounds, clip);
}

#define UI_TOGGLE_ARROW_GAP 6
static void ui_widget_row_draw_toggle(struct widget_instance *widget,
	struct ui_widget_definition *definition, point2d offset, rectangle2d *clip, int on, int inset)
{
	long tag = tag_loaded('bitm', "pc\\xbox\\bitmaps\\toggle_switch");
	struct bitmap_data *bitmap;
	struct rasterizer_dynamic_screen_geometry_parameters parameters;
	rectangle2d bounds, source;
	if (tag == NONE) return;
	bitmap = bitmap_group_get_bitmap_from_sequence(tag, 0, on);
	if (!bitmap) return;
	bounds.x1 = definition->bounds.x1 + offset.x - inset;
	bounds.x0 = bounds.x1 - 32;
	bounds.y0 = definition->bounds.y0 + offset.y + (definition->bounds.y1 - definition->bounds.y0 - 17) / 2;
	bounds.y1 = bounds.y0 + 17;
	source.x0 = source.y0 = 0;
	source.x1 = 32; source.y1 = 17;
	csmemset(&parameters, 0, sizeof(parameters));
	draw_bitmap_in_rect(bitmap, &bounds, &source, clip,
		(fast_ftol(widget_instance_get_cumulative_alpha_modifier(widget) * 255.0f) << 24) | 0x00FFFFFF,
		&parameters, TRUE);
}

/* port: RANGE rows draw directly from the live string without a frame allocation. */
static void ui_widget_row_draw_slider(struct widget_instance *widget,
	struct ui_widget_definition *definition, point2d offset, rectangle2d *clip)
{
	struct halo_menu_slider slider;
	struct halo_slider_rect thumb;
	rectangle2d bounds = definition->bounds;
	real_argb_color color = definition->text_color;
	int selected = widget->parameters.list.selected_index;
	int count = widget->parameters.list.number_of_items;
	bounds.x0 += offset.x; bounds.x1 += offset.x;
	bounds.y0 += offset.y; bounds.y1 += offset.y;
	if (spinner_string_list_extra_count(definition->text_label_string_list.index))
		bounds.x0 -= SPINNER_EXTRA_WIDTH;
	halo_menu_slider_geometry(&slider, bounds.x0, bounds.y0, bounds.x1, bounds.y1);
	thumb = halo_menu_slider_thumb(&slider, selected, count);
	bounds.x0 = slider.left; bounds.x1 = slider.right;
	bounds.y0 = slider.y - 2; bounds.y1 = bounds.y0 + 5;
	ui_dropdown_bitmap(widget, "pc\\xbox\\bitmaps\\slider_track", 0, &bounds, clip);
	bounds.x1 = thumb.x0 + 7;
	if (bounds.x1 > bounds.x0)
		ui_dropdown_bitmap(widget, "pc\\xbox\\bitmaps\\slider_fill", 0, &bounds, clip);
	bounds.x0 = thumb.x0 - 5; bounds.x1 = bounds.x0 + 25;
	bounds.y0 = thumb.y0 - 5; bounds.y1 = bounds.y0 + 25;
	ui_dropdown_bitmap(widget, "pc\\xbox\\bitmaps\\slider_thumb", 0, &bounds, clip);
	bounds = definition->bounds;
	bounds.x0 = slider.right + HALO_MENU_SLIDER_VALUE_GAP; bounds.x1 += offset.x;
	bounds.y0 += offset.y; bounds.y1 += offset.y;
	color.alpha *= widget_instance_get_cumulative_alpha_modifier(widget);
	draw_string_set_draw_mode(definition->text_font.index, NONE, 1, 0, &color);
	rasterizer_draw_unicode_string(&bounds, clip, NULL, 0,
		spinner_string_list_get_string(definition->text_label_string_list.index, (short)selected));
}

static void widget_instance_render_spinner_list(
	struct widget_instance *widget,
	struct ui_widget_definition *definition,
	rectangle2d *clip_rect,
	point2d offset,
	boolean focus)
{
	struct halo_menu_row row = ui_widget_row_control(widget);
	int pointer_controls = halo_menu_pointer_controls();
	boolean toggle = pointer_controls && row.kind == HALO_ROW_TOGGLE;
	boolean dropdown = pointer_controls && ui_widget_row_has_dropdown(widget);
	boolean slider = pointer_controls && row.kind == HALO_ROW_RANGE;
	long header_frame_index = 0;
	long footer_frame_index = 0;
	real alpha_modifier = widget_instance_get_cumulative_alpha_modifier(widget);
	short last_list_tab_direction;
	struct bitmap_data *bitmap;
	rectangle2d bounds;

	if (!widget->visible || ui_widget_is_carousel(widget))
		return;
	last_list_tab_direction = widget->parameters.list.last_list_tab_direction;
	if (last_list_tab_direction)
	{
		switch (SIGN(last_list_tab_direction))
		{
		case -1:
			widget->parameters.list.last_list_tab_direction =
				last_list_tab_direction + 1;
			header_frame_index = 1;
			break;

		case 1:
			widget->parameters.list.last_list_tab_direction =
				last_list_tab_direction - 1;
			footer_frame_index = 1;
			break;
		}
	}
	bitmap = bitmap_group_get_bitmap_from_sequence(
		definition->list_header_bitmap.index,
		0,
		header_frame_index);
	if (bitmap && !(pointer_controls && row.kind == HALO_ROW_STATIC) && !((toggle || dropdown || slider) && pointer_controls == 2))
	{
		struct rasterizer_dynamic_screen_geometry_parameters parameters;
		long alpha = fast_ftol(alpha_modifier * 255.0f);

		csmemset(&parameters, 0, sizeof(parameters));
		bounds = definition->list_header_bounds;
		/* port: a spinner with extra items is wider, to the left, its arrow
		with it */
		if (spinner_string_list_extra_count(definition->text_label_string_list.index))
		{
			bounds.x0 -= SPINNER_EXTRA_WIDTH;
			bounds.x1 -= SPINNER_EXTRA_WIDTH;
		}
		if (slider)
		{
			int width = definition->bounds.x1 - definition->bounds.x0;
			int padding;
			if (spinner_string_list_extra_count(definition->text_label_string_list.index)) width += SPINNER_EXTRA_WIDTH;
			padding = MAX(0, HALO_MENU_SLIDER_WIDTH - width);
			bounds.x0 -= padding; bounds.x1 -= padding;
		}
		bounds.x0 += offset.x;
		bounds.y0 += offset.y;
		bounds.x1 += offset.x;
		bounds.y1 += offset.y;
		draw_bitmap_in_rect(
			bitmap,
			&bounds,
			&bounds,
			clip_rect,
			(alpha << 24) | 0x00FFFFFF,
			&parameters,
			FALSE);
	}
	bitmap = bitmap_group_get_bitmap_from_sequence(
		definition->list_footer_bitmap.index,
		0,
		footer_frame_index);
	if (bitmap && !(pointer_controls && row.kind == HALO_ROW_STATIC) && !((toggle || dropdown || slider) && pointer_controls == 2))
	{
		struct rasterizer_dynamic_screen_geometry_parameters parameters;
		long alpha = fast_ftol(alpha_modifier * 255.0f);

		csmemset(&parameters, 0, sizeof(parameters));
		bounds = definition->list_footer_bounds;
		bounds.x0 += offset.x;
		bounds.y0 += offset.y;
		bounds.x1 += offset.x;
		bounds.y1 += offset.y;
		draw_bitmap_in_rect(
			bitmap,
			&bounds,
			&bounds,
			clip_rect,
			(alpha << 24) | 0x00FFFFFF,
			&parameters,
			FALSE);
	}
	if (slider)
	{
		ui_widget_row_draw_slider(widget, definition, offset, clip_rect);
		return;
	}
	if (dropdown)
		ui_widget_row_draw_chevron(widget, definition, offset, clip_rect);
	if (toggle)
		/* port: with the mouse the arrows stay, so the switch keeps the slider value's gap from the right arrow */
		ui_widget_row_draw_toggle(widget, definition, offset, clip_rect,
			widget->parameters.list.selected_index == row.on_index,
			pointer_controls == 1 ? UI_TOGGLE_ARROW_GAP : 0);
	if (definition->child_widgets.count == 0)
	{
		wchar_t *item_text;

		if (definition->text_label_string_list.index != NONE)
		{
			short string_index = widget->parameters.list.selected_index;
			wchar_t *string = spinner_string_list_get_string(
				definition->text_label_string_list.index,
				string_index);
			unsigned long length;

			length = ustrlen(string);

			item_text = pool_new_pointer(
				widget_memory_pool,
				2 * length + 2,
				"c:\\halo\\SOURCE\\interface\\ui_widget.c",
				4610);
			if (item_text)
			{
				long search_index;

				csmemcpy(item_text, string, 2 * length);
				item_text[length] = 0;
				for (search_index = 0;
					search_index < definition->search_and_replace_functions.count;
					search_index++)
				{
					struct ui_widget_search_and_replace_reference *reference =
						(struct ui_widget_search_and_replace_reference *)
							definition->search_and_replace_functions.address +
						search_index;

					if (reference && reference->search_string[0])
					{
						wchar_t search_string[32];
						wchar_t *replace = ui_widget_search_and_replace_invoke(
							widget,
							reference->replace_function);

						search_and_replace(
							ascii_to_wide(
								reference->search_string,
								search_string,
								sizeof(search_string)),
							replace,
							&item_text);
					}
				}
			}
		}
		else
		{
			item_text = widget->parameters.list.item_text;
		}
		if (item_text)
		{
			if (definition->text_font.index == NONE)
			{
				error(
					_error_silent,
					"failed to render spinner list item because the font tag was invalid");
			}
			else if (definition->justification < 0 ||
				definition->justification >= NUMBER_OF_TEXT_JUSTIFICATIONS)
			{
				error(
					_error_silent,
					"failed to render spinner list item because the justification was invalid");
			}
			else
			{
				real text_alpha_modifier =
					widget_instance_get_cumulative_alpha_modifier(widget);
				real color_alpha;
				real_argb_color color;
				rectangle2d clip = clip_rect ? *clip_rect : definition->bounds;

				bounds = definition->bounds;
				bounds.x1 += offset.x;
				bounds.y1 += offset.y;
				bounds.x0 += offset.x;
				bounds.y0 += offset.y;
				/* port: a spinner with extra items (three digits) is wider, to the
				left */
				if (spinner_string_list_extra_count(definition->text_label_string_list.index))
				{
					bounds.x0 -= SPINNER_EXTRA_WIDTH;
					clip.x0 -= SPINNER_EXTRA_WIDTH;
				}
				if (focus)
				{
					color.alpha = definition->text_color.alpha;
					color.rgb = get_ui_rgb_white();
					color_alpha = color.alpha;
				}
				else
				{
					color = definition->text_color;
					if (1.0f == color.red &&
						1.0f == color.green &&
						1.0f == color.blue)
					{
						color_alpha = definition->text_color.alpha;
					}
					else
					{
						color_alpha = color.alpha;
					}
				}
				color.alpha = text_alpha_modifier * color_alpha;
				if (TEST_FLAG(
						definition->text_box_flags,
						_text_box_flashing_text_bit))
				{
					color.alpha = (((real)sin(
						widget_globals.current_system_milliseconds *
							SECONDS_PER_MILLISECOND * 3.0f) + 1.0f) * 0.5f) *
						color.alpha;
				}
				if (toggle) bounds.x1 -= 38;
				if (dropdown) bounds.x1 -= 24;
				draw_string_set_draw_mode(
					definition->text_font.index,
					NONE,
					definition->justification,
					0,
					&color);
				rasterizer_draw_unicode_string(&bounds, &clip, NULL, 0, item_text);
			}
		}
		if (definition->text_label_string_list.index != NONE)
			dispose_pointer(widget_memory_pool, item_text);
	}

	return;
}

/* ---------- the mouse (desktop builds)

The menus were made for a controller: the d-pad moves the focus through a
screen's items and A activates the focused one. With the mouse
(port/linux/include/halo_ui_pointer.h) the item under the pointer takes the
focus, a left click presses A on it (on a spinner list, left or right by the
half clicked), a right click presses B and the wheel the d-pad. The items and
where they are drawn are noted while the menus draw (ui_mouse_note_target),
for the next frame's processing (ui_widgets_process_mouse), which forgets
them: widgets can be deleted from then on. */

#define UI_MOUSE_MAXIMUM_TARGETS 96

enum ui_mouse_target_kind
{
	/* an item the d-pad moves the focus to: a click presses A on it */
	_ui_mouse_target_item,
	/* a list showing one value at a time: a click on either half steps it
	that way */
	_ui_mouse_target_value,
	/* one of the items a list shows side by side (profiles, levels): a
	click selects its absolute item and presses A once */
	_ui_mouse_target_list_slot,
	_ui_mouse_target_carousel_peek,
	/* a button's icon and label in a screen's key: a click presses it */
	_ui_mouse_target_button,
	/* an action drawn outside the widget tree (the scores' QUIT and PICK GAME): it has no widget, and a click is
	kept for the code that drew it (ui_widget_port_postgame_pressed) instead of posted as a button */
	_ui_mouse_target_action
};

struct ui_mouse_target
{
	struct widget_instance *widget;
	rectangle2d bounds;
	short kind;
	short button_index;
	short back_arrow_end;
	short forward_arrow_start;
	rectangle2d value_bounds;
};

static struct ui_mouse_target ui_mouse_targets[UI_MOUSE_MAXIMUM_TARGETS];
static struct widget_instance *ui_slider_widget;
static long ui_slider_creation_time;
static struct halo_menu_slider ui_slider_geometry;
static int ui_slider_down_x, ui_slider_down_y, ui_slider_dragging;
static int ui_slider_was_held;
static int ui_slider_selection = -1, ui_slider_last_selection = -1;

static long ui_mouse_target_count = 0;
static boolean ui_mouse_noting_targets = FALSE;
/* port: a finger's reach (halo_ui_pointer.radius, menu units; 0 for a mouse), and
whether the menu has drawn since the pointer came: a click before the first draw
has no targets to find and would otherwise be the "nothing to pick" A */
static short ui_mouse_radius = 0;
static boolean ui_mouse_menu_drawn = FALSE;
/* port: touch BACK. A screen whose legend has a B button (BACK, CANCEL, LEAVE) shows a chevron beside
its title and answers a swipe in from the left edge with that B. The title's helmet picture places the
chevron; both are noted as the screen draws. */
static boolean ui_touch_back_seen = FALSE;
static boolean ui_touch_title_seen = FALSE;
/* port: B does something on this screen whether or not a legend button says so (see
ui_touch_screen_back_acts), so the chevron and swipe work on screens with no B legend widget */
static boolean ui_touch_back_generic = FALSE;
static rectangle2d ui_touch_title_icon;

/* port: whether this widget is the legend button of a screen-wide (global) bar action in the X slot. X is the
secondary action of the focused item only when the screen says so (select scope); a global X, like the
server browser's IP ADDRESS, acts on the screen, so it is neither a press-and-hold or right-click context
menu nor drawn with the mouse's right-button glyph (the legend is clicked) */
static boolean ui_widget_is_global_x_action(
	struct widget_instance *widget)
{
	struct halo_menu_meta const *meta;

	if (ui_mouse_key_button(widget) != _gamepad_analog_button_x)
		return FALSE;
	meta = pc_menu_meta(widget->definition_tag_index);
	return meta->role == HALO_MENU_ROLE_BAR_ACTION && meta->scope == HALO_MENU_SCOPE_GLOBAL;
}

/* whether the screen, or a child of it, carries a global X bar action */
static boolean ui_screen_has_global_x_action(
	struct widget_instance *widget,
	long depth)
{
	struct widget_instance *child;

	if (!widget || depth > 3)
		return FALSE;
	if (ui_widget_is_global_x_action(widget))
		return TRUE;
	for (child = widget->child; child; child = child->next)
		if (ui_screen_has_global_x_action(child, depth + 1))
			return TRUE;
	return FALSE;
}

/* port: the secondary action (X) of the screen's items. The screen's X legend button and its caption
(DELETE, RENAME) are noted as it draws; a press-and-hold on touch, or a right-click, opens a context
menu attached to the item: a panel with the item's name and one row per action, the row pressing X.
A destructive row is drawn in the HUD's red-orange with a trash glyph. */
#define UI_HOLD_GROW_START_MS 150
#define UI_HOLD_OPEN_MS 500
#define UI_HOLD_SCALE 1.05f
/* port: the context menu panel's corner radius, the menus' pill and button radius */
#define UI_POPUP_RADIUS 10
static boolean ui_touch_x_seen = FALSE;
static boolean ui_touch_x_destructive = FALSE;
/* the widget whose X handler deletes a gametype; its first child is the list of profile indices */
static struct widget_instance *ui_touch_x_playlist_widget = NULL;
static char ui_touch_x_caption[24];
static boolean ui_hold_active = FALSE;
static boolean ui_hold_light_done = FALSE;
static short ui_popup_pending = 0;
static short ui_popup_pending_x, ui_popup_pending_y;
static boolean ui_hold_focused = FALSE;
static long ui_hold_focus_frames = 0;
static boolean ui_hold_grows = FALSE;
static boolean ui_hold_shrinking = FALSE;
static unsigned long ui_hold_shrink_ms = 0;
static real ui_hold_shrink_from = 0.0f;
#define UI_HOLD_SHRINK_MS 100
static boolean ui_hold_swallow = FALSE;
static unsigned long ui_hold_start_ms = 0;
static rectangle2d ui_hold_bounds;
/* the held item, and where its card art was last drawn (before growing) */
static struct widget_instance *ui_hold_widget = NULL;
static rectangle2d ui_hold_drawn;
static boolean ui_hold_drawn_valid = FALSE;
static boolean ui_popup_open = FALSE;
static rectangle2d ui_popup_anchor;
static boolean ui_popup_anchor_is_item = FALSE;
#define UI_POPUP_MAXIMUM_ROWS 4
#define UI_POPUP_ROW_HEIGHT 54
/* the menu's rows as last drawn, the row under the finger or pointer, and the panel */
static rectangle2d ui_popup_rows[UI_POPUP_MAXIMUM_ROWS];
static short ui_popup_row_count = 0;
static short ui_popup_highlight = -1;
static rectangle2d ui_popup_panel;
static boolean ui_popup_panel_valid = FALSE;
static boolean ui_popup_prev_down = FALSE;
/* the item the menu belongs to, for its name */
static struct widget_instance *ui_popup_widget = NULL;

/* The presses the mouse makes, posted one a frame: the event queue keeps
only the latest event posted between two frames (queue_event). What they
lead to (the focus, a list's position) is only known once they have been
processed, so the pointer's next hover or click waits for them. */
#define UI_MOUSE_MAXIMUM_PRESSES 16

static short ui_mouse_presses[UI_MOUSE_MAXIMUM_PRESSES];
static long ui_mouse_press_count = 0;
static boolean ui_mouse_hover_pending = FALSE;
static boolean ui_mouse_click_pending = FALSE;
static short ui_mouse_hover_x, ui_mouse_hover_y;
static boolean ui_mouse_hover_dragging;
static short ui_mouse_click_x, ui_mouse_click_y;
/* port: the touch legend button a tap just hit, drawn brighter for a few frames */
static struct widget_instance *ui_touch_legend_pressed = NULL;
static short ui_touch_legend_pressed_frames = 0;

/* A screen that goes away (Save and Quit, restart, revert) leaves the
targets its last draw noted, and the pointer's pending hover, click and
presses, for the next frame's mouse handling: against widgets that are freed
and a pool the next screen already uses. */
static void ui_mouse_forget_targets(
	void)
{
	ui_slider_widget = NULL;
	ui_slider_selection = -1;
	ui_slider_was_held = FALSE;
	ui_dropdown.open = FALSE;
	ui_dropdown_widget = NULL;
	ui_dropdown_selection = -1;
	ui_mouse_target_count = 0;
	ui_mouse_press_count = 0;
	ui_mouse_hover_pending = FALSE;
	ui_mouse_click_pending = FALSE;
	ui_touch_back_seen = FALSE;
	ui_touch_back_generic = FALSE;
	ui_touch_title_seen = FALSE;
	ui_touch_x_seen = FALSE;
	ui_hold_active = FALSE;
	ui_hold_swallow = FALSE;
	ui_popup_open = FALSE;
	ui_popup_pending = 0;
	ui_popup_row_count = 0;
	ui_popup_highlight = -1;
	ui_popup_panel_valid = FALSE;
	ui_popup_widget = NULL;

	return;
}

static void ui_mouse_press(
	short button_index)
{
	if (ui_mouse_press_count < UI_MOUSE_MAXIMUM_PRESSES)
		ui_mouse_presses[ui_mouse_press_count++] = button_index;

	return;
}

/* the widget's place among its parent's children, or NONE */
static long ui_mouse_child_index(
	struct widget_instance *widget)
{
	struct widget_instance *child;
	long index = 0;

	for (child = widget->parent ? widget->parent->child : NULL; child; child = child->next, index++)
	{
		if (child == widget)
			return index;
	}

	return NONE;
}

/* a list that shows several of its items at once */
static boolean ui_mouse_list_shows_several(
	struct widget_instance *widget)
{
	return widget->type == _ui_widget_type_spinner_list &&
		ui_widget_definition_get(widget->definition_tag_index)->child_widgets.count > 1;
}

/* the button a screen key's icon stands for ("a_butn" and so on), or NONE */
static short ui_mouse_key_button(
	struct widget_instance *widget)
{
	static struct
	{
		char const *name;
		short button_index;
	} const keys[] =
	{
		{ "a_butn", _gamepad_analog_button_a },
		{ "b_butn", _gamepad_analog_button_b },
		{ "x_butn", _gamepad_analog_button_x },
		{ "y_butn", _gamepad_analog_button_y },
		{ "black_butn", _gamepad_analog_button_black },
		{ "white_butn", _gamepad_analog_button_white },
		{ "left_thumb_butn", _gamepad_binary_button_left_thumb },
		{ "start_butn", _gamepad_binary_button_start },
		{ "back_butn", _gamepad_binary_button_back },
	};
	char const *name = tag_get_name(widget->definition_tag_index);
	char const *leaf = name ? strrchr(name, '\\') : NULL;
	long key_index;

	leaf = leaf ? leaf + 1 : name;
	for (key_index = 0; leaf && key_index < NUMBEROF(keys); key_index++)
	{
		long length = (long)strlen(keys[key_index].name);

		/* and the smaller icons of the dialogs, "a_butn_sm" */
		if (!strncmp(leaf, keys[key_index].name, length) &&
			(leaf[length] == '\0' || leaf[length] == '_'))
		{
			return keys[key_index].button_index;
		}
	}

	return NONE;
}

/* whether the d-pad moves the focus to a widget: an item of a column list,
or a child its parent tabs through */
static boolean ui_mouse_widget_is_item(
	struct widget_instance *widget)
{
	struct widget_instance *parent = widget->parent;
	struct ui_widget_definition *definition;
	long index;

	if (!parent || parent->type == _ui_widget_type_spinner_list)
		return FALSE;
	index = ui_mouse_child_index(widget);
	if (index == NONE || !widget_instance_can_receive_events(widget))
		return FALSE;
	if (parent->type == _ui_widget_type_column_list)
	{
		return !parent->parameters.list.list_items ||
			index < parent->parameters.list.number_of_items;
	}
	definition = ui_widget_definition_get(parent->definition_tag_index);
	if (!TEST_FLAG(definition->flags, _widget_dpad_updown_tabs_thru_children_bit) &&
		!TEST_FLAG(definition->flags, _widget_dpad_leftright_tabs_thru_children_bit))
	{
		return FALSE;
	}
	definition = ui_widget_definition_get(widget->definition_tag_index);

	return definition->event_handlers.count > 0 ||
		TEST_FLAG(definition->flags, _widget_pass_unhandled_events_to_children_bit) ||
		widget->type == _ui_widget_type_spinner_list ||
		widget->type == _ui_widget_type_column_list;
}

static void ui_mouse_list_directions(
	struct widget_instance *widget,
	short *back,
	short *forward);

/* port: the clipped portion of an adjacent item scrolls the strip; the
visible card supplies its full height rather than a small arrow hit area. */
static void ui_mouse_note_carousel_peeks(
	struct widget_instance *list,
	point2d offset)
{
	struct ui_carousel_state *state = ui_carousel_state(list);
	struct widget_instance *child = list->child;
	int count = list->parameters.list.number_of_items;
	int begin, end, item;
	if (!child || count <= 3)
		return;
	halo_ui_carousel_visible_range(state->drag.offset / HALO_UI_CAROUSEL_PITCH, count, &begin, &end);
	for (item = begin; item < end; item++)
	{
		float left = halo_ui_carousel_item_x(item, state->drag.offset / HALO_UI_CAROUSEL_PITCH, count);
		int side = left >= state->frame.x0;
		struct ui_mouse_target *target;
		rectangle2d bounds = ui_widget_definition_get(child->definition_tag_index)->bounds;
		if (left >= state->frame.x0 && left + HALO_UI_CAROUSEL_CARD_WIDTH <= state->frame.x1)
			continue;
		if (ui_mouse_target_count >= UI_MOUSE_MAXIMUM_TARGETS)
			return;
		bounds.x0 = (short)MAX(state->frame.x0, left);
		bounds.x1 = (short)MIN(state->frame.x1, left + HALO_UI_CAROUSEL_CARD_WIDTH);
		bounds.y0 += offset.y + child->vertical_offset;
		bounds.y1 += offset.y + child->vertical_offset;
		bounds.y0 = MAX(bounds.y0, state->frame.y0);
		bounds.y1 = MIN(bounds.y1, state->frame.y1);
		if (bounds.x0 >= bounds.x1 || bounds.y0 >= bounds.y1)
			continue;
		target = &ui_mouse_targets[ui_mouse_target_count++];
		target->widget = list;
		target->bounds = bounds;
		target->kind = _ui_mouse_target_carousel_peek;
		target->button_index = side ? 1 : -1;
	}
}

/* port: the width the legend's caption is drawn with, so its button can be
centered on the text instead of on the caption widget's full width. */
static short ui_mouse_legend_label_width(
	struct widget_instance *label,
	struct ui_widget_definition const *label_definition,
	short fallback)
{
	long old_font;
	short width;

	if (!label->parameters.text_box.text || label_definition->text_font.index == NONE)
		return fallback;
	old_font = draw_string_get_font();
	draw_string_set_font(label_definition->text_font.index);
	draw_string_set_format(NONE, 0, 0);
	draw_string_set_indents(0, 0);
	width = ui_widget_port_text_width(label->parameters.text_box.text);
	if (old_font != NONE)
		draw_string_set_font(old_font);

	return width > 0 ? width : fallback;
}

/* port: the touch legend's four fixed slots. Every Xbox-look screen's bottom bar
draws its A, B, Y and X buttons in the same four places, right to left (A, B, Y,
X), so a button never drifts from screen to screen and an unused slot stays empty.
The slots are the button bar's (menu_geometry.h: as wide as the widest caption each
ever holds, in the bar's caption font, plus padding). A button is one standard
element: its screen gives only the caption and the button, and the legend owns the
pill's place and size (the slot, or the caption's measured width plus the minimum
padding when a caption is wider than its slot) and centers the caption in it. */
#define UI_TOUCH_LEGEND_SLOTS 4
#define UI_TOUCH_LEGEND_GAP MENU_BAR_GAP /* between two slots: each pointer target takes half */
#define UI_TOUCH_LEGEND_MINIMUM_PADDING 12 /* a caption wider than its slot keeps this much on each side */
#define UI_TOUCH_LEGEND_BAR_Y 395 /* a bar's buttons are centered at or below this */
#define UI_TOUCH_LEGEND_BAR_TOP 404 /* the bar's top rule: a hit area never reaches above it */
#define UI_TOUCH_LEGEND_MINIMUM_POINTS 44.0f
#define UI_TOUCH_LEGEND_CAPTION_SHIFTS 16

void halo_screen_menu_visible_bottom(float *bottom_unit, float *points_per_unit);

static short ui_touch_legend_caption_shift_count;
static struct widget_instance *ui_touch_legend_caption_shift_widgets[UI_TOUCH_LEGEND_CAPTION_SHIFTS];
static short ui_touch_legend_caption_shifts[UI_TOUCH_LEGEND_CAPTION_SHIFTS];

/* port: how far the legend moved a caption to center it in its slot, 0 for any other text box */
static short ui_touch_legend_caption_shift(
	struct widget_instance *widget)
{
	short i;

	for (i = 0; i < ui_touch_legend_caption_shift_count; i++)
	{
		if (ui_touch_legend_caption_shift_widgets[i] == widget)
			return ui_touch_legend_caption_shifts[i];
	}
	return 0;
}

static void ui_touch_legend_note_caption_shift(
	struct widget_instance *widget,
	short shift)
{
	short i;

	for (i = 0; i < ui_touch_legend_caption_shift_count; i++)
	{
		if (ui_touch_legend_caption_shift_widgets[i] == widget)
		{
			ui_touch_legend_caption_shifts[i] = shift;
			return;
		}
	}
	if (i == UI_TOUCH_LEGEND_CAPTION_SHIFTS)
		i = ui_touch_legend_caption_shift_count = 0;
	ui_touch_legend_caption_shift_widgets[i] = widget;
	ui_touch_legend_caption_shifts[i] = shift;
	ui_touch_legend_caption_shift_count = MAX(ui_touch_legend_caption_shift_count, i + 1);
}

/* the slot (0 = A ... 3 = X) a legend button owns. Any other button of a bar (a fifth action such
as the lobby's SHARE on the left stick, or the original screens' START, BACK, BLACK and WHITE) takes B's
slot, the extra slot: a touch bar never draws B, whose BACK, CANCEL or LEAVE is the title's chevron */
static short ui_touch_legend_slot_for_button(
	short button)
{
	switch (button)
	{
	case _gamepad_analog_button_a: return 0;
	case _gamepad_analog_button_b: return 1;
	case _gamepad_analog_button_y: return 2;
	case _gamepad_analog_button_x: return 3;
	case NONE: return NONE;
	}
	return 1;
}

/* where a caption's text starts, from the caption's box and its justification (the text is
`text_width` wide, as ui_widget_port_text_width measures it, leading width included) */
static short ui_touch_legend_text_x0(
	struct ui_widget_definition const *label_definition,
	rectangle2d const *label_bounds,
	short text_width)
{
	short x0 = label_bounds->x0 + label_definition->horizontal_offset;

	switch (label_definition->justification)
	{
	case _text_justification_right:
		return label_bounds->x1 - text_width;
	case _text_justification_center:
		return x0 + (label_bounds->x1 - x0 - text_width) / 2;
	}
	return x0;
}

/* the slot's x range (0 = A ... 3 = X) */
static void ui_touch_legend_slot_bounds(
	short slot,
	short *x0,
	short *x1)
{
	static short const slots[UI_TOUCH_LEGEND_SLOTS][2] =
	{
		{ MENU_BAR_SLOT_A_X0, MENU_BAR_SLOT_A_X1 },
		{ MENU_BAR_SLOT_B_X0, MENU_BAR_SLOT_B_X1 },
		{ MENU_BAR_SLOT_Y_X0, MENU_BAR_SLOT_Y_X1 },
		{ MENU_BAR_SLOT_X_X0, MENU_BAR_SLOT_X_X1 },
	};

	*x0 = slots[slot][0];
	*x1 = slots[slot][1];
}

/* port: the touch legend's button. `drawn` is the visible button: 40 units tall, in
its slot (see above) for a bottom bar's A, B, Y and X, otherwise centered on the
caption's text as before; the caption is centered in it. `bounds` is the pointer
target. A slot's pointer target runs from the button's top (never above the bar's
top rule) down to the bottom edge of the screen, through the cropped backdrop margin
and the home indicator's inset, and is at least 44 pt tall there; sideways it stops
halfway to the neighboring slot. Other buttons get a target at least 60 units square. */
static boolean ui_mouse_legend_bounds(
	struct widget_instance *widget,
	struct ui_widget_definition const *definition,
	point2d offset,
	rectangle2d *bounds,
	rectangle2d *drawn,
	struct widget_instance **caption_out)
{
	struct widget_instance *parent = widget->parent;
	short button = ui_mouse_key_button(widget);
	short owner = widget->local_player_index == NONE ?
		local_player_index_for_draw_string_and_hack_in_icons : widget->local_player_index;
	struct widget_instance *sibling, *label = NULL;
	struct ui_widget_definition *label_definition = NULL;
	rectangle2d label_bounds;
	short best_distance = 17;

	*bounds = definition->bounds;
	bounds->x0 += offset.x;
	bounds->x1 += offset.x;
	bounds->y0 += offset.y;
	bounds->y1 += offset.y;
	for (sibling = parent ? parent->child : NULL; sibling; sibling = sibling->next)
	{
		struct ui_widget_definition *sibling_definition;
		rectangle2d sibling_bounds;
		short distance;

		if (sibling->type != _ui_widget_type_text_box || !sibling->visible)
			continue;
		sibling_definition = ui_widget_definition_get(sibling->definition_tag_index);
		sibling_bounds = sibling_definition->bounds;
		sibling_bounds.x0 += offset.x - widget->horizontal_offset + sibling->horizontal_offset;
		sibling_bounds.x1 += offset.x - widget->horizontal_offset + sibling->horizontal_offset;
		sibling_bounds.y0 += offset.y - widget->vertical_offset + sibling->vertical_offset;
		sibling_bounds.y1 += offset.y - widget->vertical_offset + sibling->vertical_offset;
		distance = (short)ABS(sibling_bounds.x0 - bounds->x1);
		if (distance < best_distance &&
			sibling_bounds.y0 < bounds->y1 && sibling_bounds.y1 > bounds->y0)
		{
			best_distance = distance;
			label_bounds = sibling_bounds;
			label = sibling;
			label_definition = sibling_definition;
		}
	}
	if (caption_out) *caption_out = best_distance < 17 ? label : NULL;

	if (best_distance >= 17 || button == NONE || !controller_glyph_hidden(owner, button))
	{
		if (best_distance < 17)
		{
			bounds->x1 = MAX(bounds->x1, label_bounds.x1);
			bounds->y0 = MIN(bounds->y0, label_bounds.y0);
			bounds->y1 = MAX(bounds->y1, label_bounds.y1);
		}
		return FALSE;
	}
	{
		short text_width = ui_mouse_legend_label_width(label, label_definition,
			label_bounds.x1 - label_bounds.x0);
		short center_x;
		short center_y = (bounds->y0 + bounds->y1) / 2;
		short slot = ui_touch_legend_slot_for_button(button);
		short width, hit_width;

		if (slot != NONE && center_y >= UI_TOUCH_LEGEND_BAR_Y && label_definition->text_font.index != NONE)
		{
			short x0, x1;
			float visible_bottom, points_per_unit, minimum_units;

			ui_touch_legend_slot_bounds(slot, &x0, &x1);
			center_x = (x0 + x1) / 2;
			/* a caption wider than its slot widens the pill, centered on the slot */
			if (text_width + 2 * UI_TOUCH_LEGEND_MINIMUM_PADDING > x1 - x0)
			{
				x0 = center_x - (text_width + 2 * UI_TOUCH_LEGEND_MINIMUM_PADDING) / 2;
				x1 = x0 + text_width + 2 * UI_TOUCH_LEGEND_MINIMUM_PADDING;
			}
			drawn->x0 = x0;
			drawn->x1 = x1;
			drawn->y0 = center_y - 15;
			drawn->y1 = drawn->y0 + 40;
			/* the caption is centered in the pill, wherever its screen put it and however it is justified */
			ui_touch_legend_note_caption_shift(label, center_x - text_width / 2 -
				ui_touch_legend_text_x0(label_definition, &label_bounds, text_width));
			halo_screen_menu_visible_bottom(&visible_bottom, &points_per_unit);
			minimum_units = UI_TOUCH_LEGEND_MINIMUM_POINTS / (points_per_unit > 0.0f ? points_per_unit : 0.78125f);
			bounds->x0 = x0 - UI_TOUCH_LEGEND_GAP / 2;
			bounds->x1 = x1 + UI_TOUCH_LEGEND_GAP / 2;
			bounds->y1 = (short)ceilf(visible_bottom);
			bounds->y0 = (short)floorf(visible_bottom - minimum_units);
			if (bounds->y0 > drawn->y0)
				bounds->y0 = drawn->y0;
			if (bounds->y0 < UI_TOUCH_LEGEND_BAR_TOP)
				bounds->y0 = UI_TOUCH_LEGEND_BAR_TOP;
			return TRUE;
		}
		center_x = label_bounds.x0 + text_width / 2;
		width = MAX(60, text_width + 40);
		hit_width = MAX(60, width + 12);

		drawn->x0 = center_x - width / 2;
		drawn->x1 = drawn->x0 + width;
		drawn->y0 = center_y - 15;
		drawn->y1 = drawn->y0 + 40;
		bounds->x0 = center_x - hit_width / 2;
		bounds->x1 = bounds->x0 + hit_width;
		bounds->y0 = center_y - 30;
		bounds->y1 = bounds->y0 + 60;
	}
	return TRUE;
}

/* port: on touch the bottom legend lists global actions only. SELECT, EDIT, BACK,
CANCEL, and LEAVE (the A and B buttons of a screen's legend) are not drawn: a tap acts
on the item directly and BACK is the title chevron or the edge swipe. The smaller dialog
buttons ("a_butn_sm") keep their own touch buttons. */
/* whether the text is exactly one of the words a commit page's A caption uses */
static boolean ui_text_is_commit_caption(wchar_t const *text)
{
	static char const *const words[] = { "ACCEPT", "OK", "DONE", "SAVE" };
	long word;

	if (!text) return FALSE;
	for (word = 0; word < 4; word++)
	{
		long i = 0;
		while (words[word][i] && text[i] &&
			(text[i] == (wchar_t)words[word][i] || text[i] == (wchar_t)(words[word][i] + 32))) i++;
		if (!words[word][i] && !text[i]) return TRUE;
	}
	return FALSE;
}

/* port: a commit page's A button (ACCEPT, OK, DONE) is the page's SAVE button on touch
and mouse. The chevron discards, so the page needs one visible commit button. */
static struct widget_instance *ui_legend_save_caption(struct widget_instance *widget)
{
	point2d zero = { 0, 0 };
	rectangle2d bounds, drawn;
	struct widget_instance *caption = NULL;

	if (ui_mouse_key_button(widget) != _gamepad_analog_button_a) return NULL;
	ui_mouse_legend_bounds(widget, ui_widget_definition_get(widget->definition_tag_index), zero, &bounds, &drawn, &caption);
	return caption && ui_text_is_commit_caption(caption->parameters.text_box.text) ? caption : NULL;
}

/* the button of a bar's slot (halo_menu_meta.h) */
static short ui_bar_slot_button(unsigned char slot)
{
	switch (slot)
	{
	case HALO_MENU_SLOT_X: return _gamepad_analog_button_x;
	case HALO_MENU_SLOT_Y: return _gamepad_analog_button_y;
	case HALO_MENU_SLOT_B: return _gamepad_analog_button_b;
	case HALO_MENU_SLOT_A: return _gamepad_analog_button_a;
	}
	return NONE;
}

/* the screen's bar widget (glyph or caption, a direct child) of an action on the button, or NULL */
static struct widget_instance *ui_bar_action_widget(
	struct widget_instance *screen,
	short button,
	boolean caption)
{
	struct widget_instance *child;

	for (child = screen ? screen->child : NULL; child; child = child->next)
	{
		struct halo_menu_meta const *meta = pc_menu_meta(child->definition_tag_index);

		if (meta->role == HALO_MENU_ROLE_BAR_ACTION && ui_bar_slot_button(meta->slot) == button &&
			(child->type == _ui_widget_type_text_box) == caption)
			return child;
	}
	return NULL;
}

static boolean ui_bar_key_is_letter(
	struct widget_instance *screen,
	struct event_record const *event)
{
	struct widget_instance *action;

	if (!event || event->type != _event_type_button || event->data.button.pointer)
		return FALSE;
	action = ui_bar_action_widget(screen, event->data.button.index, FALSE);
	return action && pc_menu_meta(action->definition_tag_index)->typing == HALO_MENU_TYPING_POINTER &&
		ui_widget_port_text_editing();
}

/* port: which of its captions a bar action shows (its strings=, halo_menus' <action>), by the action's button;
the one place the port changes a caption at run time */
void ui_widget_port_bar_caption(
	struct widget_instance *screen,
	short button,
	short index)
{
	struct widget_instance *caption = ui_bar_action_widget(screen, button, TRUE);

	if (caption)
		caption->parameters.text_box.string_list_index = index;
}

/* port: an action of a screen's bar shown dimmed while it cannot be used (its glyph and caption), by the action's button */
void ui_widget_port_bar_dim(
	struct widget_instance *screen,
	short button,
	boolean dim)
{
	struct widget_instance *caption = ui_bar_action_widget(screen, button, TRUE);
	struct widget_instance *glyph = ui_bar_action_widget(screen, button, FALSE);

	if (caption)
		caption->alpha_modifier = dim ? 0.45f : 1.0f;
	if (glyph)
		glyph->alpha_modifier = dim ? 0.45f : 1.0f;
}

static boolean ui_touch_legend_suppressed(struct widget_instance *widget)
{
	short button = ui_mouse_key_button(widget);
	struct halo_menu_meta const *meta = pc_menu_meta(widget->definition_tag_index);
	char const *name;
	char const *leaf;
	short owner;

	owner = widget->local_player_index == NONE ?
		local_player_index_for_draw_string_and_hack_in_icons : widget->local_player_index;
	/* a bar's action (the menus' data says its slot and scope): on touch, select is a tap on the item and back is
	the title's chevron, so neither is drawn; a screen-wide action is */
	if (meta->role == HALO_MENU_ROLE_BAR_ACTION)
		return (meta->scope == HALO_MENU_SCOPE_SELECT || meta->scope == HALO_MENU_SCOPE_BACK) &&
			controller_glyph_hidden(owner, ui_bar_slot_button(meta->slot)) != 0;
	if (button != _gamepad_analog_button_a && button != _gamepad_analog_button_b &&
		button != _gamepad_analog_button_x) return FALSE;
	name = tag_get_name(widget->definition_tag_index);
	leaf = name ? strrchr(name, '\\') : NULL;
	leaf = leaf ? leaf + 1 : name;
	if (!leaf || strstr(leaf, "_sm")) return FALSE;
	/* the lobby and the lobby preview keep their action buttons (global actions);
	LEAVE and BACK stay on the chevron */
	if (button != _gamepad_analog_button_b && widget->parent)
	{
		char const *parent_name = tag_get_name(widget->parent->definition_tag_index);
		if (parent_name && (strstr(parent_name, "lobby_button_") || strstr(parent_name, "preview_button_")))
			return FALSE;
	}
	if (button == _gamepad_analog_button_a && ui_legend_save_caption(widget)) return FALSE;
	return controller_glyph_hidden(owner, button) != 0;
}

/* whether a suppressed touch legend button owns this caption text box */
static boolean ui_touch_legend_caption_suppressed(struct widget_instance *caption, point2d offset)
{
	struct widget_instance *sibling;

	if (!caption->parent) return FALSE;
	for (sibling = caption->parent->child; sibling; sibling = sibling->next)
	{
		struct widget_instance *paired = NULL;
		rectangle2d bounds, drawn_unused;
		struct ui_widget_definition *sibling_definition;
		point2d sibling_offset = offset;
		if (sibling == caption || ui_mouse_key_button(sibling) == NONE || !ui_touch_legend_suppressed(sibling)) continue;
		sibling_definition = ui_widget_definition_get(sibling->definition_tag_index);
		/* offset is the caption's; both share the parent, so adjust by the offset difference */
		sibling_offset.x += sibling->horizontal_offset - caption->horizontal_offset;
		sibling_offset.y += sibling->vertical_offset - caption->vertical_offset;
		ui_mouse_legend_bounds(sibling, sibling_definition, sibling_offset, &bounds, &drawn_unused, &paired);
		if (paired == caption) return TRUE;
	}
	return FALSE;
}

/* port: the touch legend's button is the menus' own text button art
(`text_button_background`, the picture behind JOIN GAME and the lobby's bottom
bar): frame 0 at rest, frame 1 (the glowing border) while a tap is held. */
/* port: draws the part [u0, u1] (0..1 across the picture) of `bitmap` into `x0..x1` by `y0..y1`. */
static void ui_widget_draw_touch_legend_slice(
	struct bitmap_data *bitmap,
	real x0, real x1, real y0, real y1,
	real u0, real u1,
	pixel32 argb)
{
	struct rasterizer_dynamic_screen_geometry_parameters parameters;
	struct dynamic_screen_vertex vertices[NUMBER_OF_POINTS_PER_RECTANGLE];
	short i;

	for (i = 0; i < NUMBER_OF_POINTS_PER_RECTANGLE; i++)
	{
		vertices[i].color = argb;
		vertices[i].texture_coordinates.x = (i % 3) ? u1 : u0;
		vertices[i].texture_coordinates.y = (i > 1) ? 1.0f : 0.0f;
		vertices[i].position.x = (i % 3) ? x1 : x0;
		vertices[i].position.y = (i > 1) ? y1 : y0;
	}
	csmemset(&parameters, 0, sizeof(parameters));
	parameters.map_texture_scale[0].j = 1.0f;
	parameters.map_texture_scale[0].i = 1.0f;
	parameters.map_scale[0].j = 1.0f;
	parameters.map_scale[0].i = 1.0f;
	parameters.map[0] = bitmap;
	parameters.meter_parameters = NULL;
	parameters.point_sampled = FALSE;
	parameters.alpha_weighted = FALSE;
	parameters.framebuffer_blend_function = 0;
	rasterizer_psuedo_dynamic_screen_quad_draw(&parameters, vertices);
}

/* port: the touch legend's button is `text_button_background` in three slices: the end caps keep their
shape (the picture's height is drawn at the button's height) and only the middle stretches. */
static void ui_widget_draw_touch_legend(rectangle2d const *bounds, rectangle2d const *clip, real alpha, boolean pressed)
{
	long tag_index = tag_loaded(BITMAP_GROUP_TAG, "pc\\xbox\\bitmaps\\text_button_background");
	struct bitmap_data *bitmap = tag_index == NONE ? NULL :
		bitmap_group_get_bitmap_from_sequence(tag_index, 0, pressed ? 1 : 0);
	pixel32 argb = (fast_ftol(alpha * 255.0f) << 24) | 0x00FFFFFF;
	real x0, x1, y0, y1, cap, cap_u;

	(void)clip;
	if (!bitmap)
		return;
	x0 = (real)bounds->x0;
	x1 = (real)bounds->x1;
	y0 = (real)bounds->y0;
	y1 = (real)bounds->y1;
	cap = (real)(y1 - y0) * 0.5f;
	cap_u = 0.5f * (real)bitmap->height / MAX(1.0f, (real)bitmap->width);
	if (cap * 2.0f > x1 - x0)
	{
		cap = (x1 - x0) * 0.5f;
		cap_u = 0.5f;
	}
	ui_widget_draw_touch_legend_slice(bitmap, x0, x0 + cap, y0, y1, 0.0f, cap_u, argb);
	if (x1 - x0 > cap * 2.0f)
		ui_widget_draw_touch_legend_slice(bitmap, x0 + cap, x1 - cap, y0, y1, cap_u, 1.0f - cap_u, argb);
	ui_widget_draw_touch_legend_slice(bitmap, x1 - cap, x1, y0, y1, 1.0f - cap_u, 1.0f, argb);
}

/* port: the scores after a game (game_engine.c, game_engine_post_rasterize_post_game) are not a widget screen: the
game draws "quit" and "pick game" as one string with button glyphs and reads the gamepads itself. On touch the
glyphs are hidden and nothing could be tapped, so the two actions are drawn here as the standard legend buttons, in
B's and A's slots, and noted as pointer targets; a tap is handed to the game through ui_widget_port_postgame_pressed.
Returns FALSE when the game should draw its own prompt (a controller, or keyboard and mouse). */
static struct widget_instance *ui_mouse_menu(void);
static short ui_postgame_pending = NONE;
static short ui_postgame_pressed = NONE;
static short ui_postgame_pressed_frames = 0;

boolean ui_widget_port_postgame_legend(
	long font_index,
	real alpha,
	boolean pick_game)
{
	static wchar_t const *const captions[2] = { L"PICK GAME", L"QUIT" };
	static short const buttons[2] = { _gamepad_analog_button_a, _gamepad_analog_button_b };
	real_argb_color color = { 1.0f, 1.0f, 1.0f, 1.0f };
	short index;

	if (halo_menu_pointer_controls() != 2 || font_index == NONE)
		return FALSE;
	/* (a menu over the scores, a dialog or the pause menu, takes the pointer: no buttons under it) */
	if (ui_mouse_menu())
	{
		ui_postgame_pending = NONE;
		return FALSE;
	}
	color.alpha = alpha;
	for (index = pick_game ? 0 : 1; index < 2; index++)
	{
		rectangle2d drawn, text_bounds;
		short x0, x1, text_width, center_x;
		boolean pressed = ui_postgame_pressed == buttons[index] && ui_postgame_pressed_frames > 0;

		ui_touch_legend_slot_bounds(index, &x0, &x1);
		draw_string_set_font(font_index);
		draw_string_set_format(NONE, 0, 0);
		draw_string_set_indents(0, 0);
		text_width = ui_widget_port_text_width(captions[index]);
		center_x = (short)((x0 + x1) / 2);
		if (text_width + 2 * UI_TOUCH_LEGEND_MINIMUM_PADDING > x1 - x0)
		{
			x0 = (short)(center_x - (text_width + 2 * UI_TOUCH_LEGEND_MINIMUM_PADDING) / 2);
			x1 = (short)(x0 + text_width + 2 * UI_TOUCH_LEGEND_MINIMUM_PADDING);
		}
		drawn.x0 = x0;
		drawn.x1 = x1;
		drawn.y0 = MENU_LEGEND_Y - 2;
		drawn.y1 = drawn.y0 + 40;
		ui_widget_draw_touch_legend(&drawn, NULL, alpha, pressed);
		text_bounds = drawn;
		text_bounds.x0 = (short)(center_x - text_width / 2);
		text_bounds.y0 = (short)(drawn.y0 + 9);
		draw_string_set_draw_mode(font_index, NONE, 0, 0, &color);
		rasterizer_draw_unicode_string(&text_bounds, NULL, NULL, 0, captions[index]);
		if (ui_mouse_target_count < UI_MOUSE_MAXIMUM_TARGETS)
		{
			struct ui_mouse_target *target = &ui_mouse_targets[ui_mouse_target_count++];

			csmemset(target, 0, sizeof(*target));
			target->widget = NULL;
			target->kind = _ui_mouse_target_action;
			target->button_index = buttons[index];
			target->bounds = drawn;
			/* (at least the 60 units a finger gets on the menus' buttons) */
			target->bounds.y0 -= 10;
			target->bounds.y1 += 10;
			target->bounds.x0 -= UI_TOUCH_LEGEND_GAP / 2;
			target->bounds.x1 += UI_TOUCH_LEGEND_GAP / 2;
		}
	}
	/* (a press the game did not take while its button showed pressed is dropped, so it cannot fire at the next
	scores) */
	if (ui_postgame_pressed_frames > 0 && --ui_postgame_pressed_frames == 0)
		ui_postgame_pending = NONE;
	return TRUE;
}

/* whether the button (A: pick game, B: quit) was tapped on the scores since the last call for it */
boolean ui_widget_port_postgame_pressed(
	short button)
{
	if (ui_postgame_pending != button)
		return FALSE;
	ui_postgame_pending = NONE;
	return TRUE;
}

/* port: whether B does something on this screen: its tag has a B or back handler, or has none and the
default go-back has a previous screen to return to (the root screens have neither, so they get no
chevron). Dialogs answer their own B and never take the chevron. */
static boolean ui_touch_screen_back_acts(
	struct widget_instance *widget)
{
	struct ui_widget_definition *definition = ui_widget_definition_get(widget->definition_tag_index);
	short widget_stack = (widget->local_player_index == NONE) ? 0 : widget->local_player_index;
	char const *name = tag_get_name(widget->definition_tag_index);
	long handler_index;

	if (!definition)
		return FALSE;
	if (name && (strstr(name, "dialog") || strstr(name, "error")))
		return FALSE;
	if (name && strstr(name, "main_menu\\main_menu") && !strstr(name, "main_menu\\main_menu\\"))
		return FALSE;
	for (handler_index = 0; handler_index < definition->event_handlers.count; handler_index++)
	{
		struct ui_widget_event_handler_reference *handler =
			(struct ui_widget_event_handler_reference *)definition->event_handlers.address +
			handler_index;

		if (handler->event_type == _widget_event_b_button ||
			handler->event_type == _widget_event_back_button)
			return TRUE;
	}
	return widget_globals.widget_stack[widget_stack] != NULL;
}

/* port: the screen's own secondary (X) action, read from its event handlers so touch (which draws no legends) knows
whether press-and-hold has a popup to open; a handler whose function name says delete is the destructive kind */
extern char const *ui_widget_event_handler_function_name(long function_index);
extern char const *pc_menu_function_name(long function_index);

static boolean ui_touch_widget_x_scan(
	struct widget_instance *widget)
{
	struct ui_widget_definition *definition = ui_widget_definition_get(widget->definition_tag_index);
	long handler_index;

	if (!definition)
		return FALSE;
	for (handler_index = 0; handler_index < definition->event_handlers.count; handler_index++)
	{
		struct ui_widget_event_handler_reference *handler =
			(struct ui_widget_event_handler_reference *)definition->event_handlers.address +
			handler_index;
		char const *function_name;
		long function_index;

		if (handler->event_type != _gamepad_analog_button_x)
			continue;
		function_index = (long)(unsigned short)handler->function;
		function_name = function_index >= PC_MENU_FUNCTION_BASE && function_index < 0x8000 ?
			pc_menu_function_name(function_index - PC_MENU_FUNCTION_BASE) :
			ui_widget_event_handler_function_name(function_index);
		ui_touch_x_seen = TRUE;
		ui_touch_x_destructive = function_name &&
			(strstr(function_name, "delete") != NULL || strstr(function_name, " del ") != NULL);
		ui_touch_x_playlist_widget = function_name && strstr(function_name, "del playlist profile") ?
			widget : NULL;
		strcpy(ui_touch_x_caption, ui_touch_x_destructive ? "DELETE" : "OPTIONS");
		halo_controller_glyph_x_destructive(ui_touch_x_destructive);
		return TRUE;
	}
	return FALSE;
}

/* FALSE when the focused gametype is a built-in (locked): it can not be deleted, so the hold has nothing to offer */
static boolean ui_touch_x_row_deletable(void)
{
	struct widget_instance *list;

	if (!ui_touch_x_playlist_widget || !(list = ui_touch_x_playlist_widget->child) ||
		!list->parameters.list.list_items ||
		(unsigned short)list->parameters.list.selected_index >= list->parameters.list.number_of_items)
		return TRUE;
	return !(((long *)list->parameters.list.list_items)[list->parameters.list.selected_index] & 0x40000000);
}

/* the screen's own handlers first, then those of the focused child chain: a list widget (LOAD GAME's saves)
carries the X handler for the item held, not the screen */
static void ui_touch_screen_x_scan(
	struct widget_instance *widget)
{
	long depth;

	if (ui_screen_has_global_x_action(widget, 0))
		return;
	for (depth = 0; widget && depth < 8; depth++, widget = widget->focused_child)
		if (ui_touch_widget_x_scan(widget))
			return;
}

/* port: the leftmost menu column a control at the left edge of a clip from
column x0 may use: clear of a camera cutout or rounded corner, in the menu
window's own units under its safe-area zoom (halo_screen_menu_screen_rect) */
static long ui_widget_safe_left(long x0)
{
	struct halo_menu_screen screen;

	halo_screen_menu_screen_rect(TRUE, &screen);
	return MAX(x0, screen.safe_x0);
}

/* port: the touch BACK chevron: left of the title's helmet, or top left of the screen when it has none, in the HUD blue of the touch legend,
with a tap target at least as tall as input.ui_touch_target */
static void ui_touch_back_chevron_draw(
	struct widget_instance *widget,
	rectangle2d const *clip)
{
	long arm = 10, thickness = 4, index;
	long reach = MAX(ui_redraw_touch_target_units(), 2 * arm + 12);
	long cy, cx;
	long safe_left = ui_widget_safe_left(clip->x0);
	rectangle2d hit;

	if (!(ui_touch_back_seen || ui_touch_back_generic) || ui_mouse_radius <= 0 ||
		ui_mouse_target_count >= UI_MOUSE_MAXIMUM_TARGETS)
		return;
	/* drawn in the menu window, like its tap target, the title's helmet it sits beside and the
	safe edge it keeps clear of (ui_widget_safe_left): all in the window's units */
	halo_screen_menu_window(1);
	if (ui_touch_title_seen)
	{
		/* (the title group itself moves clear of the safe inset, menu_title_touch_shift) */
		cy = (ui_touch_title_icon.y0 + ui_touch_title_icon.y1) / 2;
		cx = MAX(ui_touch_title_icon.x0 - reach / 2 - 14, reach / 2 + 2);
	}
	else
	{
		struct halo_menu_screen screen;

		halo_screen_menu_screen_rect(TRUE, &screen);
		cy = MAX(clip->y0, screen.safe_y0) + reach / 2 + 4;
		cx = clip->x0 + reach / 2 + 4;
		/* never under a camera cutout or rounded corner: the menu clip can run wider than the screen's safe area */
		cx = MAX(cx, safe_left + reach / 2 + 2);
	}
	for (index = -arm; index <= arm; index++)
	{
		rectangle2d row;

		row.y0 = (short)(cy + index);
		row.y1 = (short)(row.y0 + 1);
		row.x0 = (short)(cx - arm / 2 + ABS(index));
		row.x1 = (short)(row.x0 + thickness);
		draw_quad(&row, 0xff2996ff);
	}
	hit.x0 = (short)MAX(cx - reach / 2, safe_left);
	hit.x1 = (short)(cx + reach / 2);
	hit.y0 = (short)(cy - reach / 2);
	hit.y1 = (short)(cy + reach / 2);
	ui_mouse_targets[ui_mouse_target_count].widget = widget;
	ui_mouse_targets[ui_mouse_target_count].bounds = hit;
	ui_mouse_targets[ui_mouse_target_count].kind = _ui_mouse_target_button;
	ui_mouse_targets[ui_mouse_target_count].button_index = _gamepad_analog_button_b;
	ui_mouse_target_count++;
	halo_screen_menu_window(0);
}

/* port: the press-and-hold feedback and the context menu, drawn over the screen: the held item's outline
grows to 1.05x between 0.15 s and 0.5 s; the menu, attached to the item, holds it at 1.05x */
/* how far the held item has grown, 0 to 1: it starts once the item has taken focus
and kept a secondary action, eases in, stays full while its menu is open, and eases
back out when the finger lifts early */
static real ui_hold_progress(void)
{
	real progress = 0.0f;

	if (ui_hold_shrinking)
	{
		long elapsed = (long)(system_milliseconds() - ui_hold_shrink_ms);

		if (elapsed >= UI_HOLD_SHRINK_MS)
			ui_hold_shrinking = FALSE;
	}
	if (ui_popup_open && ui_popup_anchor_is_item)
		progress = 1.0f;
	else if (ui_popup_open)
		progress = 0.0f;
	else if (ui_hold_active && ui_hold_grows)
	{
		long elapsed = (long)(system_milliseconds() - ui_hold_start_ms);

		if (elapsed > UI_HOLD_GROW_START_MS)
		{
			progress = (real)(elapsed - UI_HOLD_GROW_START_MS) / (UI_HOLD_OPEN_MS - UI_HOLD_GROW_START_MS);
			progress = MIN(progress, 1.0f);
			progress *= progress;
		}
		ui_hold_shrink_from = progress;
	}
	else if (ui_hold_shrinking)
	{
		long elapsed = (long)(system_milliseconds() - ui_hold_shrink_ms);

		progress = ui_hold_shrink_from * (1.0f - (real)elapsed / UI_HOLD_SHRINK_MS);
		progress = MAX(progress, 0.0f);
	}

	return progress;
}

/* port: the held item's card art grows in place about its center, like a held Home
Screen icon (the art is stretched over the larger rectangle; the item's text stays) */
static void ui_hold_grow_card(
	struct widget_instance *widget,
	rectangle2d *bounds)
{
	real progress, scale;
	long extent_x, extent_y;

	if (!ui_hold_widget || widget != ui_hold_widget)
		return;
	ui_hold_drawn = *bounds;
	ui_hold_drawn_valid = TRUE;
	progress = ui_hold_progress();
	if (progress <= 0.0f)
		return;
	scale = 1.0f + (UI_HOLD_SCALE - 1.0f) * progress;
	extent_x = (long)((bounds->x1 - bounds->x0) * (scale - 1.0f) / 2.0f + 0.5f);
	extent_y = (long)((bounds->y1 - bounds->y0) * (scale - 1.0f) / 2.0f + 0.5f);
	bounds->x0 = (short)(bounds->x0 - extent_x);
	bounds->x1 = (short)(bounds->x1 + extent_x);
	bounds->y0 = (short)(bounds->y0 - extent_y);
	bounds->y1 = (short)(bounds->y1 + extent_y);

	return;
}

/* port: the first nonempty text in a widget's tree, the item's name for its context menu */
static wchar_t const *ui_popup_widget_name(
	struct widget_instance *widget,
	short depth)
{
	struct widget_instance *child;

	if (!widget || depth > 4)
		return NULL;
	if (widget->type == _ui_widget_type_text_box && widget->parameters.text_box.text &&
		widget->parameters.text_box.text[0])
		return widget->parameters.text_box.text;
	for (child = widget->child; child; child = child->next)
	{
		wchar_t const *name = ui_popup_widget_name(child, (short)(depth + 1));

		if (name)
			return name;
	}

	return NULL;
}

/* port: how far a rounded corner of the given radius sits in from the box edge on the
row dy units from the top (or bottom) edge */
static short ui_popup_corner_inset(
	short radius,
	short dy)
{
	real d, inset;

	if (dy >= radius)
		return 0;
	d = (real)radius - (real)dy - 0.5f;
	inset = (real)radius - (real)sqrt((real)radius * (real)radius - d * d);

	return (short)(inset + 0.5f);
}

static short ui_popup_clamp_radius(
	rectangle2d const *box,
	short radius)
{
	short limit = (short)(MIN(box->x1 - box->x0, box->y1 - box->y0) / 2);

	return (short)MAX(0, MIN(radius, limit));
}

/* a rounded rectangle, the same corner radius as the menus' pills and buttons, filled
with non-overlapping bands so a translucent color stays even */
static void ui_popup_rounded_fill(
	rectangle2d const *box,
	short radius,
	unsigned long color)
{
	rectangle2d band;
	short dy, inset;

	radius = ui_popup_clamp_radius(box, radius);
	for (dy = 0; dy < radius; dy++)
	{
		inset = ui_popup_corner_inset(radius, dy);
		band.x0 = (short)(box->x0 + inset);
		band.x1 = (short)(box->x1 - inset);
		band.y0 = (short)(box->y0 + dy);
		band.y1 = (short)(band.y0 + 1);
		draw_quad(&band, color);
		band.y0 = (short)(box->y1 - 1 - dy);
		band.y1 = (short)(band.y0 + 1);
		draw_quad(&band, color);
	}
	band = *box;
	band.y0 = (short)(box->y0 + radius);
	band.y1 = (short)(box->y1 - radius);
	if (band.y1 > band.y0)
		draw_quad(&band, color);
}

/* a one-unit outline of ui_popup_rounded_fill's shape */
static void ui_popup_rounded_outline(
	rectangle2d const *box,
	short radius,
	unsigned long color)
{
	rectangle2d line;
	short dy, inset, reach;

	radius = ui_popup_clamp_radius(box, radius);
	for (dy = 0; dy < radius; dy++)
	{
		inset = ui_popup_corner_inset(radius, dy);
		if (dy == 0)
		{
			line.x0 = (short)(box->x0 + inset);
			line.x1 = (short)(box->x1 - inset);
			line.y0 = box->y0;
			line.y1 = (short)(box->y0 + 1);
			draw_quad(&line, color);
			line.y0 = (short)(box->y1 - 1);
			line.y1 = box->y1;
			draw_quad(&line, color);
			continue;
		}
		reach = (short)MAX(inset + 1, ui_popup_corner_inset(radius, (short)(dy - 1)));
		/* left and right arcs, top and bottom */
		line.x0 = (short)(box->x0 + inset);
		line.x1 = (short)(box->x0 + reach);
		line.y0 = (short)(box->y0 + dy);
		line.y1 = (short)(line.y0 + 1);
		draw_quad(&line, color);
		line.y0 = (short)(box->y1 - 1 - dy);
		line.y1 = (short)(line.y0 + 1);
		draw_quad(&line, color);
		line.x0 = (short)(box->x1 - reach);
		line.x1 = (short)(box->x1 - inset);
		draw_quad(&line, color);
		line.y0 = (short)(box->y0 + dy);
		line.y1 = (short)(line.y0 + 1);
		draw_quad(&line, color);
	}
	line.y0 = (short)(box->y0 + MAX(radius, 1));
	line.y1 = (short)(box->y1 - MAX(radius, 1));
	if (line.y1 > line.y0)
	{
		line.x0 = box->x0;
		line.x1 = (short)(box->x0 + 1);
		draw_quad(&line, color);
		line.x0 = (short)(box->x1 - 1);
		line.x1 = box->x1;
		draw_quad(&line, color);
	}
}

/* a thin trash can, its top-left corner at (x, y), 14 by 16 units */
static void ui_popup_trash_glyph(
	short x,
	short y,
	short half_scale,
	unsigned long color)
{
	rectangle2d part;

#define UI_GLYPH(v) ((short)(((v) * half_scale) / 2))
	/* handle, lid, body sides, bottom and two inner ribs, drawn on a 14x16 grid scaled by half_scale/2 */
	part.x0 = (short)(x + UI_GLYPH(5)); part.x1 = (short)(x + UI_GLYPH(9)); part.y0 = y; part.y1 = (short)(y + UI_GLYPH(2));
	draw_quad(&part, color);
	part.x0 = x; part.x1 = (short)(x + UI_GLYPH(14)); part.y0 = (short)(y + UI_GLYPH(2)); part.y1 = (short)(y + UI_GLYPH(4));
	draw_quad(&part, color);
	part.x0 = (short)(x + UI_GLYPH(2)); part.x1 = (short)(x + UI_GLYPH(4)); part.y0 = (short)(y + UI_GLYPH(4)); part.y1 = (short)(y + UI_GLYPH(16));
	draw_quad(&part, color);
	part.x0 = (short)(x + UI_GLYPH(10)); part.x1 = (short)(x + UI_GLYPH(12));
	draw_quad(&part, color);
	part.x0 = (short)(x + UI_GLYPH(2)); part.x1 = (short)(x + UI_GLYPH(12)); part.y0 = (short)(y + UI_GLYPH(14)); part.y1 = (short)(y + UI_GLYPH(16));
	draw_quad(&part, color);
	part.x0 = (short)(x + UI_GLYPH(5)); part.x1 = (short)(x + UI_GLYPH(6)); part.y0 = (short)(y + UI_GLYPH(6)); part.y1 = (short)(y + UI_GLYPH(13));
	draw_quad(&part, color);
	part.x0 = (short)(x + UI_GLYPH(8)); part.x1 = (short)(x + UI_GLYPH(9));
	draw_quad(&part, color);
#undef UI_GLYPH
}

/* port: the held item's context menu, like a held Home Screen icon's: the rest of the
screen dims, the lifted item gets a glow and a shadow, and a panel attached to it (below,
above when there is no room, or beside a tall item, with a notch pointing at it) carries the item's
name and one full-width row per action. A destructive row is red-orange with a trash
glyph. A right-click opens the same panel at the pointer. */
#define UI_POPUP_CONTENT_TOP 74 /* below the screen title and its rule */

static void ui_hold_popup_draw(
	rectangle2d const *clip)
{
	unsigned long const hud_blue = 0xff2996ff;
	unsigned long const destructive_color = 0xffff5a1e;
	rectangle2d anchor, panel, part, row;
	real_argb_color color = {1.0f, 1.0f, 1.0f, 1.0f};
	wchar_t const *name;
	char name_text[32];
	long font_index, row_font_index;
	short width, height, header, row_index, length;
	short gap = 14;
	short top, bottom = 472;
	short safe_left, notch_x, notch_y = -1;
	short side = 0; /* 1: beside the item on its right, -1: on its left */
	boolean below = TRUE;

	(void)ui_hold_progress();
	if (!ui_popup_open)
	{
		ui_popup_row_count = 0;
		ui_popup_panel_valid = FALSE;
		return;
	}
	/* the header reads a step smaller than the rows, which use the list rows' own font */
	font_index = tag_loaded(FONT_GROUP_TAG, "ui\\small_ui");
	row_font_index = tag_loaded(FONT_GROUP_TAG, "ui\\large_ui");
	if (row_font_index == NONE)
		row_font_index = font_index;
	anchor = ui_popup_anchor;
	safe_left = (short)MAX(clip->x0 + 8, ui_widget_safe_left(clip->x0));
	top = 8;

	name = ui_popup_widget_name(ui_popup_widget, 0);
	length = 0;
	if (name)
	{
		for (; length < (short)sizeof(name_text) - 1 && name[length]; length++)
			name_text[length] = (name[length] >= 32 && name[length] < 127) ? (char)name[length] : '?';
	}
	name_text[length] = 0;
	header = (short)(length > 0 ? 30 : 0);

	width = 200;
	if (ui_popup_anchor_is_item)
		width = (short)MAX(width, MIN(anchor.x1 - anchor.x0, 320));
	height = (short)(header + UI_POPUP_ROW_HEIGHT + 4);

	if (ui_popup_anchor_is_item)
	{
		/* the rest of the screen dims around the lifted item: the whole screen, past the
		menu clip into the backdrop margins and safe-area insets (draw_quad does not clip) */
		rectangle2d screen = *clip;
		struct halo_menu_screen reach;

		halo_screen_menu_screen_rect(TRUE, &reach);
		screen.x0 = (short)MIN(clip->x0, reach.x0);
		screen.y0 = (short)MIN(clip->y0, reach.y0);
		screen.x1 = (short)MAX(clip->x1, reach.x1);
		screen.y1 = (short)MAX(clip->y1, reach.y1);
		part = screen;
		part.y1 = anchor.y0;
		draw_quad(&part, 0x99000000);
		part.y0 = anchor.y1;
		part.y1 = screen.y1;
		draw_quad(&part, 0x99000000);
		part.y0 = anchor.y0;
		part.y1 = anchor.y1;
		part.x1 = anchor.x0;
		draw_quad(&part, 0x99000000);
		part.x0 = anchor.x1;
		part.x1 = screen.x1;
		draw_quad(&part, 0x99000000);
		/* the lifted item itself draws its glow, on its own outline (ui_hold_glow_draw) */

		/* the panel stays inside the content area: never over the screen's title band
		or the legend band. Below the item first, then above it; when it fits neither
		(a tall card), beside it, top-aligned: right, or left when the right has no room */
		top = UI_POPUP_CONTENT_TOP;
		bottom = UI_TOUCH_LEGEND_BAR_TOP - 4;
		panel.x0 = anchor.x0;
		panel.y0 = (short)(anchor.y1 + gap);
		notch_x = (short)((anchor.x0 + anchor.x1) / 2);
		if (panel.y0 + height > bottom)
		{
			below = FALSE;
			panel.y0 = (short)(anchor.y0 - gap - height);
			if (panel.y0 < top)
			{
				width = 200;
				side = (anchor.x1 + gap + width <= clip->x1 - 8) ? 1 : -1;
				panel.x0 = side > 0 ? (short)(anchor.x1 + gap) : (short)(anchor.x0 - gap - width);
				panel.y0 = anchor.y0;
				notch_x = -1;
				notch_y = (short)(anchor.y0 + 28);
			}
		}
	}
	else
	{
		panel.x0 = anchor.x0;
		panel.y0 = (short)(anchor.y0 + 4);
		if (panel.y0 + height > bottom)
			panel.y0 = (short)(anchor.y0 - 4 - height);
		notch_x = -1;
	}
	if (side == 0)
		panel.x0 = (short)MAX(safe_left, MIN(panel.x0, clip->x1 - width - 8));
	panel.y0 = (short)MAX(top, MIN(panel.y0, bottom - height));
	panel.x1 = (short)(panel.x0 + width);
	panel.y1 = (short)(panel.y0 + height);
	ui_popup_panel = panel;
	ui_popup_panel_valid = TRUE;

	/* shadow, faint glow, fill and outline */
	part = panel;
	part.x0 = (short)(panel.x0 + 4);
	part.x1 = (short)(panel.x1 + 4);
	part.y0 = (short)(panel.y0 + 5);
	part.y1 = (short)(panel.y1 + 5);
	ui_popup_rounded_fill(&part, UI_POPUP_RADIUS, 0x60000000);
	/* a faint outer glow fading out from the outline, like the focused buttons */
	{
		static unsigned long const glow[] = {0x602996ff, 0x402996ff, 0x2a2996ff, 0x1a2996ff, 0x0e2996ff};
		short ring;

		for (ring = (short)(NUMBEROF(glow) - 1); ring >= 0; ring--)
		{
			part = panel;
			part.x0 = (short)(panel.x0 - 2 - ring);
			part.x1 = (short)(panel.x1 + 2 + ring);
			part.y0 = (short)(panel.y0 - 2 - ring);
			part.y1 = (short)(panel.y1 + 2 + ring);
			ui_popup_rounded_outline(&part, (short)(UI_POPUP_RADIUS + 2 + ring), glow[ring]);
		}
	}
	ui_popup_rounded_fill(&panel, UI_POPUP_RADIUS, 0xe0101c34);
	/* a bright two-unit HUD-blue outline */
	ui_popup_rounded_outline(&panel, UI_POPUP_RADIUS, hud_blue);
	part = panel;
	part.x0++; part.x1--; part.y0++; part.y1--;
	ui_popup_rounded_outline(&part, (short)(UI_POPUP_RADIUS - 1), hud_blue);

	/* the notch points at the item from the panel's near edge */
	if (notch_x >= 0)
	{
		short step;

		static short const notch_half[] = { 9, 7, 5, 4, 3, 2, 1 };
		short previous;

		notch_x = (short)MAX(panel.x0 + 16, MIN(notch_x, panel.x1 - 16));
		/* open the panel's two outline rows where the notch joins it */
		part.x0 = (short)(notch_x - notch_half[0] + 1);
		part.x1 = (short)(notch_x + notch_half[0] - 1);
		part.y0 = below ? panel.y0 : (short)(panel.y1 - 2);
		part.y1 = (short)(part.y0 + 2);
		draw_quad(&part, 0xf0101c34);
		previous = notch_half[0];
		for (step = 0; step < (short)NUMBEROF(notch_half); step++)
		{
			/* the first rows widen into a fillet, so the notch meets the panel's edge
			in a curve rather than a corner; each edge segment reaches back to the
			previous row's edge so the outline stays unbroken */
			short half = notch_half[step];

			part.x0 = (short)(notch_x - half);
			part.x1 = (short)(notch_x + half);
			if (below)
			{
				part.y0 = (short)(panel.y0 - 1 - step);
				part.y1 = (short)(part.y0 + 1);
			}
			else
			{
				part.y0 = (short)(panel.y1 + step);
				part.y1 = (short)(part.y0 + 1);
			}
			draw_quad(&part, 0xe0101c34);
			part.x0 = (short)(notch_x - previous);
			part.x1 = (short)(notch_x - half + 1);
			draw_quad(&part, hud_blue);
			part.x0 = (short)(notch_x + half - 1);
			part.x1 = (short)(notch_x + previous);
			draw_quad(&part, hud_blue);
			previous = half;
		}
	}
	else if (notch_y >= 0)
	{
		/* the same notch turned sideways, on the edge that faces the item */
		static short const notch_half[] = { 9, 7, 5, 4, 3, 2, 1 };
		short step, previous;

		notch_y = (short)MAX(panel.y0 + 16, MIN(notch_y, panel.y1 - 16));
		part.y0 = (short)(notch_y - notch_half[0] + 1);
		part.y1 = (short)(notch_y + notch_half[0] - 1);
		part.x0 = side > 0 ? panel.x0 : (short)(panel.x1 - 2);
		part.x1 = (short)(part.x0 + 2);
		draw_quad(&part, 0xf0101c34);
		previous = notch_half[0];
		for (step = 0; step < (short)NUMBEROF(notch_half); step++)
		{
			short half = notch_half[step];

			part.y0 = (short)(notch_y - half);
			part.y1 = (short)(notch_y + half);
			part.x0 = side > 0 ? (short)(panel.x0 - 1 - step) : (short)(panel.x1 + step);
			part.x1 = (short)(part.x0 + 1);
			draw_quad(&part, 0xe0101c34);
			part.y0 = (short)(notch_y - previous);
			part.y1 = (short)(notch_y - half + 1);
			draw_quad(&part, hud_blue);
			part.y0 = (short)(notch_y + half - 1);
			part.y1 = (short)(notch_y + previous);
			draw_quad(&part, hud_blue);
			previous = half;
		}
	}

	if (header > 0)
	{
		color.red = 0.16f;
		color.green = 0.59f;
		color.blue = 1.0f;
		draw_string_set_draw_mode(font_index, NONE, 0, 0, &color);
		part = panel;
		part.x0 = (short)(panel.x0 + 14);
		part.y0 = (short)(panel.y0 + 8);
		part.y1 = (short)(panel.y0 + header);
		rasterizer_draw_string(&part, clip, NULL, 0, name_text);
		part.x0 = (short)(panel.x0 + 8);
		part.x1 = (short)(panel.x1 - 8);
		part.y0 = (short)(panel.y0 + header);
		part.y1 = (short)(part.y0 + 1);
		draw_quad(&part, 0x502996ff);
	}

	/* one row today: the screen's secondary action (X) */
	ui_popup_row_count = 1;
	for (row_index = 0; row_index < ui_popup_row_count; row_index++)
	{
		unsigned long label_color = ui_touch_x_destructive ? destructive_color : hud_blue;

		row.x0 = (short)(panel.x0 + 2);
		row.x1 = (short)(panel.x1 - 2);
		row.y0 = (short)(panel.y0 + header + 2 + row_index * UI_POPUP_ROW_HEIGHT);
		row.y1 = (short)(row.y0 + UI_POPUP_ROW_HEIGHT);
		ui_popup_rows[row_index] = row;
		if (row_index == ui_popup_highlight)
		{
			part = row;
			part.x0 = (short)(row.x0 + 4);
			part.x1 = (short)(row.x1 - 4);
			part.y0 = (short)(row.y0 + 6);
			part.y1 = (short)(row.y1 - 6);
			ui_popup_rounded_fill(&part, 8, ui_touch_x_destructive ? 0x40ff5a1e : 0x402996ff);
			ui_popup_rounded_outline(&part, 8, ui_touch_x_destructive ? 0xa0ff5a1e : 0xa02996ff);
		}
		color.red = (real)((label_color >> 16) & 0xff) / 255.0f;
		color.green = (real)((label_color >> 8) & 0xff) / 255.0f;
		color.blue = (real)(label_color & 0xff) / 255.0f;
		draw_string_set_draw_mode(row_font_index, NONE, 0, 0, &color);
		part = row;
		part.x0 = (short)(row.x0 + 14);
		part.y0 = (short)(row.y0 + (UI_POPUP_ROW_HEIGHT - 22) / 2);
		rasterizer_draw_string(&part, clip, NULL, 0, ui_touch_x_caption);
		if (ui_touch_x_destructive)
			ui_popup_trash_glyph((short)(row.x1 - 36), (short)(row.y0 + (UI_POPUP_ROW_HEIGHT - 24) / 2), 3, label_color);
	}

	return;
}

static void ui_mouse_note_target(
	struct widget_instance *widget,
	struct ui_widget_definition const *definition,
	point2d offset)
{
	struct widget_instance *parent = widget->parent;
	struct ui_mouse_target *target;
	rectangle2d bounds = definition->bounds;
	short button_index;
	short kind;

	if (!ui_mouse_noting_targets ||
		ui_mouse_target_count >= UI_MOUSE_MAXIMUM_TARGETS ||
		widget->disabled)
	{
		return;
	}
	bounds.x0 += offset.x;
	bounds.x1 += offset.x;
	bounds.y0 += offset.y;
	bounds.y1 += offset.y;
	button_index = ui_mouse_key_button(widget);
	if (button_index == NONE && widget->name && !strcmp(widget->name, "scroll_up_arrow"))
		button_index = _widget_event_dpad_up;
	else if (button_index == NONE && widget->name && !strcmp(widget->name, "scroll_down_arrow"))
		button_index = _widget_event_dpad_down;
	if (button_index == _widget_event_dpad_up || button_index == _widget_event_dpad_down)
	{
		/* port: a list's scroll arrow (drawn only while rows are hidden that way): a tap steps the list that way */
		kind = _ui_mouse_target_button;
	}
	else if (button_index != NONE)
	{
		kind = _ui_mouse_target_button;
		rectangle2d drawn;
		ui_mouse_legend_bounds(widget, definition, offset, &bounds, &drawn, NULL);
	}
	else if (parent && ui_mouse_list_shows_several(parent))
	{
		if (parent->disabled ||
			ui_mouse_child_index(widget) == NONE ||
			!widget_instance_can_receive_events(parent))
		{
			return;
		}
		kind = _ui_mouse_target_list_slot;
	}
	else if (widget->type == _ui_widget_type_spinner_list)
	{
		/* a list showing several items is picked through them */
		if (parent && ui_mouse_list_shows_several(widget) && widget_instance_can_receive_events(widget))
			ui_mouse_note_carousel_peeks(widget, offset);
		if (!parent || ui_mouse_list_shows_several(widget) || !widget_instance_can_receive_events(widget))
			return;
		kind = _ui_mouse_target_value;
		/* port: retain the row band even before pointer activity, so the first
		tap after using a controller can land on the label. */
		if (ui_widget_row_control(widget).kind == HALO_ROW_TOGGLE || ui_widget_row_has_dropdown(widget))
		{
			struct ui_widget_definition *row = ui_widget_definition_get(parent->definition_tag_index);
			bounds.x0 = row->bounds.x0 + offset.x - widget->horizontal_offset;
			bounds.x1 = row->bounds.x1 + offset.x - widget->horizontal_offset;
			bounds.y0 = row->bounds.y0 + offset.y - widget->vertical_offset;
			bounds.y1 = row->bounds.y1 + offset.y - widget->vertical_offset;
		}
	}
	else if (ui_mouse_widget_is_item(widget))
	{
		kind = _ui_mouse_target_item;
	}
	else
	{
		return;
	}
	target = &ui_mouse_targets[ui_mouse_target_count++];
	target->widget = widget;
	target->bounds = bounds;
	target->kind = kind;
	target->button_index = button_index;
	target->back_arrow_end = definition->list_header_bounds.x1 + offset.x;
	target->forward_arrow_start = definition->list_footer_bounds.x0 + offset.x;
	target->value_bounds = definition->bounds;
	target->value_bounds.x0 += offset.x;
	target->value_bounds.x1 += offset.x;
	target->value_bounds.y0 += offset.y;
	target->value_bounds.y1 += offset.y;
	if (spinner_string_list_extra_count(definition->text_label_string_list.index))
	{
		target->value_bounds.x0 -= SPINNER_EXTRA_WIDTH;
		target->back_arrow_end -= SPINNER_EXTRA_WIDTH;
	}

	/* port: the pointer slider's wider value area and left arrow move together. */
	if (halo_menu_pointer_controls() && ui_widget_row_control(widget).kind == HALO_ROW_RANGE)
	{
		int padding = MAX(0, HALO_MENU_SLIDER_WIDTH - (target->value_bounds.x1 - target->value_bounds.x0));
		target->bounds.x0 = MIN(target->bounds.x0, target->value_bounds.x0 - padding - 10);
		target->back_arrow_end -= padding;
	}

	return;
}

/* the squared distance from a point to a rectangle, 0 inside it */
static long ui_mouse_distance_squared(
	short x,
	short y,
	rectangle2d const *bounds)
{
	long dx = x < bounds->x0 ? bounds->x0 - x : x >= bounds->x1 ? x - (bounds->x1 - 1) : 0;
	long dy = y < bounds->y0 ? bounds->y0 - y : y >= bounds->y1 ? y - (bounds->y1 - 1) : 0;

	return dx * dx + dy * dy;
}

/* the target drawn last (so on top, and the innermost) under a point; with a
radius (a finger), when none contains the point, the nearest within it */
static struct ui_mouse_target *ui_mouse_target_at(
	short x,
	short y)
{
	long index;
	struct ui_mouse_target *nearest = NULL;
	long nearest_distance = (long)ui_mouse_radius * ui_mouse_radius;

	for (index = ui_mouse_target_count - 1; index >= 0; index--)
	{
		rectangle2d const *bounds = &ui_mouse_targets[index].bounds;

		if (x >= bounds->x0 && x < bounds->x1 && y >= bounds->y0 && y < bounds->y1)
			return &ui_mouse_targets[index];
		if (ui_mouse_radius > 0)
		{
			long distance = ui_mouse_distance_squared(x, y, bounds);

			if (distance <= nearest_distance && (!nearest || distance < nearest_distance))
			{
				nearest = &ui_mouse_targets[index];
				nearest_distance = distance;
			}
		}
	}
	/* port: on an iPhone a tap between rows goes to the nearest row or
	button, each taken as tall as input.ui_touch_target around its center
	(port/linux/src/ui_redraw.c); the rows are drawn as they are */
	{
		long least = ui_redraw_touch_target_units();
		struct ui_mouse_target *nearest = NULL;
		long nearest_distance = 0;

		for (index = 0; least > 0 && index < ui_mouse_target_count; index++)
		{
			rectangle2d const *bounds = &ui_mouse_targets[index].bounds;
			long center = (bounds->y0 + bounds->y1) / 2;
			long half = MAX((long)(bounds->y1 - bounds->y0), least) / 2;
			long distance = ABS((long)y - center);

			if ((ui_mouse_targets[index].kind != _ui_mouse_target_item &&
					ui_mouse_targets[index].kind != _ui_mouse_target_button &&
					ui_mouse_targets[index].kind != _ui_mouse_target_action) ||
				x < bounds->x0 || x >= bounds->x1 || distance > half)
			{
				continue;
			}
			if (!nearest || distance < nearest_distance)
			{
				nearest = &ui_mouse_targets[index];
				nearest_distance = distance;
			}
		}
		if (nearest)
			return nearest;
	}

	return nearest;
}

static boolean ui_mouse_widget_has_focus(
	struct widget_instance *widget)
{
	for (; widget->parent; widget = widget->parent)
	{
		if (widget->parent->focused_child != widget)
			return FALSE;
	}

	return TRUE;
}

/* moves the focus to the widget as the d-pad would, with the selection of
the column lists on the way (widget_event_function_list_widget_goto_next_item) */
static void ui_mouse_give_focus(
	struct widget_instance *widget)
{
	struct widget_instance *ancestor;

	if (ui_mouse_widget_has_focus(widget))
		return;
	widget_instance_give_focus_directly(widget_instance_get_topmost_parent(widget), widget);
	for (ancestor = widget; ancestor->parent; ancestor = ancestor->parent)
	{
		if (ancestor->parent->type == _ui_widget_type_column_list)
			ancestor->parent->parameters.list.selected_index = (short)ui_mouse_child_index(ancestor);
	}
	ui_play_audio_feedback_sound(_ui_audio_feedback_cursor);

	return;
}

/* the d-pad buttons that step a widget back and forward */
static void ui_mouse_list_directions(
	struct widget_instance *widget,
	short *back,
	short *forward)
{
	struct ui_widget_definition *definition = ui_widget_definition_get(widget->definition_tag_index);

	if (TEST_FLAG(definition->flags, _widget_dpad_leftright_tabs_thru_list_items_bit) ||
		TEST_FLAG(definition->flags, _widget_dpad_leftright_tabs_thru_children_bit))
	{
		*back = _widget_event_dpad_left;
		*forward = _widget_event_dpad_right;
	}
	else
	{
		*back = _widget_event_dpad_up;
		*forward = _widget_event_dpad_down;
	}

	return;
}

/* steps a list that shows several items to the one shown in a slot, as
pressing the d-pad that many times would */
static void ui_mouse_step_list_to_slot(
	struct widget_instance *slot)
{
	struct widget_instance *list = slot->parent;
	long steps;
	short back, forward;

	ui_mouse_give_focus(list);
	if (!list->focused_child)
		return;
	steps = ui_mouse_child_index(slot) - ui_mouse_child_index(list->focused_child);
	ui_mouse_list_directions(list, &back, &forward);
	for (; steps > 0; steps--)
		ui_mouse_press(forward);
	for (; steps < 0; steps++)
		ui_mouse_press(back);

	return;
}

/* the wheel's vertical list, even when focus moved into its horizontal
button bar; the innermost horizontal list if there is no vertical one */
static struct widget_instance *ui_mouse_wheel_widget(
	struct widget_instance *root)
{
	struct widget_instance *result = NULL;
	struct widget_instance *vertical = NULL;
	struct widget_instance *widget;

	for (widget = root; widget; widget = widget->focused_child)
	{
		struct ui_widget_definition *definition = ui_widget_definition_get(widget->definition_tag_index);

		if (definition->flags & (FLAG(_widget_dpad_updown_tabs_thru_children_bit) |
			FLAG(_widget_dpad_updown_tabs_thru_list_items_bit)))
			vertical = widget;
		if (definition->flags & (FLAG(_widget_dpad_updown_tabs_thru_children_bit) |
			FLAG(_widget_dpad_leftright_tabs_thru_children_bit) |
			FLAG(_widget_dpad_updown_tabs_thru_list_items_bit) |
			FLAG(_widget_dpad_leftright_tabs_thru_list_items_bit)))
		{
			result = widget;
		}
	}

	return vertical ? vertical : result;
}

/* the menu the mouse drives: the first player's, or everyone's */
static struct widget_instance *ui_mouse_menu(
	void)
{
	long widget_index;

	for (widget_index = 0; widget_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; widget_index++)
	{
		struct widget_instance *widget = widget_globals.active_widgets[widget_index];

		if (widget && (widget->local_player_index == NONE || widget->local_player_index == 0))
			return widget;
	}

	return NULL;
}

static boolean ui_mouse_menus_active(
	void)
{
	if (widget_globals.initialization_thread || progress_bar_is_active())
		return FALSE;

	/* (and the scores after a game, which take A and B like the menus:
	game_engine_update_non_deterministic) */
	return virtual_keyboard_active() || ui_mouse_menu() != NULL || game_engine_showing_postgame();
}

/* the pointer's motion, clicks and wheel since the last frame, as the first
player's controller events */
/* port: a row of a list to choose from (the PC version's menus' lists of
gametypes, maps, profiles, levels and games: port/assets/menus) */
static boolean ui_mouse_selection_row(
	struct widget_instance *widget)
{
	return widget && widget->parent && pc_menu_tag(widget->parent->definition_tag_index) &&
		(!strncmp(widget->name, "list_item_", 10) || !strncmp(widget->name, "server_item_", 12));
}

/* port: a press the menus post from their updates (menu_functions.c: the
server browser's join, once its game is reached), posted where the mouse's
are: one posted while the widgets update or draw would be overwritten by the
next frame's events (queue_event keeps the latest) */
static short ui_widget_port_press_controller = NONE;
static short ui_widget_port_press_button;

void ui_widget_port_post_button(
	short controller_index,
	short button_index)
{
	ui_widget_port_press_controller = controller_index;
	ui_widget_port_press_button = button_index;

	return;
}

/* port: the previous draw must still contain this instance before it is read.
Deleting any widget clears the owner through ui_mouse_forget_targets(). */
static boolean ui_dropdown_owner_valid(void)
{
	long index;
	for (index = 0; index < ui_mouse_target_count; index++)
		if (ui_mouse_targets[index].widget == ui_dropdown_widget)
			return ui_dropdown_widget->creation_time == ui_dropdown_creation_time &&
				ui_widget_row_has_dropdown(ui_dropdown_widget);
	return FALSE;
}

/* port: legend line (4 units above the captions) plus a 2-unit gap. */
#define UI_DROPDOWN_LEGEND_CLEARANCE 6

static void ui_dropdown_open(struct ui_mouse_target *target)
{
	float visible_top, visible_bottom;
	int limit_top, limit_bottom;
	long index;

	halo_screen_menu_visible_bounds(&visible_top, &visible_bottom);
	limit_top = (int)ceilf(visible_top);
	limit_bottom = (int)floorf(visible_bottom);
	/* port: ui_mouse_note_target records each legend icon together with its
	caption in screen menu units (414 at the top of the Xbox settings bar).
	Use those drawn bounds so scroll arrows and carousel buttons do not count.
	The bar's top line sits at 410, 4 units above the captions; stop the panel
	2 units above that line so its border never touches it. */
	for (index = 0; index < ui_mouse_target_count; index++)
		if (ui_mouse_targets[index].kind == _ui_mouse_target_button &&
			ui_mouse_targets[index].widget && ui_mouse_key_button(ui_mouse_targets[index].widget) != NONE &&
			ui_mouse_targets[index].bounds.y0 >= target->value_bounds.y1)
			limit_bottom = MIN(limit_bottom,
				ui_mouse_targets[index].bounds.y0 - UI_DROPDOWN_LEGEND_CLEARANCE);

	ui_dropdown_widget = target->widget;
	ui_dropdown_creation_time = target->widget->creation_time;
	ui_dropdown_selection = -1;
	ui_dropdown_last_selection = -1;
	{
		/* port: size to the widest label in the font the rows draw in, plus the row's text insets (6 left, 22 right incl. the checkmark) and border */
		struct ui_widget_definition *definition = ui_widget_definition_get(target->widget->definition_tag_index);
		struct halo_menu_screen screen;
		int widest = 0, min_x, max_x;

		halo_screen_menu_screen_rect(TRUE, &screen);
		min_x = (int)MAX(8, screen.safe_x0);
		max_x = (int)MIN(640 - 8, screen.safe_x1);
		for (index = 0; index < target->widget->parameters.list.number_of_items; index++)
		{
			void const *label = spinner_string_list_get_string(definition->text_label_string_list.index, (short)index);
			int w = label ? ui_widget_port_font_text_width(definition->text_font.index, label) : 0;
			if (w > widest) widest = w;
		}
		halo_menu_dropdown_open(&ui_dropdown, target->value_bounds.x0,
			target->value_bounds.y0, target->value_bounds.y1,
			target->widget->parameters.list.number_of_items,
			target->widget->parameters.list.selected_index, limit_top, limit_bottom,
			halo_menu_dropdown_panel_width(widest), min_x, max_x);
	}
}

/* port: post one normal spinner step each frame, as ui_mouse_step_list_to_slot
would, without truncating a long enum to UI_MOUSE_MAXIMUM_PRESSES. Each event
runs the usual handlers and the next frame observes their result. */
static void ui_dropdown_step_pending(void)
{
	int selected = ui_dropdown_widget->parameters.list.selected_index;
	short back, forward;
	if (selected == ui_dropdown_selection || selected == ui_dropdown_last_selection ||
		ui_dropdown_selection >= ui_dropdown_widget->parameters.list.number_of_items)
	{
		ui_dropdown_selection = -1;
		return;
	}
	ui_mouse_give_focus(ui_dropdown_widget);
	ui_mouse_list_directions(ui_dropdown_widget, &back, &forward);
	ui_dropdown_last_selection = selected;
	ui_mouse_press(selected < ui_dropdown_selection ? forward : back);
}

/* port: all non-pointer input dismisses the modal without reaching the page.
The null event still updates animations and settings. */
static boolean ui_dropdown_filter_event(struct event_record *event)
{
	if (ui_dropdown.open && event->type != _event_type_null)
	{
		ui_dropdown.open = FALSE;
		return TRUE;
	}
	return FALSE;
}

/* port: consume modal input before ordinary row hit-testing. Touch drags
already arrive as wheel steps from host_menu_pointer_touch_move(). */
static boolean ui_dropdown_process_pointer(struct halo_ui_pointer *pointer)
{
	if (!ui_dropdown.open && ui_dropdown_selection < 0) return FALSE;
	if (!ui_dropdown_owner_valid())
	{
		ui_dropdown.open = FALSE;
		ui_dropdown_selection = -1;
		return TRUE;
	}
	if (ui_dropdown.open)
	{
		/* Keyboard ownership can change before its event arrives. Keep the
		modal alive until ui_dropdown_filter_event() consumes that event. */
		if (pointer->right_clicks) ui_dropdown.open = FALSE;
		else if (pointer->left_clicks)
		{
			ui_dropdown_selection = halo_menu_dropdown_click(&ui_dropdown, pointer->click_x, pointer->click_y);
			ui_dropdown_last_selection = -1;
			halo_ui_pointer_feedback(2);
		}
		else
		{
			if (pointer->wheel_steps)
				halo_menu_dropdown_scroll(&ui_dropdown, -pointer->wheel_steps);
			if (pointer->moved || pointer->wheel_steps)
			{
				int option = halo_menu_dropdown_option(&ui_dropdown, pointer->x, pointer->y);
				if (option >= 0) ui_dropdown.highlight = option;
			}
		}
		return TRUE;
	}
	if (!halo_menu_pointer_controls()) ui_dropdown_selection = -1;
	else ui_dropdown_step_pending();
	return TRUE;
}

static void ui_dropdown_draw(void)
{
	struct ui_widget_definition *definition;
	rectangle2d bounds, clip = {0, 0, 480, 640};
	real_argb_color color = {1.0f, 1.0f, 1.0f, 1.0f};
	int slot;
	if (!ui_dropdown.open || !ui_dropdown_owner_valid()) return;
	definition = ui_widget_definition_get(ui_dropdown_widget->definition_tag_index);
	bounds.x0 = ui_dropdown.x; bounds.x1 = bounds.x0 + ui_dropdown.width;
	bounds.y0 = ui_dropdown.y; bounds.y1 = bounds.y0 + ui_dropdown.rows * HALO_DROPDOWN_PITCH + 3;
	ui_dropdown_bitmap(ui_dropdown_widget, "pc\\xbox\\bitmaps\\dropdown_panel",
		ui_dropdown.rows - 1, &bounds, &clip);
	color.alpha = widget_instance_get_cumulative_alpha_modifier(ui_dropdown_widget);
	for (slot = 0; slot < ui_dropdown.rows; slot++)
	{
		int option = ui_dropdown.first + slot;
		rectangle2d text;
		bounds.x0 = ui_dropdown.x + 2; bounds.x1 = ui_dropdown.x + ui_dropdown.width - 2;
		bounds.y0 = ui_dropdown.y + 2 + slot * HALO_DROPDOWN_PITCH;
		bounds.y1 = bounds.y0 + HALO_DROPDOWN_PITCH;
		if (option == ui_dropdown.highlight)
			draw_quad(&bounds, (fast_ftol(color.alpha * 255.0f) << 24) | 0x001F4FA3);
		text = bounds;
		/* (the clip runs to the row's edge: a label measured a little short must not lose its last letter; the popup is sized with room for the checkmark) */
		text.x0 += 6; text.x1 -= 2;
		text.y0 += (HALO_DROPDOWN_PITCH - 26) / 2;
		draw_string_set_draw_mode(definition->text_font.index, NONE, 0, 0, &color);
		rasterizer_draw_unicode_string(&text, &text, NULL, 0,
			spinner_string_list_get_string(definition->text_label_string_list.index, (short)option));
		if (option == ui_dropdown_widget->parameters.list.selected_index)
		{
			bounds.x0 = bounds.x1 - 18; bounds.x1 = bounds.x0 + 12;
			bounds.y0 += (HALO_DROPDOWN_PITCH - 12) / 2; bounds.y1 = bounds.y0 + 12;
			ui_dropdown_bitmap(ui_dropdown_widget, "pc\\xbox\\bitmaps\\dropdown_check", 0, &bounds, &clip);
		}
	}
}

/* port: capture consumes touch-generated wheel steps in the guest. The host
can keep its ordinary list-scroll recognition; none reaches the page here. */
static boolean ui_slider_process_pointer(struct halo_ui_pointer *pointer)
{
	struct ui_mouse_target *target = NULL;
	long index;
	int pressed = pointer->held && !ui_slider_was_held;
	int tap_only = FALSE;
	ui_slider_was_held = pointer->held;
	if (!halo_menu_pointer_controls() || pointer->right_clicks)
	{
		ui_slider_widget = NULL;
		ui_slider_selection = -1;
		return FALSE;
	}
	if (ui_slider_widget)
	{
		for (index = 0; index < ui_mouse_target_count; index++)
			if (ui_mouse_targets[index].widget == ui_slider_widget)
			{
				target = &ui_mouse_targets[index];
				break;
			}
		if (!target || target->widget->creation_time != ui_slider_creation_time ||
			ui_widget_row_control(target->widget).kind != HALO_ROW_RANGE)
		{
			ui_slider_widget = NULL;
			ui_slider_selection = -1;
			return TRUE;
		}
	}
	else if (pressed || pointer->left_clicks)
	{
		int x = pressed ? pointer->x : pointer->click_x;
		int y = pressed ? pointer->y : pointer->click_y;
		target = ui_mouse_target_at((short)x, (short)y);
		if (!target || target->kind != _ui_mouse_target_value ||
			ui_widget_row_control(target->widget).kind != HALO_ROW_RANGE) return FALSE;
		halo_menu_slider_geometry(&ui_slider_geometry, target->value_bounds.x0,
			target->value_bounds.y0, target->value_bounds.x1, target->value_bounds.y1);
		if (!halo_menu_slider_hit(&ui_slider_geometry, x, y)) return FALSE;
		ui_slider_widget = target->widget;
		ui_slider_creation_time = target->widget->creation_time;
		ui_slider_down_x = x; ui_slider_down_y = y;
		ui_slider_dragging = FALSE;
		ui_slider_selection = ui_slider_last_selection = -1;
		tap_only = !pressed;
	}
	else return FALSE;
	if (pointer->held || pointer->left_clicks)
	{
		if (!tap_only && halo_menu_slider_dragged(ui_slider_down_x, ui_slider_down_y, pointer->x, pointer->y))
			ui_slider_dragging = TRUE;
		if (ui_slider_dragging)
			ui_slider_selection = halo_menu_slider_value(&ui_slider_geometry, pointer->x,
				ui_slider_widget->parameters.list.number_of_items);
		else if (!pointer->held && pointer->left_clicks)
		{
			int selected = ui_slider_widget->parameters.list.selected_index;
			int count = ui_slider_widget->parameters.list.number_of_items;
			ui_slider_selection = selected + halo_menu_slider_tap(&ui_slider_geometry, selected, count, ui_slider_down_x);
			if (ui_slider_selection < 0) ui_slider_selection = 0;
			if (ui_slider_selection >= count) ui_slider_selection = count - 1;
		}
	}
	if (ui_slider_selection >= 0 && !ui_mouse_press_count)
	{
		int selected = ui_slider_widget->parameters.list.selected_index;
		if (selected != ui_slider_selection && selected != ui_slider_last_selection)
		{
			short back, forward;
			ui_mouse_give_focus(ui_slider_widget);
			ui_mouse_list_directions(ui_slider_widget, &back, &forward);
			ui_slider_last_selection = selected;
			ui_mouse_press(selected < ui_slider_selection ? forward : back);
			halo_ui_pointer_feedback(2);
		}
		else if (!pointer->held) ui_slider_selection = -1;
	}
	if (!pointer->held && ui_slider_selection < 0) ui_slider_widget = NULL;
	return TRUE;
}

/* port: enum and range controls share this dispatch with toggles. */
static void ui_mouse_row_step(struct ui_mouse_target *target, short x,
	short back, short forward, boolean first_half)
{
	if (halo_menu_pointer_controls() && ui_widget_row_has_dropdown(target->widget))
	{
		if (halo_menu_pointer_controls() == 1 && x >= target->back_arrow_end - 6 && x < target->back_arrow_end)
			ui_mouse_press(back);
		else if (halo_menu_pointer_controls() == 1 && x >= target->forward_arrow_start && x < target->forward_arrow_start + 6)
			ui_mouse_press(forward);
		else ui_dropdown_open(target);
		return;
	}
	/* port: one dispatch for each runtime row control. A row flip
	uses the controller step toward the other value, even on clamped lists. */
	if (halo_menu_pointer_controls())
		switch (ui_widget_row_control(target->widget).kind)
		{
		case HALO_ROW_STATIC: break;
		case HALO_ROW_TOGGLE:
			if (halo_menu_pointer_controls() == 1 &&
				x >= target->back_arrow_end - 6 &&
				x < target->back_arrow_end)
				ui_mouse_press(back);
			else if (halo_menu_pointer_controls() == 1 &&
				x >= target->forward_arrow_start &&
				x < target->forward_arrow_start + 6)
				ui_mouse_press(forward);
			else ui_mouse_press(target->widget->parameters.list.selected_index ? back : forward);
			break;
		default: ui_mouse_press(first_half ? back : forward); break;
		}
	else ui_mouse_press(first_half ? back : forward);
}

/* port: host touch radius is 22 points converted with the menu picture's
scale. A mouse has no radius, so its guest fallback is 10 menu units. */
static boolean ui_carousel_pointer(struct halo_ui_pointer *pointer)
{
	struct ui_carousel_state *state = ui_carousel_capture;
	boolean consumed = FALSE;
	unsigned long now = system_milliseconds();
	if (pointer->held && !ui_carousel_pointer_was_down && !state)
	{
		long index;
		for (index = 0; index < ui_mouse_target_count; index++)
		{
			struct ui_mouse_target *target = &ui_mouse_targets[index];
			struct widget_instance *list = target->kind == _ui_mouse_target_list_slot ?
				target->widget->parent : target->kind == _ui_mouse_target_carousel_peek ? target->widget : NULL;
			if (list && ui_widget_is_carousel(list) && pointer->x >= 0 && pointer->x < 640 &&
				pointer->y >= target->bounds.y0 && pointer->y < target->bounds.y1)
			{
				state = ui_carousel_capture = ui_carousel_state(list);
				state->last_input = now;
				halo_ui_drag_press(&state->drag, pointer->x, pointer->y);
				break;
			}
		}
	}
	if (state)
	{
		float dt = MIN((float)(now - state->last_input) * 0.001f, 0.1f);
		float slop = pointer->radius > 0 ? pointer->radius * HALO_UI_DRAG_SLOP_POINTS / 22.0f : 10.0f;
		float maximum = halo_ui_carousel_max_first(state->widget->parameters.list.number_of_items) * HALO_UI_CAROUSEL_PITCH;
		state->last_input = now;
		consumed = TRUE;
		/* The final position matters even when the lift and last move arrive
		in one guest frame. Wheel steps made by the host's drag are discarded. */
		halo_ui_drag_move(&state->drag, pointer->x, pointer->y, slop, dt, 0, maximum);
		pointer->wheel_steps = 0;
		if (!pointer->held)
		{
			if (!halo_ui_drag_release(&state->drag, HALO_UI_CAROUSEL_PITCH, 0, maximum))
			{
				pointer->left_clicks = 0;
				ui_mouse_click_pending = FALSE;
			}
			ui_carousel_capture = NULL;
		}
	}
	ui_carousel_pointer_was_down = pointer->held;
	return consumed;
}

static void ui_carousel_rebind(struct widget_instance *list)
{
	struct ui_widget_definition *definition = ui_widget_definition_get(list->definition_tag_index);
	long index;
	for (index = 0; index < definition->game_data_inputs.count; index++)
	{
		struct ui_widget_game_data_input_reference *input =
			(struct ui_widget_game_data_input_reference *)definition->game_data_inputs.address + index;
		ui_widget_game_data_function_invoke(list, input->function);
	}
}

static void ui_carousel_peek(struct widget_instance *list, int direction)
{
	struct ui_carousel_state *state = ui_carousel_state(list);
	state->first = halo_ui_carousel_clamp_first(
		halo_ui_carousel_settled_first(state->drag.target, list->parameters.list.number_of_items) + direction,
		list->parameters.list.number_of_items);
	halo_ui_drag_glide(&state->drag, state->first * HALO_UI_CAROUSEL_PITCH);
}

static void ui_carousel_activate(struct ui_mouse_target *target)
{
	struct widget_instance *list = target->widget->parent;
	struct ui_carousel_state *state = ui_carousel_state(list);
	int item = target->button_index;
	int first = halo_ui_carousel_settled_first(state->drag.offset, list->parameters.list.number_of_items);
	if (item < first)
		first = item;
	if (item > first + 2)
		first = item - 2;
	state->first = state->binding_first = halo_ui_carousel_clamp_first(first, list->parameters.list.number_of_items);
	ui_mouse_give_focus(list);
	ui_carousel_focus(list, item, state->first);
	ui_carousel_rebind(list);
	halo_ui_drag_glide(&state->drag, state->first * HALO_UI_CAROUSEL_PITCH);
	ui_mouse_press(_gamepad_analog_button_a);
}

/* port: debug.pointer_log's second line for a tap: what it hit (the kind of target, the widget and the button it
presses), so a tap that went back can be told from one that opened a row: the BACK chevron and a screen's B legend
are button targets that press B, a swipe in from the left edge is "back swipe" */
static void ui_mouse_log_dispatch(
	char const *what,
	struct ui_mouse_target const *target)
{
	extern int config_boolean(char const *);
	extern void platform_log(char const *, ...);

	if (!config_boolean("debug.pointer_log"))
		return;
	if (!target)
		platform_log("pointer: %s", what);
	else
		platform_log("pointer: %s kind %d widget %s button %d bounds %d,%d-%d,%d", what, (int)target->kind,
			target->widget && target->widget->name ? target->widget->name : "(none)", (int)target->button_index,
			target->bounds.x0, target->bounds.y0, target->bounds.x1, target->bounds.y1);
}

static void ui_widgets_process_mouse(
	void)
{
	struct halo_ui_pointer pointer;
	struct ui_mouse_target *target;
	short controller_index = 0;
	static short claim_controller = NONE;

	/* port: use the same screen bounds for input and widget drawing. */
	ui_widget_menu_content(virtual_keyboard_active() ? NULL : ui_mouse_menu());
	if (!halo_ui_pointer_update(ui_mouse_menus_active(), &pointer) ||
		virtual_keyboard_active())
	{
		if (virtual_keyboard_active() || !ui_dropdown_owner_valid()) ui_dropdown.open = FALSE;
		ui_dropdown_selection = -1;
		ui_slider_widget = NULL;
		ui_slider_selection = -1;
		ui_slider_was_held = FALSE;
		ui_mouse_menu_drawn = FALSE;
		if (ui_carousel_capture)
		{
			halo_ui_drag_cancel(&ui_carousel_capture->drag, HALO_UI_CAROUSEL_PITCH, 0,
				halo_ui_carousel_max_first(ui_carousel_capture->widget->parameters.list.number_of_items) * HALO_UI_CAROUSEL_PITCH);
		}
		ui_carousel_capture = NULL;
		ui_carousel_pointer_was_down = FALSE;
		ui_mouse_radius = 0;
		ui_mouse_press_count = 0;
		ui_mouse_hover_pending = FALSE;
		ui_mouse_click_pending = FALSE;
	}
	else
	{
		halo_menu_pointer_activity(&pointer);
		if (ui_dropdown_process_pointer(&pointer))
		{
			ui_mouse_hover_pending = ui_mouse_click_pending = FALSE;
			goto post_pointer_presses;
		}
		ui_mouse_radius = pointer.radius;
		if (ui_slider_process_pointer(&pointer))
		{
			ui_mouse_hover_pending = ui_mouse_click_pending = FALSE;
			goto post_pointer_presses;
		}
		ui_carousel_pointer(&pointer);
		/* (a screen with no menu widget, the scores after a game, has nothing to draw first) */
		if (!ui_mouse_menu())
			ui_mouse_menu_drawn = TRUE;
		if (pointer.moved)
		{
			ui_mouse_hover_pending = TRUE;
			ui_mouse_hover_x = pointer.x;
			ui_mouse_hover_y = pointer.y;
			/* a finger already dragging (no longer a tap candidate) */
			ui_mouse_hover_dragging = pointer.radius > 0 && !pointer.down;
		}
		if (pointer.left_clicks)
		{
			ui_mouse_click_pending = TRUE;
			ui_mouse_click_x = pointer.click_x;
			ui_mouse_click_y = pointer.click_y;
		}
		/* the long-press release is not also a tap */
		if (ui_popup_open)
		{
			/* the context menu is modal. A finger pressed or dragged over a row highlights it
			(a tick as it crosses onto one); releasing on a row, or tapping or clicking one,
			runs its action; a tap or release anywhere else closes the menu. The release that
			ends the long press itself, off the rows, leaves the menu open. */
			short row_under = -1;
			boolean released = ui_popup_prev_down && !pointer.down;
			short px = released || pointer.down ? pointer.x : pointer.click_x;
			short py = released || pointer.down ? pointer.y : pointer.click_y;
			short row_index;
			boolean act = FALSE, close = FALSE;

			for (row_index = 0; row_index < ui_popup_row_count; row_index++)
			{
				if (px >= ui_popup_rows[row_index].x0 && px < ui_popup_rows[row_index].x1 &&
					py >= ui_popup_rows[row_index].y0 && py < ui_popup_rows[row_index].y1)
					row_under = row_index;
			}
			if (pointer.down)
			{
				if (row_under != ui_popup_highlight && row_under >= 0)
					halo_ui_pointer_feedback(1);
				ui_popup_highlight = row_under;
			}
			else if (released)
			{
				if (row_under >= 0)
					act = TRUE;
				else if (!ui_hold_swallow)
					close = TRUE;
				ui_popup_highlight = -1;
			}
			else if (pointer.left_clicks)
			{
				if (pointer.click_x >= 0 && row_under >= 0)
					act = TRUE;
				else
					close = TRUE;
			}
			if (act)
			{
				halo_ui_pointer_feedback(2);
				ui_mouse_press(_gamepad_analog_button_x);
				close = TRUE;
			}
			if (pointer.right_clicks || pointer.back_swipes)
				close = TRUE;
			if (close)
			{
				ui_popup_open = FALSE;
				ui_popup_highlight = -1;
			}
			pointer.back_swipes = 0;
			pointer.right_clicks = 0;
			pointer.left_clicks = 0;
			ui_mouse_click_pending = FALSE;
			ui_mouse_hover_pending = FALSE;
			if (!pointer.down)
				ui_hold_swallow = FALSE;
			ui_popup_prev_down = pointer.down;
		}
		else if (pointer.down && pointer.radius > 0 && ui_mouse_menu_drawn)
		{
			unsigned long now = (unsigned long)system_milliseconds();

			if (!ui_hold_active)
			{
				target = ui_mouse_target_at(pointer.x, pointer.y);
				if (target && (target->kind == _ui_mouse_target_item || target->kind == _ui_mouse_target_value ||
					target->kind == _ui_mouse_target_list_slot))
				{
					ui_hold_active = TRUE;
					ui_hold_light_done = FALSE;
					ui_hold_focused = FALSE;
					ui_hold_focus_frames = 0;
					ui_hold_grows = FALSE;
					ui_hold_shrinking = FALSE;
					ui_hold_start_ms = now;
					ui_hold_bounds = target->bounds;
					ui_hold_widget = target->widget;
					ui_hold_drawn_valid = FALSE;
				}
			}
			else
			{
				unsigned long elapsed = now - ui_hold_start_ms;

				/* whether the item has a secondary action is its own: the screen's X legend follows
				the focused item, so the held item takes focus at 0.15 s and grows only when the
				legend stays up; an item without one neither grows nor opens a menu */
				if (!ui_hold_focused && elapsed >= UI_HOLD_GROW_START_MS)
				{
					target = ui_mouse_target_at(pointer.x, pointer.y);
					if (target && target->kind == _ui_mouse_target_list_slot)
						ui_mouse_step_list_to_slot(target->widget);
					else if (target)
						ui_mouse_give_focus(target->widget);
					ui_hold_focused = TRUE;
				}
				else if (ui_hold_focused && !ui_hold_grows && ++ui_hold_focus_frames >= 2)
				{
					ui_hold_grows = ui_touch_x_seen && ui_touch_x_row_deletable();
				}
				if (ui_hold_grows && !ui_hold_light_done)
				{
					ui_hold_light_done = TRUE;
					halo_ui_pointer_feedback(3);
				}
				if (ui_hold_grows && elapsed >= UI_HOLD_OPEN_MS)
				{
					target = ui_mouse_target_at(pointer.x, pointer.y);
					if (target && target->kind == _ui_mouse_target_list_slot)
						ui_mouse_step_list_to_slot(target->widget);
					else if (target)
						ui_mouse_give_focus(target->widget);
					halo_ui_pointer_feedback(4);
					ui_popup_anchor = ui_hold_drawn_valid ? ui_hold_drawn : ui_hold_bounds;
					ui_popup_anchor_is_item = TRUE;
					ui_popup_widget = ui_hold_widget;
					ui_popup_highlight = -1;
					ui_popup_prev_down = TRUE;
					ui_popup_open = TRUE;
					ui_hold_active = FALSE;
					ui_hold_grows = FALSE;
					ui_hold_swallow = TRUE;
					ui_mouse_hover_pending = FALSE;
				}
			}
		}
		else
		{
			if (ui_hold_active && ui_hold_grows && !ui_popup_open)
			{
				ui_hold_shrinking = TRUE;
				ui_hold_shrink_ms = system_milliseconds();
			}
			ui_hold_active = FALSE;
			ui_hold_grows = FALSE;
		}
		if (pointer.right_clicks && ui_mouse_menu_drawn && !ui_popup_open)
		{
			/* the secondary action (X) of the item under the pointer, in the same context menu
			as a press-and-hold, opened at the pointer; right-click is never BACK (Escape, B and
			mouse button 4 stay BACK) */
			target = ui_mouse_target_at(pointer.x, pointer.y);
			if (target)
			{
				switch (target->kind)
				{
				case _ui_mouse_target_item:
				case _ui_mouse_target_value:
					ui_mouse_give_focus(target->widget);
					break;
				case _ui_mouse_target_list_slot:
					ui_mouse_step_list_to_slot(target->widget);
					break;
				default:
					target = NULL;
					break;
				}
				if (target)
				{
					/* the screen's X legend follows the focused item: the menu opens once the
					focus has taken effect and the legend is still up */
					ui_popup_pending = 3;
					ui_popup_widget = target->widget;
					ui_popup_pending_x = pointer.x;
					ui_popup_pending_y = pointer.y;
					ui_mouse_hover_pending = FALSE;
				}
			}
		}
		if (ui_popup_pending > 0 && !ui_popup_open)
		{
			if (--ui_popup_pending <= 1 && ui_touch_x_seen && ui_touch_x_row_deletable())
			{
				ui_popup_anchor.x0 = ui_popup_anchor.x1 = ui_popup_pending_x;
				ui_popup_anchor.y0 = ui_popup_anchor.y1 = ui_popup_pending_y;
				ui_popup_anchor_is_item = FALSE;
				ui_popup_highlight = -1;
				ui_popup_prev_down = FALSE;
				ui_popup_open = TRUE;
				ui_popup_pending = 0;
			}
		}
		if (pointer.back_swipes && (ui_touch_back_seen || ui_touch_back_generic) && ui_mouse_menu_drawn)
		{
			ui_mouse_log_dispatch("back swipe", NULL);
			halo_ui_pointer_feedback(2);
			ui_mouse_press(_gamepad_analog_button_b);
		}
		if (pointer.wheel_steps && ui_mouse_menu())
		{
			struct widget_instance *widget = ui_mouse_wheel_widget(ui_mouse_menu());
			short back = _widget_event_dpad_up, forward = _widget_event_dpad_down;
			long step;
			boolean finger = pointer.radius > 0, over_row = FALSE;
			long steps;

			if (widget)
				ui_mouse_list_directions(widget, &back, &forward);
			if (finger && back == _widget_event_dpad_up)
			{
				struct ui_mouse_target *under = ui_mouse_target_at(pointer.x, pointer.y);

				over_row = under && (under->kind == _ui_mouse_target_item || under->kind == _ui_mouse_target_value);
			}
			else
				finger = FALSE; /* a horizontal list keeps the wheel's direction */
			steps = halo_ui_list_wheel_steps(pointer.wheel_steps, finger, over_row);
			for (step = 0; step < ABS(steps) && step < 4; step++)
				ui_mouse_press(steps < 0 ? back : forward);
			if (steps)
				halo_ui_pointer_feedback(1);
		}
		if (!ui_mouse_press_count && ui_mouse_hover_pending)
		{
			ui_mouse_hover_pending = FALSE;
			target = ui_mouse_target_at(ui_mouse_hover_x, ui_mouse_hover_y);
			if (target)
			{
				switch (target->kind)
				{
				case _ui_mouse_target_item:
				case _ui_mouse_target_value:
					/* focus follows the pointer, selection rows included; a click then
					acts on the item directly. Focus stays where it was when the pointer
					leaves every item. */
					if (!ui_mouse_widget_has_focus(target->widget))
						halo_ui_pointer_feedback(1);
					ui_mouse_give_focus(target->widget);
					break;
				case _ui_mouse_target_list_slot:
					if (ui_mouse_hover_dragging)
						break; /* carousels drag-scroll by the wheel */
					/* hovering a slot focuses that slot, as it does an item. Only pointer
					motion hovers, so a card that slides under a still pointer does not
					step the list again. */
					if (!ui_mouse_widget_has_focus(target->widget->parent) ||
						target->widget->parent->focused_child != target->widget)
						halo_ui_pointer_feedback(1);
					ui_mouse_step_list_to_slot(target->widget);
					break;
				}
			}
		}
		if (!ui_mouse_press_count && ui_mouse_click_pending && !ui_mouse_menu_drawn)
		{
			/* (nothing has been drawn to click on yet) */
			ui_mouse_click_pending = FALSE;
		}
		if (!ui_mouse_press_count && ui_mouse_click_pending)
		{
			ui_mouse_click_pending = FALSE;
			target = ui_mouse_target_at(ui_mouse_click_x, ui_mouse_click_y);
			ui_mouse_log_dispatch(target ? "tap hit" : "tap hit nothing", target);
			if (target)
			{
				if (target->kind == _ui_mouse_target_button)
				{
					ui_touch_legend_pressed = target->widget;
					ui_touch_legend_pressed_frames = 8;
				}
				switch (target->kind)
				{
				case _ui_mouse_target_item:
					/* (a selection list's row: chosen by a click, used by
					a click on it chosen) */
					if (ui_mouse_selection_row(target->widget) && target->widget->parent &&
						target->widget->parent->focused_child != target->widget)
					{
						halo_ui_pointer_feedback(1);
						ui_mouse_give_focus(target->widget);
						break;
					}
					halo_ui_pointer_feedback(2);
					ui_mouse_give_focus(target->widget);
					ui_mouse_press(_gamepad_analog_button_a);
					break;
				case _ui_mouse_target_value:
				{
					short back, forward;
					boolean first_half;

					halo_ui_pointer_feedback(2);
					ui_mouse_give_focus(target->widget);
					ui_mouse_list_directions(target->widget, &back, &forward);
					first_half = back == _widget_event_dpad_left ?
						ui_mouse_click_x < (target->bounds.x0 + target->bounds.x1) / 2 :
						ui_mouse_click_y < (target->bounds.y0 + target->bounds.y1) / 2;
					ui_mouse_row_step(target, ui_mouse_click_x, back, forward, first_half);
					break;
				}
				case _ui_mouse_target_list_slot:
					halo_ui_pointer_feedback(2);
					ui_carousel_activate(target);
					break;
				case _ui_mouse_target_carousel_peek:
					halo_ui_pointer_feedback(1);
					ui_carousel_peek(target->widget, target->button_index);
					break;
				case _ui_mouse_target_button:
					halo_ui_pointer_feedback(2);
					ui_mouse_press(target->button_index);
					break;
				case _ui_mouse_target_action:
					/* (the scores' own buttons: the game takes them, ui_widget_port_postgame_pressed) */
					halo_ui_pointer_feedback(2);
					ui_postgame_pending = ui_postgame_pressed = target->button_index;
					ui_postgame_pressed_frames = 8;
					break;
				}
			}
			else
			{
				long index;

				/* a screen with nothing to pick (a message to dismiss): the
				click is its A */
				for (index = 0; index < ui_mouse_target_count; index++)
				{
					if (ui_mouse_targets[index].kind != _ui_mouse_target_button)
						break;
				}
				if (index == ui_mouse_target_count)
				{
					struct widget_instance *menu = ui_mouse_menu();

					/* the split screen pregame: a click or tap claims the first open slot (the
					first local player not yet joined), as that slot's controller would with A */
					claim_controller = NONE;
					if (menu && strstr(tag_get_name(menu->definition_tag_index), "splitscreen_pregame"))
					{
						short player;

						for (player = 0; player < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; player++)
						{
							if (!player_ui_local_player_wants_to_play_multiplayer(player))
							{
								claim_controller = player;
								break;
							}
						}
					}
					halo_ui_pointer_feedback(2);
					ui_mouse_press(_gamepad_analog_button_a);
				}
			}
		}
	post_pointer_presses:
		if (ui_mouse_press_count)
		{
			event_manager_post_pointer_button(claim_controller != NONE ? claim_controller : controller_index,
				ui_mouse_presses[0]);
			claim_controller = NONE;
			csmemmove(ui_mouse_presses, ui_mouse_presses + 1, --ui_mouse_press_count * sizeof(ui_mouse_presses[0]));
		}
	}
	ui_mouse_target_count = 0;

	return;
}

/* port: a menu file's column list with visible_rows= shows that many of its
rows (the children that take events, or have children that do, and are not
lists themselves) and scrolls the rest in, as the Xbox's lists scroll, with
the children named scroll_up_arrow and scroll_down_arrow showing only while
rows are hidden above or below: the
rows the game's code shows (visible) are set in a column from the first
row's place at the pitch between its first two rows, the first shown being
kept so that the focused row is among them. Done as the list is drawn, after
its game data functions, and undone after (a row past the window is hidden
only while the children draw, so the game's code and the focus see the rows
as they were) */
#define WINDOW_MAXIMUM_ROWS 64
#define WINDOW_STATE_COUNT 8

static struct
{
	struct widget_instance *widget;
	long creation_time;
	short first;
} column_list_window_states[WINDOW_STATE_COUNT];
static short column_list_window_state_next;

static short *column_list_window_first(
	struct widget_instance *widget)
{
	short index;

	for (index = 0; index < WINDOW_STATE_COUNT; index++)
	{
		if (column_list_window_states[index].widget == widget &&
			column_list_window_states[index].creation_time == widget->creation_time)
		{
			return &column_list_window_states[index].first;
		}
	}
	index = column_list_window_state_next++ % WINDOW_STATE_COUNT;
	column_list_window_states[index].widget = widget;
	column_list_window_states[index].creation_time = widget->creation_time;
	column_list_window_states[index].first = 0;
	return &column_list_window_states[index].first;
}

/* hides the rows past the window and lays the rest out; saved gets each
child's visible as it was (count of them returned, 0: nothing changed) */
static short column_list_window_begin(
	struct widget_instance *widget,
	struct ui_widget_definition *definition,
	short window,
	boolean *saved)
{
	struct widget_instance *children[WINDOW_MAXIMUM_ROWS];
	short reference_y[WINDOW_MAXIMUM_ROWS];
	short row_children[WINDOW_MAXIMUM_ROWS];
	short child_count = 0, row_count = 0, shown = 0, rank, focused_rank = NONE;
	short first_y = 0, pitch = 0, first, *state, index;
	long reference_index = 0;
	struct widget_instance *child;

	if (TEST_FLAG(definition->list_flags, _list_items_generated_in_code))
		return 0;
	for (child = widget->child; child; child = child->next)
	{
		struct ui_widget_child_reference *reference = NULL;

		if (child_count >= WINDOW_MAXIMUM_ROWS)
			return 0;
		while (reference_index < definition->child_widgets.count)
		{
			reference = (struct ui_widget_child_reference *)definition->child_widgets.address + reference_index++;
			if (reference->widget_tag.index != NONE)
				break;
			reference = NULL;
		}
		if (!reference || reference->widget_tag.index != child->definition_tag_index)
			return 0;
		children[child_count] = child;
		reference_y[child_count] = reference->vertical_offset;
		saved[child_count] = child->visible;
		if (child->type != _ui_widget_type_column_list &&
			(widget_instance_can_handle_events(child) || child->child))
		{
			if (row_count == 0)
				first_y = reference->vertical_offset;
			else if (pitch == 0 && reference->vertical_offset != first_y)
				pitch = (short)(reference->vertical_offset - first_y);
			row_children[row_count++] = child_count;
		}
		child_count++;
	}
	if (row_count == 0 || pitch == 0)
		return 0;
	for (index = 0; index < row_count; index++)
	{
		if (children[row_children[index]]->visible)
		{
			if (children[row_children[index]] == widget->focused_child)
				focused_rank = shown;
			shown++;
		}
	}
	state = column_list_window_first(widget);
	first = *state;
	if (focused_rank != NONE)
	{
		if (focused_rank < first)
			first = focused_rank;
		else if (focused_rank >= first + window)
			first = (short)(focused_rank - window + 1);
	}
	first = (short)PIN(first, 0, MAX(0, shown - window));
	*state = first;
	/* the arrows (children named scroll_up_arrow and scroll_down_arrow): shown
	only while rows are hidden above or below the window */
	for (index = 0; index < child_count; index++)
	{
		if (children[index]->name && !strcmp(children[index]->name, "scroll_up_arrow"))
			children[index]->visible = first > 0;
		else if (children[index]->name && !strcmp(children[index]->name, "scroll_down_arrow"))
			children[index]->visible = first + window < shown;
	}
	for (rank = 0, index = 0; index < row_count; index++)
	{
		child = children[row_children[index]];
		if (!child->visible)
			continue;
		if (rank >= first && rank < first + window)
			child->vertical_offset = (short)(widget->vertical_offset + first_y + (rank - first) * pitch);
		else
			child->visible = FALSE;
		rank++;
	}
	return child_count;
}

static void column_list_window_end(
	struct widget_instance *widget,
	short child_count,
	boolean const *saved)
{
	struct widget_instance *child;
	short index = 0;

	for (child = widget->child; child && index < child_count; child = child->next)
		child->visible = saved[index++];
}

/* port: the VISR helmet left of a screen's title. A title is the widget the
menus' data says is one (halo_menu_meta.h's HALO_MENU_ROLE_TITLE, a <title>
in the menus' files); nothing about it is read off its name or its picture.
Its picture is 64 units high, the capitals stand 22 units tall from 33 units
down and start 30 units in. The helmet's picture (xbox/bitmaps/visr_title_icon,
tools/xbox_menus.py) is 36 units square; the helmet in it is 22 units tall
like the capitals and 29 wide, and the picture is centered on them with 11
units between the helmet and the first letter. */
static struct bitmap_data *menu_title_icon(
	struct widget_instance const *widget,
	struct bitmap_data const *picture)
{
	long icon_tag;

	if (!picture || pc_menu_meta(widget->definition_tag_index)->role != HALO_MENU_ROLE_TITLE)
		return NULL;
	icon_tag = tag_loaded(BITMAP_GROUP_TAG, "pc\\xbox\\bitmaps\\visr_title_icon");
	return icon_tag == NONE ? NULL : bitmap_group_get_bitmap_from_sequence(icon_tag, 0, 0);
}

/* port: on touch the BACK chevron sits left of the title's helmet; when the
screen's safe inset leaves no room for it there, the chevron, helmet and title
move right together, by how far the chevron's tap target would cross the inset */
static short menu_title_touch_shift(
	struct widget_instance const *widget,
	struct bitmap_data const *picture,
	rectangle2d const *bounds,
	rectangle2d const *clip)
{
	long reach, safe_left, scale_x = 1, helmet_left, needed;

	if (ui_mouse_radius <= 0 || !clip || !menu_title_icon(widget, picture))
		return 0;
	reach = MAX(ui_redraw_touch_target_units(), 32);
	safe_left = ui_widget_safe_left(clip->x0);
	if (bounds->x1 - bounds->x0 > picture->width)
		scale_x = (bounds->x1 - bounds->x0) / picture->width;
	helmet_left = bounds->x0 + (30 - 11 - 29) * scale_x - (36 - 29) * scale_x / 2;
	/* the chevron's center is reach / 2 + 14 left of the helmet; its tap target must clear the inset by 2 */
	needed = safe_left + 2 + reach + 14 - helmet_left;
	return (short)MAX(needed, 0);
}

static void menu_title_icon_draw(
	struct widget_instance const *widget,
	struct bitmap_data const *picture,
	rectangle2d const *bounds,
	rectangle2d *clip,
	pixel32 color)
{
	struct bitmap_data *icon;
	rectangle2d rect;
	long scale_x = 1, scale_y = 1, size, left;

	icon = menu_title_icon(widget, picture);
	if (!icon)
		return;
	/* (a picture smaller than its widget is drawn stretched over it) */
	if (bounds->x1 - bounds->x0 > picture->width)
		scale_x = (bounds->x1 - bounds->x0) / picture->width;
	if (bounds->y1 - bounds->y0 > picture->height)
		scale_y = (bounds->y1 - bounds->y0) / picture->height;
	/* (the helmet is 29 of the picture's 36 units wide, 3.5 units in from each side) */
	size = 36 * scale_y;
	left = bounds->x0 + (30 - 11 - 29) * scale_x - (36 - 29) * scale_x / 2;
	rect.x0 = (short)left;
	rect.x1 = (short)(left + size);
	rect.y0 = (short)(bounds->y0 + 44 * scale_y - size / 2);
	rect.y1 = (short)(rect.y0 + size);
	if (ui_mouse_noting_targets)
	{
		ui_touch_title_icon = rect;
		ui_touch_title_seen = TRUE;
	}
	draw_bitmap_in_rect(icon, &rect, NULL, clip, color, NULL, FALSE);
}

/* port: measure the active pause layout, including its title and button hints.
Full-screen dims and the pausebox pieces are backdrop, not content. Use the
same list window as rendering so hidden rows cannot enlarge the crop. */
static void ui_widget_content_bounds(struct widget_instance *widget, long y, long *top, long *bottom)
{
	struct ui_widget_definition *definition;
	struct widget_instance *child;
	boolean saved[WINDOW_MAXIMUM_ROWS];
	short count = 0, window;
	char const *bitmap_name;

	if (!widget || !widget->visible)
		return;
	definition = ui_widget_definition_get(widget->definition_tag_index);
	y += widget->vertical_offset;
	bitmap_name = definition->background_bitmap.index == NONE ? NULL :
		tag_get_name(definition->background_bitmap.index);
	if ((definition->text_font.index != NONE ||
		(!widget->child && bitmap_name && !strstr(bitmap_name, "pausebox"))) &&
		definition->bounds.y1 - definition->bounds.y0 < 480)
	{
		*top = MIN(*top, y + definition->bounds.y0);
		*bottom = MAX(*bottom, y + definition->bounds.y1);
	}
	window = widget->type == _ui_widget_type_column_list ?
		pc_menu_list_window(widget->definition_tag_index) : 0;
	if (window > 0)
		count = column_list_window_begin(widget, definition, window, saved);
	if (widget->type == _ui_widget_type_column_list)
		ui_widget_content_bounds(widget->parameters.list.extended_description, y, top, bottom);
	for (child = widget->child; child; child = child->next)
		ui_widget_content_bounds(child, y, top, bottom);
	if (count > 0)
		column_list_window_end(widget, count, saved);
}

static void ui_widget_menu_content(struct widget_instance *widget)
{
	long top = 480, bottom = 0;
	char const *name = widget ? tag_get_name(widget->definition_tag_index) : NULL;

	/* Shared settings retain their front-end layout when opened over a game. */
	if (name && (we_are_at_the_main_menu || strstr(name, "xbox\\") || !strncmp(name, "xbox/", 5) ||
		strstr(name, "main_menu")))
	{
		top = MENU_CONTENT_TOP;
		bottom = MENU_CONTENT_BOTTOM;
	}
	else
		ui_widget_content_bounds(widget, 0, &top, &bottom);
	halo_screen_menu_content((float)top, (float)bottom);
}

/* port: reuse the native cards and their game-data callbacks for every
visible item. Each pass binds a valid three-item window; the original binding,
focus, animations, and target collection are restored before returning. */
static void ui_carousel_render(struct widget_instance *list, point2d offset)
{
	struct ui_carousel_state *state = ui_carousel_state(list);
	struct widget_instance *saved_focus = list->focused_child;
	short saved_selected = list->parameters.list.selected_index;
	int saved_binding = state->binding_first;
	int count = list->parameters.list.number_of_items;
	int begin, end, item, bound = -1;
	boolean noting = ui_mouse_noting_targets;
	rectangle2d frame;
	rectangle2d *saved_clip = ui_carousel_clip;
	/* port: a finger does not move focus, so touch draws no selected card (no focus border,
	no animated background); mouse, controller and keyboard keep both. */
	frame = state->frame;
	ui_carousel_clip = &frame;
	halo_ui_carousel_visible_range(state->drag.offset / HALO_UI_CAROUSEL_PITCH, count, &begin, &end);
	for (item = begin; item < end; item++)
	{
		struct widget_instance *child, *card;
		struct ui_widget_definition *card_definition;
		point2d card_offset = offset;
		int slot;
		float left = halo_ui_carousel_item_x(item, state->drag.offset / HALO_UI_CAROUSEL_PITCH, count);
		/* At most two windows cover the visible strip, even between items.
		Rebind once per window, rather than once per card and its cache load. */
		state->binding_first = halo_ui_carousel_clamp_first(begin + (item - begin) / 3 * 3, count);
		if (bound != state->binding_first)
		{
			bound = state->binding_first;
			list->focused_child = saved_selected >= bound && saved_selected < bound + 3 ?
				widget_instance_get_nth_child(list, saved_selected - bound) : NULL;
			ui_carousel_rebind(list);
			for (child = list->child; child; child = child->next)
				if (child->animation.number_of_sprite_frames == 2)
					child->animation.current_frame_index =
						ui_list_child_draws_focus(list, child) ? 1 : 0;
		}
		slot = item - state->binding_first;
		card = widget_instance_get_nth_child(list, slot);
		if (!card)
			continue;
		card_definition = ui_widget_definition_get(card->definition_tag_index);
		card_offset.x = (short)roundf(left) - card_definition->bounds.x0 - card->horizontal_offset;
		ui_mouse_noting_targets = FALSE;
		widget_instance_render_recursive(card, &frame, card_offset,
			ui_widget_port_draws_focus(ui_list_focus_item(list), item == saved_selected),
			ui_widget_port_draws_focus(ui_list_focus_item(list), item == saved_selected));
		ui_mouse_noting_targets = noting;
		if (noting && card->visible && !card->disabled && !list->disabled &&
			widget_instance_can_receive_events(list) && left >= frame.x0 &&
			left + HALO_UI_CAROUSEL_CARD_WIDTH <= frame.x1 &&
			ui_mouse_target_count < UI_MOUSE_MAXIMUM_TARGETS)
		{
			struct ui_mouse_target *target = &ui_mouse_targets[ui_mouse_target_count++];
			target->widget = card;
			target->kind = _ui_mouse_target_list_slot;
			target->button_index = (short)item;
			target->bounds = card_definition->bounds;
			target->bounds.x0 = (short)roundf(left);
			target->bounds.x1 = target->bounds.x0 + (short)HALO_UI_CAROUSEL_CARD_WIDTH;
			target->bounds.y0 += offset.y + card->vertical_offset;
			target->bounds.y1 += offset.y + card->vertical_offset;
			target->bounds.y0 = MAX(target->bounds.y0, frame.y0);
			target->bounds.y1 = MIN(target->bounds.y1, frame.y1);
			if (target->bounds.y0 >= target->bounds.y1)
				ui_mouse_target_count--;
		}
	}
	state->binding_first = saved_binding;
	list->focused_child = saved_focus;
	list->parameters.list.selected_index = saved_selected;
	ui_carousel_rebind(list);
	spinner_list_update(list);
	ui_carousel_clip = saved_clip;
	ui_mouse_noting_targets = noting;
}

/* port: whether a widget's background is a full-screen fill, which reaches the
whole screen rather than the centered 640x480 design: bounds over the whole
design and a flat picture (the dims and the menus' vertical gradient band, at
most 16 texels wide; a picture is drawn texel for texel, so wider bounds would
shift it, as on the loading screen). Across when the screen reaches past the
design's columns; down and up only for a picture flat both ways (a dim), since
the band's rules sit at its picture's own rows. Not in portrait, whose
full-screen pictures cover the screen by height instead. */
static void ui_widget_screen_fill_kind(
	rectangle2d const *bounds,
	long bitmap_width,
	long bitmap_height,
	struct halo_menu_screen const *screen,
	boolean *wide,
	boolean *tall)
{
	boolean full_design = bounds->x0 <= 0 && bounds->y0 <= 0 &&
		bounds->x1 >= 640 && bounds->y1 >= 480 && bitmap_width <= 16 &&
		halo_screen_width() >= 640;

	*wide = full_design && (screen->x0 < 0 || screen->x1 > 640);
	*tall = full_design && bitmap_height <= 16 && (screen->y0 < 0 || screen->y1 > 480);
}

/* port: a full-screen fill's rectangle (its bounds, or with at_edges its
clip, only on the sides where the clip is at the design's edge) out to the
screen's (halo_screen_menu_screen_rect) */
static void ui_widget_screen_fill_extend(
	rectangle2d *rect,
	struct halo_menu_screen const *screen,
	boolean wide,
	boolean tall,
	boolean at_edges)
{
	if (wide && (!at_edges || rect->x0 <= 0))
		rect->x0 = (short)MIN(rect->x0, screen->x0);
	if (wide && (!at_edges || rect->x1 >= 640))
		rect->x1 = (short)MAX(rect->x1, screen->x1);
	if (tall && (!at_edges || rect->y0 <= 0))
		rect->y0 = (short)MIN(rect->y0, screen->y0);
	if (tall && (!at_edges || rect->y1 >= 480))
		rect->y1 = (short)MAX(rect->y1, screen->y1);
}

static void widget_instance_render_recursive(
	struct widget_instance *widget,
	rectangle2d *clip_rect,
	point2d offset,
	boolean focus,
	boolean use_nifty_plasma_fx)
{
	struct ui_widget_definition *definition =
		ui_widget_definition_get(widget->definition_tag_index);
	real alpha_modifier = widget_instance_get_cumulative_alpha_modifier(widget);
	boolean render_children = TRUE;
	long input_index;
	struct widget_instance *child;
	struct bitmap_data *bitmap;
	/* port: (custom_edition_maps_picture) */
	struct bitmap_data *custom_edition_picture;
	short frame_index;
	boolean window_saved[WINDOW_MAXIMUM_ROWS];
	short window_children = 0;

	if (!use_nifty_plasma_fx &&
		TEST_FLAG(definition->flags, _widget_always_render_with_nifty_fx_bit))
	{
		use_nifty_plasma_fx = TRUE;
	}
	offset.x += widget->horizontal_offset;
	offset.y += widget->vertical_offset;
	for (input_index = 0;
		input_index < definition->game_data_inputs.count;
		input_index++)
	{
		struct ui_widget_game_data_input_reference *input =
			(struct ui_widget_game_data_input_reference *)
				definition->game_data_inputs.address +
			input_index;

		ui_widget_game_data_function_invoke(widget, input->function);
	}
	if (ui_widget_is_carousel(widget))
	{
		struct ui_carousel_state *state = ui_carousel_state(widget);
		int previous = state->binding_first;
		state->frame.x0 = clip_rect ? MAX(0, clip_rect->x0) : 0;
		state->frame.x1 = clip_rect ? MIN(640, clip_rect->x1) : 640;
		state->frame.y0 = clip_rect ? clip_rect->y0 : 0;
		state->frame.y1 = clip_rect ? clip_rect->y1 : 480;
		ui_carousel_tick(widget);
		if (previous != ui_carousel_state(widget)->binding_first)
			ui_carousel_rebind(widget);
	}

	if (!widget->visible)
	{
		/* touch draws no X legend, so the secondary action (X) is known from the screen's own X button
		even when it is hidden: its caption widget labels the press-and-hold context menu */
		if (ui_mouse_noting_targets && widget->parent &&
			ui_mouse_key_button(widget) == _gamepad_analog_button_x && !ui_widget_is_global_x_action(widget))
		{
			struct widget_instance *sibling, *caption = NULL;
			short best = 0x7fff;

			for (sibling = widget->parent->child; sibling; sibling = sibling->next)
			{
				struct ui_widget_definition *sd;
				short distance;
				if (sibling->type != _ui_widget_type_text_box) continue;
				sd = ui_widget_definition_get(sibling->definition_tag_index);
				if (sd->bounds.y0 + sibling->vertical_offset >= definition->bounds.y1 + widget->vertical_offset ||
					sd->bounds.y1 + sibling->vertical_offset <= definition->bounds.y0 + widget->vertical_offset) continue;
				distance = (short)ABS(sd->bounds.x0 + sibling->horizontal_offset - definition->bounds.x1 - widget->horizontal_offset);
				if (distance < best) { best = distance; caption = sibling; }
			}
			if (caption && best < 17 && caption->parameters.text_box.text && caption->parameters.text_box.text[0])
			{
				long length;

				ui_touch_x_seen = TRUE;
				for (length = 0; length < 23 && caption->parameters.text_box.text[length]; length++)
					ui_touch_x_caption[length] = (char)caption->parameters.text_box.text[length];
				ui_touch_x_caption[length] = 0;
				ui_touch_x_destructive = strstr(ui_touch_x_caption, "DELETE") != NULL ||
					strstr(ui_touch_x_caption, "Delete") != NULL;
				halo_controller_glyph_x_destructive(ui_touch_x_destructive);
			}
		}
		return;
	}
	ui_mouse_note_target(widget, definition, offset);
	{
		rectangle2d legend_bounds, legend_drawn;
		if (ui_mouse_noting_targets && ui_mouse_key_button(widget) == _gamepad_analog_button_b &&
			ui_touch_legend_suppressed(widget))
			ui_touch_back_seen = TRUE;
		if (ui_mouse_noting_targets && ui_mouse_key_button(widget) == _gamepad_analog_button_x &&
			!ui_widget_is_global_x_action(widget))
		{
			struct widget_instance *caption = NULL;

			ui_mouse_legend_bounds(widget, definition, offset, &legend_bounds, &legend_drawn, &caption);
			if (!caption && widget->parent)
			{
				/* the touch legend's slots move the caption away from its button, so take the
				row's nearest visible caption rather than none */
				struct widget_instance *sibling;
				short best = 0x7fff;
				for (sibling = widget->parent->child; sibling; sibling = sibling->next)
				{
					struct ui_widget_definition *sd;
					short distance;
					if (sibling->type != _ui_widget_type_text_box || !sibling->visible) continue;
					sd = ui_widget_definition_get(sibling->definition_tag_index);
					if (sd->bounds.y0 + sibling->vertical_offset >= definition->bounds.y1 + widget->vertical_offset ||
						sd->bounds.y1 + sibling->vertical_offset <= definition->bounds.y0 + widget->vertical_offset) continue;
					distance = (short)ABS(sd->bounds.x0 + sibling->horizontal_offset - definition->bounds.x1 - widget->horizontal_offset);
					if (distance < best) { best = distance; caption = sibling; }
				}
			}
			if (caption && caption->parameters.text_box.text && caption->parameters.text_box.text[0])
			{
				long length;

				ui_touch_x_seen = TRUE;
				for (length = 0; length < 23 && caption->parameters.text_box.text[length]; length++)
					ui_touch_x_caption[length] = (char)caption->parameters.text_box.text[length];
				ui_touch_x_caption[length] = 0;
				ui_touch_x_destructive = strstr(ui_touch_x_caption, "DELETE") != NULL ||
					strstr(ui_touch_x_caption, "Delete") != NULL;
				halo_controller_glyph_x_destructive(ui_touch_x_destructive);
			}
		}
		if (ui_mouse_key_button(widget) != NONE &&
			ui_mouse_legend_bounds(widget, definition, offset, &legend_bounds, &legend_drawn, NULL) &&
			!ui_touch_legend_suppressed(widget))
		{
			boolean pressed = widget == ui_touch_legend_pressed && ui_touch_legend_pressed_frames > 0;
			/* (the button is wider than the caption widget, so the caption's clip would cut its right cap) */
			ui_widget_draw_touch_legend(&legend_drawn, NULL, alpha_modifier, pressed);
			if (pressed)
				ui_touch_legend_pressed_frames--;
		}
	}
	/* port: a Custom Edition map's picture, drawn over the whole widget, or
	the unknown level's frame for a map without one
	(port/linux/game/custom_edition_maps.c) */
	frame_index = widget->animation.current_frame_index;
	custom_edition_picture = custom_edition_maps_picture(definition->background_bitmap.index, &frame_index);
	bitmap = custom_edition_picture ? custom_edition_picture : bitmap_group_get_bitmap_from_sequence(
		definition->background_bitmap.index,
		0,
		frame_index);
	if (bitmap)
	{
		real alpha = alpha_modifier;
		rectangle2d bounds = definition->bounds;
		rectangle2d *clip = clip_rect;
		rectangle2d local_clip;
		pixel32 color;
		struct rasterizer_dynamic_screen_geometry_parameters multitexture_params;
		struct bitmap_group *bitmap_group =
			bitmap_group_get(definition->background_bitmap.index);
		struct bitmap_group_sequence *sequence = TAG_BLOCK_GET_ELEMENT(
			&bitmap_group->sequences,
			0,
			struct bitmap_group_sequence);
		/* port: generated comfortable row art draws 36 units inside the
		39-unit tap band. Frame placement changes texture coordinates, so
		the quad also needs its own bounds (tools/xbox_menus.py). */
		if (pc_menu_row_tall(widget->definition_tag_index))
			bounds.y1 = MIN(bounds.y1, bounds.y0 + MENU_COMFORTABLE_ROW_ART);

		/* port: a widget whose bounds cover the whole 640x480 design space (the
		 * pause menu's dim, for example) should cover the whole screen too,
		 * not just the centered 640 columns -- same as the fade_to_black
		 * quad in render_ui_widgets(). Both the bounds and the clip are
		 * widened symmetrically below, before the centering offset is
		 * added. Only flat fills (the dims, and the menus' vertical
		 * gradient, at most 16 texels wide): a picture is drawn texel for
		 * texel, so wider bounds would shift it (the loading screen).
		 * How far is the screen's reach in this draw, which grows when the
		 * safe-area zoom shrinks the menus: the margins alone would leave
		 * the screen's edges uncovered (ui_widget_screen_fill_kind). */
		struct halo_menu_screen screen;
		boolean widen_to_screen, heighten_to_screen;

		halo_screen_menu_screen_rect(TRUE, &screen);
		ui_widget_screen_fill_kind(&bounds, bitmap->width, bitmap->height, &screen,
			&widen_to_screen, &heighten_to_screen);

		if (use_nifty_plasma_fx)
		{
			ui_plasma_effect_color.alpha = 0.0f;
			ui_plasma_effect_color.red = 0.05f;
			ui_plasma_effect_color.green = 0.05f;
			ui_plasma_effect_color.blue = 0.05f;
		}
		ui_widget_screen_fill_extend(&bounds, &screen, widen_to_screen, heighten_to_screen, FALSE);
		bounds.x0 += offset.x;
		bounds.x1 += offset.x;
		bounds.y0 += offset.y;
		bounds.y1 += offset.y;
		if (clip && !ui_carousel_clip)
		{
			local_clip = *clip;
			clip = &local_clip;
			clip->x0 += offset.x;
			clip->x1 += offset.x;
			clip->y0 += offset.y;
			clip->y1 += offset.y;
		}
		/* the clip as far (it is offset-shifted but still 640-wide at the
		 * edges), so the dim is not clipped back to the centered columns */
		if (clip)
			ui_widget_screen_fill_extend(clip, &screen, widen_to_screen, heighten_to_screen, TRUE);
		if (TEST_FLAG(definition->flags, _widget_flash_background_bitmap_bit))
		{
			alpha = (((real)cos(
				widget_globals.current_system_milliseconds *
					SECONDS_PER_MILLISECOND * 3.0f) + 1.0f) * 0.5f) *
				alpha_modifier;
		}
		color = modulate_pixel32_by_real_alpha(0xFFFFFFFF, alpha);
		/* port: in HEAD mode's full view a flat fill over the whole design
		space (the dim behind a menu or a help panel) darkens the eyes
		instead, by its opacity (halo_stereo_ui_dim_active): on the UI's
		quad it would be a dark rectangle */
		if (definition->bounds.x0 <= 0 && definition->bounds.y0 <= 0 &&
			definition->bounds.x1 >= 640 && definition->bounds.y1 >= 480 &&
			bitmap->width <= 16 && bitmap->height <= 16 && halo_stereo_ui_dim_active())
		{
			halo_stereo_ui_dim_add(_texture_cache_bitmap_get_hardware_format(bitmap, FALSE, TRUE), alpha);
		}
		else
		{
			/* port: a frame of ui.map's that the menus scale (the Xbox's
			picture of the button settings, in the profile settings' smaller
			box): drawn at their size, from where they place it, in units of
			that size rather than one to a texel. A Custom Edition map's own
			picture (or a stock map's rendered one) is stretched over the
			widget; a stock campaign level's is laid out as the stock pictures are */
			rectangle2d texels = bounds;
			short frame_x, frame_y, frame_width, frame_height;
			boolean shown = TRUE;
			boolean stretched = custom_edition_picture &&
				(custom_edition_maps_campaign_level(widget->animation.current_frame_index) == NONE ||
				custom_edition_maps_picture_rendered(custom_edition_picture));

			if (pc_menu_frame_placement(bitmap, &frame_x, &frame_y, &frame_width, &frame_height))
			{
				/* a negative place slides the picture up and left inside the
				widget (a banner cut from the middle of a taller picture) */
				short shift_x = frame_x < 0 ? -frame_x : 0;
				short shift_y = frame_y < 0 ? -frame_y : 0;

				bounds.x0 += frame_x > 0 ? frame_x : 0;
				bounds.y0 += frame_y > 0 ? frame_y : 0;
				/* port: a taller row frame (tools/xbox_menus.py) is drawn at its
				own height, over the clamp above, so its visible band fills the row */
				if (pc_menu_row_tall(widget->definition_tag_index))
					bounds.y1 = bounds.y0 + frame_height;
				texels.x0 = (short)((long)shift_x * bitmap->width / frame_width);
				texels.y0 = (short)((long)shift_y * bitmap->height / frame_height);
				texels.x1 = (short)(texels.x0 + (long)(bounds.x1 - bounds.x0) * bitmap->width / frame_width);
				texels.y1 = (short)(texels.y0 + (long)(bounds.y1 - bounds.y0) * bitmap->height / frame_height);
				shown = bounds.x1 > bounds.x0 && bounds.y1 > bounds.y0;
			}
			/* port: the menus' redraw draws this frame's texture at the
			display's resolution (port/linux/src/ui_redraw.c) */
			if (shown && !custom_edition_picture)
			{
				ui_redraw_note(tag_get_name(definition->background_bitmap.index),
					_texture_cache_bitmap_get_hardware_format(bitmap, FALSE, FALSE), bitmap->width, bitmap->height);
			}
			if (shown && ui_mouse_key_button(widget) != NONE && !custom_edition_picture)
			{
				short owner = widget->local_player_index == NONE ?
					local_player_index_for_draw_string_and_hack_in_icons : widget->local_player_index;
				pixel32 glyph_color = modulate_pixel32_by_real_alpha(0xff2996ff, alpha);
				/* port: the legend's icon box is taller than its caption's, so the
				glyph, centered in the box, sits below the caption's cap height;
				raise it to center on the capitals */
				rectangle2d glyph_bounds = controller_glyph_legend_raise(&bounds);
				rectangle2d glyph_clip = clip ? controller_glyph_legend_raise(clip) : glyph_bounds;
				halo_controller_glyph_x_global(ui_widget_is_global_x_action(widget));
				if (controller_glyph_draw(owner, NONE, ui_mouse_key_button(widget), &glyph_bounds, clip ? &glyph_clip : NULL, glyph_color, _controller_glyph_blend_menu))
					shown = FALSE;
			}
			if (shown)
			{
				ui_hold_grow_card(widget, &bounds);
				if (!custom_edition_picture)
				{
					short shift = menu_title_touch_shift(widget, bitmap, &bounds, clip);

					bounds.x0 += shift;
					bounds.x1 += shift;
				}
				/* port: a held item glows on its own outline: enlarged faint copies of
				its own frame behind it, then the frame, then a brightening pass */
				if (widget == ui_hold_widget && ui_hold_progress() > 0.0f)
				{
					real hold = ui_hold_progress();
					short ring;

					for (ring = 3; ring >= 1; ring--)
					{
						rectangle2d glow = bounds;

						glow.x0 = (short)(glow.x0 - 2 * ring);
						glow.x1 = (short)(glow.x1 + 2 * ring);
						glow.y0 = (short)(glow.y0 - 2 * ring);
						glow.y1 = (short)(glow.y1 + 2 * ring);
						draw_bitmap_in_rect(bitmap, &glow, stretched ? NULL : &texels, NULL,
							modulate_pixel32_by_real_alpha(0xFFFFFFFF, alpha * hold * 0.45f / (real)ring),
							&multitexture_params, FALSE);
					}
				}
				/* port: full-screen bitmaps (gradients, dims, loading art)
				cover portrait by height; their children keep the menu's fit.
				Clip in design space before the GPU crops the covered picture. */
				boolean portrait_backdrop = halo_screen_width() < 640 &&
					bounds.x0 <= 0 && bounds.y0 <= 0 &&
					bounds.x1 >= 640 && bounds.y1 >= 480;
				/* port: the menu band (gradient.bitmap) carries its two rules at
				rows 65 and 383 of the 480; it takes the menus' own fit so the
				rules stay just outside the title and the legend, and the
				untinted backdrop shows above and below it */
				if (portrait_backdrop)
				{
					char const *backdrop_name = tag_get_name(definition->background_bitmap.index);

					if (backdrop_name && (strstr(backdrop_name, "bitmaps\\gradient") || strstr(backdrop_name, "bitmaps/gradient")))
						portrait_backdrop = FALSE;
				}

				if (portrait_backdrop)
					halo_screen_ui_backdrop(TRUE);
				draw_bitmap_in_rect(
					bitmap,
					&bounds,
					stretched ? NULL : &texels,
					clip,
					color,
					&multitexture_params,
					FALSE);
				if (widget == ui_hold_widget && ui_hold_progress() > 0.0f)
				{
					draw_bitmap_in_rect(bitmap, &bounds, stretched ? NULL : &texels, clip,
						modulate_pixel32_by_real_alpha(0xFFFFFFFF, alpha * ui_hold_progress() * 0.5f),
						&multitexture_params, FALSE);
				}
				if (portrait_backdrop)
					halo_screen_ui_backdrop(FALSE);
				if (!custom_edition_picture)
					menu_title_icon_draw(widget, bitmap, &bounds, clip, color);
			}
		}
		if (use_nifty_plasma_fx)
		{
			ui_plasma_effect_color.alpha = 0.0f;
			ui_plasma_effect_color.red = 0.0f;
			ui_plasma_effect_color.green = 0.0f;
			ui_plasma_effect_color.blue = 0.0f;
		}
	}
	switch (widget->type)
	{
	case _ui_widget_type_text_box:
		if (ui_touch_legend_caption_suppressed(widget, offset)) break;
		{
			/* port: on touch and mouse a commit page's A caption reads SAVE */
			static wchar_t save_text[] = { 'S', 'A', 'V', 'E', 0 };
			wchar_t *original_text = widget->parameters.text_box.text;
			boolean relabel = FALSE;

			if (widget->parent && ui_text_is_commit_caption(original_text))
			{
				struct widget_instance *sibling;
				short owner = widget->local_player_index == NONE ?
					local_player_index_for_draw_string_and_hack_in_icons : widget->local_player_index;

				for (sibling = widget->parent->child; sibling && !relabel; sibling = sibling->next)
					relabel = sibling != widget && ui_legend_save_caption(sibling) == widget &&
						controller_glyph_hidden(owner, _gamepad_analog_button_a) != 0;
			}
			/* (drawn as an override: the caption's own text is a pool block its string list refreshes) */
			if (relabel) ui_text_box_render_override = save_text;
			short shift = ui_touch_legend_caption_shift(widget);
			rectangle2d shifted_clip;

			offset.x += shift;
			/* the shifted caption must not be clipped by the box it left */
			if (shift)
			{
				shifted_clip = clip_rect ? *clip_rect : definition->bounds;
				shifted_clip.x0 = MIN(shifted_clip.x0, shifted_clip.x0 + shift);
				shifted_clip.x1 = MAX(shifted_clip.x1, shifted_clip.x1 + shift);
				clip_rect = &shifted_clip;
			}
			widget_instance_render_text_box(
				widget,
				definition,
				clip_rect,
				offset,
				widget_instance_text_box_is_focused(widget));
			ui_text_box_render_override = NULL;
		}
		break;

	case _ui_widget_type_spinner_list:
		widget_instance_render_spinner_list(
			widget,
			definition,
			clip_rect,
			offset,
			focus);
		if (TEST_FLAG(
				definition->list_flags,
				_list_items_generated_from_string_list_tag) &&
			definition->child_widgets.count == 0)
		{
			render_children = FALSE;
		}
		break;

	case _ui_widget_type_column_list:
		widget_instance_render_column_list(
			widget,
			definition,
			clip_rect,
			offset,
			focus);
		render_children = !TEST_FLAG(
			definition->list_flags,
			_list_items_generated_in_code);
		break;
	}
	if (render_children && ui_widget_is_carousel(widget))
	{
		ui_carousel_render(widget, offset);
		render_children = FALSE;
	}

	if (render_children)
	{
		short window = widget->type == _ui_widget_type_column_list ?
			pc_menu_list_window(widget->definition_tag_index) : 0;

		if (window > 0)
			window_children = column_list_window_begin(widget, definition, window, window_saved);
		for (child = widget->child; child; child = child->next)
		{
			focus = widget->type == _ui_widget_type_spinner_list || widget->type == _ui_widget_type_column_list ?
				ui_list_child_draws_focus(widget, child) : child == widget->focused_child;
			use_nifty_plasma_fx = focus &&
				(widget->type == _ui_widget_type_spinner_list ||
				widget->type == _ui_widget_type_column_list);
			widget_instance_render_recursive(
				child,
				clip_rect,
				offset,
				focus,
				use_nifty_plasma_fx);
		}
		if (window_children > 0)
			column_list_window_end(widget, window_children, window_saved);
	}

	return;
}

/* port: rendering and HUD suppression must agree even when controller and local player indices differ. */
static boolean ui_widget_should_render(
	struct widget_instance const *widget,
	long widget_index,
	short local_player_index)
{
	boolean should_render = FALSE;

	if (widget)
	{
		if (widget->render_regardless_of_controller_index == TRUE)
		{
			should_render = TRUE;
		}
		else if (widget->widget_is_error_dialog == TRUE)
		{
			if (widget->local_player_index == local_player_index ||
				widget->local_player_index == NONE ||
				local_player_index == NONE ||
				we_are_at_the_main_menu)
			{
				should_render = TRUE;
			}
		}
		else if (widget->local_player_index == NONE && widget_index == 0)
		{
			should_render = TRUE;
		}
		else if (widget->local_player_index == local_player_index)
		{
			should_render = TRUE;
		}
	}
	return should_render;
}

boolean ui_widget_port_screen_visible(
	short local_player_index)
{
	long widget_index;

	if (virtual_keyboard_active())
		return FALSE;
	local_player_index = PIN(local_player_index, 0, MAXIMUM_NUMBER_OF_LOCAL_PLAYERS - 1);
	for (widget_index = 0; widget_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; widget_index++)
	{
		if (ui_widget_should_render(widget_globals.active_widgets[widget_index], widget_index, local_player_index))
			return TRUE;
	}
	return FALSE;
}

void render_ui_widgets_postgame(
	short local_player_index,
	rectangle2d *window_bounds)
{
	point2d const offsets[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS][MAXIMUM_NUMBER_OF_LOCAL_PLAYERS] =
	{
		{ { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } },
		{ { 0, 0 }, { 0, 240 }, { 0, 0 }, { 0, 0 } },
		{ { 0, 0 }, { 0, 240 }, { 320, 240 }, { 0, 0 } },
		{ { 0, 0 }, { 320, 0 }, { 0, 240 }, { 320, 240 } }
	};
	rectangle2d bounds;
	long widget_index;

	if (virtual_keyboard_active())
		return;
	local_player_index = PIN(
		local_player_index,
		0,
		MAXIMUM_NUMBER_OF_LOCAL_PLAYERS - 1);
	for (widget_index = 0;
		widget_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS;
		widget_index++)
	{
		struct widget_instance *widget = widget_globals.active_widgets[widget_index];
		boolean should_render = ui_widget_should_render(widget, widget_index, local_player_index);

		if (should_render)
		{
			bounds.x0 = 0;
			bounds.y0 = 0;
			bounds.x1 = window_bounds->x1 - window_bounds->x0;
			bounds.y1 = window_bounds->y1 - window_bounds->y0;
			widget_instance_render_recursive(
				widget_globals.active_widgets[widget_index],
				&bounds,
				offsets[local_player_count() - 1][local_player_index],
				TRUE,
				FALSE);
		}
	}

	return;
}

void render_ui_widgets(
	short local_player_index,
	rectangle2d const *window_bounds)
{
	rectangle2d bounds;
	long widget_index;

	match_assert(
		"c:\\halo\\SOURCE\\interface\\ui_widget.c",
		911,
		window_bounds != NULL);
	local_player_index_for_draw_string_and_hack_in_icons =
		local_player_index == NONE ? 0 : local_player_index;
	if (bink_playback_ui_rendering_inhibited())
		return;
	if (!virtual_keyboard_active())
	{
		local_player_index = PIN(
			local_player_index,
			0,
			MAXIMUM_NUMBER_OF_LOCAL_PLAYERS - 1);
		for (widget_index = 0;
			widget_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS;
			widget_index++)
		{
			struct widget_instance *widget = widget_globals.active_widgets[widget_index];
			boolean should_render = ui_widget_should_render(widget, widget_index, local_player_index);

			if (should_render)
			{
				point2d offset;

				bounds.x0 = 0;
				bounds.y0 = 0;
				bounds.x1 = window_bounds->x1 - window_bounds->x0;
				bounds.y1 = window_bounds->y1 - window_bounds->y0;
				/* port: portrait fits the entire design after widget clipping. */
				if (halo_screen_width() < 640)
				{
					bounds.x1 = 640;
					bounds.y1 = 480;
				}
				offset.x = 0;
				offset.y = 0;
				/* the mouse drives the first player's menus */
				ui_mouse_noting_targets = widget->local_player_index == NONE ||
					widget->local_player_index == 0;
				/* port: whether the menus' redraw draws this screen
				(port/linux/src/ui_redraw.c: not the main menu) */
				ui_widget_menu_content(widget);
				if (ui_mouse_noting_targets)
				{
					ui_touch_back_seen = FALSE;
					ui_touch_title_seen = FALSE;
					ui_touch_x_seen = FALSE;
					ui_touch_x_playlist_widget = NULL;
					ui_touch_back_generic = ui_touch_screen_back_acts(widget);
					ui_touch_screen_x_scan(widget);
				}
				ui_redraw_begin_screen(tag_get_name(widget->definition_tag_index));
				widget_instance_render_recursive(
					widget_globals.active_widgets[widget_index],
					&bounds,
					offset,
					TRUE,
					FALSE);
				if (ui_mouse_noting_targets) ui_dropdown_draw();
				ui_redraw_end_screen();
				/* the popup draws in the menu window's space, like the rows it lifts and their
				tap targets, so its glow sits on the drawn row where the safe-area zoom applies */
				if (ui_mouse_noting_targets)
				{
					halo_screen_menu_window(1);
					ui_hold_popup_draw(&bounds);
					halo_screen_menu_window(0);
				}
				if (ui_mouse_noting_targets)
					ui_touch_back_chevron_draw(widget, &bounds);
				if (ui_mouse_noting_targets)
					ui_mouse_menu_drawn = TRUE;
				ui_mouse_noting_targets = FALSE;
				if (widget_globals.debug_show_path)
				{
					real_argb_color color = { 1.0f, 1.0f, 1.0f, 1.0f };

					bounds.x0 += 32;
					bounds.x1 += 32;
					bounds.y0 += 32;
					bounds.y1 += 32;
					draw_string_set_draw_mode(
						tag_loaded(FONT_GROUP_TAG, "ui\\small_ui"),
						NONE,
						0,
						0,
						&color);
					rasterizer_draw_string(
						&bounds,
						NULL,
						NULL,
						0,
						tag_get_name(
							widget_globals.active_widgets[widget_index]->definition_tag_index));
				}
			}
		}
		if (widget_globals.fade_to_black >= 0.0f &&
			widget_globals.fade_to_black <= 1.0f)
		{
			real alpha;

			/* the whole screen, around the centered 640 columns (outside the menu window) */
			struct halo_menu_screen screen;

			halo_screen_menu_screen_rect(FALSE, &screen);
			bounds.x0 = (short)MIN(0, screen.x0);
			bounds.x1 = (short)MAX(640, screen.x1);
			bounds.y0 = 0;
			bounds.y1 = 480;
			if (widget_globals.fade_to_black >= 0.95f)
				widget_globals.fade_to_black = 1.0f;
			alpha = widget_globals.fade_to_black * 255.0f;
			/* port: a fade covers the portrait backdrop as well as the list. */
			if (halo_screen_width() < 640)
			{
				bounds.x0 = 0;
				bounds.x1 = 640;
				halo_screen_ui_backdrop(TRUE);
			}
			draw_quad(&bounds, fast_ftol(alpha) << 24);
			if (halo_screen_width() < 640)
				halo_screen_ui_backdrop(FALSE);
		}
	}
	else
	{
		/* port: the in-game keyboard is a menu screen too: the safe-area zoom keeps it clear of
		a camera cutout (halo_screen_menu_window), over the game as well as in the menus, with
		the menus' usual content span (it has no widgets to give one) */
		halo_screen_menu_content(MENU_CONTENT_TOP, MENU_CONTENT_BOTTOM);
		halo_screen_menu_window(1);
		virtual_keyboard_render();
		halo_screen_menu_window(0);
		halo_screen_menu_content(0.0f, 0.0f);
	}

	return;
}

/* ---------- private code */


static long spinner_string_list_extra_count(
	long string_list_index)
{
	if (string_list_index != NONE && !csstrcmp(tag_get_name(string_list_index),
		"ui\\shell\\main_menu\\settings_select\\multiplayer_setup\\playlist_edit\\slayer_edit\\var_kills_to_win"))
	{
		return NUMBEROF(kills_to_win_extra_strings);
	}
	return 0;
}

/* a spinner's items of its string list's own (the descriptions of a list of
spinners count those: ui_widget_game_data_input_functions.c) */
short ui_widget_spinner_own_item_count(
	struct widget_instance *spinner)
{
	struct ui_widget_definition *definition = ui_widget_definition_get(spinner->definition_tag_index);

	return (short)(spinner->parameters.list.number_of_items -
		spinner_string_list_extra_count(definition->text_label_string_list.index));
}

/* the string list index of the description of a spinner's extra item, or
NONE for an item of its own */
short ui_widget_spinner_extra_description(
	struct widget_instance *spinner,
	short item_index)
{
	short own = ui_widget_spinner_own_item_count(spinner);

	if (item_index < own)
		return NONE;
	return (short)(SPINNER_EXTRA_DESCRIPTION_BASE + item_index - own);
}

/* a string of a string list, or of its extra strings past the tag's own */
static wchar_t *spinner_string_list_get_string(
	long string_list_index,
	short string_index)
{
	struct string_list *string_list = unicode_string_list_definition_get(string_list_index);

	if (string_list && string_index >= string_list->strings.count &&
		string_index - string_list->strings.count < spinner_string_list_extra_count(string_list_index))
	{
		return (wchar_t *)kills_to_win_extra_strings[string_index - string_list->strings.count];
	}
	return unicode_string_list_get_string(string_list_index, string_index);
}

static void widget_instance_render_column_list(
	struct widget_instance *widget,
	struct ui_widget_definition *definition,
	rectangle2d *clip_rect,
	point2d offset,
	boolean focus)
{
	if (widget->parameters.list.extended_description)
	{
		widget->parameters.list.extended_description->alpha_modifier =
			widget_instance_get_cumulative_alpha_modifier(widget);
		widget_instance_render_recursive(
			widget->parameters.list.extended_description,
			clip_rect,
			offset,
			FALSE,
			TRUE);
	}
	if (TEST_FLAG(definition->list_flags, _list_items_generated_in_code))
	{
		struct widget_instance *child;
		long item_index = 0;

		for (child = widget->child; child; child = child->next)
		{
			if (item_index >= widget->parameters.list.number_of_items)
				break;
			widget_instance_render_recursive(
				child,
				clip_rect,
				offset,
				focus,
				ui_widget_port_draws_focus(HALO_MENU_FOCUS_ROW, item_index == widget->parameters.list.selected_index));
			item_index++;
		}
	}
	widget->parameters.list.last_list_tab_direction = 0;

	return;
}

static __inline void widget_instance_update_animation_parameters(
	struct widget_instance *widget)
{
	widget->animation.first_frame_index =
		FLOOR(widget->animation.first_frame_index, 0);
	widget->animation.last_frame_index =
		FLOOR(widget->animation.last_frame_index, 0);

	return;
}

static __inline void spinner_list_update(
	struct widget_instance *widget)
{
	struct widget_instance *child;

	for (child = widget->child; child; child = child->next)
	{
		child->animation.current_frame_index = 0;
		if (ui_list_child_draws_focus(widget, child) &&
			child->animation.number_of_sprite_frames == 2)
		{
			child->animation.current_frame_index = 1;
		}
	}

	return;
}

/* the definition is passed by January's only call site but is not read; the
spinner list and the column list keep separate updaters rather than sharing one
parameterised helper */

static void column_list_update(
	struct widget_instance *widget,
	struct ui_widget_definition *definition)
{
	struct widget_instance *child;

	for (child = widget->child; child; child = child->next)
	{
		if (ui_list_child_draws_focus(widget, child))
		{
			if (child->animation.number_of_sprite_frames == 2)
				child->animation.current_frame_index = 1;
		}
		else if (child->animation.number_of_sprite_frames == 2)
		{
			child->animation.current_frame_index = 0;
		}
	}

	return;
}

/* port: child tabs and item steps share a column list's selection. A stale index can make the next item step
wrap before the focused row reaches the end. Pointer focus uses the same physical child indices. */
static void widget_instance_port_focus_child(
	struct widget_instance *widget,
	struct widget_instance *child)
{
	struct widget_instance *item;
	short index = 0;

	widget->focused_child = child;
	if (widget->type == _ui_widget_type_column_list && pc_menu_tag(widget->definition_tag_index))
	{
		for (item = widget->child; item && item != child; item = item->next)
			index++;
		widget->parameters.list.selected_index = index;
	}
}

static void widget_instance_tab_to_next_valid_widget(
	struct widget_instance *widget)
{
	struct widget_instance *child;

	/* port: three-card spinners navigate items, never wrap child slots. */
	if (ui_widget_is_carousel(widget))
	{
		ui_carousel_controller_move(widget, 1);
		return;
	}

	if (widget->focused_child && widget->focused_child->next)
		child = widget->focused_child->next;
	else
		child = widget->child;
	while (child && child != widget->focused_child)
	{
		struct ui_widget_definition *definition =
			ui_widget_definition_get(child->definition_tag_index);

		if ((definition->event_handlers.count > 0 ||
			TEST_FLAG(definition->flags, _widget_pass_unhandled_events_to_children_bit) ||
			widget->type == _ui_widget_type_spinner_list ||
			widget->type == _ui_widget_type_column_list) &&
			/* port: over the PC version's labels and hidden rows */
			!(widget->type == _ui_widget_type_column_list && pc_menu_tag(widget->definition_tag_index) &&
				widget_instance_port_is_label(child)))
		{
			widget_instance_port_focus_child(widget, child);
			break;
		}
		child = child->next;
		if (!child)
			child = widget->child;
	}

	return;
}

static void widget_instance_tab_to_previous_valid_widget(
	struct widget_instance *widget)
{
	struct widget_instance *child;

	/* port: three-card spinners navigate items, never wrap child slots. */
	if (ui_widget_is_carousel(widget))
	{
		ui_carousel_controller_move(widget, -1);
		return;
	}

	if (widget->focused_child)
	{
		if (widget->focused_child->previous)
			child = widget->focused_child->previous;
		else
			child = widget_instance_get_tail_child_widget(widget);
	}
	else
	{
		child = widget->child->previous;
		if (!child)
			child = widget->child;
	}
	while (child && child != widget->focused_child)
	{
		struct ui_widget_definition *definition =
			ui_widget_definition_get(child->definition_tag_index);

		if ((definition->event_handlers.count > 0 ||
			TEST_FLAG(definition->flags, _widget_pass_unhandled_events_to_children_bit) ||
			widget->type == _ui_widget_type_spinner_list ||
			widget->type == _ui_widget_type_column_list) &&
			/* port: over the PC version's labels and hidden rows */
			!(widget->type == _ui_widget_type_column_list && pc_menu_tag(widget->definition_tag_index) &&
				widget_instance_port_is_label(child)))
		{
			widget_instance_port_focus_child(widget, child);
			break;
		}
		if (child->previous)
			child = child->previous;
		else
			child = widget_instance_get_tail_child_widget(widget);
	}

	return;
}

/* port: whether a widget of the local player (NONE: any) takes the
controller's events. In co-op's menus (Multiplayer's CO-OP CAMPAIGN,
port/linux/game/menu_functions.c) the screens it shares with one player's
campaign, New Game's levels and the difficulty, are player 1's (their rows
the first controller's), and either player's controller uses them: player 1's
is the one that chose co-op, player 2's the one that chose their profile */
static boolean widget_takes_events_of_controller(
	struct widget_instance const *widget,
	short controller_index)
{
	short player;

	if (widget->local_player_index == NONE || widget->local_player_index == controller_index)
		return TRUE;
	if (widget->local_player_index != 0 || !we_are_at_the_main_menu || player_spawn_count < 2 ||
		controller_index < 0 || controller_index >= MAXIMUM_NUMBER_OF_LOCAL_PLAYERS)
	{
		return FALSE;
	}
	for (player = 0; player < 2; player++)
	{
		if (player_ui_get_single_player_local_player_controller(player) == controller_index)
			return TRUE;
	}
	return FALSE;
}

static void widget_instance_process_one_event_recursive(
	struct widget_instance *widget,
	struct ui_widget_definition *definition,
	struct event_record *event,
	boolean *return_widget_deleted)
{
	boolean event_handled = FALSE;
	boolean widget_deleted = FALSE;
	boolean event_for_this_widget = widget_takes_events_of_controller(widget, event->controller_index);
	long audio_feedback = _ui_audio_feedback_none;

	match_assert(
		"c:\\halo\\SOURCE\\interface\\ui_widget.c",
		3067,
		widget && definition && event && return_widget_deleted);
	if (event->type == _event_type_button &&
		event->data.button.value > 1 &&
		event->controller_index >= 0 &&
		event->controller_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS &&
		event->data.button.index >= _widget_event_dpad_up &&
		event->data.button.index <= _widget_event_dpad_right &&
		widget_globals.current_system_milliseconds -
			dpad_event_times[event->controller_index]
				[event->data.button.index - _widget_event_dpad_up] >=
			DPAD_EVENT_REPEAT_MILLISECONDS)
	{
		event->data.button.value = 1;
	}
	/* port: navigation remains available on fixed graphics rows; only editing is refused. */
	if (event_for_this_widget && widget_instance_port_refuse_graphics_edit(widget, event))
	{
		*return_widget_deleted = FALSE;
		return;
	}
	if (widget->close_if_local_player_controller_present == TRUE)
	{
		if (widget->local_player_index >= 0 &&
			widget->local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS)
		{
			if (input_has_gamepad(widget->local_player_index))
			{
				ui_widget_delete(widget_instance_get_topmost_parent(widget));
				widget_deleted = TRUE;
			}
		}
		else
		{
			short controller_index;

			match_assert(
				"c:\\halo\\SOURCE\\interface\\ui_widget.c",
				3107,
				widget->local_player_index==NONE);
			for (controller_index = 0;
				controller_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS;
				controller_index++)
			{
				if (input_has_gamepad(controller_index))
				{
					ui_widget_delete(widget_instance_get_topmost_parent(widget));
					widget_deleted = TRUE;
					break;
				}
			}
		}
	}
	if (event_for_this_widget && !widget_deleted)
	{
		if (event->type == _event_type_button &&
			event->data.button.value == 1)
		{
			boolean handled_by_event_handler = FALSE;
			long handler_index;

			if (event->data.button.index == _widget_event_back_button)
			{
				for (handler_index = 0;
					handler_index < definition->event_handlers.count;
					handler_index++)
				{
					struct ui_widget_event_handler_reference *handler =
						(struct ui_widget_event_handler_reference *)
							definition->event_handlers.address + handler_index;

					if (handler->event_type == _widget_event_back_button)
					{
						handled_by_event_handler = TRUE;
						break;
					}
				}
			}
			else if (event->data.button.index == _widget_event_b_button)
			{
				for (handler_index = 0;
					handler_index < definition->event_handlers.count;
					handler_index++)
				{
					struct ui_widget_event_handler_reference *handler =
						(struct ui_widget_event_handler_reference *)
							definition->event_handlers.address + handler_index;

					if (handler->event_type == _widget_event_b_button)
					{
						handled_by_event_handler = TRUE;
						break;
					}
				}
			}
			else
			{
				handled_by_event_handler = TRUE;
			}
			if (!handled_by_event_handler)
			{
				widget_instance_go_back_to_previous(widget);
				audio_feedback = _ui_audio_feedback_back;
				widget_deleted = TRUE;
				event_handled = TRUE;
			}
		}
	}
	if (!widget_deleted)
	{
		if (widget->milliseconds_to_auto_close > 0)
		{
			if (widget_globals.current_system_milliseconds - widget->creation_time >=
				widget->auto_close_fade_time + widget->milliseconds_to_auto_close)
			{
				ui_widget_delete(widget_instance_get_topmost_parent(widget));
				widget_deleted = TRUE;
			}
			else if (widget->auto_close_fade_time > 0)
			{
				long faded_milliseconds =
					widget_globals.current_system_milliseconds -
						widget->creation_time -
						widget->milliseconds_to_auto_close;

				if (faded_milliseconds > 0)
				{
					widget->alpha_modifier = 1.0f - (real)faded_milliseconds /
						(real)widget->auto_close_fade_time;
				}
			}
		}
	}
	if (!widget_deleted)
	{
		widget_instance_update_animation_parameters(widget);
		if (widget->type == _ui_widget_type_spinner_list)
			spinner_list_update(widget);
		else if (widget->type == _ui_widget_type_column_list)
			column_list_update(widget, definition);
		if (event_for_this_widget)
		{
			if (!event_handled &&
				TEST_FLAG(definition->flags, _widget_dpad_updown_tabs_thru_children_bit) &&
				widget->focused_child &&
				!widget_deleted)
			{
				if (event->type == _event_type_button &&
					event->data.button.value == 1)
				{
					switch (event->data.button.index)
					{
					case _widget_event_dpad_up:
						widget_instance_tab_to_previous_valid_widget(widget);
						if (audio_feedback == _ui_audio_feedback_none)
							audio_feedback = _ui_audio_feedback_cursor;
						event_handled = TRUE;
						break;
					case _widget_event_dpad_down:
						widget_instance_tab_to_next_valid_widget(widget);
						if (audio_feedback == _ui_audio_feedback_none)
							audio_feedback = _ui_audio_feedback_cursor;
						event_handled = TRUE;
						break;
					}
				}
				else if (event->type == _event_type_left_stick)
				{
					switch (event->data.stick.y)
					{
					case SHORT_MIN:
						widget_instance_tab_to_next_valid_widget(widget);
						if (audio_feedback == _ui_audio_feedback_none)
							audio_feedback = _ui_audio_feedback_cursor;
						event_handled = TRUE;
						break;
					case SHORT_MAX:
						widget_instance_tab_to_previous_valid_widget(widget);
						if (audio_feedback == _ui_audio_feedback_none)
							audio_feedback = _ui_audio_feedback_cursor;
						event_handled = TRUE;
						break;
					}
				}
			}
			if (!event_handled &&
				TEST_FLAG(definition->flags, _widget_dpad_leftright_tabs_thru_children_bit) &&
				widget->focused_child &&
				!widget_deleted)
			{
				if (event->type == _event_type_button &&
					event->data.button.value == 1)
				{
					switch (event->data.button.index)
					{
					case _widget_event_dpad_left:
						widget_instance_tab_to_previous_valid_widget(widget);
						if (audio_feedback == _ui_audio_feedback_none)
							audio_feedback = _ui_audio_feedback_cursor;
						event_handled = TRUE;
						break;
					case _widget_event_dpad_right:
						widget_instance_tab_to_next_valid_widget(widget);
						if (audio_feedback == _ui_audio_feedback_none)
							audio_feedback = _ui_audio_feedback_cursor;
						event_handled = TRUE;
						break;
					}
				}
				else if (event->type == _event_type_left_stick)
				{
					switch (event->data.stick.x)
					{
					case SHORT_MIN:
						widget_instance_tab_to_previous_valid_widget(widget);
						if (audio_feedback == _ui_audio_feedback_none)
							audio_feedback = _ui_audio_feedback_cursor;
						event_handled = TRUE;
						break;
					case SHORT_MAX:
						widget_instance_tab_to_next_valid_widget(widget);
						if (audio_feedback == _ui_audio_feedback_none)
							audio_feedback = _ui_audio_feedback_cursor;
						event_handled = TRUE;
						break;
					}
				}
			}
			if (TEST_FLAG(definition->flags, _widget_dpad_updown_tabs_thru_list_items_bit) &&
				(widget->type == _ui_widget_type_spinner_list ||
					widget->type == _ui_widget_type_column_list) &&
				!event_handled &&
				!widget_deleted)
			{
				if (event->type == _event_type_button &&
					event->data.button.value == 1)
				{
					switch (event->data.button.index)
					{
					case _widget_event_dpad_up:
						widget_event_function_list_widget_goto_previous_item(
							widget,
							event,
							&widget_deleted);
						if (audio_feedback == _ui_audio_feedback_none)
							audio_feedback = _ui_audio_feedback_cursor;
						event_handled = TRUE;
						break;
					case _widget_event_dpad_down:
						widget_event_function_list_widget_goto_next_item(
							widget,
							event,
							&widget_deleted);
						if (audio_feedback == _ui_audio_feedback_none)
							audio_feedback = _ui_audio_feedback_cursor;
						event_handled = TRUE;
						break;
					}
				}
				else if (event->type == _event_type_left_stick)
				{
					switch (event->data.stick.y)
					{
					case SHORT_MIN:
						widget_event_function_list_widget_goto_next_item(
							widget,
							event,
							&widget_deleted);
						if (audio_feedback == _ui_audio_feedback_none)
							audio_feedback = _ui_audio_feedback_cursor;
						event_handled = TRUE;
						break;
					case SHORT_MAX:
						widget_event_function_list_widget_goto_previous_item(
							widget,
							event,
							&widget_deleted);
						if (audio_feedback == _ui_audio_feedback_none)
							audio_feedback = _ui_audio_feedback_cursor;
						event_handled = TRUE;
						break;
					}
				}
			}
			if (TEST_FLAG(definition->flags, _widget_dpad_leftright_tabs_thru_list_items_bit) &&
				(widget->type == _ui_widget_type_spinner_list ||
					widget->type == _ui_widget_type_column_list) &&
				!event_handled &&
				!widget_deleted)
			{
				if (event->type == _event_type_button &&
					event->data.button.value == 1)
				{
					switch (event->data.button.index)
					{
					case _widget_event_dpad_left:
						widget_event_function_list_widget_goto_previous_item(
							widget,
							event,
							&widget_deleted);
						if (audio_feedback == _ui_audio_feedback_none)
							audio_feedback = _ui_audio_feedback_cursor;
						event_handled = TRUE;
						break;
					case _widget_event_dpad_right:
						widget_event_function_list_widget_goto_next_item(
							widget,
							event,
							&widget_deleted);
						if (audio_feedback == _ui_audio_feedback_none)
							audio_feedback = _ui_audio_feedback_cursor;
						event_handled = TRUE;
						break;
					}
				}
				else if (event->type == _event_type_left_stick)
				{
					switch (event->data.stick.x)
					{
					case SHORT_MIN:
						widget_event_function_list_widget_goto_previous_item(
							widget,
							event,
							&widget_deleted);
						if (audio_feedback == _ui_audio_feedback_none)
							audio_feedback = _ui_audio_feedback_cursor;
						event_handled = TRUE;
						break;
					case SHORT_MAX:
						widget_event_function_list_widget_goto_next_item(
							widget,
							event,
							&widget_deleted);
						if (audio_feedback == _ui_audio_feedback_none)
							audio_feedback = _ui_audio_feedback_cursor;
						event_handled = TRUE;
						break;
					}
				}
			}
		}
	}
	if (event_for_this_widget)
	{
		long handler_index;

		for (handler_index = 0;
			handler_index < definition->event_handlers.count;
			handler_index++)
		{
			struct ui_widget_event_handler_reference *handler;
			boolean handler_matches = FALSE;

			if (widget_deleted)
				break;
			handler = (struct ui_widget_event_handler_reference *)
				definition->event_handlers.address + handler_index;
			switch (event->type)
			{
			case _event_type_left_stick:
				switch (handler->event_type)
				{
				case _widget_event_left_stick_up:
					handler_matches = event->data.stick.y == SHORT_MAX;
					break;
				case _widget_event_left_stick_down:
					handler_matches = event->data.stick.y == SHORT_MIN;
					break;
				case _widget_event_left_stick_left:
					handler_matches = event->data.stick.x == SHORT_MIN;
					break;
				case _widget_event_left_stick_right:
					handler_matches = event->data.stick.x == SHORT_MAX;
					break;
				}
				break;
			case _event_type_right_stick:
				switch (handler->event_type)
				{
				case _widget_event_right_stick_up:
					handler_matches = event->data.stick.y == SHORT_MAX;
					break;
				case _widget_event_right_stick_down:
					handler_matches = event->data.stick.y == SHORT_MIN;
					break;
				case _widget_event_right_stick_left:
					handler_matches = event->data.stick.x == SHORT_MIN;
					break;
				case _widget_event_right_stick_right:
					handler_matches = event->data.stick.x == SHORT_MAX;
					break;
				}
				break;
			case _event_type_button:
				handler_matches = handler->event_type == event->data.button.index &&
					event->data.button.value == 1;
				break;
			}
			if (handler_matches)
			{
				event_handled = TRUE;
				event_handler_dispatch(
					widget,
					definition,
					event,
					handler,
					&widget_deleted);
			}
		}
	}
	match_vwarn(
		"c:\\halo\\SOURCE\\interface\\ui_widget.c",
		3477,
		!TEST_FLAG(definition->flags, _widget_pass_handled_events_to_all_children_bit) ||
			TEST_FLAG(definition->flags, _widget_pass_unhandled_events_to_children_bit),
		"if the _widget_pass_handled_events_to_all_children_bit flag is checked, _widget_pass_unhandled_events_to_children_bit must also be checked for it to work");
	if ((TEST_FLAG(definition->flags, _widget_pass_handled_events_to_all_children_bit) ||
			!event_handled) &&
		(TEST_FLAG(definition->flags, _widget_pass_unhandled_events_to_children_bit) ||
			TEST_FLAG(definition->flags, _widget_pass_unhandled_events_to_all_children_bit)) &&
		!widget_deleted)
	{
		if (TEST_FLAG(definition->flags, _widget_pass_unhandled_events_to_all_children_bit))
		{
			struct widget_instance *child;

			for (child = widget->child; child; child = child->next)
			{
				if (widget_takes_events_of_controller(child, event->controller_index))
				{
					widget_instance_process_one_event_recursive(
						child,
						ui_widget_definition_get(child->definition_tag_index),
						event,
						&widget_deleted);
					if (widget_deleted == TRUE)
						break;
				}
			}
		}
		else if (widget->focused_child)
		{
			if (widget_takes_events_of_controller(widget->focused_child, event->controller_index))
			{
				widget_instance_process_one_event_recursive(
					widget->focused_child,
					ui_widget_definition_get(
						widget->focused_child->definition_tag_index),
					event,
					&widget_deleted);
			}
		}
	}
	if (widget_deleted == TRUE &&
		TEST_FLAG(definition->flags, _widget_return_to_main_menu_if_no_history_bit))
	{
		long widget_index;

		for (widget_index = 0;
			widget_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS;
			widget_index++)
		{
			if (widget_globals.active_widgets[widget_index])
				break;
		}
		if (widget_index == MAXIMUM_NUMBER_OF_LOCAL_PLAYERS)
			main_goto_main_menu();
	}
	if (event->type == _event_type_button &&
		event->data.button.value == 1 &&
		event->controller_index >= 0 &&
		event->controller_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS &&
		event->data.button.index >= _widget_event_dpad_up &&
		event->data.button.index <= _widget_event_dpad_right)
	{
		dpad_event_times[event->controller_index]
			[event->data.button.index - _widget_event_dpad_up] =
			widget_globals.current_system_milliseconds;
	}
	ui_play_audio_feedback_sound(audio_feedback);
	*return_widget_deleted = widget_deleted;

	return;
}

static boolean ui_check_for_pause_game(
	void)
{
	boolean pause_pressed = FALSE;
	boolean network_game = network_game_is_active();
	short controller_index = NONE;

	if (game_in_progress() &&
		!cinematic_in_progress() &&
		game_connection() != _game_connection_film_playback &&
		!we_are_at_the_main_menu &&
		widget_globals.pause_disabled_ticks == 0)
	{
		long gamepad_index;

		for (gamepad_index = 0;
			gamepad_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS;
			gamepad_index++)
		{
			if (input_has_gamepad(gamepad_index) &&
				local_player_exists(gamepad_index) &&
				input_get_gamepad_state(gamepad_index)->
					buttons[_gamepad_binary_button_start] == 1)
			{
				pause_pressed = TRUE;
				controller_index = gamepad_index;
				break;
			}
		}
	}
	if (pause_pressed)
	{
		boolean pressed_by_first_local_player = TRUE;
		short local_player_count = 0;
		short pressing_local_player_index = NONE;
		short local_player_index;

		for (local_player_index = local_player_get_next(NONE);
			local_player_index != NONE;
			local_player_index = local_player_get_next(local_player_index))
		{
			if (local_player_index == controller_index)
			{
				pressing_local_player_index = controller_index;
				if (local_player_count >= 1)
					pressed_by_first_local_player = FALSE;
			}
			local_player_count++;
		}
		if (network_game)
		{
			if (game_engine_allow_pause() &&
				pressing_local_player_index == controller_index)
			{
				if (!widget_globals.active_widgets[controller_index])
				{
					struct network_game_client *client = global_network_game_client_get();
					struct network_game *network_game_data =
						network_game_client_get_game(client);
					short machine_index =
						network_game_client_get_machine_index(client);
					char const *widget_name;

					/* port: a campaign map has only the campaign's pause screen */
					if (network_coop_active())
						widget_name = "ui\\shell\\solo_game\\pause_game\\pause_game";
					else switch (local_player_count)
					{
					case 1:
						widget_name =
							"ui\\shell\\multiplayer_game\\pause_game\\1p_pause_game";
						break;
					case 2:
						widget_name =
							"ui\\shell\\multiplayer_game\\pause_game\\2p_pause_game";
						break;
					case 3:
						widget_name = pressed_by_first_local_player == TRUE
							? "ui\\shell\\multiplayer_game\\pause_game\\2p_pause_game"
							: "ui\\shell\\multiplayer_game\\pause_game\\4p_pause_game";
						break;
					case 4:
						widget_name =
							"ui\\shell\\multiplayer_game\\pause_game\\4p_pause_game";
						break;
					default:
						error(
							_error_silent,
							"invalid local player count for multiplayer game");
						widget_name = NULL;
						break;
					}
					if (widget_name &&
						!ui_widget_load_by_name_or_tag(
							widget_name,
							NONE,
							NULL,
							controller_index,
							NONE,
							NONE,
							NONE))
					{
						error(
							_error_silent,
							"failed to load multiplayer pause game window");
					}
				}
				else
				{
					ui_widget_delete(widget_globals.active_widgets[controller_index]);
				}
			}
		}
		else
		{
			switch (local_player_count)
			{
			case 0:
			case 1:
				if (widget_globals.active_widgets[controller_index])
				{
					if (game_time_get_paused() == TRUE)
						ui_widgets_close_all();
					/* port: (and a multiplayer map's own, below, which pauses
					nothing, closes as in a multiplayer game) */
					else if (tag_loaded(UI_WIDGET_DEFINITION_TAG, "ui\\shell\\solo_game\\pause_game\\pause_game") == NONE)
						ui_widget_delete(widget_globals.active_widgets[controller_index]);
				}
				/* port: a multiplayer map played alone (New Game's MULTIPLAYER
				maps) has no campaign pause screen, but its own (LEAVE GAME
				goes to the main menu: network_game_remove_local_player) */
				else if (tag_loaded(UI_WIDGET_DEFINITION_TAG, "ui\\shell\\solo_game\\pause_game\\pause_game") == NONE &&
					tag_loaded(UI_WIDGET_DEFINITION_TAG, "ui\\shell\\multiplayer_game\\pause_game\\1p_pause_game") != NONE)
				{
					if (!ui_widget_load_by_name_or_tag(
						"ui\\shell\\multiplayer_game\\pause_game\\1p_pause_game",
						NONE,
						NULL,
						controller_index,
						NONE,
						NONE,
						NONE))
					{
						error(
							_error_silent,
							"failed to load multiplayer pause game window");
					}
				}
				else if (!ui_widget_load_by_name_or_tag(
					"ui\\shell\\solo_game\\pause_game\\pause_game",
					NONE,
					NULL,
					controller_index,
					NONE,
					NONE,
					NONE))
				{
					error(
						_error_silent,
						"failed to load full screen pause game window");
				}
				break;
			case 2:
				if (widget_globals.active_widgets[controller_index])
				{
					if (game_time_get_paused() == TRUE)
						ui_widgets_close_all();
				}
				else if (!game_time_get_paused())
				{
					if (!ui_widget_load_by_name_or_tag(
						"ui\\shell\\solo_game\\pause_game\\pause_game_split_screen",
						NONE,
						NULL,
						controller_index,
						NONE,
						NONE,
						NONE))
					{
						error(
							_error_silent,
							"failed to load split screen pause game window");
					}
				}
				break;
			default:
				error(
					_error_silent,
					"the ui seems to be confused... assuming you are playing full-screen single player?");
				if (!ui_widgets_active())
				{
					if (!ui_widget_load_by_name_or_tag(
						"ui\\shell\\solo_game\\pause_game\\pause_game",
						NONE,
						NULL,
						controller_index,
						NONE,
						NONE,
						NONE))
					{
						error(
							_error_silent,
							"failed to load full screen pause game window");
					}
				}
				else
				{
					ui_widgets_close_all();
				}
				break;
			}
		}
	}
	/* This runs once a frame, several frames per tick on the native builds
	(port/linux/game/render_interpolation.c): count the lock down in 30 Hz
	ticks of real time, not in frames. */
	{
		static real leftover_ticks = 0.f;
		long ticks;

		leftover_ticks += main_get_seconds_elapsed() * TICKS_PER_SECOND;
		ticks = (long)leftover_ticks;
		leftover_ticks -= (real)ticks;
		widget_globals.pause_disabled_ticks =
			FLOOR(widget_globals.pause_disabled_ticks - ticks, 0);
	}

	return pause_pressed;
}

void process_ui_widgets(
	void)
{
	boolean widgets_processed = FALSE;
	boolean widget_deleted;
	boolean any_modal_widget_active;
	boolean pause_pressed;
	boolean modal_widget_active[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS];
	long widget_index;

	match_assert(
		"c:\\halo\\SOURCE\\interface\\ui_widget.c",
		644,
		widget_globals.initialized);
	widget_globals.current_system_milliseconds = system_milliseconds();
	/* port: save ui.map pictures outside texture resolution, before a later game map needs them. */
	if (we_are_at_the_main_menu)
	{
		extern void pc_menu_art_extract(void);
		pc_menu_art_extract();
	}
	ui_widgets_process_mouse();
	if (ui_widget_port_press_controller != NONE)
	{
		event_manager_post_button(ui_widget_port_press_controller, ui_widget_port_press_button);
		ui_widget_port_press_controller = NONE;
	}
	if (widget_globals.initialization_thread)
	{
		if (!thread_has_exited(widget_globals.initialization_thread))
			return;
		dispose_thread(widget_globals.initialization_thread);
		widget_globals.initialization_thread = NULL;
		ui_widgets_inhibit_processing(FALSE);
		switch (widget_globals.filesystem_check_result)
		{
		case _file_system_check_result_not_enough_free_space:
			if (bink_playback_in_progress())
				bink_playback_stop();
			display_error_abort_to_dashboard(
				_error_hard_drive_not_enough_free_space,
				TRUE);
			return;
		case _file_system_check_result_too_many_saved_games:
			if (bink_playback_in_progress())
				bink_playback_stop();
			display_error_abort_to_dashboard(
				_error_hard_drive_maximum_saved_game_files,
				TRUE);
			return;
		}

		return;
	}
	if (progress_bar_is_active())
		return;
	if (virtual_keyboard_active())
	{
		virtual_keyboard_process();
		event_manager_flush();

		return;
	}
	if (attract_mode_should_start())
	{
		attract_mode_start();

		return;
	}
	if (widget_globals.deferred_dashboard_error_code != NONE)
	{
		display_error_abort_to_dashboard(
			widget_globals.deferred_dashboard_error_code,
			widget_globals.deferred_dashboard_optional);
		widget_globals.deferred_dashboard_error_code = NONE;

		return;
	}
	if (widget_globals.deferred_errors[0].error_code != NONE ||
		widget_globals.deferred_errors[1].error_code != NONE ||
		widget_globals.deferred_errors[2].error_code != NONE ||
		widget_globals.deferred_errors[3].error_code != NONE)
	{
		for (widget_index = 0;
			widget_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS;
			widget_index++)
		{
			struct ui_widget_deferred_error *deferred_error =
				&widget_globals.deferred_errors[widget_index];

			if (deferred_error->error_code != NONE)
			{
				if (!we_are_at_the_main_menu &&
					!network_game_is_active() &&
					game_time_get() < DEFERRED_ERROR_DELAY_TICKS)
				{
					error(
						_error_silent,
						"waiting for %d ticks before displaying deferred errors",
						DEFERRED_ERROR_DELAY_TICKS);
				}
				else
				{
					display_error(
						deferred_error->error_code,
						deferred_error->local_player_index,
						deferred_error->modal,
						deferred_error->pause_game_time);
					deferred_error->error_code = NONE;
				}
			}
		}

		return;
	}
	/* port: Start dismisses the dropdown through the event filter before it
	can close an in-game pause screen. */
	pause_pressed = ui_dropdown.open ? FALSE : ui_check_for_pause_game();
	any_modal_widget_active = FALSE;
	for (widget_index = 0;
		widget_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS;
		widget_index++)
	{
		modal_widget_active[widget_index] =
			widget_globals.active_widgets[widget_index] &&
			widget_globals.active_widgets[widget_index]->widget_is_error_dialog == TRUE;
		any_modal_widget_active |= modal_widget_active[widget_index];
	}
	for (widget_index = 0;
		widget_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS;
		widget_index++)
	{
		struct widget_instance *widget = widget_globals.active_widgets[widget_index];
		boolean process_widget;

		if (modal_widget_active[widget_index] == TRUE)
			process_widget = widget && widget->widget_is_error_dialog == TRUE;
		else if (we_are_at_the_main_menu)
			process_widget = widget && !any_modal_widget_active;
		else
			process_widget = widget != NULL;
		if (process_widget == TRUE)
		{
			struct ui_widget_definition *definition =
				ui_widget_definition_get(widget->definition_tag_index);
			struct event_record event = {0};

			if (widget_globals.processing_inhibited ||
				!get_next_event(&event, widget->local_player_index))
			{
				/* the widget still gets one empty event so that its animation,
				auto-close timer and fade keep running */
				if (!pause_pressed)
				{
					event.controller_index = widget->local_player_index;
					widget_instance_process_one_event_recursive(
						widget,
						definition,
						&event,
						&widget_deleted);
				}
			}
			else
			{
				do
				{
					if (!pause_pressed && !ui_dropdown_filter_event(&event))
					{
						widget_instance_process_one_event_recursive(
							widget,
							definition,
							&event,
							&widget_deleted);
						if (widget_deleted == TRUE)
							break;
					}
					if (widget != widget_globals.active_widgets[widget_index])
						break;
				}
				while (get_next_event(&event, widget->local_player_index));
			}
			widgets_processed = TRUE;
			if (!widget_globals.active_widgets[widget_index] &&
				widget_globals.widget_stack[widget_index])
			{
				struct widget_stack_data data;

				pop_widget(&widget_globals.widget_stack[widget_index], &data);
				if (data.previous_widget_tag != NONE)
				{
					struct widget_instance *new_widget = ui_widget_load_by_name_or_tag(
						NULL,
						data.previous_widget_tag,
						NULL,
						data.local_player_index,
						NONE,
						NONE,
						NONE);

					if (new_widget)
					{
						widget_instance_set_focused_child_by_index(
							data.focused_child_parent_widget_tag,
							new_widget,
							data.focused_child_index);
					}
				}
			}
		}
	}
	if (widgets_processed)
		event_manager_flush();

	return;
}
