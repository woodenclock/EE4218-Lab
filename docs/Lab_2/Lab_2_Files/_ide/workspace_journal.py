# 2026-02-17T16:31:07.387408300
import vitis

client = vitis.create_client()
client.set_workspace(path="Lab_2_Files")

platform = client.create_platform_component(name = "platform",hw_design = "$COMPONENT_LOCATION/../Lab-2-Files/design_1_wrapper.xsa",os = "standalone",cpu = "psu_cortexa53_0",domain_name = "standalone_psu_cortexa53_0",architecture = "64-bit",compiler = "gcc")

platform = client.get_component(name="platform")
status = platform.build()

comp = client.get_component(name="xuartps_hello_world_example")
comp.build()

