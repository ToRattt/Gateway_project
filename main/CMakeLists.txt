idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES
        board
        app_config
        uart_debug
        sim4g_a7680c
        network_mrg
        mqtt
        json_builder
        ble_client
)