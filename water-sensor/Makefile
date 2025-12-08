BUILD_DIR := build

$(BUILD_DIR)/water_level_sensor: water_level_sensor.c | $(BUILD_DIR)
	gcc $< -o $@ -lwiringPi

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)
