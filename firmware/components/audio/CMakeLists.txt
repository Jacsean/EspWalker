# 语音组件：默认编译 stub（离线可构建）。
if(CONFIG_APP_ENABLE_AUDIO)
    idf_component_register(
        SRCS "audio.c"
        INCLUDE_DIRS "include"
        PRIV_REQUIRES esp_driver_i2s)
else()
    idf_component_register(
        SRCS "audio_stub.c"
        INCLUDE_DIRS "include")
endif()
