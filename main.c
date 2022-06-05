/* Copyright 2020 Espressif Systems (Shanghai) PTE LTD
 *   
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <unistd.h>

#include "esp_loader.h"

#include "common.h"

#include "raspberry_port.h"

#define TARGET_RST_Pin 2
#define TARGET_IO0_Pin 3

#define DEFAULT_BAUD_RATE 115200
#define HIGHER_BAUD_RATE 460800
#define SERIAL_DEVICE "/dev/ttyS0"

static char* port = SERIAL_DEVICE;
static uint32_t baud = DEFAULT_BAUD_RATE;
static uint32_t en_gpio = TARGET_RST_Pin;
static uint32_t io0_gpio = TARGET_IO0_Pin;

static struct {
    const char* f_name;
    uint8_t* data;
    size_t size;
    uint32_t addr;
} parts[8];

static uint32_t n_parts = 0;

static void help(void) {
    printf("flasher [-p serial_port] [-b baud] [-e EN_gpio] [-0 IO0_gpio ] addr file ...\n");
}

int main(int argc, char** argv) {
    int c;

    opterr = 0;

    while ((c = getopt(argc, argv, "hp:b:e:0:")) != -1)
        switch (c) {
        case 'h':
            help();
            return 0;
        case 'p':
            port = optarg;
            break;
        case 'b':
            baud = atoi(optarg);
            break;
        case 'e':
            en_gpio = atoi(optarg);
            break;
        case '0':
            io0_gpio = atoi(optarg);
            break;
        case '?':
            if (optopt == 'c')
                fprintf(stderr, "Option -%c requires an argument.\n", optopt);
            else if (isprint(optopt))
                fprintf(stderr, "Unknown option `-%c'.\n", optopt);
            else
                fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
            return -1;
        default:
            abort();
        }

    printf("port = %s, baud = %d, en_gpio = %d, io0_gpio = %d\n", port, baud, en_gpio, io0_gpio);

    int i;
    for (i = optind; i < argc; i++) {
        if (*argv[i] == '0' && (*(argv[i] + 1) == 'x' || *(argv[i] + 1) == 'X'))
            parts[n_parts].addr = strtol(argv[i] + 2, NULL, 16);
        else
            parts[n_parts].addr = strtol(argv[i], NULL, 10);
        i++;
        if (i < argc) {
            parts[n_parts].f_name = argv[i];
            FILE* image = fopen(parts[n_parts].f_name, "r");
            if (image == NULL) {
                printf("Error:Failed to open file %s\n", argv[i]);
                return -1;
            }
            fseek(image, 0L, SEEK_END);
            parts[n_parts].size = ftell(image);
            rewind(image);
            parts[n_parts].data = (char*)malloc(parts[n_parts].size);
            if (parts[n_parts].data == NULL) {
                printf("Error: Failed allocate memory\n");
                return -1;
            }
            // copy file content to buffer
            size_t bytes_read = fread(parts[n_parts].data, 1, parts[n_parts].size, image);
            if (bytes_read != parts[n_parts].size) {
                printf("Error occurred while reading file");
                return -1;
            }
            fclose(image);
            n_parts++;
        } else {
            printf("Missing file name\n");
            return -1;
        }
    }
    const loader_raspberry_config_t config = {
        .device = port,
        .baudrate = baud,
        .reset_trigger_pin = en_gpio,
        .io0_trigger_pin = io0_gpio,
    };

    loader_port_raspberry_init(&config);

    if (connect_to_target(HIGHER_BAUD_RATE) == ESP_LOADER_SUCCESS) {
        for (i = 0; i < n_parts; i++)
            flash_binary(parts[i].data, parts[i].size, parts[i].addr);
    }

    loader_port_reset_target();
}
