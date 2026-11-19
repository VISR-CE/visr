"""A pending import reaches the guest without the tester launch dialog."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def test_tester_stage_bypasses_launch_menu():
    source = (ROOT / "port/ios/host/host_main.m").read_text()
    start = source.index("host_ios_prepare_assets(data_root);")
    end = source.index("host_benchmark_install();", start)
    startup = source[start:end]
    assert "if(host_map_pictures_import_stage())" in startup
    assert startup.index("if(host_map_pictures_import_stage())") < startup.index("host_benchmark_menu()")
    assert "host_data_updates_menu_start();" in startup
    assert "host_data_updates_menu_finish();" in startup
    assert "else if(!benchmark_suite" in startup


def test_import_forces_metal_before_window_creation():
    source = (ROOT / "port/ios/host/host_main.m").read_text()
    assert 'if(HALO_METAL_ONLY||host_map_pictures_import_stage())snprintf(renderer,sizeof(renderer),"metal");' in source
    assert 'const char *guest_renderer=host_map_pictures_import_stage()?"metal":' in source
