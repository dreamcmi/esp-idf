# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

set(g0_components soc hal esp_common esp_rom)  # also <arch>, i.e. xtensa or riscv, will be added below
set(g1_components spi_flash freertos log heap newlib esp_system esp_hw_support)
set(COMPONENTS ${g0_components} ${g1_components} main)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

idf_build_set_property(__BUILD_COMPONENT_DEPGRAPH_ENABLED 1)

project(g1_components)

set(extra_allowed_components
    ${CONFIG_IDF_TARGET_ARCH}
)

# These components are currently included into "G1" build, but shouldn't.
# After removing the extra dependencies, remove the components from this list as well.
set(extra_components_which_shouldnt_be_included
    # app_update gets added because of bootloader_support, spi_flash, espcoredump.
    #   bootloader_support, spi_flash, espcoredump should be removed from dependencies;
    app_update
    # of G1 components, bootloader is only included from spi_flash
    #   [refactor-todo]: see if this dependency from spi_flash can be made weak
    bootloader
    # bootloader_support is a dependency of efuse, app_update, spi_flash.
    #   efuse and app_update should be removed from G1 build;
    #   [refactor-todo]: see if the dependency from spi_flash can be made weak
    bootloader_support
    # [refactor-todo]: should cxx be in G1? Can it exist without FreeRTOS?
    cxx
    # [refactor-todo]: driver is a dependency of esp_pm, esp_timer, spi_flash, vfs, esp_wifi, ${IDF_TARGET}
    #   all of these should be removed from G1 except for spi_flash.
    driver
    # [refactor-todo]: efuse is a dependency of esp_hw_support, esp_system.
    #   Figure out if these components can exist without a dependency on efuse.
    #   If not, see if esp_hw_support can provide minimal efuse component replacement in G1 build.
    efuse
    # esp_pm is pulled in by freertos, can be made a weak dependency
    #   conditional on related Kconfig option. It is also used by esp_wifi, driver, mbedtls,
    #   all of which should be removed from G1-only build.
    esp_pm
    # esp_ringbuf is a dependency of driver, which should be removed.
    esp_ringbuf
    # esp_timer is a dependency of freertos, esp_event, esp_wifi, driver.
    #   For freertos, it can be made a weak dependency conditional on FREERTOS_RUN_TIME_STATS_USING_ESP_TIMER
    esp_timer
    # esptool_py is a dependency of bootloader, esp_wifi, app_update, partition_table, all of which
    #   should be removed from G1-only build.
    esptool_py
    # mbedtls is a dependency of bootloader_support (plus other easier-to-remove ones)
    #   it is hard to make it conditional, need to remove bootloader_support.
    mbedtls
    # partition_table is pulled in by app_update, esptool_py, bootloader; all to be removed
    partition_table
    # pthread is required by esp_system (for initialization only, can be made a weak dependency)
    #   and cxx. See also [refactor-todo] about cxx, can it work without pthread?
    pthread
)

set(expected_components
    ${COMPONENTS}
    ${extra_allowed_components}
    ${extra_components_which_shouldnt_be_included}
)

list(SORT expected_components)


idf_build_get_property(build_components BUILD_COMPONENTS)
list(SORT build_components)

if(NOT "${expected_components}" STREQUAL "${build_components}")
    message(FATAL_ERROR "Unexpected components list in G1 build. "
                        "Expected: ${expected_components}. "
                        "Actual: ${build_components}")
endif()
