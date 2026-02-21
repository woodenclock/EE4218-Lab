# 2026-02-20T22:24:23.819728700
import vitis

client = vitis.create_client()
client.set_workspace(path="Lab2_assignment")

platform = client.get_component(name="platform")
status = platform.build()

comp = client.get_component(name="app_component")
comp.build()

