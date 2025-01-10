/**
 * @file hexahedron/loader/driver.c
 * @brief Hexahedron driver loader
 * 
 * Uses a JSON file located in the initial ramdisk to determine what drivers to load.
 * Keeps track of them in a hashmap which can be accessed.
 * 
 * Drivers should expose a driver_metadata field which gives a few things:
 * 1. The name of the driver
 * 2. The author of the driver (can be left as NULL) 
 * 3. The init function of the driver
 * 4. The deinit function of the driver
 * 
 * This field should be named 'driver_metadata' and be publicly exposed. It will be looked up by the ELF loader.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of reduceOS.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/loader/driver.h>
#include <kernel/loader/elf_loader.h>
#include <kernel/mem/alloc.h>
#include <kernel/mem/mem.h>
#include <kernel/fs/vfs.h>
#include <kernel/debug.h>
#include <kernel/panic.h>
#include <structs/json.h>
#include <structs/json-builder.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>

/* List of drivers */
list_t *driver_list = NULL;

/* Current environment */
int driver_current_environment = DRIVER_ENVIRONMENT_PRELOAD;

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER", __VA_ARGS__)


/**
 * @brief Find a driver by name and return data on it
 * @param name The name of the driver
 * @returns A pointer to the loaded driver data, or NULL
 */
loaded_driver_t *driver_findByName(char *name) {
    if (!name || !driver_list) return NULL;

    foreach(node, driver_list) {
        loaded_driver_t *data = (loaded_driver_t*)node->value;
        if (data && data->metadata && !strcmp(data->metadata->name, name)) { // !!!: unsafe strcmp
            return data;
        } 
    }

    return NULL;
}

/**
 * @brief Find a driver by its address and return data o n it
 * @param addr The address of the driver
 * @returns A pointer to the loaded driver data, or NULL
 */
loaded_driver_t *driver_findByAddress(uintptr_t addr) {
    if (!driver_list) return NULL;

    foreach(node, driver_list) {
        loaded_driver_t *data = (loaded_driver_t*)node->value;
        if (data && addr >= data->load_address && addr < (data->load_address + data->size)) {
            return data;
        } 
    }

    return NULL;
}

/**
 * @brief Handle a loading error in the driver system
 * @param priority The priority of the driver
 * @param error Error string
 * @param file The file
 */
static void driver_handleLoadError(int priority, char *error, char *file) {
    switch (priority) {
        case DRIVER_CRITICAL:
            // We have to panic
            kernel_panic_extended(DRIVER_LOAD_FAILED, "driver", "*** Failed to load driver '%s' (critical driver): %s\n", file, error);
            __builtin_unreachable();
        
        case DRIVER_WARN:       // TODO: Implement some sort of keyboard support into this or waits
            LOG(WARN, "Failed to load driver '%s' (warn): %s\n", file, error);
            printf(COLOR_CODE_YELLOW "Failed to load driver '%s': %s\n" COLOR_CODE_RESET, file, error);
            break;

        case DRIVER_IGNORE:
        default: 
            LOG(WARN, "Failed to load driver '%s' (ignore): %s\n", file, error);
            printf(COLOR_CODE_YELLOW "Failed to load driver '%s': %s\n" COLOR_CODE_RESET, file, error);
            break;
    }
}


/**
 * @brief Load a driver into memory and start it
 * @param driver_file The driver file
 * @param priority The priority of the driver file
 * @param environment The environment of the driver file
 * @param file The driver filename
 * @param argc Argument count
 * @param argv Argument data
 * @returns 0 on success, anything else is a failure/panic
 */
int driver_load(fs_node_t *driver_file, int priority, int environment, char *file, int argc, char **argv) {
    // First we have to map the driver into memory. The mem subsystem provides functions for this.
    uintptr_t driver_load_address = mem_mapDriver(driver_file->length);
    memset((void*)driver_load_address, 0, driver_file->length);

    // Now we can read the file into this address
    if (fs_read(driver_file, 0, driver_file->length, (uint8_t*)driver_load_address) != (ssize_t)driver_file->length) {
        // Uh oh, read error.
        driver_handleLoadError(priority, "Read error", file);
        mem_unmapDriver(driver_load_address, driver_file->length);
        return -1;
    }

    // Load from buffer
    uintptr_t elf = elf_loadBuffer((uint8_t*)driver_load_address, ELF_DRIVER);
    if (elf == 0x0) {
        // Load failed
        driver_handleLoadError(priority, "ELF load error (check to make sure architecture matches)", file);
        mem_unmapDriver(driver_load_address, driver_file->length);
        return -1;
    }

    // Find the metadata
    struct driver_metadata *metadata = (struct driver_metadata*)elf_findSymbol(elf, "driver_metadata");
    if (!metadata) {
        // No metadata
        driver_handleLoadError(priority, "No driver metadata (checked for driver_metadata symbol)", file);
        elf_cleanup(elf);
        mem_unmapDriver(driver_load_address, driver_file->length);
        return -1;
    }

    // Construct a bit of list data first
    loaded_driver_t *loaded_driver = kmalloc(sizeof(loaded_driver_t));
    memset(loaded_driver, 0, sizeof(loaded_driver_t));

    // Copy the metadata
    loaded_driver->metadata = kmalloc(sizeof(driver_metadata_t));
    memcpy(loaded_driver->metadata, metadata, sizeof(driver_metadata_t));

extern uintptr_t mem_driverRegion;  // !!!: BAD!!!!
                                    // !!!: Because ELF loading also has to map some other things (like SHT_NOBITS sections) in driver space, we do this.

    ssize_t driver_loaded_size = (ssize_t)(mem_driverRegion - driver_load_address);

    // Copy other variables
    loaded_driver->filename = strdup(file);
    loaded_driver->priority = priority;
    loaded_driver->environment = environment;
    loaded_driver->load_address = driver_load_address;
    loaded_driver->size = driver_loaded_size;

    // Set in list
    list_append(driver_list, (void*)loaded_driver);
    
    // Now we need to execute the driver. Let's go!
    int loadstatus = metadata->init(argc, argv);
    
    if (loadstatus) {
        // Didn't return 0 - cleanup.
        driver_handleLoadError(priority, "Init function did not return 0", file);
        elf_cleanup(elf);
        list_delete(driver_list, list_find(driver_list, (void*)loaded_driver)); // Will this cause issues?
        kfree(loaded_driver->metadata);
        kfree(loaded_driver->filename);
        kfree(loaded_driver);
        mem_unmapDriver(driver_load_address, driver_file->length);
        return -1;
    }


    printf("Loaded driver '%s' successfully.\n", metadata->name);

    // Load success!
    return 0;
}


