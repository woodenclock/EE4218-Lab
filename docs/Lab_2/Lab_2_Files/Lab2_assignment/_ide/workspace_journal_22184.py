# 2026-02-18T16:07:33.151014600
import vitis

client = vitis.create_client()
client.set_workspace(path="Lab2_assignment")

status = client.add_platform_repos(platform=["/c:/Users/ELVIS/EE4218-Lab/EE4218-Lab/docs/Lab_2/Lab_2_Files/platform/export/platform"])

comp = client.create_app_component(name="app_component",platform = "$COMPONENT_LOCATION/../../platform/export/platform/platform.xpfm",domain = "standalone_psu_cortexa53_0")

vitis.dispose()

