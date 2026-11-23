# 摄像头组件：默认编译 stub（离线可构建）。
# 启用摄像头时：idf_component.yml 加 espressif/esp32-camera，
# sdkconfig.defaults 设 CONFIG_APP_ENABLE_CAMERA=y，随后自动编译 camera.c。
if(CONFIG_APP_ENABLE_CAMERA)
    idf_component_register(
        SRCS "camera.c"
        INCLUDE_DIRS "include"
        PRIV_REQUIRES espressif__esp32-camera)
else()
    idf_component_register(
        SRCS "camera_stub.c"
        INCLUDE_DIRS "include")
endif()
