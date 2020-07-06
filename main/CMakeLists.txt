idf_component_register(SRCS ./app_driver.c ./app_main.c ./bh1750.c ./i2cdev.c ./sht3x.c 
                       INCLUDE_DIRS ".")

target_add_binary_data(${COMPONENT_TARGET} "server.crt" TEXT)