/**
 * @brief Read a field in the JSON driver object
 * @param object The object to check
 * @param field The field to look for
 * @returns The value of the field or NULL 
 */
static json_value *driver_getField(json_value *object, char * field, json_type expected_type) {
    if (object->type != json_object) return NULL;

    for (unsigned int i = 0; i < object->u.object.length; i++) {
        if (!strncmp(object->u.object.values[i].name, field, strlen(object->u.object.values[i].name))) {
            // These don't have to end in a \0 but if their lengths match then its good
            if (strlen(object->u.object.values[i].name) != strlen(field)) continue;
            
            // Check the type
            json_value *obj = object->u.object.values[i].value;
            if (obj->type != expected_type) {
                kernel_panic_extended(DRIVER_LOADER_ERROR, "driver", "*** Field '%s' is corrupted in driver JSON (expected type %i)\n", field, expected_type);
                __builtin_unreachable();
            }

            // All good
            return obj;
        }
    }


    kernel_panic_extended(DRIVER_LOADER_ERROR, "driver", "*** Could not find field '%s' in driver JSON\n", field);
    __builtin_unreachable();
}

/**
 * @brief Load and parse a JSON file containing driver information
 * @param file The file to parse
 * @returns Amount of drivers loaded
 * 
 * @note This will panic if any drivers have the label of "CRITICAL"
 */
int driver_loadConfiguration(fs_node_t *file) {
    if (!file) kernel_panic(KERNEL_BAD_ARGUMENT_ERROR, "driver");

    // Read the file into a buffer
    uint8_t *data = kmalloc(file->length);
    memset(data, 0, file->length);

    if (fs_read(file, 0, file->length, data) != (ssize_t)file->length) {
        kernel_panic_extended(DRIVER_LOADER_ERROR, "driver", "*** Failed to read driver configuration file\n");
        __builtin_unreachable();
    }

    // Now we can load it as a JSON object
    json_settings settings = { 0 };
    settings.value_extra = json_builder_extra;
    char error[128];
    json_value *json_data = json_parse_ex(&settings, (char*)data, file->length, error);

    if (!data) {
        kernel_panic_extended(DRIVER_LOADER_ERROR, "driver", "*** Failed to parse JSON data of driver configuration file: %s\n", error);
        __builtin_unreachable();
    }

    
    // We can start parsing now. Check the version field
    json_value *version = driver_getField(json_data, "version", json_integer);
    if (version->u.integer != DRIVER_CURRENT_VERSION) {
        kernel_panic_extended(DRIVER_LOADER_ERROR, "driver", "*** Bad version field in JSON configuration\n");
        __builtin_unreachable();
    }

    // Now get the drivers array
    json_value *drivers_array = driver_getField(json_data, "drivers", json_array);

    // We can start parsing the JSON contents now
    int drivers = 0;
    for (int i = 0; i < (int)drivers_array->u.array.length; i++) {
        json_value *driver = drivers_array->u.array.values[i];
        
        if (!driver || driver->type != json_object) {
            kernel_panic_extended(DRIVER_LOADER_ERROR, "driver", "*** Corrupted driver object in drivers array\n");
            __builtin_unreachable();
        }

        // Get the filename field
        json_value *filename_obj = driver_getField(driver, "filename", json_string);
        char *filename = filename_obj->u.string.ptr;

        // Get the priority field
        json_value *priority_obj = driver_getField(driver, "priority", json_integer);
        int priority = priority_obj->u.integer;

        // Get the environment field
        json_value *environment_obj = driver_getField(driver, "environment", json_integer);
        int environment = environment_obj->u.integer;

        // Construct the full filename
        char full_filename[256];
        snprintf(full_filename, 256, "%s%s", DRIVER_DEFAULT_PATH, filename);

        // Try to open the driver
        LOG(INFO, "Loading driver \"%s\" with priority %i (expected environment %i)...\n", full_filename, priority, environment);
        
        fs_node_t *driver_file = kopen(full_filename, O_RDONLY);
        if (!driver_file) {
            driver_handleLoadError(priority, "File not found", filename);
        } else {
            char *arguments[] = { filename }; // by default just filename

            // Load the driver
            if (driver_load(driver_file, priority, environment, filename, 1, arguments) == 0) {
                drivers++;
            }
        }

        // Free values
        fs_close(driver_file);
        json_value_free(filename_obj);
        json_value_free(priority_obj);
        json_value_free(environment_obj);
    }

    json_value_free(json_data);
    kfree(data);

    LOG(INFO, "Successfully loaded %i drivers\n", drivers);
    return drivers;
}

/**
 * @brief Initialize the driver loading system (this doesn't load anything)
 */
void driver_initialize() {
    // Create the driver list
    if (!driver_list) driver_list = list_create("drivers");
    driver_current_environment = DRIVER_ENVIRONMENT_NORMAL;
}