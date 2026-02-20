# 2026-02-18T16:11:04.943951900
import vitis

client = vitis.create_client()
client.set_workspace(path="Lab2_assignment")

platform = client.create_platform_component(name = "platform",hw_design = "$COMPONENT_LOCATION/../../Lab2-VivadoFiles/design_1_wrapper.xsa",os = "standalone",cpu = "psu_cortexa53_0",domain_name = "standalone_psu_cortexa53_0",architecture = "64-bit",compiler = "gcc")

vitis.dispose()

